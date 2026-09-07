# HDR 兼容路线二次 review：原生 HDR、RTX HDR 与补帧

日期：2026-09-05。状态：**研究文档，未修改、构建、运行或部署程序，未执行 GPU／显示器测试。**

本次已阅读用户指定任务评估 Magpie PR #13 兼容性（本地任务记录）及其原报告（工作区 `reports/Magpie_PR13_HDR_兼容性与方案可行性研究.md`），复核当前本地源码、捆绑 Intel SDK 文档和相关官方资料。保留原报告，不覆盖其历史结论。

本地基线为 `source` 的 `0.6.5` 分支，HEAD `84d9f6abb8203a79f9a44d9a66984a293e68856d`，以及当前 r5 / fix2 未提交工作区。原报告的 PR 合并实验和 GPU 观察属于此前调查，本次没有重复执行；本文新增结论区分代码事实、SDK 文档约束和设计推论。

## 结论

**RTX HDR 值得保留，但定位应是“把 SDR 内容增强为 HDR”的可选能力，而不是所有 HDR 兼容问题的统一补丁。** 原生 HDR 若先被压成 8-bit SDR，后面再用 RTX HDR，也无法据此恢复原来的高光和宽色域信息。

建议的优先顺序是：**颜色正确的 HDR 捕获与直通呈现 → 少量明确兼容的原生 HDR 效果 → 现有 SDR 效果兼容路径 → 可选 RTX HDR 增强 → 按补帧后端扩展组合。** 这样可以先交付真实 HDR 的基础可用性，不必等所有效果器都重写，也不把基础 HDR 支持绑定到 NVIDIA。

相较原报告，最需要调整的是两点：

1. “所有补帧之后统一加 RTX HDR，然后统一 FP16 呈现”不适用于当前 XeSSFG 架构和 SDK 格式合同。XeSSFG 支持 HDR10，但不支持 FP16/scRGB；需要独立设计这条路径。
2. 先做不增强画面的 HDR 直通基线，再接入 AI 增强。否则颜色错误、显示器映射错误与 AI 效果变化混在一起，很难定位问题，也无法建立可信的原图对比。

## 对原报告的复核与修正

| 原报告判断 | 本次结论 | 后续影响 |
| --- | --- | --- |
| PR #13 不宜原样合并；固定 4.5 的 HDR↔SDR 桥有损 | 保留 | 借鉴捕获与格式传递设计，不能把成对 shader 当作 HDR 保真转换 |
| HDR 输出不应由 HDR 捕获方式单独决定 | 保留并细化 | 捕获格式、内容类型、目标显示能力、补帧后端分别参与决策 |
| RTX HDR 适合近期兼容路线 | 有条件保留 | 适合 SDR→HDR 增强；基础原生 HDR 支持应先独立建立 |
| 自动尺寸适配应保留 | 保留 | 基础缩放必须支持目标颜色合同，不能整体关闭自动 Bicubic 规避格式问题 |
| RTX HDR 统一放在所有 FG 之后 | 修正 | DLSSFG 可研究后置，XeSSFG 代理交换链需要 HDR 在送入 SDK 前准备好 |
| XeSSFG 的困难主要是现有 R8 写死 | 加重 | SDK 本身不接受 FP16/scRGB；不是改一个格式常量就能解决 |
| R10/PQ 需要设置 HDR metadata | 修正 | 正确像素编码及颜色空间声明是必要环节；不能将 SetHDRMetaData 作为可靠显示的保证 |
| TrueHDR 四项参数范围及部分 Live 分类 | 降级为待确认 | 原报告范围来自头文件镜像；集成前必须以采用版本的官方 SDK 合同复核 |
| 非 AI 的逆映射画质通常低于 RTX HDR | 删除这一概括 | 无本项目对照证据；不同内容、时序和目标亮度下应分别评价 |
| 原生 HDR 要等完整渲染图改造 | 缩小首阶段范围 | 先做直通及少量白名单节点，再扩展；格式和颜色元数据从边界开始贯通 |

PR #13 当前可在[原 PR](https://github.com/SAOG0721/Magpie/pull/13)查看；本次查阅仍指向原报告讨论的头提交 `9fa5387b552df7d7db40b7bc16fa83c39a2873bd`。PR 讨论中的单设备成功观察不能覆盖当前分支的光流、补帧、UI 与截图组合。

## 首先区分三种含义

| 名称 | 实际目标 | 不能由什么推断 |
| --- | --- | --- |
| HDR 显示／捕获兼容 | 捕获和呈现时亮度、色域及编码正确 | 不能由“Windows HDR 已开启”推断窗口内容就是真 HDR |
| 原生 HDR 效果处理 | 尽可能保留原始 HDR 信息并施加效果 | 不能由“纹理改成 FP16”推断算法已兼容 HDR |
| SDR→HDR 增强 | 从 SDR 推算或映射出更大的亮度范围 | 不能称作恢复曾被丢弃的原生 HDR 数据 |

同一个 HDR 桌面可以同时包含 SDR 应用、HDR 视频和 UI。窗口捕获的容器格式并不自动携带每块内容的原始创作意图。建议内容来源允许“自动／按 SDR／按 HDR”，自动识别证据不足时保留“未知”，不要仅凭像素最大值或显示器 HDR 开关自动启用 RTX HDR。以上为产品与架构建议，尚无分类器实现。

## 补充：具体提供什么界面，是否要用户手搭三个效果器

2026-09-05，用户询问是否应提供 HDR→SDR、SDR→HDR、RTX HDR（SDR 输入）三个效果器，由用户自己排列。本节明确本报告的**设计建议**，尚未由用户选定或实施。

建议做成 **效果组的 HDR 输出设置，加上由软件管理的必要颜色转换**。普通用户继续添加去噪、锐化等效果器，在组设置中决定是否使用 HDR 增强；无需知道转换器应该放在哪一行。

### 三项功能的分工

| 功能 | 实际作用 | 推荐呈现方式 |
| --- | --- | --- |
| HDR→SDR 色调映射 | 将原生 HDR 映射到 SDR，使 SDR 输出或旧效果链能够使用；这个步骤有损 | 软件在用户选择 SDR 输出／SDR 兼容处理后插入；可在高级设置调节映射策略 |
| SDR→HDR 显示适配 | 把 SDR 正确放到 HDR 输出环境中，匹配白点和颜色编码；默认不创造高光细节 | 输出阶段自动处理，通常无需成为用户手动添加的效果器 |
| RTX HDR 增强 | 对 SDR 内容做可选的 HDR 增强 | 效果组中的独立开关和参数卡；软件负责放在经验证的执行位置 |

“SDR→HDR”这个名字容易混淆两种行为：**正确显示 SDR** 和 **增强为 HDR**。本方案把前者称为“显示适配”，把后者称为“HDR 增强”。若以后增加非 RTX 的逆色调映射算法，它会成为增强方式的另一个选项。

普通显示适配与 RTX HDR 不是要求串联的两次增强。RTX HDR 的结果仍可能需要转换成呈现器要求的格式，但那是输出适配，不再把结果当 SDR 重做一次提升。

### 用户看到的建议结构

效果组页面末尾增加“HDR 输出与增强”分区：

- **输出方式：跟随显示器／SDR。** 跟随显示器表示选择合适的输出环境，不意味着自动把所有 SDR 内容变成增强 HDR。
- **HDR 增强：关闭／RTX HDR。** 仅在来源按 SDR 处理、目标支持 HDR 且后端能力满足时可用；选中后展开相应参数，沿用效果参数页的交互。
- **HDR 内容兼容方式：保持原生 HDR／转为 SDR 兼容处理。** 出现不兼容的效果时明确指出，由用户决定采用有损兼容还是停用该效果，不暗中丢弃 HDR 信息。

“来源内容：自动／按 SDR／按 HDR”更适合放在程序配置或当前会话的高级设置中，因为同一个效果组可能用于不同内容。识别证据不足时让用户覆盖，不能由效果组名称决定来源类型。

具体控件布局可继续沿用现有收起项。RTX HDR 可以拥有与效果器相同风格的参数卡，但其受约束的执行阶段由软件管理，首版不作为可以随意拖动、重复插入的普通节点。

### 典型情况下软件怎样组装

| 用户意图 | 组装后的概念流程 |
| --- | --- |
| SDR 内容正常显示在 HDR 显示器上，不做增强 | SDR 捕获适配 → 现有效果组 → SDR 的 HDR 显示适配 → 显示 |
| SDR 内容使用 RTX HDR | SDR 捕获适配 → 现有效果组 → RTX HDR → 必要的输出编码适配 → 显示 |
| 原生 HDR 内容，只使用已确认兼容的效果器 | 保留 HDR 的捕获 → HDR 兼容效果组 → 目标显示适配 → 显示 |
| 原生 HDR 内容必须使用旧 SDR 效果器 | 用户选择 SDR 兼容 → HDR→SDR 映射 → 旧效果组 → SDR 显示或放入 HDR 输出环境 |

最后一条不保留完整原生 HDR，输出到 HDR 显示器也不会改变这一事实。若以后允许再加 RTX HDR，那应明确属于“有损转换后重新增强”，不能默认启用或称为恢复原生 HDR。前文的差分回注实验是另一条待研究路线。

此表只说明颜色与效果之间的关系；存在补帧时，HDR 增强和输出适配的位置仍按下文各后端的合同确定，不能把表中的“末端”理解为永远在所有 SDK 补帧之后。

### 对内部代码的具体要求

启动时读取来源设置、效果器能力、HDR 增强选择及呈现后端要求，生成受校验的执行计划。普通效果链仍复用现有实现；必要的捕获适配、SDR 兼容入口、增强阶段、尺寸适配和输出转换由运行器管理。

内部可以分别实现 HDR→SDR、SDR 显示适配和 TrueHDR 后端，但它们不等同于三个任意可串联的列表项。增强结果与显示适配共享输出合同，避免重复转换。遇到尚未支持的组合，在启动前指出具体节点及可用处理方式；Live / Restart 按实际资源及 SDK 参数合同判断。

高级手动转换节点以后可以作为可选能力提供，但必须检查相邻节点的颜色语义、重复增强和输出要求，不能把非法排列的黑屏／偏色风险完全交给用户。这项高级模式不作为首版 HDR 的必要条件。

## 新发现一：XeSSFG 需要 HDR10，独立覆盖层却可用 FP16

捆绑的 `XeSS-SDK-3.0.1` 中，XeSSFG 开发指南明确要求 HDR 使用 `R10G10B10A2_UNORM`；back buffer、HUD-less color 与提供给 SDK 的 UI-only texture 要匹配 HDR10 / BT.2100。文档明确排除 FP16 HDR 和 scRGB。本地依据（工作区 `dependencies/XeSS-SDK-3.0.1/doc/xess_fg_developer_guide_english.md:626`）、[Intel 官方指南](https://github.com/intel/xess/blob/main/doc/xess_fg_developer_guide_english.md)。

当前 Magpie 的 XeSSFG 在多个位置共用 RGBA8 常量，包括代理交换链和独立 DirectComposition 表面。[当前格式与创建](<../../../src/Magpie.Core/XeSSFGPresenter.cpp#L17>)、[独立表面](<../../../src/Magpie.Core/XeSSFGPresenter.cpp#L358>)。

Microsoft 的 DirectComposition surface 支持格式列表包含 FP16，未包含 R10。因此应拆开“SDK 主画面格式”和“外部覆盖表面格式”：候选组合是 **HDR10/PQ 主交换链 + 正确处理颜色和透明度的 FP16 外部合成层**。外部 DirectComposition 层并不是提供给 XeSS SDK 的 UI-only texture，两者不能套用同一格式约束。[官方表面格式说明](https://learn.microsoft.com/en-us/windows/win32/api/dcomp/nf-dcomp-idcompositiondevice2-createvirtualsurface)。

这是从两份合同和当前架构得到的可行方向，**不是已经证明本程序混合合成呈现正确**。仍需验证 DWM 合成、UI 白点、透明混合、遮罩光标、缩放偏移和驱动行为。不能简单把 `COLOR_FORMAT` 全局改成 R10 或 FP16。

建议 XeSSFG 的 HDR 增强候选顺序为：

```text
SDR 内容 → 现有 SDR 效果与最终尺寸适配
        → RTX HDR（输出合同先核对）
        → 必要的色域／传递函数转换 → R10 HDR10
        → XeSSFG 代理交换链 → 显示

外部覆盖层：独立颜色适配后的原图对比、工具栏和光标
```

优点是 RTX HDR 只处理基础帧，不按补帧倍率重复运行；代价是 HDR 增强的时间变化进入补帧，必须观察场景切换、亮度泵动和运动伪影。当前代码没有“截获所有 SDK 内部生成帧并再执行任意后处理”的通用入口，不能在流程图里凭空假定存在。

## 新发现二：原生 HDR 可以先做小范围白名单

XeSS-SR 官方指南接受线性 HDR，说明 scRGB 与曝光约定，并建议使用 FP16 颜色输入。当前 Magpie 的 XeSS SR 实现却限制 R8 输入／输出、启用 `XESS_INIT_FLAG_LDR_INPUT_COLOR`，执行时曝光固定 1.0。本地 SDK 颜色合同（工作区 `dependencies/XeSS-SDK-3.0.1/doc/xess_sr_developer_guide_english.md:299`）、[Intel 官方指南](https://github.com/intel/xess/blob/main/doc/xess_sr_developer_guide_english.md)、[本地格式限制](<../../../src/Magpie.Core/XeSSZeroMVUpscaler.cpp#L199>)、[LDR 初始化](<../../../src/Magpie.Core/XeSSZeroMVUpscaler.cpp#L433>)。

因此 XeSS-SR 可作为以后审核原生 HDR 效果的候选，不能把它与 XeSS-FG 的格式限制混为一谈。当前 ZeroMV／深度等输入近似仍影响画质，SDK 支持 HDR 并不等于当前封装直接可用。

建议第一批白名单从以下范围开始：

1. 原图直通与明确按线性 HDR 工作的基础插值；先确认恒等路径和目标尺寸处理。
2. 经逐项合同审核的原生 SR 后端，分别检查输入编码、曝光、输出范围及历史重置。
3. 少量数学意义明确的 shader，再逐个扩展；含 `saturate`、固定 SDR gamma、8-bit 中间结果的节点默认不自动放行。

一个容易误判的本地例子是 `RTXVideoDenoiser.cpp`：外围允许浮点纹理，但内部 `VideoSuperRes` 图像仍分配为 `NVCV_U8`，浮点传输按 255 倍缩放。**接受 FP16 资源不等于保留高于 1 的亮度及宽色域。** 现有 Maxine VideoSuperRes 集成也不能据此视为 RTX Video TrueHDR 已接入。[实现依据](<../../../src/Magpie.Core/RTXVideoDenoiser.cpp#L102>)。

## 颜色基线：不能共用一个固定白点

Windows 的 HDR scRGB 约定以线性值 1 对应 80 nit；FP16 只是承载方式，颜色空间声明与实际像素处理必须一致。[Microsoft HDR / Advanced Color 指南](https://learn.microsoft.com/en-us/windows/win32/direct3darticles/high-dynamic-range)。

`DISPLAYCONFIG_SDR_WHITE_LEVEL` 按显示路径查询，换算为 `SDRWhiteLevel / 1000 × 80 nit`。因此原 PR 固定的 4.5 只代表特定 360 nit 参考，不是跨设备常数。[结构体官方说明](https://learn.microsoft.com/en-us/windows/win32/api/wingdi/ns-wingdi-displayconfig_sdr_white_level)。

还应区分 **源显示器的 SDR 白点** 与 **目标显示器的 SDR 白点**。例如源窗口在一个 HDR 显示器上，Magpie 输出在另一个显示器上，两个白点设置可能不同。

在“已知捕获确实是按源白点嵌入 scRGB 的 SDR 内容”这一前提下，可用以下关系检查转换设计：

```text
SdrEncoded = sRGB_OETF(CaptureScRGB × 80 / SourceSdrWhiteNits)
OutputScRGB = sRGB_EOTF(ProcessedSdrEncoded) × TargetSdrWhiteNits / 80
```

这是条件成立时的数学归一化关系，**不是所有 WGC 内容都可无损反推的保证**。混合内容、应用自身映射、ICC／系统处理以及未知捕获语义需要单独确认。真实 HDR 的绝对亮度也不能直接套用 SDR 白点重标定，否则会把 HDR 本身的亮度关系改掉。

目标能力可以读取 `IDXGIOutput6::GetDesc1`，但小面积峰值 `MaxLuminance` 与全屏亮度 `MaxFullFrameLuminance` 含义不同；信息通常来自 EDID 或覆盖设置，不能视为本次实测校准。建议保留合理性校验和用户覆盖入口。[DXGI_OUTPUT_DESC1](https://learn.microsoft.com/en-us/windows/win32/api/dxgi1_6/ns-dxgi1_6-dxgi_output_desc1)。

输出颜色空间应通过匹配的 API 声明，像素转换本身仍由管线执行。不能将 `SetColorSpace1` 当作把错误编码自动转换正确的 shader。[SetColorSpace1](https://learn.microsoft.com/en-us/windows/win32/api/dxgi1_4/nf-dxgi1_4-idxgiswapchain3-setcolorspace1)。Microsoft 已不推荐应用依赖 `SetHDRMetaData`；metadata 可能不送达或被显示器忽略，应依据目标能力做恰当映射。这修正了原报告将其列为 R10 路线必要步骤的说法。[SetHDRMetaData 官方说明](https://learn.microsoft.com/en-us/windows/win32/api/dxgi1_5/nf-dxgi1_5-idxgiswapchain4-sethdrmetadata)。

## RTX HDR 的可行性与尚未确认的合同

NVIDIA 当前公开入门页列出 RTX Video SDK 1.1、DX11／DX12 等 API、RTX 20 系列及更新 GeForce 支持，并描述 Rec.709 SDR 到 HDR10 兼容 Rec.2020 的增强目标。这支持继续调查该集成方向，但不保证任意硬件、驱动和其他 NGX 功能组合均可初始化。[官方入门页](https://developer.nvidia.com/rtx-video-sdk/getting-started)。

2024 年 NVIDIA 技术文章正文又以 sRGB→scRGB 描述 RTX Video HDR。两种表述并不必然矛盾：算法表面、应用工作空间和最终显示编码是不同层次；**产品页写 HDR10，不能据此猜测 SDK 输入／输出纹理一定是 R10/PQ**。[NVIDIA 技术文章](https://developer.nvidia.com/blog/enhancing-low-resolution-sdr-video-with-the-nvidia-rtx-video-sdk/)。

| 需要确认 | 为什么会改变实现 |
| --- | --- |
| 所采用官方 SDK 的准确输入／输出格式、色域、传递函数和亮度单位 | 决定前处理、输出适配，以及能否直接接 FP16 或必须额外转换 |
| API 版本、驱动与适配器上的 TrueHDR capability | RTX 型号判断不足；失败要能区分驱动、运行库、功能不可用和初始化错误 |
| Contrast、Saturation、MiddleGray、MaxLuminance 的范围、默认值及单位 | 原报告的 0–200、10–100、400–2000 等镜像值仅作查找线索，不宜直接成为产品约束 |
| 参数在 evaluate 时还是 create 时读取 | 决定 Live / Restart；不能先依据参数名称认定“对比度当然可以 Live” |
| 多实例、线程、尺寸变化与历史处理要求 | 影响直通后台保持、动态窗口和与降噪／补帧并存 |
| 与 DLSSNR、DLSSFG、VSR 的运行库和核心生命周期共存 | 影响 DLL 选择、初始化和最终 shutdown；失败不应损坏另一功能 |
| 随应用分发的组件及采用版本许可 | 在实际集成版本确定后核对，不凭网页概述作结论 |

2024 年 NVIDIA 官方论坛回复曾确认 DLSS 与 RTX Video SDK 的 TrueHDR capability 查询存在已知兼容问题，并表示计划在后续 SDK 处理；本次没有找到足以证明当前组合已经修复的明确版本结论。它是需要验证的历史线索，不能当作当前版本必然不兼容的证据。[NVIDIA 官方人员回复](https://forums.developer.nvidia.com/t/compatibility-issue-between-dlss-and-rtx-video-sdk-nvsdk-ngx-parameter-truehdr-available-fails/313309)。

本地已有集中管理的 D3D12 NGX 生命周期。选择 D3D11 TrueHDR 可能减少某些互操作成本，D3D12 可能利于与已有核心协调；二者仍需根据实际 SDK 合同比较，不能承诺 D3D11 自动避开 NGX 冲突。[当前核心初始化](<../../../src/Magpie.Core/NgxD3D12Core.cpp#L118>)、[释放逻辑](<../../../src/Magpie.Core/NgxD3D12Core.cpp#L153>)。

本次没有下载或安装新 SDK，没有改运行库或调用 capability 探测；上述未知项保留到用户决定推进集成之后。

## 补帧与 HDR 的顺序：分别设计

| 组合 | 推荐调查路线 | 关键限制 |
| --- | --- | --- |
| 无补帧 + SDR→HDR | SDR 效果、最终尺寸适配 → HDR 增强 → 输出适配 → UI／光标 | 最适合先建立 AI 增强基线 |
| DLSSFG + HDR 在补帧前 | SDR 效果 → HDR 增强 → 经确认支持的 DLSSFG HDR 输入 → 输出 | 每个基础帧增强一次，但需核对 SDK 格式、颜色语义及运动／深度合同 |
| DLSSFG + HDR 在补帧后 | SDR 效果 → DLSSFG → 每张输出帧 HDR 增强 → 输出 | 更接近旧报告；需处理所有生成帧与真实帧，增强吞吐量随实际输出帧率增加 |
| XeSSFG + HDR | SDR 效果 → HDR 增强及 HDR10 编码 → XeSS 代理交换链 | SDK HDR 输入是 R10/PQ 路线，不能直接统一 FP16 |
| 原生 HDR + 补帧 | 原生 HDR 白名单链 → 后端要求的编码 → FG | 首阶段不承诺组合支持，先证明无 FG 的颜色基线 |

本地 DLSSFG 会根据输入格式设置 `NativeBackbufferFormat`，这只是代码传参事实，不能证明 FP16、HDR10 及当前代理输入方式全部已受支持。[传参位置](<../../../src/Magpie.Core/DLSSFrameGenerator.cpp#L557>)。

后置 AI 增强还会新增队列同步及延迟，不能只比较单次 dispatch 的耗时。例如 60 个基础帧按 x4 生成并实际提交 240 个输出帧时，逐输出帧增强的调用量可达前置的四倍；这是调用数量推算，不是实测性能比例。

## 新方案候选：保留原生 HDR，并回注 SDR 效果的改变量

这是对“原生 HDR 想使用已有 SDR 效果”问题的补充研究，**不是已验证算法，也不是对 RTX HDR 的画质排名**。

原路线把 HDR 全部压到 SDR，效果处理后再往上扩展。可以探索保留完整原始 HDR 分支，只让 SDR 代理进入旧效果链，再将两者之间的改变量映射回 HDR：

```text
原始 HDR H ───────────────────→ HDR 基础尺寸适配 G(H) ─┐
       └→ 色调映射 T(H) → 同尺寸 SDR 基线 P0           ├→ 回注 → HDR 输出
                       └→ 旧效果链 → 结果 P1 → 差分 ─┘
```

用于讨论的形式为：

`Hout = G(H) + strength × mask × [L(P1) − L(P0)]`

其中 `P0` 和 `P1` 必须对应同一捕获时刻、同一目标网格和一致的基准尺寸适配；`L` 是两者共用的 SDR 解码与参考白嵌入，而非色调映射的“完美逆函数”。对无改动效果，要求 `P1 = P0`，此时输出回到 HDR 基线 `G(H)`，不会仅因经过 SDR 兼容分支就删除原图高光。最终显示映射仍可能受目标色域和亮度限制。

有价值的地方是原 HDR 数据始终保留，不必让 AI 重新猜测被兼容转换抹掉的信息。与当前 DLSSNR 的“重建残差”思路有相似之处，但作用域、颜色空间和时序不同，不能直接复用现有残差参数或算法后宣称成立。

主要风险：

- 色调映射在高光处压缩强，SDR 差分与 HDR 中应施加的改变量不等价；不能保证去噪／锐化在高光区保留相同强度。
- 直接 RGB 回注可能产生负值、色偏或过冲。亮度比例／亮度差、色度保护、亮度相关 mask 均需比较，不能先选一个公式当通用标准。
- 自动曝光变化会引入时间闪烁，需要一致且稳定的代理生成条件。
- CRT、几何扭曲、像素化、整体调色和时间重建不一定适合这种局部改变量假设；先限制少量空间效果。
- 补帧时原 HDR 参考和生成 SDR 结果时间不一致，不能直接回注；首个原型应关闭补帧组合研究。

建议作为 **原生 HDR 兼容实验的独立分支**，在正确的 HDR 直通建立后再评估。它不应阻塞基础 HDR，也不宜第一阶段就对所有效果器开放。

跨厂商 SDR→HDR 可另外研究确定性映射，ITU-R BT.2446-1 报告可作为方法资料；不预设其画质一定低于 AI。普通“把 SDR 白点放进 HDR 容器”也有兼容价值，但应明确没有新增高光细节。[ITU 官方报告入口](https://www.itu.int/pub/R-REP-BT.2446-1-2021)。

## 当前代码中 HDR 迁移必须处理的边界

以下是切入 HDR 后的阻断点，不表示当前 SDR 运行已经因此出错。

| 边界 | 当前代码事实 | 建议 |
| --- | --- | --- |
| 捕获 | WGC 帧池和输出使用 BGRA8 | HDR 捕获建立 FP16 路径，帧池／中间资源／副本合同一致 |
| 帧引导 | 在效果前读取捕获帧；NVOF 内部 BGRA8 并直接复制输入 | 保留 HDR 主图，另建明确定义的光流代理；不要把 FP16 直接 CopyResource 到 BGRA8 |
| 效果边界 | 现有节点不能仅靠纹理位深表达支持范围 | 校验实际资源及语义；不兼容时给出原因，转换集中在兼容边界 |
| 自动尺寸适配 | 会追加 Bicubic 等尺寸处理 | 提供颜色合同匹配的适配节点，保留原有目标矩形行为 |
| 跨线程共享 | 当前颜色共享资源为 R8，帧槽配有锁和完成同步 | 将颜色格式与元数据随资源代次传递；同时核对副本、视图、缓存及生命周期 |
| 呈现 | Adaptive、CompSwapchain 以及 XeSSFG 均有 R8 固定点 | 按输出合同创建，XeSS 主链与外部合成层分别选格式 |
| UI／光标 | SDR 数值与 R8 背景复制假设仍存在 | 明确 UI 白点、解码和混合；背景相关光标取当前可见场景 |
| 截图 | 通用 PNG 路径断言 R8 | 提供明确映射后的 SDR PNG；HDR 保存选择真正支持 HDR 的格式与元数据，不能只移除断言 |
| 窗口跨屏／HDR 开关 | 需要更新输入与输出能力 | 使用资源代次处理变化，避免新格式帧误入旧交换链；回退可解释 |

代码依据：[WGC 帧池](<../../../src/Magpie.Core/GraphicsCaptureFrameSource.cpp#L323>)、[NVOF 复制](<../../../src/Magpie.Core/NvidiaOpticalFlowProvider.cpp#L699>)、[共享纹理](<../../../src/Magpie.Core/Renderer.cpp#L1825>)、[AdaptivePresenter](<../../../src/Magpie.Core/AdaptivePresenter.cpp#L32>)、[CompSwapchainPresenter](<../../../src/Magpie.Core/CompSwapchainPresenter.cpp#L150>)、[截图格式限制](<../../../src/Magpie.Core/TextureHelper.cpp#L325>)。

Microsoft 的捕获指南同样建议 HDR 捕获处理中采用 FP16，避免中间步骤造成过曝或截断；这不表示已有 BGRA8 光流与 SDR 效果应无差别改成 FP16，而是应把有损代理与保留 HDR 的主分支分清。[官方屏幕捕获指南](https://learn.microsoft.com/en-us/windows/apps/develop/media-authoring-processing/screen-capture)。

建议最小帧合同包括：像素格式、传递函数、原色／色域、亮度尺度或参考白、内容分类及其可信度、alpha 约定、捕获帧标识、捕获时间、资源代次和源／目标显示路径。先在捕获、兼容转换、共享发布和呈现边界落实，不必首个版本就重写全部 170 个随包效果器。

## 与工具栏原图对比共用设计

用户新提出的 pass through 很适合作为颜色基线入口，但两条分支要使用 **同一个目标输出合同**：对比切换只改变被显示的内容，不切换 Windows HDR，不在每次点击时重建交换链。

- SDR 源：原图使用不增强的 SDR→目标显示适配；另一分支执行效果和可选 RTX HDR。
- 原生 HDR 源：原图保留捕获 HDR，只做必要的裁剪、基础尺寸和显示适配，不能先转 SDR 再当“原图”。
- 对比期间 RTX HDR 和其他已启用效果仍继续处理；切回使用当前结果。
- XeSSFG：原图通过独立合成层覆盖，代理交换链保持处理后 HDR10 输入；需验证下层在遮挡时仍持续处理。

工具栏的完整方案见[直通对比 review](<20260905-toolbar-pass-through-REVIEW.md>)。HDR 的 UI 白点、光标和截图也应复用这一“当前可见场景”边界，避免分别维护互相矛盾的画面来源。

## 建议的产品设置与报错分类

用户界面先围绕内容和目标描述：来源自动／SDR／HDR；输出跟随显示器／SDR；SDR 的 HDR 增强关闭／RTX HDR。FP16、PQ、交换链等信息放在详情中，除非用户确实需要手动选择来排错。

| 分类示例 | 面向用户的建议提示 | 详情中保留 |
| --- | --- | --- |
| 目标未启用 HDR | “当前显示器未启用 HDR，可在 Windows 显示设置中开启，或使用 SDR 输出。” | 显示路径、当前能力与色彩空间 |
| 捕获方式不支持保留 HDR | “当前捕获方式无法保留 HDR 信息，请改用支持 HDR 的捕获方式。” | 捕获后端、实际格式、能力查询结果 |
| 效果不兼容 | “此效果暂不支持原生 HDR；可停用此效果或选择 SDR 兼容处理。” | 具体效果及输入／输出合同差异 |
| RTX 功能不可用 | 根据实际原因提示更新驱动、检查运行库或改用不增强的显示方式 | GPU／驱动、SDK 版本、capability 与原始错误码 |
| 来源不明确 | “无法确认内容是否为 HDR，请选择来源类型后启用 HDR 增强。” | 判定来源及其可信度 |
| 补帧格式组合不支持 | “当前 HDR 输出方式与所选补帧不兼容。”并列出实际可用选择 | FG 后端、格式与颜色语义 |
| 显示环境变化 | “显示器状态已变化，正在重新适配输出。”失败时给出可用回退 | 变化前后合同、资源代次和失败阶段 |

这些是待实施的分类方向，复用现有错误详情入口即可。不要把所有 capability 失败都提示为“显卡不支持”，也不要无提示地将真实 HDR 截成 SDR 后继续显示。

## 推荐实施顺序与放行条件

| 阶段 | 范围 | 必须先证明什么 |
| --- | --- | --- |
| 0：合同核验 | 确定采用的 SDK 版本、TrueHDR 格式与参数、NGX 共存方式 | 文档／样例支持拟定输入输出；未确认参数不标为 Live |
| 1：HDR 基础 | WGC FP16、HDR 直通、基础尺寸处理、输出适配、UI 与光标 | 灰阶、参考白、高光、宽色域和跨屏行为正确；不依赖 AI |
| 2：受限原生效果 | 小范围白名单，单独审核 SR 与 shader | 恒等／默认行为可解释；不夹断亮度与色域 |
| 3：SDR 兼容与 RTX HDR | 明确来源的 SDR 效果链；无补帧 HDR 增强 | 不重复增强原生 HDR；参数语义及 Live / Restart 有据可查 |
| 4：补帧组合 | 分别接 DLSSFG 与 XeSSFG，不共用错误格式假设 | 每个组合的颜色、吞吐量、时序、历史及直通覆盖通过验证 |
| 5：可选实验 | SDR 改变量回注、跨厂商逆映射 | 在明确内容样本上评估收益与失败案例，不承诺通用兼容 |

建议后续用户验收材料包含已知 nit 的灰阶与高光、超出 SDR 的颜色、细文本和光标、静态／运动画面、快速场景切换、SDR 与原生 HDR 来源、双显示器白点差异、系统 HDR 切换，以及各 FG 倍率。应区分资源／数值检查、屏幕视觉检查和算法主观画质比较。

**本次只有源代码和文档核查，没有执行上述验收，也没有测得 RTX HDR 延迟、效果质量或 NGX 共存结果。**

## 留待用户 review 的决策

本次推荐先批准路线原则，而非立即批准整个集成：保留 RTX HDR 方向；基础 HDR 独立建立；XeSSFG 单独采用 HDR10 合同；原生 HDR 默认旁路 SDR→HDR 增强；差分回注留作实验。

是否把 RTX HDR 的视觉增强优先于首批原生 HDR 效果、是否研究差分回注、是否首个 HDR 版本就包含 XeSSFG，均可在 review 后决定。用户暂时离开期间，本任务到文档交付为止，不继续执行程序修改。
