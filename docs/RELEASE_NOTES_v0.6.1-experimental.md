# Magpie Experimental v0.6.1

## 功能更新

### Magpie 本体易用性更新

- 重做配置文件列表：非默认配置文件可直接在列表中启动、打开更多选项和拖拽排序；拖动时会实时预览其他配置文件的位置变化，按钮尺寸、间距和交互状态也进行了统一。
- 重做缩放模式管理：缩放模式和效果器均可拖拽排序；新建时直接创建空白模式并自动打开重命名；新增复制、删除、重命名和重置配置。重置只恢复默认缩放模式，不会删除效果器；新的默认模式中不再自动加入 DLSS SR。
- 参数设置面板允许使用更大的高度；配置导入、配置导出以及各项操作按钮的名称和布局更加明确。
- 每个程序配置都可以选择具体的物理显示器作为全屏缩放目标。显示器暂时断开时会保留选择，并在启动缩放时回退到可用显示器。新安装默认允许缩放最大化或全屏窗口。
- 修复拖拽完成时的闪退、删除新建项后再次新建不自动进入重命名等问题。多项失败提示现在会显示约 5 秒，并提供权限、窗口状态、捕获方式、裁剪、快捷键、导入导出等下一步处理建议。

### 效果器更新

- 修正 DLSSNR / Frame Guidance 的时序历史管理：第一张真实捕获帧用于建立历史；切换窗口、捕获恢复或画面停顿后会重置历史；重复呈现同一帧时复用已有结果，减少错误的时序累积。
- 修复 DLSSNR 输入分辨率缩放时可能出现的红蓝通道反转，并同步修正 Depth Anything V2 的 BGRA 输入预处理。
- DLSSNR 新增 `Residual Multiplier`。它只在启用输入分辨率调整后显示和生效，范围为 `1.00`–`2.00`，默认值为 `1.00`，步进为 `0.05`。提高数值会增强回填到原始分辨率的降噪残差，但也可能放大伪影或过度处理。

## 应该下载哪个文件？

### 如果你的 Magpie 版本是实验版 0.6.0

可以在 Magpie 的“关于”页面查看当前版本号。如果显示为 `0.6.0`，并且你使用的是本仓库发布的 Experimental 版本，可以下载：

`Magpie-v0.6.0-to-v0.6.1-Minimal-Update-x64.zip`

这是最小更新包，不能独立运行。它同时兼容已经安装和没有安装 v0.6.0 Hotfix 的环境。请完全退出 Magpie，将压缩包中的下列文件按原目录结构解压到现有安装目录并允许覆盖：

- `Magpie.exe`
- `resources.pri`
- `effects/DLSSNR/DLSSNR_AI_Filter.hlsl`

### 如果你使用更早的版本，或者第一次下载

请下载完整主包：

`Magpie-Experimental-x64.zip`

完全退出旧版 Magpie，将主包完整解压到一个新目录，再运行其中的 `Magpie.exe`。不要直接在压缩包内运行，也不要只替换主程序。完整主包已包含可直接使用的 DirectML 深度估算路径，但不包含下面的可选 TensorRT 深度估算组件。

### 可选 TensorRT 深度估算组件

RTX 40 系用户，以及希望进一步降低深度推理开销的 RTX 50 系用户，可以另外下载：

- `Magpie-v0.6.1-TensorRT-Depth-Components-x64.7z.001`
- `Magpie-v0.6.1-TensorRT-Depth-Components-x64.7z.002`

这两个文件是同一个压缩包的两个分卷，必须同时下载并放在同一目录。无需手动合并；全部下载完成后，使用 7-Zip 打开或解压以 `.7z.001` 结尾的文件，两个分卷会被自动作为一个完整压缩包读取。

完全退出 Magpie 后，将解压得到的全部文件和目录复制到 `Magpie.exe` 所在目录，保持目录结构并允许覆盖。不要只复制其中几个 DLL，也不要混用其他版本的 CUDA、cuDNN 或 TensorRT 文件。

### 可选 DLSSNR DLL：`DLSSNR-DLL-Options-310.8.0.0.zip`

如果需要在 NVIDIA 官方版和 RTX 40/50 社区兼容版 `nvngx_dlssnr.dll` 之间切换，请下载此文件。完全退出 Magpie，选择其中一个版本并解压到 Magpie 根目录，替换现有的 `nvngx_dlssnr.dll`；不要同时混用两个版本。

如果你在其他程序或测试环境中也需要这些 DLL，也可以从该压缩包中单独解压使用；是否兼容由对应程序和使用环境决定。

### NGX OTA 开关：`NGX_OTA_Switch.bat`

如果你注意到异常的内存占用或内存泄漏，这通常与 NVIDIA NGX 的 OTA 更新进程异常累积有关。可以使用这个 BAT 关闭 NGX OTA，并清理已经启动的 `nvngx_update.exe` 进程；需要时也可以随时使用它恢复 NVIDIA 默认设置。

该工具修改系统级 NGX 设置，运行相关操作时需要管理员权限，也可能影响其他使用 NGX 的程序。

---

# Magpie Experimental v0.6.1

## Feature Updates

### Magpie Usability Improvements

- Reworked the profile list. Non-default profiles can now be launched, opened for more options, and reordered directly from the list. Other profiles preview their new positions while dragging, and the action-button sizing, spacing, and interaction states have been unified.
- Reworked scaling-mode management. Scaling modes and effects can both be reordered by dragging. Creating a mode now produces a blank mode and immediately opens rename. Duplicate, delete, rename, and reset actions are available. Reset restores only the default scaling modes and does not delete effects; DLSS SR is no longer automatically included in the new default modes.
- Increased the available height of the effect-parameter panel, and clarified the names and layout of configuration import, configuration export, and related actions.
- Each application profile can select a specific physical monitor as its fullscreen scaling target. The selection is preserved while a monitor is disconnected, with fallback to an available monitor when scaling starts. New installations allow scaling maximized or fullscreen windows by default.
- Fixed crashes when completing drag operations and automatic rename failing after deleting and recreating an item. Many failure messages now remain visible for about five seconds and provide actionable guidance for permissions, window state, capture methods, cropping, shortcuts, import, and export.

### Effect Updates

- Corrected DLSSNR / Frame Guidance temporal-history management. The first real captured frame seeds history; switching windows, capture recovery, or a pause in captured frames resets history; repeated presentation of the same frame reuses the existing result instead of accumulating it again.
- Fixed possible red/blue channel inversion when using DLSSNR input-resolution scaling, together with the corresponding BGRA preprocessing in Depth Anything V2.
- Added `Residual Multiplier` to DLSSNR. It is visible and effective only when input-resolution adjustment is enabled. Its range is `1.00`–`2.00`, the default is `1.00`, and the step is `0.05`. Higher values strengthen the denoising residual reconstructed at the original resolution, but may also amplify artifacts or over-processing.

## Which File Should I Download?

### If your Magpie version is Experimental 0.6.0

You can check the current version on Magpie's About page. If it shows `0.6.0` and you installed the Experimental build from this repository, download:

`Magpie-v0.6.0-to-v0.6.1-Minimal-Update-x64.zip`

This is a minimal update and cannot run by itself. It supports installations both with and without the v0.6.0 Hotfix. Fully exit Magpie, then extract the following files into the existing installation while preserving their relative paths and allowing overwrite:

- `Magpie.exe`
- `resources.pri`
- `effects/DLSSNR/DLSSNR_AI_Filter.hlsl`

### If you use an earlier version or are downloading Magpie for the first time

Download the complete main package:

`Magpie-Experimental-x64.zip`

Fully exit the old version of Magpie, extract the complete package into a new directory, and run `Magpie.exe` from that directory. Do not run Magpie from inside the ZIP or replace only the main executable. The main package includes the ready-to-use DirectML depth-estimation path, but it does not include the optional TensorRT depth-estimation components described below.

### Optional TensorRT Depth-Estimation Components

RTX 40-series users, and RTX 50-series users who want to further reduce depth-inference overhead, can also download:

- `Magpie-v0.6.1-TensorRT-Depth-Components-x64.7z.001`
- `Magpie-v0.6.1-TensorRT-Depth-Components-x64.7z.002`

These are two volumes of the same archive. Both files must be downloaded and placed in the same directory. Do not combine them manually. After both downloads finish, open or extract the file ending in `.7z.001` with 7-Zip; the two volumes will automatically be read as one complete archive.

Fully exit Magpie, then copy every extracted file and directory into the directory containing `Magpie.exe`. Preserve the directory structure and allow overwrite. Do not copy only selected DLLs, and do not mix these CUDA, cuDNN, or TensorRT files with other versions.

### Optional DLSSNR DLLs: `DLSSNR-DLL-Options-310.8.0.0.zip`

Download this file if you need to switch between the official NVIDIA build and the community RTX 40/50-compatible build of `nvngx_dlssnr.dll`. Fully exit Magpie, select one version, and extract it into the Magpie root directory to replace the existing `nvngx_dlssnr.dll`. Do not mix both versions.

You may also extract these DLLs for another application or test environment that needs them. Compatibility depends on that application and environment.

### NGX OTA Switch: `NGX_OTA_Switch.bat`

If you notice abnormal memory usage or a memory leak, it is commonly associated with accumulated NVIDIA NGX OTA update processes. This BAT can disable NGX OTA and clean up already-running `nvngx_update.exe` processes. It can also restore NVIDIA's default setting at any time.

The tool changes a system-wide NGX setting, requires administrator privileges for the relevant actions, and may affect other applications that use NGX.
