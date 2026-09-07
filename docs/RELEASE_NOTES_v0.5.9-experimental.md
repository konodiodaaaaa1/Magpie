# Magpie Experimental v0.5.9 使用说明（未发布历史草稿）

> 此版本的 GitHub Release Draft 已删除，从未正式发布。本文件仅保留为 0.6.0 发布准备过程的历史参考，不应作为可下载版本或当前安装说明引用。

## 本版本更新

- 新增可选 TensorRT 深度估算组件包。RTX 40 系用户在使用深度估算时，如果遇到初始化失败或性能开销过大的问题，可以下载并安装此组件包；其他用户无需下载。
- 已确认部分用户遇到的 NVIDIA VSR 初始化错误与 Magpie 所在路径有关。Magpie 所在路径及所有上级目录不能包含中文或特殊字符。

## 应该下载哪个文件

### 主包：`Magpie-Experimental-x64.zip`

所有用户都需要下载。请完整解压后运行，不要直接在压缩包内运行，也不要只替换 `Magpie.exe`。

主包不包含可选 TensorRT 深度估算组件。未安装可选组件时，深度估算使用 DirectML。

### 可选 TensorRT 深度估算组件包

仅建议 RTX 40 系用户在深度估算失败或开销过大时使用。

组件包由两个分卷组成：

- `Magpie-v0.5.9-TensorRT-Depth-Components-x64.7z.001`
- `Magpie-v0.5.9-TensorRT-Depth-Components-x64.7z.002`

请下载全部分卷，将它们放在同一目录，并使用 7-Zip 从 `.7z.001` 开始解压。随后按照包内《安装说明.txt》安装。不要将其中的 CUDA、cuDNN 或 TensorRT 文件与其他版本混用。

### `DLSSNR-DLL-Options-310.8.0.0.zip`

用于在 NVIDIA 原版 DLL 与 RTX 40/50 社区兼容 DLL 之间切换。替换前请完全退出 Magpie，并按照包内说明操作。

## NVIDIA VSR 错误 -2

如果看到“NVIDIA VSR 初始化失败（错误 -2）”，请将 Magpie 完整解压到符合要求的路径。Magpie 所在路径及所有上级目录不能包含中文或特殊字符，然后重新运行。

## 文件校验

发布附件的 SHA-256 校验值见本 Release 页面。

Contributor: [HexBen123](https://github.com/HexBen123)
