# Magpie HDR/SDR 全链路源码审计

审计范围：`Magpie-0.6.5-research` 当前源码、`Magpie-src` 基线、需求文档
`docs/experimental/HDR_COMPATIBILITY_ARCHITECTURE.md` 与
`HDR_MECHANICAL_IMPLEMENTATION_REQUIREMENTS.md`，以及
`E:\Magpie-0.6.5-build\bin\x64\Release\logs\magpie.log` 的实际启动记录。

本报告属于 review 模式，只读分析。报告中的“修复”表示应采用的源码改动顺序，当前文件没有改动生产代码。

## 结论摘要

当前黑屏、HDR 过暗和关闭兼容后部分效果失效，来自多个相互叠加的合同断裂：

1. HDR 捕获的数值域与后续适配器声明不一致。WGC FP16 线性 scRGB 在捕获处理器中保持相对 scRGB 数值，后续 `HdrSurfaceAdapter` 却按绝对 nits canonical 解释，并在发布阶段再次除以 80。
2. `FrameSourceBase::GetOutput()` 在会话初始化和首帧到达前返回原始 `_output`，首帧处理后切换为 HDR processor 的 canonical texture。`Renderer::_BuildEffects()` 和 `EffectDrawer::_hdrInputSource` 在切换发生前已经保存了原始指针，效果链随后读取错误的资源语义。
3. HDR `EffectDrawer` 把 route 的格式直接写入 `_textures[0]`/`_textures[1]`，生产 shader CSO 仍按照 `EffectDesc` 编译时的原始纹理格式生成。SRV/UAV 的实际格式因此可能与 shader 类型不匹配，输出纹理保留初始化清零值，最终呈现为黑屏。
4. research 分支的 `NativeEffectBackendFactory` 把基线的 ZeroMV/Jitter/OpticalFlow 专用 backend 合并到普通 SR wrapper，改变了 SDR 及 HDR 关闭状态的运动、抖动、深度和辅助资源合同。
5. 前端发布链新增了 FP16/scRGB、共享环和 pass-through 多重转换，但 `_CopySceneToTarget()` 只做 `CopyResource`，没有验证源/目标格式或输出合同；任一阶段失败后，前端仍可能提交清零的稳定纹理。
6. 运行日志只证明了初始化、adapter dispatch、DLSSNR Evaluate 和“首帧提交”，没有任何最终纹理读回或像素统计，无法把当前图像判定为有效。

## 需求合同

需求文档规定：HDR 关闭时保持原有 SDR 捕获、效果、backend handoff、publication 和 presenter 语义；HDR 开启时，捕获之后统一形成 canonical `R16G16B16A16_FLOAT`，每个效果边界执行“canonical → route input → 生产效果 → route output → canonical”，最后只在 presentation terminal 编码到目标显示协议。捕获方式不能因 HDR 开关改变，纹理格式不能单独推断颜色含义，route 选择必须来自结构化协议，未知 route 必须显式走兼容回退并记录诊断。需求还要求 terminal frame generation 单独建模，不能把普通 FP16 链直接交给不接受 FP16/scRGB 的 backend。

## 1. 捕获与 canonical 生命周期

### 1.1 指针生命周期断裂（最高优先级）

`FrameSourceBase.h:50-54` 的 `GetOutput()` 在 `_hdrEnabled` 且 `_hdrProcessor.GetCanonicalTexture()` 存在时返回 canonical，否则返回 `_output`。`FrameSourceBase.cpp:43-66` 在 `_Initialize()` 阶段只创建并准备 processor；真正填充 canonical 发生在 `Update()` 的 `FrameSourceBase.cpp:78-110`。因此初始化顺序是：

```text
FrameSource::_Initialize -> raw _output
Renderer::_BuildEffects -> 读取 raw _output
FrameSource::Start -> 首帧到达
FrameSource::Update -> 填充 canonical
FrameSource::GetOutput -> 改为 canonical
```

`Renderer.cpp:1569-1579` 在首帧以前取得 `inOutTexture` 和 `initialHdrFrame`；`EffectDrawer.cpp:89-105` 把传入纹理保存到裸指针 `_hdrInputSource`。首帧以后 `Renderer` 的 `_frameSource->GetOutput()` 已经是另一张 texture，而 drawer 仍从旧 `_hdrInputSource` 读取。这个状态同时破坏普通 HLSL drawer、native backend 输入、Frame Guidance 输入和 `_CreateSharedTexture()` 的输出合同。

### 1.2 直接修复方案

把 canonical texture 的对象生命周期前移到 FrameSource 初始化阶段，并让 `GetOutput()` 从初始化到销毁始终返回同一张 canonical texture；首帧以前只保持 `hdrFrameReady=false`，不允许 backend 运行。`_output` 继续作为捕获 API 的原始接收面，processor canonical 作为稳定的交付面。所有 duplicate 检查可以继续针对 `_output`，但效果链、Frame Guidance、DLSSNR 和发布链必须只使用 `GetOutput()` 返回的稳定 canonical。resize 时先重建 canonical，再以同一指针更新所有 drawer/backend，最后才恢复运行。

## 2. 捕获数值域与适配器数值域

### 2.1 当前实现的实际变换

`GraphicsCaptureFrameSource.cpp:79-87` 在 HDR 模式创建 FP16 `_output`，`GraphicsCaptureFrameSource.cpp:103-112` 将其声明为线性 scRGB、scene-referred、80 nit reference white。运行日志 `20:11:40.181` 也确认：`sourceFormat=10`、`sourceTransfer=Linear`、`sourceRange=SceneLinear`、`referenceWhite=80`。

`HdrCaptureProcessor.cpp:54-61` 当前 `ToCanonical()` 对 `inputTransfer == Linear` 直接保留采样值，对 sRGB/HLG 乘 `referenceWhiteNits / 80`，对 PQ 除以 80。这个实现形成的是“线性 scRGB、1.0 对应 80 nit”的 canonical 数值域。

`HdrSurfaceAdapter.cpp:118-121` 的 mode 4 又把值直接当作绝对 nits并执行 `value / 80`；`Renderer.cpp:3032-3051` 在每次 HDR 发布前调用该 mode 4。于是 WGC 的 scRGB `1.0` 被发布为 `0.0125`，白场整体变暗约 80 倍。反向的 SDR 兼容路径也按 `referenceWhiteNits / 80` 参与归一化，数值域与 capture canonical 的定义没有统一。

### 2.2 需求对齐方案

需要在架构层固定一种 canonical 语义，并让所有代码使用同一合同。按照需求文档中对 Windows scRGB 的描述，最稳定的选择是 canonical 使用线性 scRGB 相对值：`1.0 == 80 nit`，HDR 高光可大于 1。这样：

- WGC FP16 Linear 输入直接复制到 canonical，不再做“绝对 nits”解释。
- 8-bit sRGB 输入先做 sRGB EOTF，得到线性相对值，再写入 canonical。
- PQ 输入先解码到 nits，再除以 80 写入 canonical。
- `ConvertHdrToScRgb()` 变为同域复制/必要的 primaries 转换，不能再次除以 80。
- SDRCompatible 的 tone-map 以 canonical 相对值为输入；inverse 也返回相对 scRGB 值。
- 日志字段明确写 `canonicalValueDomain=scRGB-relative` 或采用另一套全局定义，禁止“注释写 absolute nits、shader 按 scRGB”这种混合状态。

当前代码和注释在 `FrameSourceBase.cpp:250-253`、`HdrSurfaceAdapter.h:42-44`、`PassThroughFrames.cpp:29-33` 使用 absolute-nits 表述，而 shader 行为使用 scRGB-relative；这些说明必须与最终统一语义同步。

## 3. EffectDrawer 与 shader descriptor 合同

### 3.1 格式替换发生在 shader 编译之后

`EffectCompiler.cpp:1221-1233` 使用 `EffectDesc::textures[*].format` 生成每个 pass 的 `Texture2D<T>` 和 `RWTexture2D<T>` 类型。CSO 已经绑定了这些编译时类型。`EffectDrawer.cpp:92-114` 在运行时却用 `HdrEffectBoundary::SelectedRoute()->inputFormat/outputFormat` 创建 `_textures[0]` 和 `_textures[1]`，这两个格式可能与 `desc.textures[0/1].format` 不同。`EffectDrawer.cpp:645-674` 随后仍按原 pass texture 索引获取 SRV/UAV，没有重编 shader，也没有验证 route 格式与 `EffectDesc` 类型兼容。

典型冲突是 Group-C 的 `SdrRoute()` 在 `EffectProtocolCatalogC.h:10-31` 声明输入为线性、输出为 sRGB，而实际 HLSL effect 可能是 `R8G8B8A8_UNORM` 或 FP16；route 描述本身没有把“适配器 scratch texture”和“生产 shader texture”分开。对于多通道/中间纹理效果，替换 0/1 还会影响 pass 之间的 alias 和尺寸推导。

### 3.2 黑屏机制

`EffectDrawer::Draw()` 在 `EffectDrawer.cpp:234-247` 中，如果 `PrepareHdrInput()` 失败只记录错误并直接返回；如果 pass dispatch 后 `CompleteHdrOutput()` 失败，函数仍结束，`_hdrOutput` 保持初始化清零值。`Renderer::_BackendRender()` 在 `Renderer.cpp:2886-2920` 对 native 失败也只记录后继续；最后仍调用 `_CompleteBackendFrame(effectsOutput, ...)`。这使任何边界错误都变成“发布黑帧”，而不是阻止提交并保留上一张有效帧。

### 3.3 修复方案

保留生产 `EffectDesc` 的 texture 格式给 shader 和 `_textures[]`，为 route 转换单独建立 `routeInputScratch`、`routeOutputScratch`。执行顺序固定为：

```text
canonical input
  -> adapter scratch (route format)
  -> production shader/backend using its declared EffectDesc resources
  -> adapter scratch output
  -> canonical output
```

当 route 格式与生产 shader 格式相同，可以复用资源；只有经过结构化兼容检查才允许复用。每个 pass 必须验证 SRV/UAV 创建成功、尺寸和 format 与 CSO descriptor 相符。`PrepareHdrInput()` 或 `CompleteHdrOutput()` 失败时，当前帧应终止发布并保留上一帧，不能把清零的 `_hdrOutput` 交给 publication。

## 4. Native backend 工厂与 SDR 回归

`Magpie-src` 基线的 `NativeEffectBackendFactory.cpp:111-159` 使用：

- `DLSSSRUpscaler` 只处理 `DLSS_SR` 及其显式 legacy 参数，并区分 `isJitter`、`useMotionVectors`、`useEstimatedDepth`；
- `FSR2ZeroMVUpscaler` 处理 FSR2 ZeroMV/Jitter/OpticalFlow；
- `FSR3ZeroMVUpscaler` 处理 FSR3/FSR4 ZeroMV/Jitter/OpticalFlow；
- `XeSSZeroMVUpscaler` 处理 XeSS ZeroMV/Jitter/OpticalFlow。

research 版 `NativeEffectBackendFactory.cpp:78-135` 将这些 ID 全部归入 `DLSSSRUpscaler`、`FSR2Upscaler`、`FSR3Upscaler` 或 `XeSSUpscaler` 的统一分支，并用 `ParseOpticalFlowRequest()` 只对部分路径设置 motion。这个改动改变了 legacy backend 的初始化参数、depth/motion fallback、jitter 开关、输出资源和时序历史，违反需求中“HDR 关闭时 SDR 行为不变”。

同文件 `:43-53` 还只识别 `FrameGuidance_Motion` 与 `FrameGuidance_Confidence`，基线 `:41-54` 同时支持 `Depth` 和 `DepthResidual`。这会让诊断效果本身失效，进一步降低对 Frame Guidance 和 DLSSNR 的可观测性。

修复顺序：先完整恢复基线 include 和分派分支，保持所有 legacy 类的原始构造参数；HDR 适配器通过 `NativeEffectBackend` 外层 context 接入，不能替换 backend 类型。对每个 native 类分别声明支持的 route，未验证的 HDR route 走显式 SDRCompatible scratch，SDR 关闭时完全跳过 HDR context。

### 4.1 Bicubic 追加后的越界

`Renderer::_BuildEffects()` 在 `Renderer.cpp:1655-1669` 追加 Bicubic 后调用 `_UpdateHdrEffectBoundaryContexts()`。该函数在 `Renderer.cpp:1714-1724` 遍历 `_effectDrawers.size()`，却用同一索引读取 `_runtimeEffectOptions[i]`。追加 Bicubic 时 drawer 数量比 runtime option 数量多一项，最后一次迭代越过 `_runtimeEffectOptions` 末尾。`_ResizeEffects()` 在 `Renderer.cpp:2088-2134` 也会在追加/移除 Bicubic 后重新调用该函数。此越界足以损坏 route context、backend 指针或后续发布状态，并直接解释“有缩放比例/窗口尺寸时黑屏”的不稳定表现。修复方式是按 runtime effect 数量更新真实效果，另为 Bicubic 构造明确的独立 boundary，或让 Bicubic 使用其 drawer 自带的 canonical context，禁止用 runtime option 数组越界索引。

### 4.2 Group-C R8 route 的 transfer 字段方向错误

`EffectProtocolCatalogC.h:10-31` 的 `SdrRoute()` 将 `inputTransfer=Linear/inputRange=SceneLinear`、`outputTransfer=SRGB` 写入所有 R8 效果。`EffectDrawer::PrepareHdrInput()` 实际把 `route->inputTransfer` 作为 `ConvertHdrToSdr(..., outputTransfer)` 参数 (`EffectDrawer.cpp:506-514`)，所以这些效果收到线性 R8 值；`CompleteHdrOutput()` 又把 `route->outputTransfer` 作为 `ConvertSdrToHdr(..., inputTransfer)` (`EffectDrawer.cpp:534-542`)，按 sRGB 解码同一线性值，造成二次 EOTF、过暗和颜色偏移。R8 SDR-compatible route 的输入/输出 transfer 应描述 backend 真实的编码，通常为 `SRGB/SRGB`，range 为 `Full/Full`；route 字段方向和调用语义需要统一后再推广到 Group-A/B/C。

## 5. Publication、frontend 和 presenter

### 5.1 新增的多重颜色边界

research 版在 `Renderer.cpp:2310-2426` 将共享 publication texture 改为 HDR 时 FP16，在 `Renderer.cpp:3032-3051` 先把 canonical 送入 `_hdrPresentationTexture`，再复制到 shared texture；`Renderer.cpp:617-754` 前端又复制到 stable base/presented base；`Renderer.cpp:757-774` 最后直接复制到 presenter frame。`AdaptivePresenter.cpp:33-84` 和 `:416-424` 也把 swap chain/DirectComposition surface 改成 FP16，并设置 scRGB 色彩空间。

`_CopySceneToTarget()` 没有检查 `scene`、`target` 的 format、尺寸、sample count、bind flags，也没有处理“只允许同格式 CopyResource”的失败结果。`CopyResource` 的 HRESULT 不返回给调用者，失败后 presenter 仍可能提交刚刚清空的目标。

### 5.2 Pass-through 分支

`PassThroughFrames.cpp:11-35` 的 reference shader按 output 尺寸采样 input，并在 HDR 时执行 `/80`。`InitializeBackend()` 以 `outputDesc` 决定 reference 尺寸 (`:65-71`)，但 input 可能是捕获尺寸、output 可能是最终效果尺寸；当两者不同，采样 UV 与边界不一致。`ReferenceConstants` 的 exposure、sdrWhiteNits、shoulder 在 shader 中没有使用 (`:94-104`)，导致诊断参数与实际行为脱离。

需求要求 pass-through 与 processed publication 使用同一输出颜色合同。当前 reference branch 自己做了一套 `/80`，processed branch 另做 canonical→scRGB，两个分支无法保证逐点可比。

修复方案：pass-through 直接保存 canonical stable frame；在唯一的 presentation encoder 处与 processed frame 使用相同的颜色转换。reference shader只负责尺寸/滤波和 alpha，禁止重复颜色缩放。输入、输出尺寸不同时采用明确的 source-to-destination sampling contract，并记录 source/destination descriptor。

### 5.3 Screenshot/export 也跨越了错误的格式边界

`Renderer::_TakeScreenshotImpl()` 对 effect output 使用 `_effectDrawers[effectIdx].GetOutputTexture()` (`Renderer.cpp:3272-3275`)，而 HDR drawer 的 canonical 对外输出是 `GetExternalOutputTexture()`；随后函数把 effect descriptor 的原始 `EffectIntermediateTextureFormat` 当作读回格式。显示图导出分支还把 frontend FP16/scRGB texture 按 `R8G8B8A8_UNORM`/PNG 语义处理 (`Renderer.cpp:3263-3271`)。这会把 HDR 纹理以错误的通道/数值域编码，产生黑图或过暗图，即使 presenter 本身已经提交成功。导出必须先选择 canonical/presentation domain，再执行一次明确的 HDR-to-SDR PNG 编码；中间 pass 才使用其真实 descriptor 格式。

## 6. 运行日志证据

`magpie.log` 的 20:11:40 会话记录：

```text
sourceFormat=10
sourceTransfer=Linear
sourceRange=SceneLinear
sourceReferenceWhiteNits=80
canonicalFormat=10
selectedAdapterProfile=DirectFP16
selectedRouteId=(not set)
```

随后每帧出现：

```text
HDR adapter dispatch: mode=0 input=10 output=28
HDR adapter dispatch: mode=0 input=28 output=10
HDR adapter dispatch: mode=4 input=10 output=10
DLSSNR STATUS ... sourceFormat=28 ... experimentalHdrPath=false
First frontend frame submitted by regular renderer
```

这组日志证明 WGC 首帧、DLSSNR Evaluate 和 presenter 首次提交都发生了；`selectedRouteId=(not set)` 说明 capture diagnostics 没有关联具体效果 route；全日志没有最终 publication texture 的 min/max/mean、NaN/Inf、读回 hash 或截图路径。`First frontend frame submitted` 只表示 API 提交成功，不能证明像素内容有效。

## 7. 点对点修复顺序

1. **恢复 SDR 基线**：恢复 `NativeEffectBackendFactory` 的 legacy backend 类和 Diagnostics Depth/DepthResidual 分支；让 HDR 关闭时 `EffectDrawer`、Renderer publication、presenter 与 `Magpie-src` 保持原始行为。
2. **稳定 FrameSource 输出指针**：canonical texture 在初始化时创建并从始至终作为 `GetOutput()`；首帧 ready 之前禁止 backend draw。
3. **统一 canonical 数值域**：在代码、shader、注释、metadata 和日志中选择唯一的 scRGB-relative 或 absolute-nits 语义；本审计建议采用 `1.0 == 80 nit` 的 scRGB-relative 语义，以匹配 Windows WGC/Advanced Color。
4. **拆分 route scratch 与 production textures**：route 转换资源独立于 `EffectDesc` 生产资源，禁止运行时改写已编译 shader 的 texture format。
5. **统一唯一 presentation encoder**：processed 与 pass-through 都先保留 canonical，只有 publication→presenter 边界做一次 scRGB/HDR10 编码；所有 Copy/Present 调用检查 HRESULT 和 descriptor。
6. **失败帧隔离**：任一 effect/backend/adapter/publish 阶段失败时丢弃当前帧并保留上一张有效帧，禁止清零纹理继续发布。
7. **真实生产 readback bridge**：bridge 直接调用生产 `FrameSourceBase`、`EffectDrawer`、native backend、publication 和 presenter 资源；逐阶段保存 GPU readback，记录 format/size/finite/min/max/mean/hash。没有完整阶段证据的矩阵行保持失败状态。

## 8. 验收条件

每个效果组、HDR 开关两种状态都必须满足：

- capture method 与需求一致，首帧 `frameId=1` 使用真实捕获；
- effect shader CSO 的 descriptor 与实际 SRV/UAV format/size 一致；
- native backend 类型、辅助输入、jitter/depth/motion 语义与基线或已验证 route 一致；
- canonical 输入/输出在每个边界保持统一数值域，alpha 明确；
- publication、pass-through、presenter 只执行一次目标颜色编码；
- GPU readback 全部 finite、非全黑、没有 NaN/Inf，输出 hash 与输入 hash 不同；
- adapter、backend、publication、presenter 每一层都有实际执行记录；
- HDR 关闭矩阵与基线图像及路径通过回归比较；HDR 开启矩阵覆盖 SDR-compatible、DirectFP16、BoundedHDR、ConditionalFP16 和 terminal 分支；
- 任意一行失败都单独保留失败证据，不能用 aggregate 初始化成功代替图片通过。
