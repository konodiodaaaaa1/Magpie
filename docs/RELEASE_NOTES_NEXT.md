# Magpie Experimental v0.6.1（开发中） / In Development

当前公开常规版本为 [v0.6.0-experimental](RELEASE_NOTES_v0.6.0-experimental.md)，其已发布的最小修复为 [v0.6.0-experimental-hotfix](RELEASE_NOTES_v0.6.0-experimental-hotfix.md)。当前 `experimental` 开发线的下一版本确定为 **v0.6.1-experimental**；尚未创建标签或 GitHub Release。

`version.json` 在 v0.6.1 正式发布前继续指向当前公开常规版 v0.6.0。仓库中已有一个 2021 年的历史标签 `v0.6.1`，因此本实验版发布时必须使用完整标签 `v0.6.1-experimental`，不得移动或复用历史标签。

## 当前功能范围

- 新增按程序配置指定物理显示器作为全屏缩放目标；保存稳定设备 ID，显示器断开时保留配置并在启动缩放时回退到最近显示器。
- 重做缩放模式管理交互：支持缩放模式和效果器拖拽排序、复制模式、一键重置、直接删除/重命名以及新建后自动命名。
- 修复缩放模式拖拽完成时的 XAML fail-fast，以及删除后再次新建不自动打开重命名的问题。
- 修复 Issue #8：DLSSNR 输入分辨率缩放路径不再对 BGRA typed SRV 的逻辑 RGBA 结果重复交换红蓝通道；同步修复 Depth Anything V2 预处理，并记录 `sourceFormat` 诊断字段。
- DLSSNR 输入分辨率缩放新增 `Residual Multiplier`，用于调节低分辨率处理结果回填原始分辨率时的残差强度。
- 修复 DLSSNR / Frame Guidance 时序生命周期：首张真实捕获建立历史；切屏、捕获恢复和至少 500 ms 停顿重置历史；相同 `frameId` 复用已有结果。
- “允许缩放最大化或全屏窗口”的新安装默认值改为开启。
- 改进中英文错误提示和显示时长，为捕获、权限、裁剪、3D 游戏模式、Desktop Duplication、快捷键、导入导出及更新失败提供可执行建议。

## 尚待验证或未纳入

- 待完成 NVIDIA GPU 上的 PotPlayer 全屏、暂停/恢复、游戏切屏、Frame Guidance 模式对照及颜色通道回归。
- 待完成缩放模式和效果器高频拖拽、连续新建/删除以及管理员模式回归。
- `.review-pr4` 中“缩放期间实时修改效果参数”和“实时应用帧生成/FrameRate Filter 参数”尚未合并到 `experimental`，不属于当前 v0.6.1 包。

后续版本按照 [实验版发布规范](experimental/RELEASE-WORKFLOW.md) 准备和审核。

---

The current public regular release is [v0.6.0-experimental](RELEASE_NOTES_v0.6.0-experimental.md), with the published minimal fix [v0.6.0-experimental-hotfix](RELEASE_NOTES_v0.6.0-experimental-hotfix.md). The next version on the `experimental` development line is now **v0.6.1-experimental**. No tag or GitHub Release has been created yet.

Until v0.6.1 is formally released, `version.json` continues to identify the current public regular release, v0.6.0. A historical `v0.6.1` tag from 2021 already exists, so this experimental release must use the complete `v0.6.1-experimental` tag without moving or reusing the historical tag.

## Current feature scope

- Per-profile selection of a physical monitor as the fullscreen scaling target, using a stable device ID with disconnected-monitor preservation and closest-monitor fallback.
- Reworked scaling-mode management with drag reordering for modes and effects, mode duplication, reset-to-default, direct delete/rename actions, and automatic naming after creation.
- Fixes for XAML fail-fast during drag completion and automatic rename failing after deleting and recreating a mode.
- Issue #8 fix: DLSSNR input-resolution scaling no longer swaps red and blue after a typed BGRA SRV has already returned logical RGBA values. The matching Depth Anything V2 preprocessing error is fixed, and `sourceFormat` is logged for diagnostics.
- A DLSSNR `Residual Multiplier` for controlling the residual strength when reconstructing reduced-resolution processing at the original resolution.
- DLSSNR / Frame Guidance lifetime fixes: seed history from the first real capture; reset after focus/capture recovery or pauses of at least 500 ms; reuse results for duplicate `frameId` values.
- New installations allow scaling maximized or fullscreen windows by default.
- More actionable Chinese and English error messages with longer display time for capture, permissions, cropping, 3D game mode, Desktop Duplication, shortcuts, import/export, and update failures.

## Pending validation or excluded work

- NVIDIA GPU validation remains for PotPlayer fullscreen, pause/resume, game focus switching, Frame Guidance mode comparisons, and color-channel regression.
- High-frequency mode/effect dragging, repeated create/delete flows, and elevated-mode behavior still require runtime regression testing.
- The `.review-pr4` live effect-parameter editing and live frame-generation/FrameRate Filter updates have not been merged into `experimental` and are not part of the current v0.6.1 package.

Future release preparation and review follow the [experimental release workflow](experimental/RELEASE-WORKFLOW.md).
