# Magpie Experimental v0.6.0 使用说明

## 本版本更新

### 加入 DLSSNR 的性能优化，建议所有人更新

- DLSSNR 新增可选的输入分辨率调整功能。开启后可将 DLSSNR 的输入分辨率设置为原始分辨率的 25%–100%，以降低性能开销。
  此功能默认关闭；滑块默认值为 100%，步进为 1%。关闭时保持原有的 DLSSNR 处理方式，且不显示输入分辨率滑块。
- 降低输入分辨率会逐渐降低 DLSSNR 的处理质量，当分辨率过低时质量会快速下降。
  因此不要降得太低，建议输入分辨率至少在 480p 以上。
  DLSSNR 以调整后的分辨率处理颜色、运动和深度输入，随后将处理差值重建到原始分辨率并叠加回原始颜色。
- 调整了 DLSSNR 参数顺序：输入分辨率开关和滑块位于参数列表顶部；Automatic Mask 与 NR UI Correction 位于 Skin Structure Strength 和 Frame Guidance 之间。

## 应该下载哪个文件

### 主包：`Magpie-Experimental-x64.zip`

所有用户都需要下载。请完整解压后运行，不要直接在压缩包内运行，也不要只替换 `Magpie.exe`。

主包不包含可选 TensorRT 深度估算组件。未安装可选组件时，深度估算使用 DirectML。

### 可选 TensorRT 深度估算组件包

- 建议 RTX 50 系用户追求更低的深度推理开销时使用。在 2K 分辨率和 RTX 5070 Ti 上，可以降低约 0.2–1.2 ms 的深度估算开销；实际结果会因硬件和使用环境而异。
- 建议所有 RTX 40 系用户使用；40 系用户更容易遇到深度估算失败或开销过大的问题。

组件包由两个分卷组成：

- `Magpie-v0.6.0-TensorRT-Depth-Components-x64.7z.001`
- `Magpie-v0.6.0-TensorRT-Depth-Components-x64.7z.002`

请下载全部分卷，将它们放在同一目录，并使用 7-Zip 从 `.7z.001` 开始解压。完全退出 Magpie 后，将解压得到的全部文件和目录直接复制到 `Magpie.exe` 所在目录，保持原有目录结构并允许覆盖同名文件。不要只复制个别 DLL，也不要与其他版本的 CUDA、cuDNN 或 TensorRT 文件混用。

### `DLSSNR-DLL-Options-310.8.0.0.zip`

用于在 NVIDIA 原版 DLL 与 RTX 40/50 社区兼容 DLL 之间切换。替换前请完全退出 Magpie，并按照包内说明操作。

## DLSSNR 输入分辨率调整

在 DLSSNR 参数列表顶部开启“Adjust Input Resolution (Reduces DLSSNR Quality)”后，可使用输入分辨率滑块选择 25%–100%。数值越低，DLSSNR 的性能开销通常越小，但画面细节、稳定性和降噪质量也可能随之下降。

建议先从较高比例开始逐步降低，并根据内容、输入分辨率、GPU 性能和可接受的画质损失选择合适数值。设置为 100% 时不会降低 DLSSNR 输入分辨率。

## NVIDIA VSR 错误 -2

如果看到“NVIDIA VSR 初始化失败（错误 -2）”，请将 Magpie 完整解压到符合要求的路径。Magpie 所在路径及所有上级目录不能包含中文或特殊字符，然后重新运行。

## 文件校验

- `Magpie-Experimental-x64.zip`：`CB5216CC426758755A5CCF10F16EDFD4E193C44C6AC4AB9713A3201BDFD1DA9A`
- `Magpie-v0.6.0-TensorRT-Depth-Components-x64.7z.001`：`7CAD54CB3B88A5B77DADECBE95BF8FF184E191BB5B14D208B44DD389536F6824`
- `Magpie-v0.6.0-TensorRT-Depth-Components-x64.7z.002`：`C8963D1CD8A98A8B5106B1FE20ACDD5D08520A4E4C9B8738974390BCC415084B`
- `DLSSNR-DLL-Options-310.8.0.0.zip`：`DBA1547FA9D54BC6F1C6E09B20AD8A21AF9408349D5F28FD24C83AE1966909E8`

Contributor：[HexBen123](https://github.com/HexBen123) 提供了深度估算的开销优化思路。

---

# Magpie Experimental v0.6.0 User Guide

## What's New

### DLSSNR performance optimization—recommended update for all users

- DLSSNR now provides an optional input-resolution adjustment. When enabled, the DLSSNR input resolution can be set to 25%–100% of the original resolution to reduce performance overhead.
  This feature is disabled by default. The slider defaults to 100% with a 1% step. When disabled, DLSSNR keeps its previous processing behavior and the input-resolution slider is hidden.
- Lowering the input resolution gradually reduces DLSSNR processing quality, and quality drops rapidly when the resolution becomes too low.
  Therefore, avoid setting it too low; an input resolution of at least 480p is recommended. DLSSNR processes the color, motion, and depth inputs at the adjusted resolution, reconstructs the processed difference at the original resolution, and composites it back onto the original color input.
- The DLSSNR parameter order has been adjusted: the input-resolution toggle and slider are at the top of the parameter list, while Automatic Mask and NR UI Correction are placed between Skin Structure Strength and Frame Guidance.

## Which file should I download?

### Main package: `Magpie-Experimental-x64.zip`

All users should download this package. Fully extract it before running Magpie. Do not run Magpie directly from the ZIP archive, and do not replace only `Magpie.exe`.

The main package does not include the optional TensorRT depth-estimation components. Without the optional components, depth estimation uses DirectML.

### Optional TensorRT depth-estimation component package

- Recommended for RTX 50-series users who want lower depth-inference overhead. At 2K resolution on an RTX 5070 Ti, it can reduce depth-estimation overhead by approximately 0.2–1.2 ms.
- Recommended for all RTX 40-series users, who are more likely to encounter depth-estimation failures or excessive overhead.

The component package consists of two volumes:

- `Magpie-v0.6.0-TensorRT-Depth-Components-x64.7z.001`
- `Magpie-v0.6.0-TensorRT-Depth-Components-x64.7z.002`

Download both volumes, place them in the same directory, and use 7-Zip to extract starting from `.7z.001`. Fully exit Magpie, then copy all extracted files and directories directly into the directory containing `Magpie.exe`. Preserve the directory structure and allow files with the same names to be overwritten. Do not copy only individual DLLs, and do not mix these CUDA, cuDNN, or TensorRT files with other versions.

### `DLSSNR-DLL-Options-310.8.0.0.zip`

Use this package to switch between the original NVIDIA DLL and the community compatibility DLL for RTX 40/50-series GPUs. Fully exit Magpie before replacing the DLL, and follow the instructions included in the package.

## NVIDIA VSR error -2

If you see “NVIDIA VSR initialization failed (error -2),” fully extract Magpie to a compatible path. The Magpie path and all parent directories must not contain Chinese characters or special characters. Then run Magpie again.

## Asset checksums

- `Magpie-Experimental-x64.zip`: `CB5216CC426758755A5CCF10F16EDFD4E193C44C6AC4AB9713A3201BDFD1DA9A`
- `Magpie-v0.6.0-TensorRT-Depth-Components-x64.7z.001`: `7CAD54CB3B88A5B77DADECBE95BF8FF184E191BB5B14D208B44DD389536F6824`
- `Magpie-v0.6.0-TensorRT-Depth-Components-x64.7z.002`: `C8963D1CD8A98A8B5106B1FE20ACDD5D08520A4E4C9B8738974390BCC415084B`
- `DLSSNR-DLL-Options-310.8.0.0.zip`: `DBA1547FA9D54BC6F1C6E09B20AD8A21AF9408349D5F28FD24C83AE1966909E8`

Contributor: [HexBen123](https://github.com/HexBen123) contributed the optimization idea for reducing depth-estimation overhead.
