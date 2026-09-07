# 默认效果组预设 review

日期：2026-09-05。对象为当前 0.6.5 r5 local / fix2 的出厂默认效果组，界面现仍称“缩放模式”。依据当前工作区源码和已部署发行包，未读取或修改用户个人配置，也未运行、编译或部署程序。

## 结论与优先事项

程序内置 **6 个默认效果组、11 个效果器实例**，全局默认选择 **Lanczos**。另有源码仓库中的 3 组导入预设；它们不是创建默认配置时的读取来源，当前发行目录也没有附带这份 JSON。

静态核对没有发现这 6 组引用缺失效果器、混用多个帧生成效果器或默认参数越界的问题。主要需要产品决策的是：4 组统一带 60 FPS 限帧；高成本 VSR Ultra 被列为唯一 VSR 默认入口；DLSSFG 默认启用 NVIDIA 平衡光流，而 XeSSFG 默认不启用外部光流；DLSSNR 默认不启用降分辨率残差路径。

这些不自动等于错误。建议先明确“默认列表是通用入门选项，还是各实验后端示例”，再决定调整。未进行画质、帧率、延迟和硬件兼容性测试，不能据此宣称某档质量最好或性能达标。

## 默认来源与保留规则

| 来源                                          | 当前内容                       | 使用时机                                                             |
| --------------------------------------------- | ------------------------------ | -------------------------------------------------------------------- |
| AppSettings::_SetDefaultScalingModes          | 下表 6 组                      | 没有既有配置时创建；执行重置效果组时恢复；特定配置恢复路径为空时补回 |
| 效果器 HLSL 的 DEFAULT                        | 各效果器自身的参数默认         | 组未显式指定该参数时使用；原生后端另有缺省解析，需保持一致           |
| presets/ScalingModes-v0.6.5-experimental.json | DLSSFG、XeSSFG、DLSSNR 共 3 组 | 手动导入用；导入为追加，不替换同名组                                 |
| 用户已保存的配置                              | 用户自己的名称、顺序和参数     | 不会因源码默认列表调整而自动替换成新的 6 组                          |

重置操作会删除当前全部组，将自定义程序配置的组引用改为继承全局默认，然后恢复 6 组并选择 Lanczos。因此不能用“只是恢复某个参数”的文案描述它。

依据：[默认初始化](<../../../src/Magpie/AppSettings.cpp#L227>)、[默认组与重置](<../../../src/Magpie/AppSettings.cpp#L1329>)、[追加导入](<../../../src/Magpie/ScalingModesService.cpp#L474>)。

## 6 组总览

下面顺序与程序定义一致。箭头表示列表顺序；帧生成由渲染器／呈现后端安排，不等同于普通 HLSL 通道在该位置直接生成帧。

| 序号 | 名称                | 配置的效果器顺序                                  | 尺寸规则                       | 参数来源                                  |
| ---- | ------------------- | ------------------------------------------------- | ------------------------------ | ----------------------------------------- |
| 1    | Lanczos             | Lanczos                                           | Fit，倍率 1×1                  | 全部继承效果器默认                        |
| 2    | FSR                 | FSR_EASU → FSR_RCAS                               | EASU 为 Fit；RCAS 保持输入尺寸 | 显式 sharpness=0.87，与效果器默认相同     |
| 3    | RTX Video VSR Ultra | FrameRate_Filter → RTXVideo_VSR_Ultra             | VSR 为 Fit                     | 限帧继承 60；VSR Ultra 对应原生质量等级 4 |
| 4    | DLSSFG              | FrameRate_Filter → DLSS_FrameGeneration           | 两项声明输出与输入等尺寸       | 60 FPS；倍率 2；NVIDIA 光流质量 2（平衡） |
| 5    | XeSSFG              | FrameRate_Filter → XeSS_FrameGeneration_x2_ZeroMV | 两项声明输出与输入等尺寸       | 60 FPS；固定 x2；光流方法 0（无外部光流） |
| 6    | DLSSNR              | FrameRate_Filter → DLSSNR_AI_Filter               | 两项声明输出与输入等尺寸       | 60 FPS；15 项参数继承，详见下表           |

Fit 表示尺寸适应规则，并非每个显示模式都简单照搬“充满屏幕”：窗口模式下倍率 1 的 Fit 有视为 Fill 的处理。Normal 的默认倍率是 1×1；效果器声明固定等输入尺寸时也不会因此自动成为超分效果器。

此外，渲染器可以按显示尺寸追加 **Bicubic（B=0，C=0.5）**：窗口模式下最后输出尺寸与目标渲染尺寸不一致，或全屏模式下最后输出大于渲染区域时。它不是保存的组内预设，也不是 Bicubic 自身默认的 B=0.33、C=0.33。不能只看列表便断言最终画面绝无尺寸重采样。

依据：[EffectItem 缺省](<../../../src/Magpie/ScalingMode.h#L7>)、[尺寸计算](<../../../src/Magpie.Core/EffectDrawer.cpp#L365>)、[追加 Bicubic](<../../../src/Magpie.Core/Renderer.cpp#L1490>)。

## 逐组 review

### 1. Lanczos

唯一参数：抗振铃强度 `ARStrength=0.5`。组没有显式参数覆盖；作为全局默认时，依赖随版本发布的 Lanczos 默认值。

判断：适合作为不依赖实验原生后端的基本放大入口。保留为默认是合理的产品选择；这不是对所有素材的画质排名。若将来调整 Lanczos 的 DEFAULT，未显式保存该参数的既有组也会随之改变，需要在版本说明中交代。

### 2. FSR

`FSR_EASU` 无可调参数；`FSR_RCAS.sharpness=0.87`。组在 C++ 中显式写入锐化 0.87，当前与 HLSL 默认值一致。

判断：顺序表达了先放大、后锐化，结构明确。但“FSR”可能被误解成当前目录中的 FSR2／FSR3／FSR4 原生版本。若以后采用更明确的默认组名，可考虑“FSR 1（EASU + RCAS）”，不应据此强制重命名用户的同名组。是否降低锐化应由画质对比决定，本次不凭静态数值指定替代值。

### 3. RTX Video VSR Ultra

`FrameRate_Filter.targetFrameRate=60`；VSR 没有参数页控件，Ultra 文件名在原生工厂选择质量等级 4。默认列表没有 Low／Medium／High 组，但这些效果器文件都随包提供。

判断：作为最高档示例可保留；若面向初次使用者，应标明它是硬件相关高档入口，不能把“默认列表中有它”等同于当前机器可用。值得确认是否增加一档更低成本的 VSR 示例，或在后续界面中先展示可用性。当前没有测得性能数据，不直接推荐替换为某一档。

### 4. DLSSFG

`targetFrameRate=60`，`multiplier=2`，`motionVectorQuality=2（平衡）`。默认启用外部 NVIDIA 光流，原生帧生成设置的缺省值与参数声明一致。

判断：x2 是明确的起点。但“60 FPS”限制的是捕获处理基础帧率，不是最终显示帧率保证；基础输入不足、重复帧、硬件支持、呈现路径都会影响结果。不要把这个组直接命名为“120 FPS”。如果基础帧率超过 60，它可能限制原本更高帧率的输入。

### 5. XeSSFG

固定 x2，无可调 multiplier。`opticalFlowMethod=0（无）`，潜在 AMD 光流质量 `amdOpticalFlowMode=1（质量）`，潜在 NVIDIA 光流质量 `nvidiaOpticalFlowQuality=2（平衡）`。

两个质量值在方法为 0 时不代表已启用外部光流；它们是切换对应方法后使用的值。这里“无”只描述 Magpie 提供的外部光流，不表示 SDK 内部完全不做运动分析。

判断：与 DLSSFG 默认光流策略不同，应在说明中显式区分。不能为了形式统一就将 XeSSFG 默认改为 NVIDIA 光流；该选择会改变适用硬件和运行成本。是否启用 AMD 光流也应有对应的画质／成本依据。

### 6. DLSSNR

| 参数                         | 自身默认值 | 默认情况下的含义                                                           |
| ---------------------------- | ---------- | -------------------------------------------------------------------------- |
| enableInputResolutionScaling | 0          | 关闭输入分辨率调整                                                         |
| inputResolutionPercent       | 100        | 若启用开关则初始保持 100%；当前默认开关关闭                                |
| residualMultiplier           | 1          | 残差倍率；当前默认未进入该残差路径                                         |
| residualSaturation           | 1          | 残差饱和度不额外放大；同上                                                 |
| residualLightness            | 1          | 残差明度不额外放大；同上                                                   |
| shadowStructureMultiplier    | 1          | 阴影／结构控制中性倍率；同上                                               |
| reflectionGlowMultiplier     | 1          | 反射／辉光控制中性倍率；同上                                               |
| motionVectorQuality          | 2          | NVIDIA 光流平衡档                                                          |
| style                        | 0          | 默认风格                                                                   |
| intensity                    | 1          | 当前声明的 NR 强度默认值                                                   |
| localToneStrength            | 1          | 局部色调强度默认值                                                         |
| localStructureStrength       | 1          | 局部结构强度默认值                                                         |
| skinStructureStrength        | -1         | 原样传给后端的特殊默认数值；本地接口未给出足够依据将其解释为“自动”或“关闭” |
| useAutoMask                  | 0          | 自动遮罩关闭                                                               |
| uiCorrection                 | 0          | NR 界面修正关闭                                                            |

判断：默认配置选择全分辨率路径，未默认开启 r5 新增的降采样／残差重建处理。这与减少默认图像变化的方向一致。若需要方便评估新路径，可以另建明确命名的可选效果组，不必修改所有用户已有的 DLSSNR 组。

初始化失败时当前会报告问题并透传画面，参数面板显示不可用；因此“成功显示窗口”不等于 NR 已实际运行。`intensity=1` 等值只能描述请求设置，不能据数值推算实际画面增强程度。

依据：[DLSSNR 声明](<../../../src/Effects/DLSSNR/DLSSNR_AI_Filter.hlsl#L8>)、[原生解析](<../../../src/Magpie.Core/DLSSNRFilter.cpp#L13>)、[初始化回退](<../../../src/Magpie.Core/NativeEffectBackendFactory.cpp#L62>)。

## 60 FPS 限制的共同影响

VSR、DLSSFG、XeSSFG、DLSSNR 四组都带 FrameRate_Filter。渲染器会与捕获／程序配置的其他帧率上限取更小值；链中多个限帧效果器也取更小值。所以“组默认 60”既不保证有 60 帧，也不保证补帧后有 120 帧。

建议后续说明采用“基础帧率上限：60 FPS”；是否继续在这四组内置限帧，需明确优先照顾高刷新率游戏还是视频／固定帧率使用场景。当前不直接去掉限帧，因为它会改变资源占用和时序行为。

依据：[限帧汇总](<../../../src/Magpie.Core/Renderer.cpp#L2030>)。

## 3 组 JSON 导入预设的核对

[ScalingModes-v0.6.5-experimental.json](<../../../presets/ScalingModes-v0.6.5-experimental.json>) 包含 DLSSFG、XeSSFG、DLSSNR，每组两个效果器。20 个显式参数值均与当前效果器声明默认值一致；DLSSNR 未写入的输入分辨率开关、百分比、残差倍率也继承相同默认。与对应内置组的有效参数一致，但 JSON 显式保存更多值，将来 DEFAULT 变化时二者可能出现分歧。

JSON 没有写 scalingType 是合法缺省，不是格式损坏：`JsonHelper::ReadUInt` 的 required 默认为 false，字段不存在时保留 `EffectItem` 的 Normal 值。手动导入采用追加方式，同名组不会自动合并或覆盖；重复导入会出现同名组，用户沟通时应同时指出组内效果器。

当前发行包未附带这个文件，因此它暂时不能作为包内可直接选择的另外三项预设。旧 `ScalingModes-v0.5.7-experimental.json` 仍在仓库中，包含旧参数命名；它属于历史兼容材料，不能作为当前默认值依据。

依据：[可选字段读取](<../../../src/Magpie/JsonHelper.cpp#L51>)、[导入条目](<../../../src/Magpie/ScalingModesService.cpp#L168>)。

## 建议后续决策顺序

1. 保留 Lanczos 作为全局初始组；确认默认列表承担“入门选项”还是“后端示例”的职责。
2. 确认四个实验组是否继续默认加 60 FPS 限制。
3. 决定是否为 VSR 增加较低档入口、将 FSR 组名写明版本；不自动覆盖用户组。
4. 说明 DLSSFG／XeSSFG 的默认光流策略差异，以及 DLSSNR 默认关闭残差重建路径。
5. 维护默认组的唯一权威定义或增加静态对照，避免 C++ 默认列表、导入 JSON、效果器 DEFAULT、原生缺省值分别漂移。

以上是后续建议；本次没有改变默认组、参数、导入文件或已部署版本。






人工review结果：


| DLSSNR | FrameRate_Filter → DLSSNR_AI_Filter |
| ------ | ----------------------------------- |

这个效果组可以去掉前面的过滤器 其他的参数目前看起来没问题

## 本轮 review 确认记录（2026-09-05）

用户确认前四份 review 无异议，并在上方提出默认组修改意见。记录的后续修改目标为：**默认 DLSSNR 组移除 FrameRate_Filter，只保留 DLSSNR_AI_Filter；其他效果参数保持不变。** 其余默认组未提出移除限帧器的要求。

本文件前文仍是调查时的源码快照，并非修改后的默认列表。后续实施时应同步核对内置默认组及同名导入预设，避免定义分歧；用户自行保存的现有效果组不应仅凭同名自动改写。删除组内限帧器也不表示绕过程序配置中的其他帧率上限。

本轮继续讨论 HDR 设计，尚未执行上述程序修改。
