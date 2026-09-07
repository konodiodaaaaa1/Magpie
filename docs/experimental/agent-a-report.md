# Effect protocol audit — group A

审计范围：Anime4K、CAS、CRT、CuNNy、CuNNy2、Diagnostics、FSRCNNX、FXAA、MLAA。结论同时记录公开证据矩阵与当前 Magpie shader 的可观察事实；shader 中没有声明的输入/输出格式保持 `Unknown`。`//!CAPABILITY FP16` 仅代表编译/数学能力，HDR 传输协议仍需独立证据。

## 总结

| 效果 | 实际 shader 输入/输出 | 明确的辅助纹理 | 数值/transfer/alpha 事实 | HDR profile | 代码缺口 |
|---|---|---|---|---|---|
| Anime4K | `INPUT`/`OUTPUT` 未声明格式；大多数 CNN 变体中间面 `R16G16B16A16_FLOAT`；`Thin_HQ` 的梯度面 `R16G16_FLOAT` | CNN 变体 2–16 个 FP16 中间面；无外部纹理 | CNN 以正负分支 `max(x,0)`/`max(-x,0)` 工作；大量最终写入 `MF4(...,1)`，alpha 固定 1；Denoise 仅 RGB 并固定 alpha 1；Thin_HQ 直接采样 RGBA 并保留 alpha | `Unknown`，generic SDR fallback | 缺少统一输入/输出格式、transfer、原色域、范围和 alpha 契约；当前路由应保持 generic SDR fallback |
| CAS | `INPUT`/`OUTPUT` 未声明格式；`CAS.hlsl` 有 `//!CAPABILITY FP16`，`CAS_Scaling.hlsl` 无 | 无 | 读取 `.rgb`；`saturate` 作用于邻域振幅和最终 RGB；所有输出写入 alpha 1；shader 没有 transfer 解码/编码 | `SDRCompatible`（现有证据允许 FP16 参考实现，当前 Magpie shader 仍是有界 RGB 路径） | 需补充具体 Magpie 资源格式、线性/伽马入口、峰值范围和 alpha 规则；当前 FP16 capability 不足以建立 HDR-native 路径 |
| CRT | 除 `GTU_v050.tex1` 明确为 `R16G16B16A16_FLOAT` 外，`INPUT`/`OUTPUT` 未声明格式 | GTU `tex1` FP16；其余无 | Easymode/Geom/Hyllian/Lottes 使用 gamma `pow`；GTU RGB 多处 `clamp(...,0,1)`，最终 alpha 1；Hyllian 输出 RGB clamp 到 `[0,1]` 后 gamma 输出 | `Unknown`，generic SDR fallback | 每个 preset 都是独立协议；gamma 参数、clamp、mask/scanline 对 HDR 的行为尚未建立；GTU 仅有内部 FP16 证据 |
| CuNNy | `INPUT`/`OUTPUT` 未声明格式，2x–8x 输出尺寸为 `INPUT_WIDTH/HEIGHT * 2` | 所有 `t0..t7` 中间面明确 `R8G8B8A8_SNORM` | RGB 转 YUV/网络张量；ReLU 正负分支；NVL 输出对 Y 分量使用 `saturate`，写回 RGB 后 alpha 固定 1 | `Unknown`，generic SDR fallback | 现有矩阵把格式记为 unspecified；实际内部 SNORM 是可靠本地事实，但模型归一化区间、输入格式、transfer、alpha 尚未形成 HDR 契约 |
| CuNNy2 | `INPUT`/`OUTPUT` 未声明格式，全部输出尺寸为 2x | 所有 `T0..T15` 明确 `R8G8B8A8_UNORM` | RGB→YUV，网络层使用 `max(x,0)`；out-shuffle 对 Y 使用 `saturate`；输出 alpha 固定 1 | `Unknown`，generic SDR fallback | 公开矩阵没有独立 CuNNy2 上游；本地 shader 明确 UNORM 中间面和 `[0,1]` 型输出，仍缺模型归一化/transfer/峰值证据 |
| Diagnostics | `INPUT`/`OUTPUT` 未声明格式，同尺寸 | 无 | Confidence 直接 `return INPUT.SampleLevel(...)`；Motion 同样直接采样返回；shader 层 alpha 保留输入值；显示增益参数仅在对应 native/backend 侧生效 | `Unknown`，generic SDR fallback | 诊断输入的真实来源（MV/置信度资源格式、范围）由 native backend 决定；通用 shader 的 HDR 图像语义仍待定义 |
| FSRCNNX | `INPUT`/`OUTPUT` 未声明格式，输出尺寸 2x；中间面明确 `R16G16B16A16_FLOAT` | `featureMap1/2`、`tex1..tex4` 均 FP16 | 先以 `0.299/0.587/0.114` 取 luma；多层 leaky-ReLU 使用 `max(x,0)+C*min(x,0)`；最终 RGB 加回输入 RGB，alpha 固定 1 | `Unknown`，generic SDR fallback | FP16 中间面没有给出输入/输出色彩协议；luma 权重、训练归一化、负值/超 1 值范围、alpha 恢复均待验证 |
| FXAA | `INPUT`/`OUTPUT` 未声明格式，同尺寸；Linear sampler | 无 | FXAA luma 估计 `rgb.y*(0.587/0.299)+rgb.x`，注释范围约 `0..2.9632`；最终 RGB 经邻域插值，wrapper 写 alpha 1；无 transfer 处理 | `Unknown`，generic SDR fallback | 当前实现缺统一格式、范围、transfer、alpha 透传契约；阈值是显示相关量，直接场景线性 HDR 需验证 |
| MLAA | `INPUT`/`OUTPUT` 未声明格式，同尺寸；`edgeMask`=`R8G8_UNORM`；`edgeCounts`=`R8G8B8A8_UNORM` | `edgeMask`、`edgeCounts` | luma 为 `dot(rgb,[0.2126,0.7152,0.0722])`；edge mask 二值写入 UNORM；edge count 量化为 `0..15 / 15`；混合在平方域后 `sqrt`；最终返回原 `color`，alpha 保留 | `Unknown`，generic SDR fallback | 辅助面是明确 SDR/UNORM 边界；主面格式、transfer、HDR 阈值和 alpha 语义缺少公共契约 |

## 逐项证据

### Anime4K

实际文件集合为 `src/Effects/Anime4K/*.hlsl`。`Anime4K_Restore_*`、`Anime4K_Upscale_*`、`Anime4K_Upscale_Denoise_*`、`Anime4K_3D_*`、`Anime4K_Upscale_GAN_*` 均带 `//!CAPABILITY FP16`，并将网络中间面声明为 `R16G16B16A16_FLOAT`；典型声明位于 `Anime4K_Restore_L.hlsl:20-41`。`Anime4K_Thin_HQ.hlsl` 的梯度纹理为 `R16G16_FLOAT`，主输入输出仍未声明格式。多数网络最终写入 `MF4(result, 1)` 或 `MF4(result + INPUT.rgb, 1)`，例如 `Anime4K_Restore_L.hlsl:690`、`Anime4K_Upscale_L.hlsl:647-659`，因此 alpha 由 shader 固定为 1。三个 bilateral denoise 变体只采集 RGB，最终 `float4(...,1)`，证据见 `Anime4K_Denoise_Bilateral_Mean.hlsl:68-83,121`；`Thin_HQ` 的 warp pass 直接写入 `INPUT.SampleLevel`，这是该变体保留 alpha 的局部事实。数值上，CNN 激活显式拆分正负分量，`max(x,0)` 与 `max(-x,0)` 让网络内部允许负特征，同时没有输入 transfer、峰值或 scene-linear 标注。公开证据矩阵 `HDR_PROTOCOL_EVIDENCE.md:34,67-116` 仅证明上游是 Anime4K 用户 shader，并把公共 I/O、范围、色彩空间、HDR 记为缺失。当前应保持 `Unknown`，由通用 SDR fallback 进入效果边界；FP16 中间面可作为资源分配事实记录，现有证据不足以升级为 HDR-native profile。代码缺口是：按具体 preset 记录主面格式、采样 transfer、可接受负值/超 1 值、alpha policy，并在有测试证据后再开放 `ConditionalFP16`。

### CAS

`CAS.hlsl` 和 `CAS_Scaling.hlsl` 都是单 pass、`INPUT`→`OUTPUT`，无 `//!FORMAT`；前者带 `//!CAPABILITY FP16`（`CAS.hlsl:3-5`），后者没有。两者只读取 RGB，最终输出多处写成 `MF4(...,1)`，例如 `CAS.hlsl:262-284` 与 `CAS_Scaling.hlsl:661-683`，alpha 固定为 1。CAS 核心对邻域振幅和最终像素使用 `saturate`，如 `CAS.hlsl:115-121,137-139`，实际 shader 因而具有 `[0,1]` 型 RGB 钳制行为；代码中没有 sRGB/PQ/HLG 解码或编码。公开证据矩阵 `HDR_PROTOCOL_EVIDENCE.md:35,118-197` 记录 AMD 参考 CLI 的 `R8G8B8A8_UNORM` 与 `R16G16B16A16_FLOAT`，以及官方文档的线性输入说明；这些证据属于参考实现/SDK 层，当前 Magpie shader 的资源声明仍为空。现有 catalog 的 `SDRCompatible` 决策与本地实现一致：FP16 编译能力和参考 CLI FP16 选项支持后续实验方向，当前 shader 的饱和/alpha 规则尚未构成 HDR-native 保障。缺口集中在具体 Magpie 资源分配、输入输出 transfer、峰值范围、alpha 透传策略；需要独立的 FP16 无钳制测试后才可调整 profile。

### CRT

文件集合为 `CRT_Easymode.hlsl`、`CRT_Geom.hlsl`、`CRT_Hyllian.hlsl`、`CRT_Lottes.hlsl`、`GTU_v050.hlsl`。除 GTU 的 `tex1` 明确 `//!FORMAT R16G16B16A16_FLOAT`（`GTU_v050.hlsl:80-90`）外，主 `INPUT`、`OUTPUT` 和其他 preset 都无格式声明。Easymode、Geom、Hyllian、Lottes 都对输入做 gamma `pow`，参数范围来自 shader：Easymode `gammaInput 0.1..5`、`gammaOutput 0.1..5`（`CRT_Easymode.hlsl:141-154`），Geom `Target Gamma/Monitor Gamma 0.1..5`（`CRT_Geom.hlsl:30-43`），Hyllian `Input/Output Gamma 0..5`（`CRT_Hyllian.hlsl:50-63`）。Hyllian 在 `CRT_Hyllian.hlsl:221-239,253-255` 对重建颜色执行 `[0,1]` clamp 后再 gamma 输出，并固定 alpha 1；Geom 在 `CRT_Geom.hlsl:327-371` 进行 clamp、gamma 和固定 alpha；Lottes 以 `pow(...,2.2)` 取样并在 `CRT_Lottes.hlsl:331` 以 `pow(...,1/2.2)` 输出，alpha 固定 1。GTU 将 RGB 转 YIQ 后在 `GTU_v050.hlsl:139,149,152` clamp 到 `[0,1]`，最终 alpha 1。公开矩阵 `HDR_PROTOCOL_EVIDENCE.md:36,199-247` 将 CRT 记为无唯一上游协议；本地代码显示每个 preset 都有独立 gamma、clamp 和 mask/scanline 约束。profile 应保持 `Unknown` 与 generic SDR fallback；只有建立逐 preset 的范围、transfer、alpha 和峰值测试后，才有条件使用 `ConditionalFP16`。

### CuNNy

所有 `src/Effects/CuNNy/CuNNy-*.hlsl` 变体带 `//!CAPABILITY FP16`，主输入和输出没有 `//!FORMAT`，输出尺寸为 2x。网络中间面统一明确为 `R8G8B8A8_SNORM`，典型 `CuNNy-2x4C-NVL.hlsl:46-56`；大模型的 `t0..t7` 也遵循同一格式。代码先把输入 RGB 投影到 YUV/特征张量，卷积层交替执行正负 ReLU 分支；最终 NVL out-shuffle 对 Y 分量使用 `saturate`，并以 `MF4(...,1.0)` 写回，典型 `CuNNy-2x4C-NVL.hlsl` 的 `l0` 与 out-shuffle。SNORM 中间面意味着实际张量边界约为 `[-1,1]` 的有符号 8 位归一化存储；此处是 DXGI 资源声明事实，模型输入归一化和输出 denormalization 仍缺少独立说明。公开矩阵 `HDR_PROTOCOL_EVIDENCE.md:37,249-297` 只确认 Blinue/CuNNy 上游可导出 mpv/Magpie shader，未枚举格式。profile 保持 `Unknown`，generic SDR fallback 负责把 canonical FP16 映射到现有 bounded 路径；FP16 capability 仅覆盖算术/编译，SNORM 张量协议仍需单独记录。缺口包括：主面格式、YUV 矩阵的 transfer 假设、模型训练范围、alpha 恢复、SNORM 溢出/钳制行为。

### CuNNy2

所有 `src/Effects/CuNNy2/*.hlsl` 变体带 `//!CAPABILITY FP16`，主输入输出未声明格式，输出尺寸为 2x。中间纹理 `T0..T15` 明确使用 `R8G8B8A8_UNORM`，例如 `CuNNy-3x12-NVL.hlsl:47-80`；快速变体同样使用 UNORM。网络层采用 `max(r,0)`，out-shuffle 以 `saturate(yuv.r + r0.*)` 限制 Y，再通过 YUV→RGB 写回并固定 alpha 1，典型 `CuNNy-fast-NVL.hlsl:349-354,392-402`。因此内部张量和最终输出都表现出 bounded `[0,1]` 倾向，且 alpha 由 shader 重建为不透明。公开矩阵 `HDR_PROTOCOL_EVIDENCE.md:38,299-347` 没有找到独立 CuNNy2 上游协议；本地 shader 是当前最可靠的实现证据。profile 保持 `Unknown` 与 generic SDR fallback，原因是模型归一化、输入 transfer、峰值和颜色原色域均未定义。缺口是把每个模型尺寸/变体的归一化常量、主面格式、alpha policy 和实际输出范围写入结构化 route；在此之前不启用 DirectFP16。

### Diagnostics

`FrameGuidance_Confidence.hlsl` 与 `FrameGuidance_Motion.hlsl` 都只有 `INPUT`、同尺寸 `OUTPUT`、Linear sampler 和单个 PS pass；没有 `//!FORMAT` 或辅助纹理声明。Confidence 的 `Pass1` 是 `return INPUT.SampleLevel(sam, pos, 0);`（`FrameGuidance_Confidence.hlsl:14-18`），Motion 同样直接返回输入（`FrameGuidance_Motion.hlsl:22-26`），所以 shader 层 alpha 保留输入值。Motion 的 `Display Gain` 参数范围为 `0.005..1`（`FrameGuidance_Motion.hlsl:5-10`）；其真正的 motion/confidence 资源格式由 `Magpie.Core/FrameGuidanceDiagnostics` native backend 提供，通用 shader 文件没有声明。公开矩阵 `HDR_PROTOCOL_EVIDENCE.md:39,349-397` 将 Diagnostics 定义为通用诊断名，缺少上游 I/O 协议。profile 保持 `Unknown`，generic SDR fallback 只用于把诊断结果留在安全的效果边界；诊断输出不得隐式当作普通 HDR 图像继续传播。缺口是记录 native backend 的输入资源类型、向量/置信度范围、显示增益是否改变 alpha，以及 raw-capture tap 与 canonical FP16 的对应关系。

### FSRCNNX

`FSRCNNX.hlsl` 与 `FSRCNNX_LineArt.hlsl` 带 `//!CAPABILITY FP16`；主 `INPUT`/`OUTPUT` 无格式，输出尺寸为 2x；`featureMap1/2`、`tex1..tex4` 明确为 `R16G16B16A16_FLOAT`，见 `FSRCNNX.hlsl:21-54`。第一 pass 以 `GetLuma` 的 `0.299/0.587/0.114` 权重从 RGB 提取 luma（`FSRCNNX.hlsl:79-99`），映射层用 `max(target,0)+C*min(target,0)` 的 leaky-ReLU 形式，保留负特征。最终 pass 将网络 RGB 加回输入 RGB 并以 `MF4(...,1)` 写出，见 `FSRCNNX.hlsl:644-660`；alpha 固定为 1。公开矩阵 `HDR_PROTOCOL_EVIDENCE.md:47,682-730` 只确认 FSRCNNX 是 mpv/社区 shader 变体，缺乏独立 SDK 格式和 HDR 协议。FP16 中间面是实现事实，输入 transfer、训练归一化、负值/超 1 值接受区间、alpha 复原均无证据，因此 profile 保持 `Unknown` 与 generic SDR fallback。缺口是按模型变体验证输入/输出格式和范围，并记录 luma 投影是否在线性 RGB 上执行。

### FXAA

`FXAA_High.hlsl`、`FXAA_Medium.hlsl`、`FXAA_Ultra.hlsl` 都只有未格式化的 `INPUT`/`OUTPUT`、Linear sampler 和一个 pass；没有辅助纹理或 FP16 capability。公共实现 `FXAA.hlsli:70-75` 明确 luma 估计式，并写出估计范围约 `0.0..2.963210702`；阈值、range、subpixel blend 全部基于该 luma。wrapper 最终写 `float4(FXAA(...),1)`，见三份 preset 的末尾，因此 alpha 固定为 1。算法没有 transfer 解码/编码，直接把采样值当作阈值运算输入；场景线性 HDR 下亮度比例会改变 edge threshold 行为。公开矩阵 `HDR_PROTOCOL_EVIDENCE.md:48,732-794` 记录 FXAA 的引擎/社区实现与缺少统一格式契约。profile 保持 `Unknown`，generic SDR fallback 负责 bounded 输入；缺口是建立具体格式、transfer、alpha、阈值尺度和 FP16 未钳制测试，完成后再考虑 `SDRCompatible` 或 `ConditionalFP16` 的显式 route。

### MLAA

`MLAA.hlsl` 主 `INPUT`/`OUTPUT` 未声明格式；`edgeMask` 明确 `R8G8_UNORM`，`edgeCounts` 明确 `R8G8B8A8_UNORM`（`MLAA.hlsl:45-63`）。Pass 1 从 RGB 计算 `dot(rgb,[0.2126,0.7152,0.0722])`，以 `threshold 0.02..0.30` 生成二通道 edge mask（`MLAA.hlsl:29-42,69-102`）；Pass 2 把边长度量化为 `0..15/15` 并写入 UNORM（`MLAA.hlsl:105-170`）。Pass 3 在平方域混合后开方，最终返回完整 `color`，所以 alpha 保留输入值（`MLAA.hlsl:228-265`）。主面没有 transfer 说明，辅助 UNORM 面构成明确的 bounded SDR 边界；HDR 场景线性亮度会直接影响 edge threshold。公开矩阵 `HDR_PROTOCOL_EVIDENCE.md:49,796-844` 只确认 MLAA 算法族，没有统一 SDK 格式。profile 保持 `Unknown` 与 generic SDR fallback；缺口是主面格式、transfer、HDR 阈值尺度、alpha 预期和辅助资源重建策略。

## Magpie.Core HDR 边界核对

当前公共边界已提供 canonical `DXGI_FORMAT_R16G16B16A16_FLOAT`：`HdrAdapterDispatcher.h:16-24`。`EffectDrawer::_UsesDirectHdrPath` 只在 route profile 为 `DirectFP16` 或 `ConditionalFP16` 且输入/输出资源均为 FP16 时返回 true，见 `EffectDrawer.cpp:433-449`。其余 profile 在 `PrepareHdrInput` 走 `HdrSurfaceAdapter::ConvertHdrToSdr`，transfer 未声明时默认 SRGB（`EffectDrawer.cpp:467-486`）；输出阶段对应 `ConvertSdrToHdr`，见 `EffectDrawer.cpp:490-509`。`HdrAdapterDispatcher.cpp:73-89` 对 `Unknown` 选择 `canonicalFP16 -> SdrCompatibleFallback -> backend -> SdrToHdr -> canonicalFP16`，未声明 alpha 时设置 `ForceOpaque`。这些接口足以承载本组的声明性 route；本次没有修改 Renderer.cpp、EffectDrawer.cpp、Hdr* 通用组件、配置 UI 或其他分组文件。当前缺口全部属于效果级事实：每个 preset/模型缺少主面格式、transfer、范围、alpha 和 HDR evidence 字段，且现有 route 表仍需把本报告的 Unknown/SDR fallback 决策录入对应效果声明后才能被 dispatcher 读取。

## 修改与验证

修改文件：仅 `docs/experimental/agent-a-report.md`。

静态验证命令：

```powershell
$repo = 'D:\AI\workspace\Magpie-0.6.5-research'
rg -n '^//!FORMAT|^//!CAPABILITY|^//!PASS|^//!IN |^//!OUT |Texture2D|SamplerState|saturate|clamp\(|pow\(|MF4\(.*1\)' `
  "$repo\src\Effects\Anime4K" "$repo\src\Effects\CAS" "$repo\src\Effects\CRT" `
  "$repo\src\Effects\CuNNy" "$repo\src\Effects\CuNNy2" "$repo\src\Effects\Diagnostics" `
  "$repo\src\Effects\FSRCNNX" "$repo\src\Effects\FXAA" "$repo\src\Effects\MLAA"

rg -n 'CanonicalFormat|_UsesDirectHdrPath|ConvertHdrToSdr|ConvertSdrToHdr|HdrAdapterProfile::Unknown|ForceOpaque' `
  "$repo\src\Magpie.Core\HdrAdapterDispatcher.h" `
  "$repo\src\Magpie.Core\HdrAdapterDispatcher.cpp" `
  "$repo\src\Magpie.Core\EffectDrawer.cpp"

Get-Content -Raw "$repo\docs\experimental\agent-a-report.md" | `
  Select-String 'Anime4K|CAS|CRT|CuNNy|CuNNy2|Diagnostics|FSRCNNX|FXAA|MLAA'
```
