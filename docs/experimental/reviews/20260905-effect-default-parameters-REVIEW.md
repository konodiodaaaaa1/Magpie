# 全部效果器默认参数 review

日期：2026-09-05。范围已按用户回复收窄为**各效果器自身默认参数**，不展开每个参数的全部枚举选项和取值范围。默认效果组的组合与覆盖值另见配套文档。

基线：0.6.5 r5 local / fix2 当前工作区，HEAD `84d9f6abb8203a79f9a44d9a66984a293e68856d`（含未提交修改）。本次仅静态阅读和文档生成，未修改、编译、运行或部署程序；未更改用户配置。

## 覆盖情况

以已部署包 effects 和 Effects.vcxproj 的发布清单交叉核对，共 **170 个效果器文件**，对应源码逐文件内容一致。其中 **37 个效果器有可调参数，共 144 项；133 个没有可调参数**。下面全部列出，没有将“无参数”当成遗漏。

源目录还有一个未纳入本次发布的 `DLSS/DLSS_OpticalFlow.hlsl`，因此直接数源目录会得到 171 个。它是未发布旧入口，不混入本版本参数总数。第三方用户自行增加的效果器不属于本次发行包盘点。

## 需要优先处理的两项一致性问题

### P2：运动可视化声明 0.08，原生后端缺省却使用 1.0

触发条件：添加 `Diagnostics\FrameGuidance_Motion`，未显式保存 gain，随后启用。效果描述默认 0.08，原生工厂从参数表缺省取 1.0；EffectItem 转换只复制已保存参数，没有统一补齐默认值。UI 由描述生成，会显示 0.08。因此参数页展示和后端实际初始设置可以不一致，影响诊断图含义和恢复默认行为。

建议后续统一描述、原生解析和设置结构的默认来源。此处是源码路径确认的问题，尚未实机复现；并非修改后端或参数的记录。

依据：[运动可视化声明](<../../../src/Effects/Diagnostics/FrameGuidance_Motion.hlsl#L5>)、[原生工厂](<../../../src/Magpie.Core/NativeEffectBackendFactory.cpp#L57>)、[参数复制](<../../../src/Magpie/ScalingMode.cpp#L6>)、[原生设置](<../../../src/Magpie.Core/FrameGuidanceDiagnostics.h#L12>)。

### P2：Jinc 默认 0.825 不在滑条步进上

`Jinc.sinc` 的 DEFAULT 为 0.825，STEP 为 0.01。普通 HLSL 渲染缺省直接采用 0.825；UI 和运行草稿则通过 NormalizeEffectParameterValue 量化，按当前 float 数值路径推演约为 0.83。恢复默认不能只给现有归一化路径赋 0.825 后就宣称已恢复。

建议保持声明的 0.825，并使合法刻度能表达它；例如后续单独评估将 STEP 调整为 0.005。不要在本次 review 中把默认值改为 0.83。对全部 144 项声明做静态检查，未发现其他有显著偏差的默认值步进冲突；此检查不替代控件运行验收。

依据：[Jinc 默认](<../../../src/Effects/Jinc.hlsl#L27>)、[数值归一化](<../../../src/Magpie.Core/include/EffectParameterValue.h#L23>)、[HLSL 初始参数](<../../../src/Magpie.Core/EffectDrawer.cpp#L520>)。

## 默认值的含义与 review 边界

表格中的“默认值”是当前效果器文件的 DEFAULT；括号只解释默认对应的复选状态或枚举项。反向命名的开关需按标签理解，例如“关闭扫描线”未勾选，表示没有禁用扫描线。中文标签取自当前简体资源，保留参数内部名称用于定位。它不是用户当前配置，不是本次会话启动值，也不保证原生后端实际生效。

对已知原生入口复核了 DLSSNR 的 15 项、DLSS SR 的光流、DLSSFG 的倍率和光流、XeSSFG 的方法／质量／倍率、FrameRate_Filter 的缺省；未发现除上述诊断增益以外的对应默认值不一致。该结论只覆盖公开可调参数的缺省解析，不表示所有 SDK 内部参数、自动降级或质量档都已全面验证。

修改 DEFAULT 会影响未显式保存此参数的既有组；显式保存了旧值的组则通常保持旧值。因而“把默认改得更好”也属于用户可感知行为变更，不能假定只影响首次安装。

## 有参数的效果器：37 个、144 项

### 1. Anime4K / Anime4K_Denoise_Bilateral_Mean

来源：[Anime4K/Anime4K_Denoise_Bilateral_Mean.hlsl](<../../../src/Effects/Anime4K/Anime4K_Denoise_Bilateral_Mean.hlsl#L9>)。

| 参数名称 | 中文标签 | 自身默认值 |
| --- | --- | --- |
| `intensitySigma` | 强度 | 0.1 |

Review：默认强度属于该滤镜的起始设置，不是关闭效果；此处没有画质对比依据要求调整。

### 2. Anime4K / Anime4K_Denoise_Bilateral_Median

来源：[Anime4K/Anime4K_Denoise_Bilateral_Median.hlsl](<../../../src/Effects/Anime4K/Anime4K_Denoise_Bilateral_Median.hlsl#L8>)。

| 参数名称 | 中文标签 | 自身默认值 |
| --- | --- | --- |
| `intensitySigma` | 强度 | 0.1 |

Review：默认强度属于该滤镜的起始设置，不是关闭效果；此处没有画质对比依据要求调整。

### 3. Anime4K / Anime4K_Denoise_Bilateral_Mode

来源：[Anime4K/Anime4K_Denoise_Bilateral_Mode.hlsl](<../../../src/Effects/Anime4K/Anime4K_Denoise_Bilateral_Mode.hlsl#L9>)。

| 参数名称 | 中文标签 | 自身默认值 |
| --- | --- | --- |
| `intensitySigma` | 强度 | 0.1 |

Review：默认强度属于该滤镜的起始设置，不是关闭效果；此处没有画质对比依据要求调整。

### 4. Anime4K / Anime4K_Thin_HQ

来源：[Anime4K/Anime4K_Thin_HQ.hlsl](<../../../src/Effects/Anime4K/Anime4K_Thin_HQ.hlsl#L8>)。

| 参数名称 | 中文标签 | 自身默认值 |
| --- | --- | --- |
| `strength` | 强度 | 0.6 |
| `iterations` | 迭代次数 | 1 |

Review：默认强度属于该滤镜的起始设置，不是关闭效果；此处没有画质对比依据要求调整。

### 5. Bicubic

来源：[Bicubic.hlsl](<../../../src/Effects/Bicubic.hlsl#L9>)。

| 参数名称 | 中文标签 | 自身默认值 |
| --- | --- | --- |
| `paramB` | B 参数 | 0.33 |
| `paramC` | C 参数 | 0.33 |

Review：B=0.33、C=0.33 是自身默认。渲染器自动追加的 Bicubic 使用 B=0、C=0.5，属于另一层显式配置。

### 6. CAS / CAS

来源：[CAS/CAS.hlsl](<../../../src/Effects/CAS/CAS.hlsl#L10>)。

| 参数名称 | 中文标签 | 自身默认值 |
| --- | --- | --- |
| `sharpness` | 锐度 | 0.4 |

Review：两个变体默认锐度相同；是否改变尺寸由所选变体及尺寸设置决定。

### 7. CAS / CAS_Scaling

来源：[CAS/CAS_Scaling.hlsl](<../../../src/Effects/CAS/CAS_Scaling.hlsl#L9>)。

| 参数名称 | 中文标签 | 自身默认值 |
| --- | --- | --- |
| `sharpness` | 锐度 | 0.4 |

Review：两个变体默认锐度相同；是否改变尺寸由所选变体及尺寸设置决定。

### 8. CRT / CRT_Easymode

来源：[CRT/CRT_Easymode.hlsl](<../../../src/Effects/CRT/CRT_Easymode.hlsl#L37>)。

| 参数名称 | 中文标签 | 自身默认值 |
| --- | --- | --- |
| `sharpnessH` | 水平锐度 | 0.5 |
| `sharpnessV` | 垂直锐度 | 1 |
| `maskStrength` | 遮罩强度 | 0.3 |
| `maskDotWidth` | 遮罩点宽度 | 1 |
| `maskDotHeight` | 遮罩点高度 | 1 |
| `maskStagger` | 遮罩错位 | 0 |
| `maskSize` | 遮罩尺寸 | 1 |
| `scanlineStrength` | 扫描线强度 | 1 |
| `scanlineBeamWidthMin` | 扫描线束最小宽度 | 1.5 |
| `scanlineBeamWidthMax` | 扫描线束最大宽度 | 1.5 |
| `scanlineBrightMin` | 扫描线最低亮度 | 0.35 |
| `scanlineBrightMax` | 扫描线最高亮度 | 0.65 |
| `scanlineCutoff` | 扫描线截止阈值 | 400 |
| `gammaInput` | 输入伽马 | 2 |
| `gammaOutput` | 输出伽马 | 1.8 |
| `brightBoost` | 亮度增强 | 1.2 |
| `dilation` | 膨胀量 | 1（已勾选） |

Review：这些是 CRT 风格起始值，并非中性透传。恢复默认会恢复该风格本身，不代表去掉扫描线、曲率或色彩处理。

### 9. CRT / CRT_Geom

来源：[CRT/CRT_Geom.hlsl](<../../../src/Effects/CRT/CRT_Geom.hlsl#L30>)。

| 参数名称 | 中文标签 | 自身默认值 |
| --- | --- | --- |
| `CRTGamma` | 目标伽马 | 2.4 |
| `monitorGamma` | 显示器伽马 | 2.2 |
| `distance` | 观察距离 | 1.5 |
| `curvature` | 屏幕曲率 | 1（已勾选） |
| `radius` | 曲率半径 | 2 |
| `cornerSize` | 圆角大小 | 0.03 |
| `cornerSmooth` | 圆角平滑度 | 1000 |
| `xTilt` | 水平倾斜 | 0 |
| `yTilt` | 垂直倾斜 | 0 |
| `overScanX` | 水平过扫描 | 100 |
| `overScanY` | 垂直过扫描 | 100 |
| `dotMask` | 点状遮罩 | 0.3 |
| `sharper` | 锐度 | 1 |
| `scanlineWeight` | 扫描线权重 | 0.3 |
| `lum` | 明亮度增强 | 0 |

Review：这些是 CRT 风格起始值，并非中性透传。恢复默认会恢复该风格本身，不代表去掉扫描线、曲率或色彩处理。

### 10. CRT / CRT_Hyllian

来源：[CRT/CRT_Hyllian.hlsl](<../../../src/Effects/CRT/CRT_Hyllian.hlsl#L34>)。

| 参数名称 | 中文标签 | 自身默认值 |
| --- | --- | --- |
| `phosphor` | 荧光粉效果 | 1（已勾选） |
| `vScanlines` | 垂直扫描线 | 0（未勾选） |
| `inputGamma` | 输入伽马 | 2.5 |
| `outputGamma` | 输出伽马 | 2.2 |
| `sharpness` | 锐度 | 1 |
| `colorBoost` | 色彩增强 | 1.5 |
| `redBoost` | 红色增强 | 1 |
| `greenBoost` | 绿色增强 | 1 |
| `blueBoost` | 蓝色增强 | 1 |
| `scanlinesStrength` | 扫描线强度 | 0.5 |
| `beamMinWidth` | 最小束宽 | 0.86 |
| `beamMaxWidth` | 最大束宽 | 1 |
| `crtAntiRinging` | 抗振铃 | 0.8 |

Review：这些是 CRT 风格起始值，并非中性透传。恢复默认会恢复该风格本身，不代表去掉扫描线、曲率或色彩处理。

### 11. CRT / CRT_Lottes

来源：[CRT/CRT_Lottes.hlsl](<../../../src/Effects/CRT/CRT_Lottes.hlsl#L22>)。

| 参数名称 | 中文标签 | 自身默认值 |
| --- | --- | --- |
| `hardScan` | 扫描线硬度 | -8 |
| `hardPix` | 像素硬度 | -3 |
| `warpX` | 水平画面弯曲 | 0.031 |
| `warpY` | 垂直画面弯曲 | 0.041 |
| `maskDark` | 遮罩暗部 | 0.5 |
| `maskLight` | 遮罩亮部 | 1.5 |
| `shadowMask` | 荫罩样式 | 3 |
| `brightBoost` | 亮度增强 | 1 |
| `hardBloomPix` | 水平辉光柔化 | -1.5 |
| `hardBloomScan` | 垂直辉光柔化 | -2 |
| `bloomAmount` | 辉光强度 | 0.15 |
| `shape` | 滤波核形状 | 2 |

Review：这些是 CRT 风格起始值，并非中性透传。恢复默认会恢复该风格本身，不代表去掉扫描线、曲率或色彩处理。

### 12. CRT / GTU_v050

来源：[CRT/GTU_v050.hlsl](<../../../src/Effects/CRT/GTU_v050.hlsl#L15>)。

| 参数名称 | 中文标签 | 自身默认值 |
| --- | --- | --- |
| `compositeConnection` | 复合视频连接 | 0（未勾选） |
| `noScanlines` | 关闭扫描线 | 0（未勾选） |
| `signalResolution` | Y 信号分辨率 | 256 |
| `signalResolutionI` | I 信号分辨率 | 83 |
| `signalResolutionQ` | Q 信号分辨率 | 25 |
| `tvVerticalResolution` | 电视垂直分辨率 | 250 |
| `blackLevel` | 黑位 | 0.07 |
| `contrast` | 对比度 | 1 |

Review：这些是 CRT 风格起始值，并非中性透传。恢复默认会恢复该风格本身，不代表去掉扫描线、曲率或色彩处理。

### 13. DLSS / DLSS_SR

来源：[DLSS/DLSS_SR.hlsl](<../../../src/Effects/DLSS/DLSS_SR.hlsl#L9>)。

| 参数名称 | 中文标签 | 自身默认值 |
| --- | --- | --- |
| `motionVectorQuality` | 光流质量 | 2（平衡） |

Review：默认启用 NVIDIA 平衡档光流；不能将默认值解释为自动选择任意厂商后端。

### 14. DLSSFG / DLSS_FrameGeneration

来源：[DLSSFG/DLSS_FrameGeneration.hlsl](<../../../src/Effects/DLSSFG/DLSS_FrameGeneration.hlsl#L8>)。

| 参数名称 | 中文标签 | 自身默认值 |
| --- | --- | --- |
| `multiplier` | 帧数倍率 | 2 |
| `motionVectorQuality` | 光流质量 | 2（平衡） |

Review：倍率 2 与平衡光流默认相匹配；最终显示帧率取决于输入及后端，不能由倍率数值保证。

### 15. DLSSNR / DLSSNR_AI_Filter

来源：[DLSSNR/DLSSNR_AI_Filter.hlsl](<../../../src/Effects/DLSSNR/DLSSNR_AI_Filter.hlsl#L8>)。

| 参数名称 | 中文标签 | 自身默认值 |
| --- | --- | --- |
| `enableInputResolutionScaling` | 调整输入分辨率<br>（会降低 DLSSNR 质量） | 0（未勾选） |
| `inputResolutionPercent` | 输入分辨率（%） | 100% |
| `residualMultiplier` | 残差倍率 | 1 |
| `residualSaturation` | 残差饱和度倍率 | 1 |
| `residualLightness` | 残差明度倍率 | 1 |
| `shadowStructureMultiplier` | 阴影／结构控制 | 1 |
| `reflectionGlowMultiplier` | 反射／辉光控制 | 1 |
| `motionVectorQuality` | 光流质量 | 2（平衡） |
| `style` | NR 风格<br>（0 默认，1 自然，2 电影） | 0（默认风格） |
| `intensity` | NR 强度 | 1 |
| `localToneStrength` | 局部色调强度 | 1 |
| `localStructureStrength` | 局部结构强度 | 1 |
| `skinStructureStrength` | 皮肤结构强度 | -1 |
| `useAutoMask` | 自动遮罩 | 0（未勾选） |
| `uiCorrection` | NR 界面修正 | 0（未勾选） |

Review：默认关闭输入分辨率调整，因此残差相关数值虽然存在，默认未启用对应路径。-1 的皮肤结构值按当前接口原样记录，不擅自解释为自动。

### 16. Deband

来源：[Deband.hlsl](<../../../src/Effects/Deband.hlsl#L7>)。

| 参数名称 | 中文标签 | 自身默认值 |
| --- | --- | --- |
| `threshold` | 阈值 | 64 |
| `range` | 范围 | 8 |
| `iterations` | 迭代次数 | 4 |
| `grain` | 颗粒强度 | 48 |

Review：默认会进行去色带处理并请求颗粒强度 48；不是无颗粒的中性设置。是否去掉默认颗粒需用户画质确认。

### 17. Diagnostics / FrameGuidance_Motion

来源：[Diagnostics/FrameGuidance_Motion.hlsl](<../../../src/Effects/Diagnostics/FrameGuidance_Motion.hlsl#L5>)。

| 参数名称 | 中文标签 | 自身默认值 |
| --- | --- | --- |
| `gain` | 显示增益 | 0.08 |

Review：发现原生后端缺省 gain=1.0 与声明 0.08 不一致，详见前面的 P2 项。

### 18. FSR / FSR_RCAS

来源：[FSR/FSR_RCAS.hlsl](<../../../src/Effects/FSR/FSR_RCAS.hlsl#L10>)。

| 参数名称 | 中文标签 | 自身默认值 |
| --- | --- | --- |
| `sharpness` | 锐度 | 0.87 |

Review：FSR_RCAS 自身默认与内置 FSR 组显式值都是 0.87；不是 0.5。

### 19. FrameRate_Filter

来源：[FrameRate_Filter.hlsl](<../../../src/Effects/FrameRate_Filter.hlsl#L8>)。

| 参数名称 | 中文标签 | 自身默认值 |
| --- | --- | --- |
| `targetFrameRate` | 目标帧率 | 60 FPS |

Review：60 表示捕获处理帧率上限候选值；还会与其他上限取较小值，不是最终补帧显示帧率。

### 20. ImageAdjustment

来源：[ImageAdjustment.hlsl](<../../../src/Effects/ImageAdjustment.hlsl#L7>)。

| 参数名称 | 中文标签 | 自身默认值 |
| --- | --- | --- |
| `targetGamma` | 目标伽马 | 2.2 |
| `monitorGamma` | 显示器伽马 | 2.2 |
| `saturation` | 饱和度 | 1 |
| `luminance` | 明亮度 | 1 |
| `contrast` | 对比度 | 1 |
| `brightBoost` | 亮度增强 | 0 |
| `blackLevel` | 黑位 | 0 |
| `r` | 红色通道 | 1 |
| `g` | 绿色通道 | 1 |
| `b` | 蓝色通道 | 1 |

Review：默认伽马比值为 1、通道及对比度等倍率为 1、偏移为 0；公式意图接近保持原色。仍有颜色空间往返及截断，不能保证逐位透传。

### 21. Jinc

来源：[Jinc.hlsl](<../../../src/Effects/Jinc.hlsl#L19>)。

| 参数名称 | 中文标签 | 自身默认值 |
| --- | --- | --- |
| `windowSinc` | 窗函数 Sinc 参数 | 0.5 |
| `sinc` | Sinc 参数 | 0.825 |
| `ARStrength` | 抗振铃强度 | 0.5 |

Review：sinc=0.825 不对齐当前步进 0.01，UI 归一化可能改变为约 0.83；应先解决再实现精确重置。

### 22. Lanczos

来源：[Lanczos.hlsl](<../../../src/Effects/Lanczos.hlsl#L8>)。

| 参数名称 | 中文标签 | 自身默认值 |
| --- | --- | --- |
| `ARStrength` | 抗振铃强度 | 0.5 |

Review：抗振铃为 0.5；内置 Lanczos 组继承该值。

### 23. MLAA / MLAA

来源：[MLAA/MLAA.hlsl](<../../../src/Effects/MLAA/MLAA.hlsl#L29>)。

| 参数名称 | 中文标签 | 自身默认值 |
| --- | --- | --- |
| `threshold` | 边缘阈值 | 0.08 |
| `strength` | 强度 | 1.0 |

Review：强度 1 表示启用预设处理强度，不应将恢复默认理解为关闭抗锯齿。

### 24. NIS / NIS

来源：[NIS/NIS.hlsl](<../../../src/Effects/NIS/NIS.hlsl#L7>)。

| 参数名称 | 中文标签 | 自身默认值 |
| --- | --- | --- |
| `sharpness` | 锐度 | 0.5 |

Review：NIS 和 NVSharpen 的锐度同为 0.5；名字相近不意味着同一个效果器或同一种尺寸行为。

### 25. NIS / NVSharpen

来源：[NIS/NVSharpen.hlsl](<../../../src/Effects/NIS/NVSharpen.hlsl#L9>)。

| 参数名称 | 中文标签 | 自身默认值 |
| --- | --- | --- |
| `sharpness` | 锐度 | 0.5 |

Review：NIS 和 NVSharpen 的锐度同为 0.5；名字相近不意味着同一个效果器或同一种尺寸行为。

### 26. SGSR

来源：[SGSR.hlsl](<../../../src/Effects/SGSR.hlsl#L7>)。

| 参数名称 | 中文标签 | 自身默认值 |
| --- | --- | --- |
| `EdgeSharpness` | 边缘锐度 | 2.0 |
| `EdgeThreshold` | 边缘阈值 | 8.0 |

Review：边缘参数是算法内部量纲；不要按名称将 2 或 8 当成界面百分比。

### 27. SMAA / SMAA_4x_Experimental

来源：[SMAA/SMAA_4x_Experimental.hlsl](<../../../src/Effects/SMAA/SMAA_4x_Experimental.hlsl#L11>)。

| 参数名称 | 中文标签 | 自身默认值 |
| --- | --- | --- |
| `historyWeight` | 历史帧权重 | 0.75 |
| `historyRejection` | 历史帧拒绝阈值 | 8.0 |

Review：4x 与 T2x 的历史权重不同，分别为 0.75 与 0.5；Jitter/NoJitter 同组默认参数相同，算法路径仍不同。时间稳定性留待实际素材检查。

### 28. SMAA / SMAA_4x_NoJitter_Experimental

来源：[SMAA/SMAA_4x_NoJitter_Experimental.hlsl](<../../../src/Effects/SMAA/SMAA_4x_NoJitter_Experimental.hlsl#L10>)。

| 参数名称 | 中文标签 | 自身默认值 |
| --- | --- | --- |
| `historyWeight` | 历史帧权重 | 0.75 |
| `historyRejection` | 历史帧拒绝阈值 | 8.0 |

Review：4x 与 T2x 的历史权重不同，分别为 0.75 与 0.5；Jitter/NoJitter 同组默认参数相同，算法路径仍不同。时间稳定性留待实际素材检查。

### 29. SMAA / SMAA_T2x_Experimental

来源：[SMAA/SMAA_T2x_Experimental.hlsl](<../../../src/Effects/SMAA/SMAA_T2x_Experimental.hlsl#L10>)。

| 参数名称 | 中文标签 | 自身默认值 |
| --- | --- | --- |
| `historyWeight` | 历史帧权重 | 0.5 |
| `historyRejection` | 历史帧拒绝阈值 | 8.0 |

Review：4x 与 T2x 的历史权重不同，分别为 0.75 与 0.5；Jitter/NoJitter 同组默认参数相同，算法路径仍不同。时间稳定性留待实际素材检查。

### 30. SMAA / SMAA_T2x_NoJitter_Experimental

来源：[SMAA/SMAA_T2x_NoJitter_Experimental.hlsl](<../../../src/Effects/SMAA/SMAA_T2x_NoJitter_Experimental.hlsl#L10>)。

| 参数名称 | 中文标签 | 自身默认值 |
| --- | --- | --- |
| `historyWeight` | 历史帧权重 | 0.5 |
| `historyRejection` | 历史帧拒绝阈值 | 8.0 |

Review：4x 与 T2x 的历史权重不同，分别为 0.75 与 0.5；Jitter/NoJitter 同组默认参数相同，算法路径仍不同。时间稳定性留待实际素材检查。

### 31. SSimDownscaler

来源：[SSimDownscaler.hlsl](<../../../src/Effects/SSimDownscaler.hlsl#L9>)。

| 参数名称 | 中文标签 | 自身默认值 |
| --- | --- | --- |
| `oversharp` | 额外锐化 | 1 |

Review：额外锐化默认 1；数值语义由效果公式决定，不将它当作 100% 的统一锐度量表。

### 32. Sharpen / AdaptiveSharpen

来源：[Sharpen/AdaptiveSharpen.hlsl](<../../../src/Effects/Sharpen/AdaptiveSharpen.hlsl#L12>)。

| 参数名称 | 中文标签 | 自身默认值 |
| --- | --- | --- |
| `curveHeight` | 锐度 | 0.8 |

Review：各算法锐化强度的量纲不同，不能把 0.5、0.65、0.8、2.0 排成跨效果器强弱排名。

### 33. Sharpen / FineSharp

来源：[Sharpen/FineSharp.hlsl](<../../../src/Effects/Sharpen/FineSharp.hlsl#L12>)。

| 参数名称 | 中文标签 | 自身默认值 |
| --- | --- | --- |
| `sstr` | 锐化强度（sstr） | 2.0 |
| `cstr` | 均衡强度（cstr） | 0.9 |
| `xstr` | 最终锐化强度（xstr） | 0.19 |
| `xrep` | 锐化伪影修复（xrep） | 0.25 |

Review：各算法锐化强度的量纲不同，不能把 0.5、0.65、0.8、2.0 排成跨效果器强弱排名。

### 34. Sharpen / LCAS

来源：[Sharpen/LCAS.hlsl](<../../../src/Effects/Sharpen/LCAS.hlsl#L6>)。

| 参数名称 | 中文标签 | 自身默认值 |
| --- | --- | --- |
| `sharpness` | 锐度 | 0.5 |

Review：各算法锐化强度的量纲不同，不能把 0.5、0.65、0.8、2.0 排成跨效果器强弱排名。

### 35. Sharpen / LumaSharpen

来源：[Sharpen/LumaSharpen.hlsl](<../../../src/Effects/Sharpen/LumaSharpen.hlsl#L19>)。

| 参数名称 | 中文标签 | 自身默认值 |
| --- | --- | --- |
| `sharpStrength` | 锐化强度 | 0.65 |
| `sharpClamp` | 锐化限制 | 0.035 |
| `pattern` | 采样模式 | 1 |
| `offsetBias` | 采样偏移 | 1 |

Review：各算法锐化强度的量纲不同，不能把 0.5、0.65、0.8、2.0 排成跨效果器强弱排名。

### 36. XeSSFG / XeSS_FrameGeneration_x2_ZeroMV

来源：[XeSSFG/XeSS_FrameGeneration_x2_ZeroMV.hlsl](<../../../src/Effects/XeSSFG/XeSS_FrameGeneration_x2_ZeroMV.hlsl#L9>)。

| 参数名称 | 中文标签 | 自身默认值 |
| --- | --- | --- |
| `opticalFlowMethod` | 光流方法 | 0（无） |
| `amdOpticalFlowMode` | 光流质量 | 1（质量） |
| `nvidiaOpticalFlowQuality` | 光流质量 | 2（平衡） |

Review：默认光流方法为 0（无外部光流）；AMD/NVIDIA 的质量值是选择相应方法后才使用的预置值。x2 版本倍率固定；多帧版本自身默认倍率为 3。

### 37. XeSSFG / XeSS_MultiFrameGeneration_ZeroMV

来源：[XeSSFG/XeSS_MultiFrameGeneration_ZeroMV.hlsl](<../../../src/Effects/XeSSFG/XeSS_MultiFrameGeneration_ZeroMV.hlsl#L8>)。

| 参数名称 | 中文标签 | 自身默认值 |
| --- | --- | --- |
| `multiplier` | 帧数倍率 | 3 |
| `opticalFlowMethod` | 光流方法 | 0（无） |
| `amdOpticalFlowMode` | 光流质量 | 1（质量） |

Review：默认光流方法为 0（无外部光流）；AMD/NVIDIA 的质量值是选择相应方法后才使用的预置值。x2 版本倍率固定；多帧版本自身默认倍率为 3。

## 无可调参数的效果器：133 个

“无可调参数”指没有 `//!PARAMETER` 声明，因此没有单个参数的默认值或双击重置目标。并不表示没有算法常量、没有质量档、没有运算，也不表示无需硬件支持。按用户确认的范围，本节不展开权重、所有着色器常量或 SDK 内部设置。

几个重要区别：

- RTX Video 的 Low／Medium／High／Ultra 由文件选择固定原生质量档：VSR 对应 1／2／3／4，Denoise 对应 8／9／10／11；不是参数页枚举。
- FSR2／FSR3／FSR4、XeSS 的 ZeroMV／Jitter／OpticalFlow 是不同入口，文件名携带固定路径选择；无滑条不代表行为相同。
- Anime4K、CuNNy、RAVU、NNEDI3、xBRZ 等变体包含固定网络或尺寸／算法选择；不可将它们归为同一个“空预设”。
- Diagnostics/FrameGuidance_Confidence 是诊断效果，输出不是常规增强画面。
- 源文件中的 SORT_NAME 可能用于排序，不应将它不加核对地当作独立参数或默认档位。

固定后端档位的依据：[NativeEffectBackendFactory.cpp](<../../../src/Magpie.Core/NativeEffectBackendFactory.cpp#L129>)。

### Anime4K（23 个）

| 效果器文件 | 参数默认值 |
| --- | --- |
| [Anime4K/Anime4K_3D_AA_Upscale_US.hlsl](<../../../src/Effects/Anime4K/Anime4K_3D_AA_Upscale_US.hlsl>) | 无可调参数 |
| [Anime4K/Anime4K_3D_Upscale_US.hlsl](<../../../src/Effects/Anime4K/Anime4K_3D_Upscale_US.hlsl>) | 无可调参数 |
| [Anime4K/Anime4K_Restore_L.hlsl](<../../../src/Effects/Anime4K/Anime4K_Restore_L.hlsl>) | 无可调参数 |
| [Anime4K/Anime4K_Restore_M.hlsl](<../../../src/Effects/Anime4K/Anime4K_Restore_M.hlsl>) | 无可调参数 |
| [Anime4K/Anime4K_Restore_S.hlsl](<../../../src/Effects/Anime4K/Anime4K_Restore_S.hlsl>) | 无可调参数 |
| [Anime4K/Anime4K_Restore_Soft_L.hlsl](<../../../src/Effects/Anime4K/Anime4K_Restore_Soft_L.hlsl>) | 无可调参数 |
| [Anime4K/Anime4K_Restore_Soft_M.hlsl](<../../../src/Effects/Anime4K/Anime4K_Restore_Soft_M.hlsl>) | 无可调参数 |
| [Anime4K/Anime4K_Restore_Soft_S.hlsl](<../../../src/Effects/Anime4K/Anime4K_Restore_Soft_S.hlsl>) | 无可调参数 |
| [Anime4K/Anime4K_Restore_Soft_UL.hlsl](<../../../src/Effects/Anime4K/Anime4K_Restore_Soft_UL.hlsl>) | 无可调参数 |
| [Anime4K/Anime4K_Restore_Soft_VL.hlsl](<../../../src/Effects/Anime4K/Anime4K_Restore_Soft_VL.hlsl>) | 无可调参数 |
| [Anime4K/Anime4K_Restore_UL.hlsl](<../../../src/Effects/Anime4K/Anime4K_Restore_UL.hlsl>) | 无可调参数 |
| [Anime4K/Anime4K_Restore_VL.hlsl](<../../../src/Effects/Anime4K/Anime4K_Restore_VL.hlsl>) | 无可调参数 |
| [Anime4K/Anime4K_Upscale_Denoise_L.hlsl](<../../../src/Effects/Anime4K/Anime4K_Upscale_Denoise_L.hlsl>) | 无可调参数 |
| [Anime4K/Anime4K_Upscale_Denoise_S.hlsl](<../../../src/Effects/Anime4K/Anime4K_Upscale_Denoise_S.hlsl>) | 无可调参数 |
| [Anime4K/Anime4K_Upscale_Denoise_UL.hlsl](<../../../src/Effects/Anime4K/Anime4K_Upscale_Denoise_UL.hlsl>) | 无可调参数 |
| [Anime4K/Anime4K_Upscale_Denoise_VL.hlsl](<../../../src/Effects/Anime4K/Anime4K_Upscale_Denoise_VL.hlsl>) | 无可调参数 |
| [Anime4K/Anime4K_Upscale_GAN_x2_M.hlsl](<../../../src/Effects/Anime4K/Anime4K_Upscale_GAN_x2_M.hlsl>) | 无可调参数 |
| [Anime4K/Anime4K_Upscale_GAN_x2_S.hlsl](<../../../src/Effects/Anime4K/Anime4K_Upscale_GAN_x2_S.hlsl>) | 无可调参数 |
| [Anime4K/Anime4K_Upscale_GAN_x3_L.hlsl](<../../../src/Effects/Anime4K/Anime4K_Upscale_GAN_x3_L.hlsl>) | 无可调参数 |
| [Anime4K/Anime4K_Upscale_L.hlsl](<../../../src/Effects/Anime4K/Anime4K_Upscale_L.hlsl>) | 无可调参数 |
| [Anime4K/Anime4K_Upscale_S.hlsl](<../../../src/Effects/Anime4K/Anime4K_Upscale_S.hlsl>) | 无可调参数 |
| [Anime4K/Anime4K_Upscale_UL.hlsl](<../../../src/Effects/Anime4K/Anime4K_Upscale_UL.hlsl>) | 无可调参数 |
| [Anime4K/Anime4K_Upscale_VL.hlsl](<../../../src/Effects/Anime4K/Anime4K_Upscale_VL.hlsl>) | 无可调参数 |

### CuNNy（20 个）

| 效果器文件 | 参数默认值 |
| --- | --- |
| [CuNNy/CuNNy-16x16C-NVL-DN.hlsl](<../../../src/Effects/CuNNy/CuNNy-16x16C-NVL-DN.hlsl>) | 无可调参数 |
| [CuNNy/CuNNy-16x16C-NVL.hlsl](<../../../src/Effects/CuNNy/CuNNy-16x16C-NVL.hlsl>) | 无可调参数 |
| [CuNNy/CuNNy-2x4C-NVL-DN.hlsl](<../../../src/Effects/CuNNy/CuNNy-2x4C-NVL-DN.hlsl>) | 无可调参数 |
| [CuNNy/CuNNy-2x4C-NVL.hlsl](<../../../src/Effects/CuNNy/CuNNy-2x4C-NVL.hlsl>) | 无可调参数 |
| [CuNNy/CuNNy-3x4C-NVL-DN.hlsl](<../../../src/Effects/CuNNy/CuNNy-3x4C-NVL-DN.hlsl>) | 无可调参数 |
| [CuNNy/CuNNy-3x4C-NVL.hlsl](<../../../src/Effects/CuNNy/CuNNy-3x4C-NVL.hlsl>) | 无可调参数 |
| [CuNNy/CuNNy-4x16C-NVL-DN.hlsl](<../../../src/Effects/CuNNy/CuNNy-4x16C-NVL-DN.hlsl>) | 无可调参数 |
| [CuNNy/CuNNy-4x16C-NVL.hlsl](<../../../src/Effects/CuNNy/CuNNy-4x16C-NVL.hlsl>) | 无可调参数 |
| [CuNNy/CuNNy-4x4C-NVL-DN.hlsl](<../../../src/Effects/CuNNy/CuNNy-4x4C-NVL-DN.hlsl>) | 无可调参数 |
| [CuNNy/CuNNy-4x4C-NVL.hlsl](<../../../src/Effects/CuNNy/CuNNy-4x4C-NVL.hlsl>) | 无可调参数 |
| [CuNNy/CuNNy-4x8C-NVL-DN.hlsl](<../../../src/Effects/CuNNy/CuNNy-4x8C-NVL-DN.hlsl>) | 无可调参数 |
| [CuNNy/CuNNy-4x8C-NVL.hlsl](<../../../src/Effects/CuNNy/CuNNy-4x8C-NVL.hlsl>) | 无可调参数 |
| [CuNNy/CuNNy-6x8C-NVL-DN.hlsl](<../../../src/Effects/CuNNy/CuNNy-6x8C-NVL-DN.hlsl>) | 无可调参数 |
| [CuNNy/CuNNy-6x8C-NVL.hlsl](<../../../src/Effects/CuNNy/CuNNy-6x8C-NVL.hlsl>) | 无可调参数 |
| [CuNNy/CuNNy-8x16C-NVL-DN.hlsl](<../../../src/Effects/CuNNy/CuNNy-8x16C-NVL-DN.hlsl>) | 无可调参数 |
| [CuNNy/CuNNy-8x16C-NVL.hlsl](<../../../src/Effects/CuNNy/CuNNy-8x16C-NVL.hlsl>) | 无可调参数 |
| [CuNNy/CuNNy-8x4C-NVL-DN.hlsl](<../../../src/Effects/CuNNy/CuNNy-8x4C-NVL-DN.hlsl>) | 无可调参数 |
| [CuNNy/CuNNy-8x4C-NVL.hlsl](<../../../src/Effects/CuNNy/CuNNy-8x4C-NVL.hlsl>) | 无可调参数 |
| [CuNNy/CuNNy-8x8C-NVL-DN.hlsl](<../../../src/Effects/CuNNy/CuNNy-8x8C-NVL-DN.hlsl>) | 无可调参数 |
| [CuNNy/CuNNy-8x8C-NVL.hlsl](<../../../src/Effects/CuNNy/CuNNy-8x8C-NVL.hlsl>) | 无可调参数 |

### CuNNy2（9 个）

| 效果器文件 | 参数默认值 |
| --- | --- |
| [CuNNy2/CuNNy-3x12-NVL.hlsl](<../../../src/Effects/CuNNy2/CuNNy-3x12-NVL.hlsl>) | 无可调参数 |
| [CuNNy2/CuNNy-4x12-NVL.hlsl](<../../../src/Effects/CuNNy2/CuNNy-4x12-NVL.hlsl>) | 无可调参数 |
| [CuNNy2/CuNNy-4x16-NVL.hlsl](<../../../src/Effects/CuNNy2/CuNNy-4x16-NVL.hlsl>) | 无可调参数 |
| [CuNNy2/CuNNy-4x24-NVL.hlsl](<../../../src/Effects/CuNNy2/CuNNy-4x24-NVL.hlsl>) | 无可调参数 |
| [CuNNy2/CuNNy-4x32-NVL.hlsl](<../../../src/Effects/CuNNy2/CuNNy-4x32-NVL.hlsl>) | 无可调参数 |
| [CuNNy2/CuNNy-8x32-NVL.hlsl](<../../../src/Effects/CuNNy2/CuNNy-8x32-NVL.hlsl>) | 无可调参数 |
| [CuNNy2/CuNNy-fast-NVL.hlsl](<../../../src/Effects/CuNNy2/CuNNy-fast-NVL.hlsl>) | 无可调参数 |
| [CuNNy2/CuNNy-faster-NVL.hlsl](<../../../src/Effects/CuNNy2/CuNNy-faster-NVL.hlsl>) | 无可调参数 |
| [CuNNy2/CuNNy-veryfast-NVL.hlsl](<../../../src/Effects/CuNNy2/CuNNy-veryfast-NVL.hlsl>) | 无可调参数 |

### DLSS（1 个）

| 效果器文件 | 参数默认值 |
| --- | --- |
| [DLSS/DLSS_ZeroMV_Jitter.hlsl](<../../../src/Effects/DLSS/DLSS_ZeroMV_Jitter.hlsl>) | 无可调参数 |

### Diagnostics（1 个）

| 效果器文件 | 参数默认值 |
| --- | --- |
| [Diagnostics/FrameGuidance_Confidence.hlsl](<../../../src/Effects/Diagnostics/FrameGuidance_Confidence.hlsl>) | 无可调参数 |

### FSR（1 个）

| 效果器文件 | 参数默认值 |
| --- | --- |
| [FSR/FSR_EASU.hlsl](<../../../src/Effects/FSR/FSR_EASU.hlsl>) | 无可调参数 |

### FSR2（3 个）

| 效果器文件 | 参数默认值 |
| --- | --- |
| [FSR2/FSR2_OpticalFlow.hlsl](<../../../src/Effects/FSR2/FSR2_OpticalFlow.hlsl>) | 无可调参数 |
| [FSR2/FSR2_ZeroMV.hlsl](<../../../src/Effects/FSR2/FSR2_ZeroMV.hlsl>) | 无可调参数 |
| [FSR2/FSR2_ZeroMV_Jitter.hlsl](<../../../src/Effects/FSR2/FSR2_ZeroMV_Jitter.hlsl>) | 无可调参数 |

### FSR3（3 个）

| 效果器文件 | 参数默认值 |
| --- | --- |
| [FSR3/FSR3_OpticalFlow.hlsl](<../../../src/Effects/FSR3/FSR3_OpticalFlow.hlsl>) | 无可调参数 |
| [FSR3/FSR3_ZeroMV.hlsl](<../../../src/Effects/FSR3/FSR3_ZeroMV.hlsl>) | 无可调参数 |
| [FSR3/FSR3_ZeroMV_Jitter.hlsl](<../../../src/Effects/FSR3/FSR3_ZeroMV_Jitter.hlsl>) | 无可调参数 |

### FSR4（3 个）

| 效果器文件 | 参数默认值 |
| --- | --- |
| [FSR4/FSR4_OpticalFlow.hlsl](<../../../src/Effects/FSR4/FSR4_OpticalFlow.hlsl>) | 无可调参数 |
| [FSR4/FSR4_ZeroMV.hlsl](<../../../src/Effects/FSR4/FSR4_ZeroMV.hlsl>) | 无可调参数 |
| [FSR4/FSR4_ZeroMV_Jitter.hlsl](<../../../src/Effects/FSR4/FSR4_ZeroMV_Jitter.hlsl>) | 无可调参数 |

### FSRCNNX（2 个）

| 效果器文件 | 参数默认值 |
| --- | --- |
| [FSRCNNX/FSRCNNX.hlsl](<../../../src/Effects/FSRCNNX/FSRCNNX.hlsl>) | 无可调参数 |
| [FSRCNNX/FSRCNNX_LineArt.hlsl](<../../../src/Effects/FSRCNNX/FSRCNNX_LineArt.hlsl>) | 无可调参数 |

### FXAA（3 个）

| 效果器文件 | 参数默认值 |
| --- | --- |
| [FXAA/FXAA_High.hlsl](<../../../src/Effects/FXAA/FXAA_High.hlsl>) | 无可调参数 |
| [FXAA/FXAA_Medium.hlsl](<../../../src/Effects/FXAA/FXAA_Medium.hlsl>) | 无可调参数 |
| [FXAA/FXAA_Ultra.hlsl](<../../../src/Effects/FXAA/FXAA_Ultra.hlsl>) | 无可调参数 |

### NNEDI3（10 个）

| 效果器文件 | 参数默认值 |
| --- | --- |
| [NNEDI3/NNEDI3_nns128_win8x4.hlsl](<../../../src/Effects/NNEDI3/NNEDI3_nns128_win8x4.hlsl>) | 无可调参数 |
| [NNEDI3/NNEDI3_nns128_win8x6.hlsl](<../../../src/Effects/NNEDI3/NNEDI3_nns128_win8x6.hlsl>) | 无可调参数 |
| [NNEDI3/NNEDI3_nns16_win8x4.hlsl](<../../../src/Effects/NNEDI3/NNEDI3_nns16_win8x4.hlsl>) | 无可调参数 |
| [NNEDI3/NNEDI3_nns16_win8x6.hlsl](<../../../src/Effects/NNEDI3/NNEDI3_nns16_win8x6.hlsl>) | 无可调参数 |
| [NNEDI3/NNEDI3_nns256_win8x4.hlsl](<../../../src/Effects/NNEDI3/NNEDI3_nns256_win8x4.hlsl>) | 无可调参数 |
| [NNEDI3/NNEDI3_nns256_win8x6.hlsl](<../../../src/Effects/NNEDI3/NNEDI3_nns256_win8x6.hlsl>) | 无可调参数 |
| [NNEDI3/NNEDI3_nns32_win8x4.hlsl](<../../../src/Effects/NNEDI3/NNEDI3_nns32_win8x4.hlsl>) | 无可调参数 |
| [NNEDI3/NNEDI3_nns32_win8x6.hlsl](<../../../src/Effects/NNEDI3/NNEDI3_nns32_win8x6.hlsl>) | 无可调参数 |
| [NNEDI3/NNEDI3_nns64_win8x4.hlsl](<../../../src/Effects/NNEDI3/NNEDI3_nns64_win8x4.hlsl>) | 无可调参数 |
| [NNEDI3/NNEDI3_nns64_win8x6.hlsl](<../../../src/Effects/NNEDI3/NNEDI3_nns64_win8x6.hlsl>) | 无可调参数 |

### Pixel Art（3 个）

| 效果器文件 | 参数默认值 |
| --- | --- |
| [Pixel Art/MMPX.hlsl](<../../../src/Effects/Pixel Art/MMPX.hlsl>) | 无可调参数 |
| [Pixel Art/Pixellate.hlsl](<../../../src/Effects/Pixel Art/Pixellate.hlsl>) | 无可调参数 |
| [Pixel Art/SharpBilinear.hlsl](<../../../src/Effects/Pixel Art/SharpBilinear.hlsl>) | 无可调参数 |

### RAVU（26 个）

| 效果器文件 | 参数默认值 |
| --- | --- |
| [RAVU/RAVU_3x_R2.hlsl](<../../../src/Effects/RAVU/RAVU_3x_R2.hlsl>) | 无可调参数 |
| [RAVU/RAVU_3x_R2_RGB.hlsl](<../../../src/Effects/RAVU/RAVU_3x_R2_RGB.hlsl>) | 无可调参数 |
| [RAVU/RAVU_3x_R3.hlsl](<../../../src/Effects/RAVU/RAVU_3x_R3.hlsl>) | 无可调参数 |
| [RAVU/RAVU_3x_R3_RGB.hlsl](<../../../src/Effects/RAVU/RAVU_3x_R3_RGB.hlsl>) | 无可调参数 |
| [RAVU/RAVU_3x_R4.hlsl](<../../../src/Effects/RAVU/RAVU_3x_R4.hlsl>) | 无可调参数 |
| [RAVU/RAVU_3x_R4_RGB.hlsl](<../../../src/Effects/RAVU/RAVU_3x_R4_RGB.hlsl>) | 无可调参数 |
| [RAVU/RAVU_Lite_AR_R2.hlsl](<../../../src/Effects/RAVU/RAVU_Lite_AR_R2.hlsl>) | 无可调参数 |
| [RAVU/RAVU_Lite_AR_R3.hlsl](<../../../src/Effects/RAVU/RAVU_Lite_AR_R3.hlsl>) | 无可调参数 |
| [RAVU/RAVU_Lite_AR_R4.hlsl](<../../../src/Effects/RAVU/RAVU_Lite_AR_R4.hlsl>) | 无可调参数 |
| [RAVU/RAVU_Lite_R2.hlsl](<../../../src/Effects/RAVU/RAVU_Lite_R2.hlsl>) | 无可调参数 |
| [RAVU/RAVU_Lite_R3.hlsl](<../../../src/Effects/RAVU/RAVU_Lite_R3.hlsl>) | 无可调参数 |
| [RAVU/RAVU_Lite_R4.hlsl](<../../../src/Effects/RAVU/RAVU_Lite_R4.hlsl>) | 无可调参数 |
| [RAVU/RAVU_R2.hlsl](<../../../src/Effects/RAVU/RAVU_R2.hlsl>) | 无可调参数 |
| [RAVU/RAVU_R2_RGB.hlsl](<../../../src/Effects/RAVU/RAVU_R2_RGB.hlsl>) | 无可调参数 |
| [RAVU/RAVU_R3.hlsl](<../../../src/Effects/RAVU/RAVU_R3.hlsl>) | 无可调参数 |
| [RAVU/RAVU_R3_RGB.hlsl](<../../../src/Effects/RAVU/RAVU_R3_RGB.hlsl>) | 无可调参数 |
| [RAVU/RAVU_R4.hlsl](<../../../src/Effects/RAVU/RAVU_R4.hlsl>) | 无可调参数 |
| [RAVU/RAVU_R4_RGB.hlsl](<../../../src/Effects/RAVU/RAVU_R4_RGB.hlsl>) | 无可调参数 |
| [RAVU/RAVU_Zoom_AR_R2.hlsl](<../../../src/Effects/RAVU/RAVU_Zoom_AR_R2.hlsl>) | 无可调参数 |
| [RAVU/RAVU_Zoom_AR_R2_RGB.hlsl](<../../../src/Effects/RAVU/RAVU_Zoom_AR_R2_RGB.hlsl>) | 无可调参数 |
| [RAVU/RAVU_Zoom_AR_R3.hlsl](<../../../src/Effects/RAVU/RAVU_Zoom_AR_R3.hlsl>) | 无可调参数 |
| [RAVU/RAVU_Zoom_AR_R3_RGB.hlsl](<../../../src/Effects/RAVU/RAVU_Zoom_AR_R3_RGB.hlsl>) | 无可调参数 |
| [RAVU/RAVU_Zoom_R2.hlsl](<../../../src/Effects/RAVU/RAVU_Zoom_R2.hlsl>) | 无可调参数 |
| [RAVU/RAVU_Zoom_R2_RGB.hlsl](<../../../src/Effects/RAVU/RAVU_Zoom_R2_RGB.hlsl>) | 无可调参数 |
| [RAVU/RAVU_Zoom_R3.hlsl](<../../../src/Effects/RAVU/RAVU_Zoom_R3.hlsl>) | 无可调参数 |
| [RAVU/RAVU_Zoom_R3_RGB.hlsl](<../../../src/Effects/RAVU/RAVU_Zoom_R3_RGB.hlsl>) | 无可调参数 |

### RTXVideo（8 个）

| 效果器文件 | 参数默认值 |
| --- | --- |
| [RTXVideo/RTXVideo_Denoise_High.hlsl](<../../../src/Effects/RTXVideo/RTXVideo_Denoise_High.hlsl>) | 无可调参数 |
| [RTXVideo/RTXVideo_Denoise_Low.hlsl](<../../../src/Effects/RTXVideo/RTXVideo_Denoise_Low.hlsl>) | 无可调参数 |
| [RTXVideo/RTXVideo_Denoise_Medium.hlsl](<../../../src/Effects/RTXVideo/RTXVideo_Denoise_Medium.hlsl>) | 无可调参数 |
| [RTXVideo/RTXVideo_Denoise_Ultra.hlsl](<../../../src/Effects/RTXVideo/RTXVideo_Denoise_Ultra.hlsl>) | 无可调参数 |
| [RTXVideo/RTXVideo_VSR_High.hlsl](<../../../src/Effects/RTXVideo/RTXVideo_VSR_High.hlsl>) | 无可调参数 |
| [RTXVideo/RTXVideo_VSR_Low.hlsl](<../../../src/Effects/RTXVideo/RTXVideo_VSR_Low.hlsl>) | 无可调参数 |
| [RTXVideo/RTXVideo_VSR_Medium.hlsl](<../../../src/Effects/RTXVideo/RTXVideo_VSR_Medium.hlsl>) | 无可调参数 |
| [RTXVideo/RTXVideo_VSR_Ultra.hlsl](<../../../src/Effects/RTXVideo/RTXVideo_VSR_Ultra.hlsl>) | 无可调参数 |

### SMAA（4 个）

| 效果器文件 | 参数默认值 |
| --- | --- |
| [SMAA/SMAA_High.hlsl](<../../../src/Effects/SMAA/SMAA_High.hlsl>) | 无可调参数 |
| [SMAA/SMAA_Low.hlsl](<../../../src/Effects/SMAA/SMAA_Low.hlsl>) | 无可调参数 |
| [SMAA/SMAA_Medium.hlsl](<../../../src/Effects/SMAA/SMAA_Medium.hlsl>) | 无可调参数 |
| [SMAA/SMAA_Ultra.hlsl](<../../../src/Effects/SMAA/SMAA_Ultra.hlsl>) | 无可调参数 |

### XeSS（3 个）

| 效果器文件 | 参数默认值 |
| --- | --- |
| [XeSS/XeSS_OpticalFlow.hlsl](<../../../src/Effects/XeSS/XeSS_OpticalFlow.hlsl>) | 无可调参数 |
| [XeSS/XeSS_ZeroMV.hlsl](<../../../src/Effects/XeSS/XeSS_ZeroMV.hlsl>) | 无可调参数 |
| [XeSS/XeSS_ZeroMV_Jitter.hlsl](<../../../src/Effects/XeSS/XeSS_ZeroMV_Jitter.hlsl>) | 无可调参数 |

### xBRZ（6 个）

| 效果器文件 | 参数默认值 |
| --- | --- |
| [xBRZ/xBRZ_2x.hlsl](<../../../src/Effects/xBRZ/xBRZ_2x.hlsl>) | 无可调参数 |
| [xBRZ/xBRZ_3x.hlsl](<../../../src/Effects/xBRZ/xBRZ_3x.hlsl>) | 无可调参数 |
| [xBRZ/xBRZ_4x.hlsl](<../../../src/Effects/xBRZ/xBRZ_4x.hlsl>) | 无可调参数 |
| [xBRZ/xBRZ_5x.hlsl](<../../../src/Effects/xBRZ/xBRZ_5x.hlsl>) | 无可调参数 |
| [xBRZ/xBRZ_6x.hlsl](<../../../src/Effects/xBRZ/xBRZ_6x.hlsl>) | 无可调参数 |
| [xBRZ/xBRZ_Freescale.hlsl](<../../../src/Effects/xBRZ/xBRZ_Freescale.hlsl>) | 无可调参数 |

### 根目录（4 个）

| 效果器文件 | 参数默认值 |
| --- | --- |
| [ACNet.hlsl](<../../../src/Effects/ACNet.hlsl>) | 无可调参数 |
| [Bilinear.hlsl](<../../../src/Effects/Bilinear.hlsl>) | 无可调参数 |
| [Nearest.hlsl](<../../../src/Effects/Nearest.hlsl>) | 无可调参数 |
| [k7_modernAnime_FHD_x2.hlsl](<../../../src/Effects/k7_modernAnime_FHD_x2.hlsl>) | 无可调参数 |

## 后续建议

优先统一运动可视化默认值，并解决 Jinc 的默认值与刻度矛盾；这两项直接关系到“双击恢复效果器默认值”的可靠性。

其余默认值主要是产品和画质取舍：CRT 默认即有风格化处理、Deband 默认带颗粒、不同锐化算法的强度不能横向按数值比较。没有对应素材和运行观察，不应仅凭默认值大小就判定错误或批量改成中性值。

这份表可以作为后续逐项确认依据；当前仅交付 review，不把任何建议直接写回效果器文件。
