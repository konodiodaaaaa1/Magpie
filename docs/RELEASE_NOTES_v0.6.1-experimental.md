# Magpie Experimental v0.6.1 使用说明

## 本版本更新

### 重点修复 DLSSNR 切屏、捕获恢复和输入缩放问题，并改进缩放模式管理

建议正在使用 v0.6.0 的 DLSSNR 用户更新。尤其是遇到游戏切屏后画面异常、PotPlayer 全屏缩放异常、输入分辨率缩放红蓝通道反转，或需要更灵活管理缩放模式的用户。

- 修正 DLSSNR / Frame Guidance 时序生命周期：第一张真实捕获帧只用于建立历史；切屏、捕获恢复和至少 500 ms 的停顿会重置历史；相同 `frameId` 的重复呈现复用已有结果，避免把同一帧再次当作新的时序输入。
- 修复 Issue #8：DLSSNR 输入分辨率缩放不再对 BGRA typed SRV 已返回的逻辑 RGBA 结果重复交换红蓝通道；Depth Anything V2 的对应 BGRA 预处理同步修正，并新增 `sourceFormat` 日志字段用于排错。
- DLSSNR 输入分辨率缩放新增 `Residual Multiplier`。范围为 `1.00`–`2.00`，默认 `1.00`，步进 `0.05`；只在启用输入分辨率缩放时显示和生效。`1.00` 保持原残差强度，调高会增强回填到原分辨率的降噪残差，也可能放大伪影或过度处理。
- 支持按程序配置指定物理显示器作为全屏缩放目标。设置保存稳定设备 ID；目标显示器断开时保留配置，并在启动缩放时回退到最近的可用显示器。
- 重做缩放模式管理交互：缩放模式和效果器支持拖拽排序；新增模式复制、一键重置、直接删除/重命名以及新建后自动命名。
- 完成配置文件栏 UI 收尾：启动、更多选项和拖动提示采用平衡后的图标尺寸与间距；操作按钮默认透明并提供悬停、按下反馈；配置文件拖拽排序及位置预览统一采用与缩放模式组相同的直接位移反馈。
- 缩放模式和效果器操作列始终保留拖动区域；只有一个项目时隐藏并禁用拖动提示但不收回占位，避免删除按钮移入拖动位置造成误触。恢复默认缩放模式不再创建 DLSS SR 预设，DLSS SR 效果本身及已有配置不受影响。
- 修复拖拽完成时的 XAML fail-fast，以及删除模式后再次新建时自动重命名不再弹出的问题。
- 新安装默认允许缩放最大化或全屏窗口。
- 改进中英文错误提示和显示时长，为捕获失败、权限、裁剪、3D 游戏模式、Desktop Duplication、快捷键、导入导出和更新失败提供更明确的操作建议。

## 应该下载哪个文件

### 主包：`Magpie-Experimental-x64.zip`

所有用户均可使用的独立完整包。请完全退出 Magpie，将 ZIP 完整解压到新目录后运行；不要直接在压缩包内运行，也不要只替换 `Magpie.exe`。

主包不包含 `FrameGuidance/TensorRT` 可选深度估算组件；未安装可选组件时使用 DirectML 路径。

### v0.6.0 → v0.6.1 最小更新包：`Magpie-v0.6.0-to-v0.6.1-Minimal-Update-x64.zip`

这不是独立完整包。它仅用于覆盖官方 `v0.6.0-experimental` 主包，兼容已经应用 GitHub `v0.6.0-experimental-hotfix` 的安装；本更新包会完整取代 Hotfix 中的旧 `Magpie.exe`。

完全退出 Magpie 后，将包内以下 3 个文件按原目录结构覆盖到现有安装目录：

- `Magpie.exe`
- `resources.pri`
- `effects/DLSSNR/DLSSNR_AI_Filter.hlsl`

建议的覆盖顺序是“官方 v0.6.0 完整主包 → 官方 v0.6.0 Hotfix → 本最小更新包”。需要回退时，请重新解压官方 v0.6.0 完整主包；若仍停留在 v0.6.0，再按需重新覆盖 v0.6.0 Hotfix。不要只恢复上述文件中的一部分。

### 可选 TensorRT 深度估算组件

需要同时下载以下两个大小接近的分卷，并放在同一目录：

- `Magpie-v0.6.1-TensorRT-Depth-Components-x64.7z.001`
- `Magpie-v0.6.1-TensorRT-Depth-Components-x64.7z.002`

使用 7-Zip 从 `.7z.001` 开始解压。完全退出 Magpie 后，将解压得到的全部文件和目录复制到 `Magpie.exe` 所在目录，保持目录结构并允许覆盖。不要只复制个别 DLL，也不要混用其他版本的 CUDA、cuDNN 或 TensorRT 文件。

### DLSSNR DLL 替换包：`DLSSNR-DLL-Options-310.8.0.0.zip`

用于在 NVIDIA 原版 DLL 与 RTX 40/50 社区兼容 DLL 之间切换。两者文件版本均为 `310.8.0.0`。完全退出 Magpie 后只选择其中一个 `nvngx_dlssnr.dll` 覆盖到 `Magpie.exe` 所在目录，具体哈希与回退步骤见包内 `README.txt`。

### NGX OTA 开关：`NGX_OTA_Switch.bat`

用于查看、启用、禁用或恢复 NVIDIA NGX OTA 默认设置，也可以结束 `nvngx_update.exe`。修改系统注册表时会请求管理员权限；不需要排查 NGX 在线更新问题的用户无需运行。

## 已知问题与验证状态

- 本地候选已通过干净源码的 Release x64 重建和静态包结构检查；还使用与 GitHub 公布哈希一致的官方 v0.6.0 完整包及 Hotfix，按顺序覆盖本最小更新包并完成启动冒烟测试。PotPlayer 全屏/缩放、暂停恢复、游戏切屏、Frame Guidance 模式对照和颜色通道仍需在目标 NVIDIA GPU 上完成最终运行时回归。
- `.review-pr4` 中“缩放期间实时修改效果参数”和“实时应用帧生成/FrameRate Filter 参数”尚未合并，不包含在 v0.6.1 中。

## 文件校验

| 文件 | 字节数 | SHA-256 |
| --- | ---: | --- |
| `NGX_OTA_Switch.bat` | 4,294 | `D0F96B57F7E91101460EEB68CD618B2C11866A4DFC6901741C17ACECAC04C465` |
| `DLSSNR-DLL-Options-310.8.0.0.zip` | 226,489,312 | `9F5A69EFE34C93ADA337F35BD137646BB769F2795780646BB3802A10E32C7171` |
| `Magpie-v0.6.0-to-v0.6.1-Minimal-Update-x64.zip` | 2,174,892 | `62F940A23F754023AE532662FC9669F35F65CFC9A8128018062E0CD766EF405B` |
| `Magpie-Experimental-x64.zip` | 801,193,852 | `E6186F04E0AE3B046F741746626F09DE24ABFA7EF2422DCC8692FAD45C2ACDC0` |
| `Magpie-v0.6.1-TensorRT-Depth-Components-x64.7z.001` | 1,338,992,585 | `C6868853155422A0238843C6392F5A3601DBB5CFC5D9F27659FB1EEF26471048` |
| `Magpie-v0.6.1-TensorRT-Depth-Components-x64.7z.002` | 1,338,992,586 | `7A8DC85BEC7EE7A2FFD6671018898A2404AF29F036FD160C36A2F6F145F5EBC3` |

TensorRT 两分卷按顺序拼接后的数据流 SHA-256 为 `6FE6B6771E2B6B34204EA7571E3F54CB2E0332953F5B7CB0266E80CC7A423D0C`。

---

# Magpie Experimental v0.6.1 User Guide

## What's New

### DLSSNR focus-switching, capture-recovery, and input-scaling fixes, plus improved scaling-mode management

This update is recommended for DLSSNR users on v0.6.0, especially anyone affected by image corruption after switching away from a game, PotPlayer fullscreen scaling issues, red/blue inversion with input-resolution scaling, or cumbersome scaling-mode management.

- Corrected the DLSSNR / Frame Guidance temporal lifetime. The first real capture seeds history without being treated as a temporal update; switching focus, capture recovery, and pauses of at least 500 ms reset history; repeated presentation of the same `frameId` reuses the existing result instead of feeding the same frame into history again.
- Fixed Issue #8. DLSSNR input-resolution scaling no longer swaps red and blue after a typed BGRA SRV has already returned logical RGBA values. The corresponding Depth Anything V2 BGRA preprocessing path is also fixed, and a `sourceFormat` diagnostic field is now logged.
- Added a DLSSNR `Residual Multiplier` for input-resolution scaling. Its range is `1.00`–`2.00`, default is `1.00`, and step is `0.05`. It is visible and effective only while input-resolution scaling is enabled. `1.00` preserves the previous residual strength; higher values strengthen the denoising residual reconstructed at the original resolution and can also amplify artifacts or over-processing.
- Added per-profile selection of a physical monitor as the fullscreen scaling target. Magpie stores a stable device ID, preserves the setting when a monitor disconnects, and falls back to the closest available monitor when scaling starts.
- Reworked scaling-mode management with drag reordering for modes and effects, mode duplication, reset-to-default, direct delete/rename actions, and automatic naming after creation.
- Finished the profile-list UI polish: launch, more-options, and drag affordances now use visually balanced sizing and spacing; action buttons stay transparent until pointer-over or pressed; profile drag ordering and position previews now use the same direct displacement feedback as scaling-mode groups.
- Scaling-mode and effect action rows now keep a fixed drag column. When only one item exists, the drag glyph is hidden and disabled while its space remains reserved, preventing the delete action from moving into the drag target. Resetting scaling modes no longer creates the DLSS SR preset; the DLSS SR effect and existing configurations remain available.
- Fixed a XAML fail-fast during drag completion and automatic rename failing after deleting and recreating a mode.
- New installations allow scaling maximized or fullscreen windows by default.
- Improved Chinese and English error messages and display durations, with actionable guidance for capture failures, permissions, cropping, 3D game mode, Desktop Duplication, shortcuts, import/export, and update failures.

## Which File Should I Download?

### Main package: `Magpie-Experimental-x64.zip`

This is the standalone full package for all users. Fully exit Magpie, extract the entire ZIP to a new directory, and run it there. Do not run Magpie inside the archive and do not replace only `Magpie.exe`.

The main package excludes the optional `FrameGuidance/TensorRT` depth-estimation components. Without them, Magpie uses the DirectML path.

### v0.6.0 → v0.6.1 minimal update: `Magpie-v0.6.0-to-v0.6.1-Minimal-Update-x64.zip`

This is not a standalone package. It is only for an installation made from the official `v0.6.0-experimental` main package, and it is compatible with installations that already have the GitHub `v0.6.0-experimental-hotfix` applied. The older `Magpie.exe` from that Hotfix is fully replaced by this update.

Fully exit Magpie, then copy all three files from the archive into the existing installation while preserving their relative paths and allowing overwrite:

- `Magpie.exe`
- `resources.pri`
- `effects/DLSSNR/DLSSNR_AI_Filter.hlsl`

The recommended overlay order is “official v0.6.0 full package → official v0.6.0 Hotfix → this minimal update.” To roll back, extract the official v0.6.0 full package again. If remaining on v0.6.0, reapply the v0.6.0 Hotfix as needed. Do not restore only a subset of these files.

### Optional TensorRT depth-estimation components

Download both similarly sized volumes and place them in the same directory:

- `Magpie-v0.6.1-TensorRT-Depth-Components-x64.7z.001`
- `Magpie-v0.6.1-TensorRT-Depth-Components-x64.7z.002`

Use 7-Zip to extract starting from `.7z.001`. Fully exit Magpie, then copy everything produced by extraction into the directory containing `Magpie.exe`, preserving the directory structure and allowing overwrite. Do not copy only selected DLLs and do not mix these CUDA, cuDNN, or TensorRT files with other versions.

### DLSSNR DLL options: `DLSSNR-DLL-Options-310.8.0.0.zip`

Use this package to switch between the NVIDIA original DLL and the community RTX 40/50 compatibility DLL. Both have file version `310.8.0.0`. Fully exit Magpie, choose only one `nvngx_dlssnr.dll`, and copy it beside `Magpie.exe`. See the included `README.txt` for hashes and rollback instructions.

### NGX OTA switch: `NGX_OTA_Switch.bat`

Use this script to view, enable, disable, or restore the default NVIDIA NGX OTA setting, or to terminate `nvngx_update.exe`. Administrator access is requested when changing the system registry. Users who are not troubleshooting NGX online updates do not need to run it.

## Known Issues and Validation Status

- The local candidate passed a clean-source Release x64 rebuild and static package-layout checks. The minimal update was also overlaid, in order, onto official v0.6.0 full-package and Hotfix assets whose hashes match GitHub, followed by a successful startup smoke test. Final target-NVIDIA-GPU regression is still required for PotPlayer fullscreen/scaling, pause/resume, game focus switching, Frame Guidance mode comparisons, and color-channel behavior.
- The `.review-pr4` live effect-parameter editing and live frame-generation/FrameRate Filter parameter updates have not been merged and are not included in v0.6.1.

## File Verification

| File | Bytes | SHA-256 |
| --- | ---: | --- |
| `NGX_OTA_Switch.bat` | 4,294 | `D0F96B57F7E91101460EEB68CD618B2C11866A4DFC6901741C17ACECAC04C465` |
| `DLSSNR-DLL-Options-310.8.0.0.zip` | 226,489,312 | `9F5A69EFE34C93ADA337F35BD137646BB769F2795780646BB3802A10E32C7171` |
| `Magpie-v0.6.0-to-v0.6.1-Minimal-Update-x64.zip` | 2,174,892 | `62F940A23F754023AE532662FC9669F35F65CFC9A8128018062E0CD766EF405B` |
| `Magpie-Experimental-x64.zip` | 801,193,852 | `E6186F04E0AE3B046F741746626F09DE24ABFA7EF2422DCC8692FAD45C2ACDC0` |
| `Magpie-v0.6.1-TensorRT-Depth-Components-x64.7z.001` | 1,338,992,585 | `C6868853155422A0238843C6392F5A3601DBB5CFC5D9F27659FB1EEF26471048` |
| `Magpie-v0.6.1-TensorRT-Depth-Components-x64.7z.002` | 1,338,992,586 | `7A8DC85BEC7EE7A2FFD6671018898A2404AF29F036FD160C36A2F6F145F5EBC3` |

The SHA-256 of the ordered, concatenated TensorRT volume stream is `6FE6B6771E2B6B34204EA7571E3F54CB2E0332953F5B7CB0266E80CC7A423D0C`.
