# HDR + DLSS5 初步适配

这是一套给 Windows HDR 桌面使用的实验配置。它保留系统 HDR 开启，让 DLSSNR，也就是当前测试使用的 DLSS5 神经滤镜，继续处理捕获画面。配置重点在于把 HDR 捕获帧先转换成 DLSSNR 能稳定处理的 SDR R8 颜色缓冲，处理完成后再转换回 FP16 scRGB，交给 HDR 呈现器。

## 使用前必须确认

1. Windows 设置中的显示器 HDR 已经开启。
2. Magpie 配置文件的“捕获方式”必须选择 `Graphics Capture (HDR)`。普通 `Graphics Capture` 走 B8G8R8A8 SDR 捕获路径，这套 HDR 配置的 FP16 输入不会建立。
3. 缩放模式选择 `HDR DLSS5 Bridge`。这个模式来自 `presets/ScalingModes-HDR-DLSS5.json`，导入方式和普通缩放模式相同。
4. `nvngx_dlssnr.dll` 必须放在 Magpie 程序目录，当前测试使用 RTX 4070 和实验包中的 DLSSNR 运行库。
5. 测试时关闭其他帧生成或后处理链，避免 DLSSFG、XeSSFG、RTX Video 等效果混入结果。

## 配置步骤

导入 `presets/ScalingModes-HDR-DLSS5.json` 后，在目标配置文件中设置：

```text
捕获方式: Graphics Capture (HDR)
缩放模式: HDR DLSS5 Bridge
```

配置文件里对应的效果顺序必须保持如下：

```text
HDRToSDR
FrameRate_Filter (targetFrameRate = 60)
DLSSNR\\DLSSNR_AI_Filter
SDRToHDR
```

只导入缩放模式、捕获方式仍选普通 `Graphics Capture` 时，HDR 输入不会进入这条链。只选择 HDR 捕获、缩放模式继续使用普通 `DLSSNR` 时，DLSSNR 会直接接收 FP16 scRGB，当前运行库在本机测试出现过红色通道异常。

## 四段效果分别做什么

| 顺序 | 效果 | 输入 -> 输出 | 作用 |
| --- | --- | --- | --- |
| 1 | `HDRToSDR` | FP16 scRGB -> R8 SDR | 按 Windows HDR 桌面的 SDR 白点缩放，并把线性颜色转换成 sRGB |
| 2 | `FrameRate_Filter` | R8 SDR -> R8 SDR | 将处理帧率限制为 60 FPS，降低静态浏览器画面的重复提交 |
| 3 | `DLSSNR\\DLSSNR_AI_Filter` | R8 SDR -> R8 SDR | 执行 DLSS5/DLSSNR 神经降噪和细节处理 |
| 4 | `SDRToHDR` | R8 SDR -> FP16 scRGB | 将 DLSS5 输出恢复为 HDR 呈现器使用的 FP16 格式 |

DLSSNR 在这套配置中保持原有的 R8 输入输出合约。它的前后格式转换由两个独立效果完成，避免跨格式 `CopyResource` 和 FP16 通道解释问题。

## SDR 白点参数

`HDRToSDR.hlsl` 和 `SDRToHDR.hlsl` 当前使用 `sdrWhiteScale = 4.5`。这个值来自测试显示器的 Windows 显示配置：

```text
DISPLAYCONFIG_SDR_WHITE_LEVEL = 4500
4500 / 1000 = 4.5x
4.5x * 80 nits = 360 nits
```

它决定浏览器、文件管理器和其他 SDR 窗口在 HDR 桌面中的亮度。换显示器后，这个值需要重新读取。两个 HLSL 文件必须使用同一个值，否则处理前后的亮度会发生变化。

Windows 原生 HDR 截图工具通常也会读取 `DISPLAYCONFIG_SDR_WHITE_LEVEL`，再将 FP16 scRGB 除以该参考白点。固定写入 4.5 只对应本次测试显示器，其他设备建议按实际参数调整。

## 测试环境和结果

- 操作系统：Windows 11，系统 HDR 保持开启
- 显卡：NVIDIA GeForce RTX 4070
- 测试窗口：Microsoft Edge
- 捕获方式：`Graphics Capture (HDR)`
- 缩放模式：`HDR DLSS5 Bridge`
- DLSSNR：默认 Preset、默认 Style、Intensity 1、Guidance Mode 0
- 帧率限制：60 FPS
- SDR 白点：4.5x，360 nits

浏览器视频画面可以正常显示，DLSSNR 连续执行，测试过程中没有黑屏或红色通道异常。经过 SDR 工作缓冲后，极亮 HDR 高光会受到 R8 范围限制，原始 HDR 动态范围暂时无法完整保留；这也是本次适配使用“初步”描述的原因。

## 日志检查

启动缩放后，在程序目录的 `logs\\magpie.log` 中确认：

```text
当前捕获模式: Graphics Capture (HDR)
读取文本文件: effects\\HDRToSDR.hlsl
读取文本文件: effects\\SDRToHDR.hlsl
DLSSNR STATUS: Feature=18 ... result=0x1 success=...
```

如果看不到 `当前捕获模式: Graphics Capture (HDR)`，请回到配置文件检查捕获方式。如果出现效果文件缺失，说明 Release 没有完整解压到 `effects` 目录。如果 DLSSNR 成功计数持续增加，说明 DLSS5 已经在处理帧。

## 当前限制

- 这是 HDR 的初步适配，HDR 画面会经过 SDR R8 工作缓冲。
- 极亮高光会受到 SDR 工作范围限制，原始 HDR 高光细节暂时无法完整保留。
- SDR 白点当前按测试显示器写入效果文件，换显示器需要重新校准。
- 最大化源窗口会沿用 Magpie 的全屏缩放路径。
