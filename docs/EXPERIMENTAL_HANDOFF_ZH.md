# Magpie 实验分支交接

更新：2026-09-03

这是 [Blinue/Magpie](https://github.com/Blinue/Magpie) 的非官方实验 Fork。主要新增了仅依赖捕获颜色帧的 DLSS-SR、DLSS Frame Generation、FSR2/3/4、XeSS、RTX Video，以及 MLAA/SMAA 时域近似效果。由于没有引擎提供的真实深度、运动向量、曝光、反应遮罩和投影 jitter，画质和稳定性不能等同于原生游戏接入。

## 当前实现

- DLSS SR_Experimental：非 Jitter 主路径默认输入共享 NVOF Motion，可选输入 DAV2 Estimated Depth；旧内部标识继续兼容已有配置。
- FSR2/FSR3/FSR4/XeSS：Zero-MV、伪 jitter、50% 颜色光流等实验路径。
- RTX Video：同分辨率降噪和 VSR Low/Medium/High/Ultra。
- DLSSNR：可选将颜色、运动和深度输入调整到原分辨率的 25%–100%，再把处理差值重建到原分辨率；默认关闭，100% 默认值，1% 步进。
- DLSS FG_Experimental：2x/3x/4x，默认输入共享 Motion、可选输入 Estimated Depth；不能与 Smooth Motion 同时使用。
- XeSSFG：通用显卡 x2，以及 Intel Arc 显卡 x2-x4 多帧生成；请求倍率会受 GPU/驱动报告能力限制。
- Smooth Motion 兼容模式：缩放结束后重启 Magpie，保留窗口/最小化/托盘状态；新进程会等待旧进程完全退出。
- 原生 SDK Effect 已统一通过 `NativeEffectBackend` 和 `NativeEffectBackendFactory` 分派；DLSSFG 因多帧发布仍是独立终端阶段。

DLSSFG 当前保留 CPU Fence，并通过 D3D11/D3D12 共享资源让最终颜色与源分辨率 Guidance 使用同一捕获帧 ID。失败时依次重置历史、重建一次；仍失败则只在当前缩放会话禁用帧生成并继续显示真实帧，避免无限重试把窗口拖死。

内置更新检查自 0.5.3-experimental 起仍暂时关闭，应用不会后台联网检查，也不显示手动检查入口。当前公开常规版本为 v0.6.0-experimental，另有一个已发布的最小 Hotfix；当前 `experimental` 开发线目标版本为 v0.6.1-experimental。Release 使用的社区修改版 `nvngx_dlssnr.dll` 不进入源码仓库。

## 关键位置

- 原生后端：`src/Magpie.Core/*Upscaler.*`、`RTXVideoDenoiser.*`、`DLSSFrameGenerator.*`
- 统一分派：`src/Magpie.Core/NativeEffectBackend*`
- Renderer 接入：`src/Magpie.Core/Renderer.*`
- Effect：`src/Effects/DLSS*`、`FSR*`、`XeSS`、`RTXVideo`、`DLSSFG`、`XeSSFG`
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
- 光流是假运动输入，不是引擎 MV；目前所有 Optical Flow 路径都不推荐日常使用。
- 伪 jitter 只有 DLSS 的主观结果尚可，其余后端的 jitter 结果不推荐。
- FSR4 会绕过 INT8 provider 的能力检查，只适合研究；二进制发布前必须单独检查授权。
- GPLv3 与 NVIDIA 专有组件的组合分发存在未解决风险。SDK、模型、wheel 和本机依赖目录不要提交；公开二进制前按许可证专项文档逐项审核。
