# Magpie 实验分支交接

> 2026-09-05：当前准备版本为 0.6.5，最新行为和附件结构见 [0.6.5 Release Note](RELEASE_NOTES_v0.6.5-experimental.md)。下文保留历史交接背景，旧参数、深度组件和手动保存说明已被后续实现取代。


更新：2026-09-04

这是 [Blinue/Magpie](https://github.com/Blinue/Magpie) 的非官方实验 Fork。主要新增了仅依赖捕获颜色帧的 DLSS-SR、DLSS Frame Generation、FSR2/3/4、XeSS、RTX Video，以及 MLAA/SMAA 时域近似效果。由于没有引擎提供的真实深度、运动向量、曝光、反应遮罩和投影 jitter，画质和稳定性不能等同于原生游戏接入。

当前实现以 [r8 记录](experimental/todos/20260905-v0.6.5-r8-TODO.md) 为准；旧 TODO 中的被替代方案仅作历史资料。

## 当前实现

- DLSS SR：默认 NVOF 均衡，可选择 None / AMD OF / NVOF；Depth 固定为全零纹理。旧标识和档位在导入时迁移。
- FSR 2/3/4、XeSS SR：每种一个入口，默认不使用光流，可选择 AMD OF / NVOF 及质量档位。r8 删除 metadata-only jitter 与内置光流，旧 Optical Flow 迁移到 AMD OF 质量档。
- RTX Video：同分辨率降噪和 VSR Low/Medium/High/Ultra。
- DLSSNR：可选将颜色、Motion 和固定 Zero Depth 调整到原分辨率的 25%–100%，再把处理差值重建到原分辨率；默认关闭，100% 默认值，1% 步进。r5 颜色使用 vertical-first Lanczos-2 AA，残差使用 Catmull-Rom 4+4，共用 FP16 中间纹理。重建阶段按原图与参考合成结果的整像素线性 Rec.709 亮度差，选择阴影/结构或反射/发光倍率并作用于完整 RGB 残差，再处理相对原图的 HSL 饱和度/明度变化。NR Preset 固定为 0；NR Intensity、Local Tone、Local Structure 范围统一为 0–1。
- DLSS FG_Experimental：2x/3x/4x，默认输入共享 Motion，Depth 固定为 Zero；不能与 Smooth Motion 同时使用。
- XeSSFG：通用显卡 x2，以及 Intel Arc 显卡 x2-x4 多帧生成；请求倍率会受 GPU/驱动报告能力限制。
- Smooth Motion 兼容模式：缩放结束后重启 Magpie，保留窗口/最小化/托盘状态；新进程会等待旧进程完全退出。
- 原生 SDK Effect 已统一通过 `NativeEffectBackend` 和 `NativeEffectBackendFactory` 分派；DLSSFG 因多帧发布仍是独立终端阶段。
- 缩放 Overlay 已提供由 `EffectDesc::params` 自动生成的效果参数窗口。能力按参数解析为 Live 或 RestartRequired；实时更新通过 backend 线程的 last-write-wins 邮箱合并，参数自动保存；会话状态分别保存目标值和已生效值。自动重建使用已生效快照，显式应用并重启使用目标值；两处编辑器同步，尚未生效项显示当前值。

DLSSFG 当前保留 CPU Fence，并通过 D3D11/D3D12 共享资源让最终颜色与源分辨率 Guidance 使用同一捕获帧 ID。失败时依次重置历史、重建一次；仍失败则只在当前缩放会话禁用帧生成并继续显示真实帧，避免无限重试把窗口拖死。

内置更新检查自 0.5.3-experimental 起仍暂时关闭，应用不会后台联网检查，也不显示手动检查入口。当前 `0.6.5` 开发线已移除 DAV2、ONNX Runtime 和 TensorRT 深度估算链，仅保留 SDK 合同要求的 Zero Depth；Release 使用的社区修改版 `nvngx_dlssnr.dll` 不进入源码仓库。

## 关键位置

- 原生后端：`src/Magpie.Core/*Upscaler.*`、`RTXVideoDenoiser.*`、`DLSSFrameGenerator.*`
- 统一分派：`src/Magpie.Core/NativeEffectBackend*`
- Renderer 接入：`src/Magpie.Core/Renderer.*`
- 缩放中参数编辑：`src/Magpie.Core/OverlayDrawer.*`、`EffectDrawer.*`、`include/ScalingOptions.h`、`src/Magpie/ScalingService.*`
- Effect：`src/Effects/DLSS*`、`FSR*`、`XeSS`、`RTXVideo`、`DLSSFG`、`XeSSFG`
- DLSSNR 残差合成与参数钳制：`src/Magpie.Core/DLSSNRFilter.*`、`NativeEffectBackendFactory.cpp`
- 参数多列布局与条件显隐：`src/Magpie/ScalingModesPage.xaml`、`EffectParametersViewModel.*`
- 本机开关：`src/BuildOptions.props.user`（不可提交）
- 可复现打包：`scripts/Build-Release.ps1`
- 发布规范：`docs/experimental/RELEASE-WORKFLOW.md`
- 许可证清单：`docs/THIRD_PARTY_AND_REDISTRIBUTION.md`

## 构建与输出

开发环境使用 VS 2022、Windows SDK 10.0.26100、Conan 2。打包命令：

```powershell
./scripts/Build-Release.ps1 -Version 0.0.0-experimental `
    -ReleaseDirectory v0.0.0-experimental
```

脚本自动发现 MSBuild/Conan/CMake，使用 Conan 锁文件和 `/Brepro` 执行 Rebuild，覆盖 `release/<版本>/Magpie-Experimental-x64` 和同目录 ZIP，并生成含提交、源码状态、功能开关及文件哈希的 `build-manifest.json`。正式包默认拒绝脏工作区；本地临时测试才使用 `-AllowDirtySource`。

## 维护注意

- DLSSFG 不要直接跳过 CPU Fence；此前会造成 D3D12 allocator/list 提前复用、NGX Evaluate 失败及整体卡死。
- 光流由捕获颜色帧估算，不是引擎 MV；不同场景的画质仍需实机确认。共享池优先选 NVOF，其次 AMD OF，取选定来源实际申请中的较高档位，不自动启用未申请的最高档。
- DLSSNR 四个精细残差参数仅在输入分辨率调整开启时生效；默认均为 1 并走旧公式快路径。r5 阴影/结构与反射/发光依据方向控制前的整像素线性亮度变化，以零为硬边界选用一个倍率，再做 HSL 相对变化；不是原图亮度分区，也没有反向倍率。
- 参数窗口保留启动值用于撤销；会话的目标值和已生效值分开，保存回执跨自动重建保留。普通 shader 实时写常量；Inline、资源尺寸、Motion provider、DLSSFG 和未知 native 参数都要求重新缩放。DLSSNR residual 参数只有在本次启动已经创建 residual 路径时才是 Live。
- r3 参考 [SAOG0721/Magpie PR #4](https://github.com/SAOG0721/Magpie/pull/4)，但没有合并其效果级布尔能力或运行中重建 DLSSFG 的做法；本地实现采用参数级能力表、有限合并邮箱和 App UI 线程上的事务式保存/重启。
- FSR4 会绕过 INT8 provider 的能力检查，只适合研究；二进制发布前必须单独检查授权。
- GPLv3 与 NVIDIA 专有组件的组合分发存在未解决风险。SDK、模型、wheel 和本机依赖目录不要提交；公开二进制前按许可证专项文档逐项审核。
