# Agent B：DLSS / FSR / NIS / Optical Flow 效果协议核对

本报告覆盖 DLSS、DLSSFG、DLSSNR、FSR1、FSR2、FSR3 SR、FSR3 FG、FSR4、NIS、AMD FidelityFX Optical Flow。证据分为三层：官方协议材料、`Magpie-0.6.5-research` 当前 native backend、`DLSSNR-HDR-Experiments` 的真实运行日志。修改范围仅为本文件；Renderer、EffectDrawer、Hdr 通用组件、配置 UI 和 CMake 保持原状。

## 先给结论

DLSS、FSR2、FSR3 SR、FSR4、NIS 的官方资料都给出了可用于 HDR 的路径，但它们对 transfer function、exposure、depth、motion vector、资源状态的要求各自独立。当前 Magpie 的 SR backend 已经有一套可工作的 D3D11/D3D12 资源桥接，核心缺口集中在颜色格式识别、HDR 开关、exposure 传递和真实 depth。FSR1 与 NVIDIA Optical Flow 的公开协议偏向 `[0,1]`/8-bit SDR，适合走 SDR-compatible 辅助路径。DLSSFG 与 FSR3 FG 都属于呈现终端，backbuffer、HUD-less/UI、MV、depth、光流以及 frame lifetime 必须一起建模；单独把 FP16 颜色送入 backend 时，帧生成仍缺少这些配套资源与生命周期条件。

DLSSNR 的实验结论更窄。`results/hdr-scales` 使用 RTX 4070、驱动 32.0.16.1656、DLSSNR DLL 310.8.0.0、`DXGI_FORMAT_R16G16B16A16_FLOAT`、零运动和常量深度完成了真实 Feature 18 Create/Evaluate。`scale=1` 与基线的 MAE 为 0.01182021、PSNR 31.652 dB；`scale=2` 的 MAE 为 0.03846685、PSNR 18.109 dB；`scale=4.5` 的 MAE 为 0.20317990、PSNR 5.152 dB，最大差达到 3.5，并伴随红色/高光爆炸。实验只证明输入值域放大与输出失真之间存在强相关，资料没有给出可复核的 DLSSNR 归一化公式、参考白、峰值或逆变换参数。实现层面保留 `scale=1/2/4.5` 作为可配置实验轴，4.5 的含义保持为实验值，暂不解释为已确认的原生 HDR 白点；现有 bounded-HDR 文案继续单独标注为 NVIDIA 官方资料之外的本地协议。

## 十项协议矩阵

| 效果 | 官方输入/输出边界 | 当前 native backend | 主要缺口 | 可落地适配 |
|---|---|---|---|---|
| DLSS SR | Color 输入/输出为 API 支持格式；MV `RG16_FLOAT/RG32_FLOAT`；depth 单通道或 depth-stencil；exposure 1x1，`R16F` 优先；HDR 通过 `IsHDR`，输入为线性 HDR | `DLSSSRUpscaler` 创建零 MV `R16G16_FLOAT`、零 depth `R32_FLOAT`、bias mask `R8_UNORM`；创建标志含 AutoExposure；Evaluate 固定 jitter=0、MV scale=1、pre-exposure=1、exposure scale=1 | 没有从 canonical HDR 传入 `IsHDR`/exposure；depth 默认 zero-contract；颜色格式/色域没有协议字段 | 新增 DLSS 专属 HDR 参数桥：输入格式/transfer、HDR flag、1x1 exposure、真实 depth；保持现有 zero fallback，并记录 fallback 状态 |
| DLSSFG | backbuffer/final color；输出与 backbuffer 同格式；MV/depth 遵循 DLSS SR；HUD-less/UI 必须同尺寸、格式、色彩空间；有 HDR 开关 | `DLSSFrameGenerator` 从输入纹理复制格式，`NativeBackbufferFormat=inputDesc.Format`；zero MV/depth；MV scale=1；`depthInverted=false`；HUDLess/UI 等资源统一标记 never-provided | backbuffer 格式传递存在，HDR 色彩元数据、HUD-less/UI、真实 depth、HDR 开关没有完整接线 | 保留 PresentationTerminal；增加 backbuffer protocol、HDR flag、HUD-less/UI 生命周期和真实 depth/MV 适配，完成前不宣称 HDR 完整支持 |
| DLSSNR | NVIDIA 公开资料没有稳定的 Feature 18 HDR/FP16 色彩契约；本地实验只覆盖 FP16 值域探测 | `DLSSNRFilter` 强制同分辨率；输入仅 `R8G8B8A8_UNORM/B8G8R8A8_UNORM`，输出仅 `R8G8B8A8_UNORM`；NGX 颜色共享面为 RGBA8；创建 `Scale=1`、`ScalingRatio=1`；MV scale=1、depth inverted=1 | 当前路径是 SDR RGBA8；实验 FP16 路径与 Magpie 路径分离；缺少可验证 normalize/inverse、HDR metadata、exposure、真实 depth | 作为 BoundedHDR 专属 adapter：`canonical FP16 -> configurable scale/normalizer -> DLSSNR -> inverse -> canonical FP16`；第一版只暴露 scale=1/2/4.5 实验值和诊断，不固化公式 |
| FSR1 | 官方核心输入为感知 sRGB `[0,1]`；具体 DXGI 格式未枚举；负 RCAS 输入会产生 NaN；输出由宿主决定 | 当前工厂没有 FSR1 native backend，按 shader effect 处理 | 没有 native HDR/FP16 协议，算法输入是 display-referred SDR | SDR-compatible adapter：HDR 外观映射到 sRGB `[0,1]`，运行 FSR1，再按记录的映射恢复 |
| FSR2 | Color 应用指定；depth 1x float；MV 2x float；reactive/T&C `R8_UNORM`；exposure `R32_FLOAT`；HDR flag 后输入 linear，输出恢复原输入域 | `FSR2Upscaler` 零 MV `R16G16_FLOAT`、零 depth `R32_FLOAT`、reactive `R8_UNORM`；启用 auto exposure/depth inverted/infinite；exposure 传 `nullptr`；preExposure=1；MV scale=1 | HDR flag 没有接入；真实 depth/exposure 没有接入；当前 MV 只接受 frame guidance | DirectFP16 adapter：canonical FP16 color，显式 HDR flag，接真实 exposure/depth/MV；保持 masks 与 resource state 独立 |
| FSR3 SR | 与 FSR2 同类：应用指定 color，float depth/MV，`R8_UNORM` masks，`R32_FLOAT` exposure；HDR flag 后输入 linear | `FSR3Upscaler` 复制输入/输出纹理原格式；aux 为 depth `R32_FLOAT`、exposure 1x1 `R32_FLOAT=1`、reactive/transparency `R8_UNORM`、zero MV `R16G16_FLOAT`；create flags 含 NON_LINEAR_COLORSPACE；dispatch 固定 `NON_LINEAR_COLOR_SRGB`、preExposure=1 | 当前颜色路径明确使用 sRGB/non-linear，和 HDR linear 证据存在语义偏差；真实 depth/exposure/jitter 没有接入 | DirectFP16 adapter：HDR 模式切换为 linear，删去隐含 sRGB flag，提供真实 exposure/depth/MV；FSR3 provider 版本选择保持现有机制 |
| FSR3 FG | current backbuffer、可选 HUD-less、depth、MV、FSR Optical Flow `R16G16_SINT`、SCD `R32_UINT`；需 backBufferFormat、HDR transfer/luminance、present callback | 当前代码没有 FSR3 FG native backend 或效果注册；Frame Generation 代码路径只识别 DLSSFG/XeSSFG | 缺少整个终端协议：swapchain/backbuffer、HUD-less/UI、光流/SCD、HDR luminance、present lifetime | 新增独立 FSR3 FG backend 后再接入；先定义 PresentationTerminal 与 `backBufferFormat/transfer/minMaxLuminance`，使用独立于普通 SR backend 的终端适配 |
| FSR4 | Color 应用指定，推荐 linear；depth 1x float；MV 2x float；exposure 1x1 `R32_FLOAT`；可用 NON_LINEAR_COLORSPACE 标志 | `FSR3Upscaler` 以 `useFsr4` 选择 provider 4.1.1；资源和 dispatch 形态与 FSR3 SR 共用；仍固定 `NON_LINEAR_COLOR_SRGB`、exposure=1、zero depth | FSR4 选择存在，HDR transfer/exposure/depth 语义仍沿用 FSR3 的硬编码；缺少真实 ML 输入校准 | 在 FSR3 backend 内增加 FSR4 专属 protocol profile，显式 linear/non-linear 选择与 exposure/depth 绑定，保持 provider version 独立 |
| NIS | 输入/输出为非整数格式，文档例为 `R8G8B8A8_UNORM`/NV12；支持 LDR、PQ、linear HDR；linear HDR 建议 `[0,12.5]`；输入 SRV、输出 UAV、linear clamp sampler | 当前 native 工厂没有 NIS backend；NIS 作为 shader effect 时由通用 effect surface 决定 | 缺少 `NIS_HDR_MODE`、PQ/linear 选择、NV12 路径、viewport/resource state 专属适配 | 新增 NIS 专属 shader/native declaration：HDR mode、输入范围、PQ Rec.2020 或 linear `[0,12.5]`，保证 SRV/UAV 与 sampler contract |
| AMD FidelityFX Optical Flow | color 输入格式公开资料未枚举；输出光流 `R16G16_SINT`、SCD `R32_UINT`；8x8 block；HDR 依赖 transfer/luminance | `AmdOpticalFlowProvider` 固定 color shared texture 为 `R8G8B8A8_UNORM`；Performance 为 1/2 extent，Quality 为 full extent；SDK flow 期望 `R16G16_SINT`；输出 dense motion `R16G16_FLOAT`、confidence `R8_UNORM`；dispatch 固定 sRGB、luminance `{0,1}` | 输入格式被硬编码为 RGBA8；HDR transfer/luminance 没有暴露；SDK sparse 输出被本地 densify 改成 frame-guidance 格式 | BoundedHDR auxiliary adapter：先定义 color transfer/luminance 参数，再转换为 SDK 输入；保留 sparse `R16G16_SINT/SCD` 与 dense frame-guidance 输出的边界 |

## DLSSNR 实验记录与边界

实验材料：`DLSSNR-HDR-Experiments/README.md`、`results/README.md`、`results/hdr-scales/*/run.log` 与 `output_diagnostic.log`、`scripts/Run-HdrScales.ps1`。三组测试均为 2560x1392、FP16 线性输入、零 MV、常量 depth、同分辨率输出，Feature 18 Create/Evaluate/Release 均返回成功。`scale=1` 的诊断写明输入/输出均为 `DXGI_FORMAT_R16G16B16A16_FLOAT`，sRGB EOTF 转 linear 后写入 GPU，输出匹配逆转换且无 tone mapping。`scale=2` 仍能完成评估，但误差明显增加。`scale=4.5` 的结果出现明显红色/高光爆炸，PSNR 降到 5.152 dB，最大差为 3.5。

这些日志支持三条实现判断。第一，DLSSNR Feature 18 在当前 DLL/驱动组合上接受该实验调用链，成功返回值本身不代表颜色协议正确。第二，`scale` 是实验输入值域倍率，`scale=4.5` 与 scRGB 传统白点代理有关，实验材料没有证明它等同于 DLSSNR 内部 reference white、曝光或 PQ 峰值。第三，现有材料没有足够信息写出归一化函数、曲线、参考白、峰值、alpha 规则或逆变换，因此适配器必须把这些字段作为可配置实验参数，记录输入/输出统计和版本哈希。

Magpie 当前 `DLSSNRFilter` 与实验 harness 处于两条协议线上。当前 backend 在 `DLSSNRFilter.cpp:1895-1911` 限制输入为 RGBA8/BGRA8、输出为 RGBA8，在 `:1951-1958` 把共享面强制成 RGBA8；`SetCreateParametersUnsafe` 在 `:1343-1365` 固定尺寸、`Upscaling=0`、`Scale=1`、`ScalingRatio=1`；`SetEvaluateParametersUnsafe` 在 `:1432-1464` 固定 MV scale=1、depth inverted=1、reset/style/mask 等参数，代码里没有 HDR flag、exposure texture 或公开 normalize 参数。可落地的第一步是单独新增 DLSSNR protocol/adapter 文件，承载 scale、输入 transfer、reference white、peak/headroom、inverse policy 和 diagnostics；公共 Renderer 与 Hdr 组件继续只处理 canonical FP16 表面。

## 官方证据入口

逐条来源、引用片段和证据等级已经汇总在 [`HDR_PROTOCOL_EVIDENCE.md`](HDR_PROTOCOL_EVIDENCE.md) 与 [`HDR_PROTOCOL_EVIDENCE.json`](HDR_PROTOCOL_EVIDENCE.json)。本组实际使用的上游入口如下：

- NVIDIA DLSS Programming Guide：<https://github.com/NVIDIA/DLSS/blob/main/doc/DLSS_Programming_Guide_Release.pdf>
- NVIDIA DLSS-G / Frame Generation：<https://raw.githubusercontent.com/NVIDIA-RTX/Streamline/main/docs/ProgrammingGuideDLSS_G.md>
- NVIDIA DLSSG header：<https://raw.githubusercontent.com/NVIDIA/DLSS/main/include/nvsdk_ngx_defs_dlssg.h>
- AMD FSR2：<https://raw.githubusercontent.com/GPUOpen-LibrariesAndSDKs/FidelityFX-SDK/main/Kits/FidelityFX/docs/techniques/super-resolution-temporal.md>
- AMD FSR3 Upscaler：<https://raw.githubusercontent.com/GPUOpen-LibrariesAndSDKs/FidelityFX-SDK/main/Kits/FidelityFX/docs/techniques/super-resolution-upscaler.md>
- AMD FSR3 Frame Interpolation / Optical Flow：<https://raw.githubusercontent.com/GPUOpen-LibrariesAndSDKs/FidelityFX-SDK/release-FSR3-3.0.3/docs/techniques/frame-interpolation.md>、<https://raw.githubusercontent.com/GPUOpen-LibrariesAndSDKs/FidelityFX-SDK/release-FSR3-3.0.3/docs/techniques/optical-flow.md>
- NVIDIA Image Scaling：<https://raw.githubusercontent.com/NVIDIAGameWorks/NVIDIAImageScaling/main/README.md>
- NVIDIA Optical Flow SDK：<https://raw.githubusercontent.com/NVIDIA/NVIDIAOpticalFlowSDK/master/nvOpticalFlowCommon.h>

## 当前代码证据索引

- `src/Magpie.Core/DLSSSRUpscaler.cpp:50-171,262-276`：DLSS SR 尺寸检查、零 MV/depth/mask 格式、AutoExposure 创建标志、固定 exposure/jitter/MV scale。
- `src/Magpie.Core/DLSSFrameGenerator.cpp:390-608,637-798`：DLSSFG backbuffer/native format、zero resources、resource flags、MV/depth binding、unit scale、backbuffer/output subrect。
- `src/Magpie.Core/DLSSNRFilter.cpp:1343-1464,1863-1958,2124-2142`：DLSSNR 创建参数、评估参数、RGBA8 输入输出限制和状态日志。
- `src/Magpie.Core/FSR2Upscaler.cpp:50-178`：FSR2 auxiliary formats、context flags、exposure/motion/preExposure 常量。
- `src/Magpie.Core/FSR3Upscaler.cpp:185-255,262-474,482-550`：FSR3/FSR4 shared color format、aux formats、provider version、HDR/non-linear flag、dispatch 常量。
- `src/Magpie.Core/AmdOpticalFlowProvider.cpp:258-390,393-465,521-620`：AMD OF 输入 RGBA8、Performance/Quality 尺寸、SDK sparse formats、sRGB/luminance 固定值、dense output formats。
- `src/Magpie.Core/NvidiaOpticalFlowProvider.cpp:16-66,366-409,748-756`：NVIDIA OF S10.5 `SHORT2` 解码为 float motion、grid 1/2/4 路径、dense motion `R16G16_FLOAT` 与 confidence `R8_UNORM`。NVIDIA OF 作为 DLSS/FSR 的共享辅助 provider，官方输入仍限 GRAYSCALE8/NV12/ABGR8，当前 provider 没有 FP16/HDR 输入路由。
- `src/Magpie.Core/NativeEffectBackendFactory.cpp:63-89`：DLSSNR、DLSS SR、FSR2、FSR3/FSR4 的当前 native 分派；FSR1、NIS、FSR3 FG、AMD OF 均不在普通 SR 分派里。

## 适配优先级

1. DLSSNR：新增专属 bounded-HDR protocol 文件，先保留 scale=1/2/4.5 实验选择和统计诊断，把归一化公式保持为可配置实验参数。
2. FSR3/FSR4 与 FSR2：把 HDR linear、真实 exposure、depth、MV、reset 作为 effect-local protocol，修正当前 non-linear sRGB 硬编码语义。
3. DLSS SR：补齐 HDR flag 与 exposure，保留 zero guidance 作为明确的降级状态。
4. NIS：新增 HDR mode 与 PQ/linear range 声明，空间算法适合较早接入。
5. DLSSFG、FSR3 FG：统一 PresentationTerminal 资源描述，再分别接 backbuffer/UI/HUD-less/光流协议；颜色适配单独记录 transfer/luminance。
6. AMD FidelityFX Optical Flow：先把 SDK color transfer/luminance 从固定 sRGB `{0,1}` 提升为 provider-local 参数，再决定 bounded HDR 的默认值。

## 验证命令

本报告的证据复核：

```powershell
rg -n "scale=1|scale=2|scale=4\.5|PSNR|MAE|DXGI_FORMAT_R16G16B16A16_FLOAT|颜色转换" `
  D:\AI\workspace\DLSSNR-HDR-Experiments\results\hdr-scales

rg -n "DXGI_FORMAT|HDR|exposure|Exposure|motion|Motion|depth|Depth|Scale|ScalingRatio|NON_LINEAR_COLOR_SRGB" `
  D:\AI\workspace\Magpie-0.6.5-research\src\Magpie.Core\DLSSNRFilter.cpp `
  D:\AI\workspace\Magpie-0.6.5-research\src\Magpie.Core\DLSSSRUpscaler.cpp `
  D:\AI\workspace\Magpie-0.6.5-research\src\Magpie.Core\FSR2Upscaler.cpp `
  D:\AI\workspace\Magpie-0.6.5-research\src\Magpie.Core\FSR3Upscaler.cpp
```

DLSSNR 真实实验重跑路径：

```powershell
powershell -ExecutionPolicy Bypass -File D:\AI\workspace\DLSSNR-HDR-Experiments\scripts\Run-HdrScales.ps1
```

该脚本会分别执行 `--hdr-scale 1`、`2`、`4.5`，结果写入 `results\hdr-scales\`。报告没有把重跑作为本次完成条件，当前结论直接取自已经存在的真实日志与输出统计。
