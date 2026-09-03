# 本地实验构建依赖清单

本文记录 `local-0.6.1-hdr` 当前完整构建使用过的本地依赖。依赖目录不提交到源码仓库；下次构建前按表恢复到相同路径即可。所有版本号、提交号和关键文件名都来自本次实际构建环境。

## 构建工具

| 项目 | 当前版本/位置 |
| --- | --- |
| Visual Studio | `D:\Program Files\Microsoft Visual Studio\2022\Community` |
| MSBuild | `D:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\amd64\MSBuild.exe`，17.14.40 |
| CMake | Visual Studio 内置 CMake 3.31.6 |
| Conan | Conan 2，通过 `D:\AI\workspace\conan.cmd` 调用 |
| Windows SDK | `D:\Windows Kits\10\Include\10.0.26100.0` |
| NuGet 包 | 源码目录下的 `packages/`，版本由各项目 `packages.config` 固定 |

## SDK 与运行库

| 功能开关 | 开发 SDK 目录 | 版本/提交 | 获取地址 | 构建需要的内容 |
| --- | --- | --- | --- | --- |
| `EnableDLSSSR`、`EnableDLSSFrameGeneration`、`EnableDLSSNR` | `D:\AI\workspace\NVIDIA-DLSS` | DLSS `v310.7.0`，提交 `a291cc7d2cc642a51566f3dfd5376f635cd1b284` | [NVIDIA/DLSS](https://github.com/NVIDIA/DLSS/releases/tag/v310.7.0) | `include/`、`lib/Windows_x86_64/x64/nvsdk_ngx_s.lib` |
| DLSSNR 运行库 | `D:\AI\Magpie-Experimental-x64` | 当前测试包随附的 `nvngx_dlssnr.dll` | [Magpie v0.6.1 Release](https://github.com/SAOG0721/Magpie/releases/tag/v0.6.1-experimental) | `nvngx_dlssnr.dll`、`DLSSNRRuntimeDir` 指向程序目录 |
| `EnableXeSSZeroMV`、`EnableXeSSFrameGeneration` | `D:\AI\workspace\dependencies\XeSS-SDK` | XeSS `v3.0.2`，提交 `8fe81bdbbaf00b3c1b733fd0d830c333dc84e6f0` | [intel/xess v3.0.2](https://github.com/intel/xess/releases/tag/v3.0.2) | `inc/`、`lib/libxess.lib`、`lib/libxess_fg.lib`、`lib/libxell.lib` |
| `EnableFSR2ZeroMV` | `D:\AI\workspace\dependencies\FSR2-DX11-source` | FidelityFX FSR2 `v2.2.1`，提交 `1680d1edd5c034f88ebbbb793d8b88f8842cf804` | [GPUOpen-Effects/FidelityFX-FSR2 v2.2.1](https://github.com/GPUOpen-Effects/FidelityFX-FSR2/releases/tag/v2.2.1) | `src/ffx-fsr2-api/`，另需下方 DX11 头文件 |
| FSR2 DX11 兼容头 | 同上 | 来自 OptiScaler 提交 `65a5f6f9c5cec6e0a898bf7accc4bfcdfc807a02` | [optiscaler/OptiScaler](https://github.com/optiscaler/OptiScaler) | `src/ffx-fsr2-api/dx11/ffx_fsr2_dx11.h`、`dx11/shaders/ffx_fsr2_shaders_dx11.h` |
| FSR2 运行库 | `D:\AI\Magpie-Experimental-x64` | 当前测试包中的 FSR2 D3D11 DLL | [Magpie v0.6.1 Release](https://github.com/SAOG0721/Magpie/releases/tag/v0.6.1-experimental) | `ffx_fsr2_api_x64.dll`、`ffx_fsr2_api_dx11_x64.dll` |
| `EnableFSR3ZeroMV` | `D:\AI\workspace\dependencies\FidelityFX-SDK` | FidelityFX SDK `v2.3.0`，提交 `60f4ea81909200d8542eca14dccb2628b763a9a3` | [GPUOpen-LibrariesAndSDKs/FidelityFX-SDK](https://github.com/GPUOpen-LibrariesAndSDKs/FidelityFX-SDK) | `Kits/FidelityFX/api/include/`、`Kits/FidelityFX/upscalers/include/` |
| `EnableRTXVideoDenoise` | `D:\AI\workspace\dependencies\NVIDIA-VFX-SDK` | Maxine VFX SDK `v0.7.6`，提交 `f12bd18929e9065cff4f24cb93ac5f4202dc1c4a` | [NVIDIA-Maxine/Maxine-VFX-SDK](https://github.com/NVIDIA-Maxine/Maxine-VFX-SDK) | `nvvfx/include/`、`nvvfx/src/NVVideoEffectsProxy.cpp`、`nvvfx/src/nvCVImageProxy.cpp` |
| NVIDIA Optical Flow | `D:\AI\workspace\dependencies\NvidiaOpticalFlow-SDK` | NVOF 3.0.15 兼容头，提交 `330daf5d14a40e8fc1684bc00d32a993dc701dac` | [兼容头来源](https://github.com/mbucchia/Optical-Flow-SDK) | `nvOpticalFlowCommon.h`、`nvOpticalFlowD3D11.h`。当前代码需要 `predDirection`、反向输出和 `inputBufferFormat` 字段 |

## 深度与 TensorRT

`EnableDepthAnythingV2` 使用程序目录中的本地运行时和模型，不需要把这些大文件提交到源码仓库：

| 项目 | 当前路径 |
| --- | --- |
| Depth Anything V2 模型 | `D:\AI\Magpie-Experimental-x64\FrameGuidance\DepthAnythingV2\model_fp16.onnx` |
| TensorRT / CUDA / cuDNN / ONNX Runtime | `D:\AI\Magpie-Experimental-x64\FrameGuidance\TensorRT` |
| DirectML / ONNX Runtime | `D:\AI\Magpie-Experimental-x64\FrameGuidance\DirectML` |
| DirectML 开发头 | `E:\Program Files\ONNXRuntime\1.24.1\build\native\include` |

程序目录还需要保留对应许可证目录和 `ThirdPartyNotices.txt`。这些运行库体积很大，适合单独保存成测试组件包。

## 当前 `BuildOptions.props.user` 路径

本次完整构建使用的路径集中在 `src/BuildOptions.props.user`。恢复依赖后，先检查这些路径：

```powershell
Test-Path 'D:\AI\workspace\NVIDIA-DLSS\include\nvsdk_ngx.h'
Test-Path 'D:\AI\workspace\dependencies\XeSS-SDK\inc\xess\xess.h'
Test-Path 'D:\AI\workspace\dependencies\FSR2-DX11-source\src\ffx-fsr2-api\dx11\ffx_fsr2_dx11.h'
Test-Path 'D:\AI\workspace\dependencies\FidelityFX-SDK\Kits\FidelityFX\api\include\ffx_api.h'
Test-Path 'D:\AI\workspace\dependencies\NVIDIA-VFX-SDK\nvvfx\include\nvVideoEffects.h'
Test-Path 'D:\AI\workspace\dependencies\NvidiaOpticalFlow-SDK\nvOpticalFlowD3D11.h'
```

然后从源码根目录执行：

```powershell
pwsh -File scripts/Build-Release.ps1 `
  -PackageName Magpie-Experimental-x64 `
  -Version 0.6.1-hdr-fp16fix `
  -AllowDirtySource
```

这份清单只描述本地恢复位置和来源，不会自动下载专有运行库。大体积 SDK 和运行库可以放在单独的本地依赖盘，源码仓库只保留路径配置、版本记录和构建说明。

## 本次构建结果

本次 `Release x64` 构建使用全部功能开关，生成：

```text
D:\AI\workspace\Magpie-src\bin\x64\Release\Magpie.exe
```

构建版本为 `0.6.1-hdr-fp16fix`。构建完成后，`bin/`、`obj/`、本地 SDK 克隆和中间包仍应按当前测试需要保留；确认安装目录测试通过后再清理。
