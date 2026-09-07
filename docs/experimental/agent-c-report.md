# Agent C：效果协议与当前实现核对

本报告覆盖 NNEDI3、Pixel Art、RAVU、RTX Video VSR、RTX Video Denoiser、RTX Video HDR、Sharpen、SMAA、xBRZ、XeSS、XeSSFG、NVIDIA Optical Flow。证据来源分成三层：公开协议记录 `docs/experimental/HDR_PROTOCOL_EVIDENCE.md` 与 `.json`，实施目录 `docs/experimental/HDR_EFFECT_IMPLEMENTATION_CATALOG.md`，以及本仓库内的 HLSL 与 native backend。仓库当前没有可用的 Git 元数据，因此报告以文件路径和行号作为可复核锚点。

## 总结

| 效果 | 真实协议与范围 | alpha / 颜色语义 | 当前调用链 | 结论 |
|---|---|---|---|---|
| NNEDI3 | 外部 `Texture2D` 格式由宿主决定；中间 `R16_FLOAT`，内部只处理 luma，输出重建 RGB | luma 通道在 `0..1` clamp；输出写入 `float4(..., 1.0)` | 纯 HLSL，多 pass，走普通 EffectDrawer | `ConditionalFP16` 仅代表中间表面；HDR 颜色契约仍 Unknown |
| Pixel Art | `MMPX` 2x、`Pixellate` 任意尺寸、`SharpBilinear` 普通采样；外部格式未枚举 | MMPX/Pixellate 仅取 RGB 且 alpha 置 1；SharpBilinear 返回整四通道采样 | 纯 HLSL | `SDRCompatible`；精确比较和艺术阈值要求记录范围 |
| RAVU | luma/YUV、RGB、Lite、Zoom、compute 变体分开；LUT 为 `R16G16B16A16_FLOAT`，luma 临时面为 `R16_FLOAT` | luma 变体由 YUV 重建并 alpha=1；RGB 变体写入四通道结果，alpha 随具体变体 | 纯 HLSL，多 pass / compute | `ConditionalFP16` 候选；需按变体绑定颜色与 alpha 规则 |
| RTX Video VSR | NVIDIA VFX `VideoSuperRes` 使用 interleaved RGBA U8 GPU buffer，通道值 `0..255`，可放大 | SDK 公开资料未定义 HDR transfer、primaries、metadata、alpha；本地 scale 显式做 `0..1 <-> 0..255` | `NativeEffectBackendFactory` -> `RTXVideoDenoiser`，quality 1..4 | `SDRCompatible`，U8 是硬边界 |
| RTX Video Denoiser | 与 VSR 共用 RGBA U8 interleaved GPU buffer；quality 8..11 同分辨率 | 同上；Denoise 受同分辨率约束 | `NativeEffectBackendFactory` -> `RTXVideoDenoiser`，quality 8..11 | `SDRCompatible`，当前 backend 已实现 |
| RTX Video HDR | 产品语义为 SDR 到 HDR10；公开记录没有可调用纹理格式、数值范围或 metadata API | alpha、transfer、primaries、输出所有权均未形成仓库协议 | 没有对应 HLSL/native backend 路径 | `Unknown`，保持 fallback |
| Sharpen | Adaptive/Fine/Luma/LCAS 四个 shader 家族；部分内部面 `R16G16B16A16_FLOAT`，输入输出格式依宿主 | 多数 shader 只取 RGB 且输出 alpha=1；FineSharp 保留 alpha 参与内部运算后最终置 1；存在 clamp、overshoot 和 `0..255` 风格参数 | 纯 HLSL | 具体 shader 逐项审计；默认 `SDRCompatible`，FP16 只能按实测变体提升 |
| SMAA | 外部颜色纹理未枚举；edges 为 `R8G8_UNORM` 或 `R16G16_FLOAT`，blend/history 为 `R8G8B8A8_UNORM` 或 `R16G16B16A16_FLOAT`，Area/Search 为 `R8*` | 普通路径由 SMAA neighborhood 输出四通道；实验 temporal resolve 明确输出 alpha=1，history alpha 作为有效标记 | 纯 HLSL，多 pass；无 native backend | `SDRCompatible`；中间 FP16 只说明存储格式 |
| xBRZ | 固定 2x..6x compute；Freescale 另有内部 `R8G8B8A8_UNORM`；输入输出外部格式由宿主 | 算法只采样 RGB，所有标准变体写 `float4(dst, 1)`；Freescale 输出同样 alpha=1 | 纯 HLSL | `SDRCompatible`，整数式颜色比较和 YCbCr 阈值依赖显示域 |
| XeSS SR | Intel 公共协议列出 `R16G16B16A16_FLOAT`、`R11G11B10_FLOAT`、`R8G8B8A8_UNORM` 等 linear 格式；MV `R16G16_FLOAT`，depth 常见 `R32_FLOAT` | 公共协议要求输出与输入同格式同色彩空间，alpha 不保留并填 1；本地实验 backend 实际只接受输入 RGBA/BGRA8 UNORM，输出 RGBA8 UNORM | `NativeEffectBackendFactory` -> `XeSSUpscaler`；D3D11/D3D12 shared texture，Zero-MV 或共享 MV | 公共协议为 `DirectFP16` 候选；当前 Magpie 路径是 LDR/UNORM 实验实现，存在明确实现差异 |
| XeSSFG | 公共 HDR terminal 为 HDR10/BT.2100 `R10G10B10A2_UNORM`，backbuffer/HUD-less/UI 要求同格式、色彩空间、尺寸；FP16/scRGB 路径未形成 | UI alpha 单独传递；terminal 输出随 proxy swap chain | marker HLSL -> `XeSSFGPresenter` -> XeSS D3D12 proxy swap chain | 文档要求 `BoundedHDR` terminal；当前 presenter 固定 `R8G8B8A8_UNORM` 且 alpha ignore，形成关键差异 |
| NVIDIA Optical Flow | 公共 NvOF 输入 `GRAYSCALE8`、`NV12`、`ABGR8`；flow 为 `SHORT2` S10.5，cost 为 UINT/UINT8 | flow 是 signed motion vector，不属于 RGB 颜色；本地 densify 除以 32，cost 除以 255 | `FrameGuidance` provider -> `NvidiaOpticalFlowProvider` -> NVOF D3D11 | SDR auxiliary path；当前 init 固定 `ABGR8`，公共多格式能力尚未映射 |

## 逐项证据与差异

### NNEDI3

公开证据记录为 mpv/user-shader 族，`rgba16f/rgba16hf` 只证明中间 surface 习惯，输入输出颜色契约、transfer、HDR 上限和 alpha 没有统一声明（`HDR_PROTOCOL_EVIDENCE.md` NNEDI3 条目；`HDR_EFFECT_IMPLEMENTATION_CATALOG.md:103-107`）。仓库 shader 对每个 NNEDI3 预设声明 `temp` 为 `R16_FLOAT`，输入先经 `dot(x.rgb, rgb2y)` 转成 luma。`nnedi3` 结果在 `0.0..1.0` 之间 clamp；第二 pass 从输入取 UV，再用 `yuv2rgb` 重建 RGB，输出明确写成 `float4(..., 1.0)`，见 `src/Effects/NNEDI3/NNEDI3_nns16_win8x4.hlsl:38-42,326-344,651-675`。当前没有 NNEDI3 native backend，调用停留在普通 HLSL multi-pass。结论是中间 FP16 可记录为实现格式，直接把 canonical HDR RGB 交给该路径缺少可靠颜色协议，alpha 需要按输出置 1 记录。

### Pixel Art

Pixel Art 组由 `MMPX`、`Pixellate`、`SharpBilinear` 三类 shader 组成，公开协议条目只给出泛化算法族，输入输出 DXGI 格式和 HDR 数值范围保持 Unknown。仓库实现显示三种 alpha 语义：MMPX 采样 `.rgb`，四个放大像素全部写 `float4(J/K/M/L, 1)`，见 `src/Effects/Pixel Art/MMPX.hlsl:27,122-131`；Pixellate 对四个角点的 RGB 求平均并返回 `float4(averageColor, 1.0)`，见 `Pixellate.hlsl:33-48`；SharpBilinear 直接返回 `INPUT.SampleLevel`，四通道采样结果沿用宿主 alpha，见 `SharpBilinear.hlsl:40-43`。MMPX 还依赖逐分量精确相等比较，Pixellate 的平均操作没有 HDR transfer 语义，SharpBilinear 的区域限制只作用于坐标。当前全组没有 native backend，默认采用 SDR-compatible 处理，HDR 进入前需要明确艺术变换和 alpha 规则。

### RAVU

RAVU 变体包含 luma/YUV、RGB、Lite、Zoom、anti-ringing、compute 与非 compute 路径。公开证据确认 `rgba16f/rgba16hf` 中间面和 luma/RGB 分化，实施目录要求按具体变体绑定颜色协议（`HDR_PROTOCOL_EVIDENCE.md` RAVU 条目；`HDR_EFFECT_IMPLEMENTATION_CATALOG.md:109-113`）。本地 luma 变体的 LUT 声明为 `R16G16B16A16_FLOAT`，中间 luma 面为 `R16_FLOAT`，见 `src/Effects/RAVU/RAVU_R2.hlsl:37-49`；第二 pass 从输入保留 UV，YUV 转 RGB 后 alpha 固定为 1，见 `RAVU_R2.hlsl:210-213`。RGB 变体的内部 LUT 同为 FP16，luma 变体写 `vec4(value,0)` 到临时面，RGB 变体最终写入四通道结果，见 `RAVU_R2_RGB.hlsl:34-48,70-71,212-213`。当前没有 native backend。结论为 RGB 变体具备 FP16 存储候选，luma/YUV 变体需要显式 luma、UV、transfer 和 alpha 适配，FP16 surface 本身不构成 HDR 颜色保证。

### RTX Video VSR 与 RTX Video Denoiser

公开 VFX 协议把 VSR 与 Denoiser 都限定为 GPU-resident interleaved RGBA/BGRA U8，每分量数值边界是 `0..255`；Denoise 模式保持输入输出分辨率一致（`HDR_PROTOCOL_EVIDENCE.md` RTXVideo 两条；`HDR_EFFECT_IMPLEMENTATION_CATALOG.md:156-166`）。本地 `RTXVideoDenoiser.cpp` 使用 `NvCVImage_Alloc(... NVCV_RGBA, NVCV_U8, NVCV_INTERLEAVED, NVCV_GPU, 32)` 创建 input/output GPU image，quality 1..4 映射 VSR，8..11 映射 Denoise，见 `src/Magpie.Core/RTXVideoDenoiser.cpp:78-118`。对浮点 D3D11 surface，`inputScale=255.0f`，`outputScale=1.0f/255.0f`，并在 `NvCVImage_Transfer` 前后完成量化和还原，见 `RTXVideoDenoiser.cpp:101-109,193-231`。Denoise quality 校验输出与输入宽高一致，见 `RTXVideoDenoiser.cpp:90-95`。工厂把 `RTXVideo\\RTXVideo_VSR_*` 和 `RTXVideo\\RTXVideo_Denoise_*` 都路由到同一 native 类，再由 quality level 区分，见 `src/Magpie.Core/NativeEffectBackendFactory.cpp:92-113`；对应 HLSL 仅是 placeholder pass，native backend 接管实际处理。alpha、HDR transfer、primaries、metadata 没有 VFX API 证据，canonical HDR 必须先转成选定 SDR appearance 并量化到 U8，再通过成对策略重建。

### RTX Video HDR

公开记录只有产品级 SDR-to-HDR10 语义，纹理格式、接口调用、数值范围、颜色 metadata、alpha 和输出所有权保持 Unknown。仓库中存在 RTX Video VSR/Denoise shader 与 backend，没有独立 `RTXVideo_HDR` shader、native 类或 factory 分支。当前调用链没有可落地的 HDR terminal。实现状态保持 `Unknown/fallback`，不把 canonical FP16 直接送入该产品能力。

### Sharpen

Sharpen 组包含 AdaptiveSharpen、FineSharp、LCAS、LumaSharpen，公开目录要求按具体 backend 记录 clamp、negative lobe、overshoot、alpha 和 strength units。仓库 shader 普遍只取 RGB：LCAS、LumaSharpen 最终写 `float4(...,1)`，AdaptiveSharpen 写 `float4(src.rgb + sharpdiff,1)`，见 `src/Effects/Sharpen/LCAS.hlsl:39-54,73-95`、`AdaptiveSharpen.hlsl:102-124,206-210`、`LumaSharpen.hlsl:187-211`。FineSharp 声明两个内部 `R16G16B16A16_FLOAT` 面，内部读取和传播 alpha，最终 RGB 转换后仍写 alpha=1，见 `FineSharp.hlsl:49-67,337-368,442-444`；其参数计算含 `/255.0f`，属于参数标度线索，不等于外部纹理协议。当前没有 native backend，四个 shader 均由普通效果链执行。报告 profile 采用按变体选择：明确归一化、clamp 的路径为 SDR-compatible；完成线性 FP16 保真测试的路径才具备 ConditionalFP16 资格。

### SMAA

公开 SMAA 资料给出 RGBA color、edges、area、search、depth 资源，普通路径中间纹理多为非 sRGB，最终 neighborhood blending 才涉及 sRGB 选项（`HDR_PROTOCOL_EVIDENCE.md` SMAA 条目；`HDR_EFFECT_IMPLEMENTATION_CATALOG.md:168-173`）。仓库 Low/Medium/T2x/4x 路径使用 `R8G8_UNORM` edges、`R8G8B8A8_UNORM` blend/history；High/Ultra 使用 `R16G16_FLOAT` edges、`R16G16B16A16_FLOAT` blend，Area 为 `R8G8B8A8_UNORM`，Search 为 `R8_UNORM`，例如 `src/Effects/SMAA/SMAA_High.hlsl:6-34` 与 `SMAA_Low.hlsl:6-34`。普通三 pass 是 luma edge -> blending weights -> neighborhood blending；实验 temporal 还维护 current/history/historyNext，resolve 明确 `return float4(...,1.0)`，history alpha 作为有效历史标记，见 `SMAA_T2x_Experimental.hlsl:151-182`。当前没有 native backend。外部输入输出格式由宿主提供，内部 FP16 只说明中间资源格式，颜色 transfer 和 alpha 仍按 shader 结果记录。

### xBRZ

公开 xBRZ 社区接口常见 RGBA U8 `Uint8ClampedArray`，缩放因子 2..6，alpha 被保留；本地 native HLSL 实现采用 compute shader，标准变体覆盖 2x、3x、4x、5x、6x，Freescale 使用内部 `R8G8B8A8_UNORM`。标准变体采样 `INPUT.rgb`，用打包 RGB 值和 YCbCr 距离做等色判断，最终所有输出写 `float4(dst,1)`，见 `src/Effects/xBRZ/xBRZ_2x.hlsl:36-56,125-145,290-299`；其他倍率保持同一结构。Freescale 第二 pass 使用 `floor(info * 255 + 0.5)` 解码内部标志，输出仍置 alpha=1，见 `xBRZ_Freescale.hlsl:276-318,388`。当前调用链是纯 HLSL，无 native backend。精确颜色比较、整数式阈值和 alpha 置 1 使其保持 SDR-compatible，直接 scene-linear HDR 会改变阈值意义。

### XeSS Super Resolution

公共 XeSS-SR 证据列出 `R16G16B16A16_FLOAT`、`R11G11B10_FLOAT`、`R8G8B8A8_UNORM` 与其他 linear color format，输出要求与输入保持同格式同色彩空间，alpha 不保留并填 1，MV 常用 `R16G16_FLOAT`，depth 可用 `R32_FLOAT`（`HDR_PROTOCOL_EVIDENCE.md` XeSS 条目；`HDR_EFFECT_IMPLEMENTATION_CATALOG.md:88-93`）。本地实现存在清晰差异：`XeSSUpscaler::Initialize` 只接受 `R8G8B8A8_UNORM` 或 `B8G8R8A8_UNORM` 输入，输出必须是 `R8G8B8A8_UNORM`，并限制最高 3x，见 `src/Magpie.Core/XeSSUpscaler.cpp:184-214`；BGRA 输入先经 compute shader 转 RGBA，shared input/output 纹理也按 RGBA8 创建，见 `XeSSUpscaler.cpp:49-60,240-263`。XeSS 初始化设置 `XESS_INIT_FLAG_LDR_INPUT_COLOR`，并使用 flat `R32_FLOAT` depth、`R8_UNORM` responsive mask、zero/shared `R16G16_FLOAT` motion，见 `XeSSUpscaler.cpp:308-334,417-426`。调用链由 `NativeEffectBackendFactory` 识别 `XeSS\\XeSS_SR` 后创建 `XeSSUpscaler`，见 `NativeEffectBackendFactory.cpp:77-89`；效果 HLSL 只是 marker pass。结论是公共协议具备 DirectFP16 候选，当前仓库路径仍是 LDR/UNORM Zero-MV 或共享 MV 实验实现，HDR 直接接入需要单独 backend 改造与 alpha 清理。

### XeSS Frame Generation

公共 XeSS-FG HDR terminal 证据要求 HDR10/BT.2100 `R10G10B10A2_UNORM`，backbuffer、HUD-less、UI-only 资源需匹配像素格式、色彩空间和尺寸，FP16 HDR/scRGB 路径没有对应公开 terminal 契约（`HDR_PROTOCOL_EVIDENCE.md` XeSSFG 条目；`HDR_EFFECT_IMPLEMENTATION_CATALOG.md:130-134`）。本地 presenter 固定 `COLOR_FORMAT = DXGI_FORMAT_R8G8B8A8_UNORM`，创建 proxy swap chain 时使用同一格式并设置 `DXGI_ALPHA_MODE_IGNORE`，见 `src/Magpie.Core/XeSSFGPresenter.cpp:18-20,548-558`；D3D11 shared color、D3D12 backbuffer、frame resource tagging 和 proxy `Present` 组成完整 terminal，见 `XeSSFGPresenter.cpp:155-195,782-899`。外部 motion 若启用，必须为 `R16G16_FLOAT` 且尺寸与 destination 完全匹配，见 `XeSSFGPresenter.cpp:632-661`；没有外部 motion 时使用 zero motion 与 flat depth。`XeSSFG\\XeSS_FrameGeneration_x2_ZeroMV.hlsl` 只保留 marker pass，实际插值由 `XeSSFGPresenter` 接管，见该 shader:1-52。实现目录建议的 PQ/BT.2100 `R10G10B10A2_UNORM` terminal 尚未落入当前 presenter，报告将其标为 bounded-HDR 文档目标与 R8 presenter 现状之间的实现差异。

### NVIDIA Optical Flow

公开 NvOF 头文件协议列出 `NV_OF_BUFFER_FORMAT_GRAYSCALE8`、`NV_OF_BUFFER_FORMAT_NV12`、`NV_OF_BUFFER_FORMAT_ABGR8` 输入，flow 输出为 `SHORT2` S10.5，cost 可为 UINT/UINT8，支持 grid 1/2/4（`HDR_PROTOCOL_EVIDENCE.md` NVIDIA Optical Flow 条目；`HDR_EFFECT_IMPLEMENTATION_CATALOG.md:168-173`）。本地 provider 查询输入、输出、cost caps，并要求 ABGR8 与 S10.5 可用；初始化参数最终固定 `inputBufferFormat = NV_OF_BUFFER_FORMAT_ABGR8`，见 `src/Magpie.Core/NvidiaOpticalFlowProvider.cpp:503-545`。flow 结果被 densify shader 读取为 `Texture2D<int2>` 并除以 32，cost 读取为 uint 并除以 255，见 `NvidiaOpticalFlowProvider.cpp:15-61,103-105`；输出落入 `FrameGuidance` 的 motion/confidence 纹理，不进入普通 RGB 效果 surface。质量配置映射为 4x 或 2x grid 与 FAST/MEDIUM/SLOW perf level，见 `NvidiaOpticalFlowProvider.cpp:119-134`。当前调用链由 FrameGuidance provider 驱动，可被 XeSS SR、XeSSFG、DLSS 等请求；公共三种输入格式在本地只落地 ABGR8，属于 auxiliary SDR path。

## 文件修改与验证

本次只新增本报告文件：`docs/experimental/agent-c-report.md`。没有修改 `Renderer.cpp`、`EffectDrawer.cpp`、`Hdr*` 通用组件、配置 UI、CMake 或任何既有公共文件，也没有修改分组外效果文件。

已完成的只读核对命令：

```powershell
rg -n -i "NNEDI3|Pixel Art|RAVU|RTXVideo|Sharpen|SMAA|xBRZ|XeSS|XeSSFG|Optical Flow|R10G10B10A2|R11G11B10|R16G16B16A16|GRAYSCALE8|ABGR8|NV12" src docs/experimental
Get-Content docs/experimental/HDR_PROTOCOL_EVIDENCE.md
Get-Content docs/experimental/HDR_EFFECT_IMPLEMENTATION_CATALOG.md
Get-Content src/Magpie.Core/NativeEffectBackendFactory.cpp
Get-Content src/Magpie.Core/RTXVideoDenoiser.cpp
Get-Content src/Magpie.Core/XeSSUpscaler.cpp
Get-Content src/Magpie.Core/XeSSFGPresenter.cpp
Get-Content src/Magpie.Core/NvidiaOpticalFlowProvider.cpp
```

报告所引用的格式、范围、alpha、调用链和实现差异均来自上述只读结果；没有进行构建或运行时验证，因为本交付只新增证据报告，且当前工作树没有为这些 SDK 路径提供统一可复现的构建环境。
