# RTSS Front Edge Sync 与 Magpie 呈现节奏调查

日期：2026-09-05。基于 `0.6.5` 主线当前 r9 工作区，以及本机 RTSS 7.3.5.28314 的官方帮助和 SDK 示例。本次只调查、交付方案，没有修改程序、RTSS 配置或部署文件，没有执行画面测试。

## 结论与测试条件

**Magpie 可以实现类似 Front Edge Sync 的稳定呈现模式，当前普通呈现路径确实缺少对应的输出节奏控制。建议首先针对无 FG 的 DLSSNR 场景实现，再分别适配 FG。**

用户确认：游戏与 Magpie 都锁 80 FPS，使用 DLSSNR，没有开启 FG；显示器支持 FreeSync，但用户观察到 Magpie 呈现时 FreeSync 没有生效。目标周期因此是 **12.5 ms**。

这项观察支持继续调查“输入交付稳定性、输出呈现间隔与 GPU 工作量”的关系，但不能单独区分锁低帧率减轻负载和 Front Edge 等待位置的收益。也不能据此认定游戏与 Magpie 已建立逐帧同步协议。

调查时读到的 RTSS 磁盘全局配置是 `Limit=100`、`SyncLimiter=1`、`PassiveWait=1`、scanline 为 0。**它只代表读取时的配置，不覆盖用户明确说明的 80 FPS 测试条件，也不能证明过去两个进程的实际挂钩状态。**

## RTSS 实际控制的是什么

本机随附帮助明确区分三种模式：

| 模式 | 主要控制位置和目标 | 卡顿后的策略 |
| --- | --- | --- |
| Async | 约束下一帧开始，优先保证严格的最小帧间隔 | 性能不足时允许时间基准漂移 |
| Front Edge Sync | 在实际 Present 调用之前等待，稳定 Present 开始时刻 | 优先恢复与高精度时钟的同步，允许某些帧暂时超过限帧值 |
| Back Edge Sync | 在 Present 之后等待，稳定下一帧开始时刻 | 同样偏向恢复同步，可能短暂超过限帧值 |

来源：[本机 RTSS 官方帮助](<C:/Program Files (x86)/RivaTuner Statistics Server/Help/Properties/General/SYNC_LIMITER>)。RTSS 作者 Unwinder 也直接说明了 Present 前后等待的区别，以及帧开始和 Present 两种统计口径为什么会得到不同的曲线：[作者说明](https://forums.blurbusters.com/viewtopic.php?start=40&t=7551)。该讨论发生在 2020 年，现行模式选项以本机 7.3.5 帮助为准。

这里的 front edge 指 **Present 调用的前沿**。单独使用它并不等于等待屏幕 VBlank，也不等于启用 Scanline Sync。RTSS 将扫描线同步列为另一个可组合的功能：[本机扫描线帮助](<C:/Program Files (x86)/RivaTuner Statistics Server/Help/TEXT_SSYNC>)。

Passive Waiting 是另一个维度：选择主要使用可等待定时器还是忙等待。改变它会影响 CPU 开销和唤醒精度，却不改变 Front Edge 的限帧位置。[本机等待方式帮助](<C:/Program Files (x86)/RivaTuner Statistics Server/Help/Properties/General/ENABLE_PASSIVE_WAITING>)

本次没有获得 RTSS 核心限帧实现源码。上面的模式语义有官方说明支持；其内部时间原点、补偿公式、忙等待窗口和跨进程相位细节不能当作已经反向确认。

## 为什么当前场景可能受益

1. **游戏先稳定交付。** 游戏的 Present 节奏更均匀，可能让 WGC 收到的输入更均匀；WGC、DWM 和 GPU 调度仍可能引入额外抖动，不能保证一对一即时交付。
2. **Magpie 吸收处理耗时的短时变化。** 只稳定处理开始时刻，无法消除处理结束时刻的变化。在还有时间余量时，将输出对齐到计划时刻可以吸收这些变化。
3. **两侧限帧可能降低竞争。** 游戏少产生超出目标的帧，Magpie 少提交额外的 UI／光标帧，都可能留出 GPU 调度余量。只给 Magpie 的 Present 加等待，并不必然降低其后端效果计算量。

以下是说明原理的理想化例子，单位为 ms，假设每张内容已经完成必要的 GPU 处理；不是本次实测或 RTSS 内部算法：

| 帧 | 输入到达 | 处理耗时 | 处理后立即提交 | 对齐到固定输出时刻 | 额外等待 |
| --- | ---: | ---: | ---: | ---: | ---: |
| 1 | 0 | 4 | 4 | 8 | 4 |
| 2 | 12.5 | 7 | 19.5 | 20.5 | 1 |
| 3 | 25 | 4 | 29 | 33 | 4 |

立即提交的间隔为 15.5、9.5 ms；留出适当处理余量后的间隔为 12.5、12.5 ms。代价是已完成内容的额外等待；总延迟还取决于是否减少了原有排队，不能直接宣称增加或减少固定一整帧。

如果输入真的中断 50–60 ms，有限余量的呈现器无法制造缺失的新画面。反复提交旧图能让某些 Present 统计变平，却不等于解决内容停顿。之前 r9 debug 已记录过约 60、53 ms 的捕获输入间隔，必须继续与输出抖动分开分析：[首份诊断 review](<20260905-v0.6.5-r9-debug-first-trace-REVIEW.md>)。

同时设置 80 FPS 只说明目标周期相同。游戏第 n 帧还要经过捕获、NR 和共享纹理发布，Magpie 才能提交其结果；即便使用同一系统时钟，也需要合理的相位偏移和队列控制。当前 WGC 接口没有向游戏传递“Magpie 已准备接收下一帧”的反向限帧协议。

## 当前 Magpie 的具体差距

| 位置 | 已确认行为 | 对方案的影响 |
| --- | --- | --- |
| `StepTimer.cpp:38,88` | 等待最大帧率发生在捕获／处理入口；限帧时维护开始时间网格 | 不是普通输出的 Present 前限帧器 |
| `Renderer.cpp:2263` | 最大帧率和 Frame Rate Filter 最终进入 StepTimer | 当前配置约束基础处理速率，不能解释为全部窗口提交上限 |
| `GraphicsCaptureFrameSource.cpp:111,434` | 到帧事件直接唤醒；取出有限数量帧并保留最新张 | 应保留到帧通知和低延迟取帧，不需要为限帧改回消息轮询 |
| `Renderer.cpp:692,777`、`AdaptivePresenter.cpp:211` | 取处理结果、绘制前端，然后直接 `Present(0, 0)` | 普通路径没有统一的 80 FPS 输出截止时间 |
| `FramePresentationTiming.h:10` | 纯叠加层按显示器刷新周期合并，新内容和关键动作可以绕过 | r9 已减少重复工作，但不等于全部输出限制为用户指定的 80 FPS |
| `PresenterBase.cpp:30`、`AdaptivePresenter.cpp:75` | 3D 模式 4 个缓冲，其他模式 8 个；普通最大帧延迟设为缓冲数减 1，DLSSFG 单独设为 1 | 普通路径允许的排队上限仍较宽；缓冲数量和实际积压数量不能画等号 |
| `Renderer.cpp:2582` | 普通发布不等待 DLSSFG 专用槽可用事件，后端仍可持续生成更新 | 单独限制前端 Present 不能保证停止多余 NR／OF 工作 |
| `Renderer.cpp:1054` | DLSSFG 已有逐输出帧截止时间与 FIFO，但到期后才开始前端绘制 | 已具备部分基础；仍不同于在实际提交边界精确对齐 |
| `XeSSFGPresenter.cpp:499,662,855` | XeLL 低延迟开启；BeginFrame 调用 xellSleep；通过代理交换链呈现 | 应由 XeLL／XeSS 接管对应的限帧与补帧节奏 |

主要源码入口：[StepTimer](<../../../src/Magpie.Core/StepTimer.cpp#L38>)、[Renderer 前端](<../../../src/Magpie.Core/Renderer.cpp#L692>)、[普通 Present](<../../../src/Magpie.Core/AdaptivePresenter.cpp#L211>)、[XeSSFG](<../../../src/Magpie.Core/XeSSFGPresenter.cpp#L499>)。

近期日志记录纯叠加层目标周期为 3.333 ms，意味着程序当时按约 300 Hz 调度叠加层。它不是实测提交率，但说明即使内容约 80 FPS，r9 的现有叠加层规则仍允许更频繁重绘。RTSS 在 Magpie 的 DXGI Present 外层限帧，可能进一步压低这部分工作；本次没有同步 trace，不能量化收益。

### 与 FreeSync 相关的明确检查点

普通 AdaptivePresenter 创建／调整交换链时设置 `DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING`，但实际提交使用 `Present(0, 0)`；XeSSFG 路径则在支持时传入 `DXGI_PRESENT_ALLOW_TEARING`。当前默认 `UseCompSwapchain=false`，普通路径选用 AdaptivePresenter。

微软对窗口及无边框窗口的显式 VRR 路径要求创建和 Present 两端满足条件，建议支持时在 `SyncInterval=0` 的 Present 中传入对应 flag。[微软 VRR 指南](https://learn.microsoft.com/en-us/windows/win32/direct3ddxgi/variable-refresh-rate-displays)、[Present flag 条件](https://learn.microsoft.com/en-us/windows/win32/direct3ddxgi/dxgi-present)

**因此存在一个具体的显式 VRR 接入缺口，值得独立修正和验证。** 但驱动设置、实际呈现模式、显示连接和窗口状态仍会影响 VRR；补上 flag 不等于保证 FreeSync 一定生效。允许 tearing 也不等于检测到了 VRR 显示器，在 VRR 未生效时可能引入撕裂，实施时需保留合适的呈现选择。

Front Edge Sync 也不能消除固定刷新屏的所有节奏量化。例如若实际固定为 300 Hz，80 FPS 对应每帧 3.75 次刷新，完整画面不可能总是保持整数个且相同的刷新周期。应区分长卡顿减少、提交均匀和最终扫描输出均匀这三个目标。

## 建议的实现路线

### 第一阶段：普通输出的稳定呈现模式

复用现有帧率配置和呈现结构，提供可选的稳定呈现模式，首先覆盖无 FG 的 NR／SR。它属于运行时调度，不应做成一个占效果组位置的新效果器。

1. **一个输出时钟。** 以 80 FPS 为例维护 12.5 ms 的目标周期。普通内容和同交换链的 UI／光标共用输出预算，在目标时刻附近合并绘制；输入消息即时处理，视觉更新进入下一次计划呈现。窗口移动／缩放等系统交互可临时退出稳定节奏，恢复后重新定相。
2. **在提交边界控制，而非到点才开始全部绘制。** 将前端准备与提交分离，尽量提前准备，截止时间控制实际 Present 的开始。若最后的合成耗时不可忽略，需要测量并预留余量。不能仅在 `_FrontendRender` 前睡眠就宣称完成了 Front Edge 对齐。
3. **有界待呈现内容与后端开工控制。** 优先复用已有处理结果和前端基础纹理；控制待提交内容的积压，并据输出计划决定是否接受下一张捕获输入进入 NR。普通画面只保留有用的新内容，避免先计算大量结果再全部丢弃。队列试验可比较最大帧延迟 1 与原值，不能仅因缓冲多就认定它是卡顿根因。
4. **避免两个独立的同频门槛。** 输入处理上限和输出目标仍可有不同语义，但应共享节奏信息；不要让输入恰好错过一次 80 FPS 取帧门槛后，再错过一次独立的 80 FPS 呈现门槛。后续可以利用捕获时间戳与处理耗时估计输出相位，先采用有界、可测量的方案，避免一开始引入复杂预测器。
5. **等待期间响应消息。** 使用已有的高精度定时器／交换链容量事件／线程消息；不要在持有共享纹理锁时等待整个帧周期，不要在窗口线程内增加不可中断的长 Sleep。若需要短尾等待，应测量其精度收益和 CPU 成本，不能默认整周期忙等。
6. **迟到不无限追赶。** 小幅迟到可有限修正相位；长时间无输入、切屏、重建后重新定相，不积攒错过的时隙后连续补交。RTSS 帮助明确允许短暂超限；Magpie 是否采用同等追赶幅度，需要结合 GPU 竞争验证，不能机械照搬。
7. **保持历史正确。** 普通无新帧不重复送入 DLSSNR 来凑 80 FPS，不修改已有的有效帧／历史重置规则；旧内容的 UI 重绘不计作新内容帧。

DXGI 的 frame-latency waitable object 可以提供队列进度／容量同步，但它本身不是用户指定 80 FPS 的高精度节拍器；应与输出截止时间配合使用。[微软接口说明](https://learn.microsoft.com/en-us/windows/win32/api/dxgi1_3/nf-dxgi1_3-idxgiswapchain2-getframelatencywaitableobject)

**GPU 调度优先级继续保持 REALTIME，不纳入此方案的调整范围。**

### 第二阶段：分别接入 FG 和其他呈现器

- **DLSSFG：** 保留当前生成帧／真实帧顺序和槽生命周期，扩展现有输出时钟，不能照普通路径直接合并或丢弃任意一张补帧。基础 80 FPS、2 倍 FG 的名义输出为 160 FPS，不能把应用输出仍锁 80 后误认为只是限了基础帧。
- **XeSSFG：** 优先将目标交给 XeLL 的帧率限制器。当前初始化只启用低延迟，未传入限帧目标。Intel 文档说明 XeLL 能结合 FG 计算基础帧率，并不推荐叠加另一个限帧器；若应用仍需自行等待，应放在 xellSleep 之前。不能在代理 Present 前后随意再套一层等待。[Intel XeLL 官方指南](https://github.com/intel/xess/blob/main/doc/xell_developer_guide_english.md)
- **Windows 11 Composition Swapchain：** 可进一步研究 `IPresentationManager::SetTargetTime`，让系统按目标显示时刻处理提交。当前该呈现器没有设置目标时间，而且是编译开关分支，不是本次默认路径。其时钟类型为 SystemInterruptTime，必须正确换算，不能直接塞入 QPC tick；此接口也不保证精确到屏幕扫描瞬间。[微软目标时间接口](https://learn.microsoft.com/en-us/windows/win32/api/presentation/nf-presentation-ipresentationmanager-settargettime)

### 游戏端的限帧边界

Magpie 自己可以管理捕获、后端处理和自身呈现，但在现有 WGC 架构下不能直接限制任意游戏的渲染循环。因此第一阶段仍由游戏自己的限帧或 RTSS 控制源程序，Magpie 内置自己的稳定输出。

如果以后希望在 Magpie 中一个入口同时设置两侧，可以做可选 RTSS 联动。本机官方 SDK 示例已有 `LoadProfile / GetProfileProperty / SetProfileProperty / SaveProfile / UpdateProfiles` 和 `FramerateLimit` 属性。同步模式属性的公开支持范围仍需核实，不能直接假设所有磁盘键都支持 SDK 设置。[RTSS SDK 示例](<C:/Program Files (x86)/RivaTuner Statistics Server/SDK/Samples/SharedMemory/RTSSSharedMemorySample/RTSSProfileInterface.h>)

这种联动属于配置管理，不能称为跨进程逐帧同步。应只管理选定的应用配置，保存并恢复自己改动的字段，避免覆盖用户中途修改；Magpie 自身用内置限帧时也应避免外层 RTSS 再限一次。自行注入游戏以控制 Present 是另一个范围更大的实现，不是加入 Magpie 呈现时钟的必要条件。

## 如何验证实际收益

保留 80 FPS、同一效果参数、同一场景和显示条件，先由用户分别比较：只限游戏、只限 Magpie、双方 Front Edge、游戏限帧加 Magpie 内置稳定输出。再在双方目标不变时比较 Async 与 Front Edge，区分限低帧率本身和限帧位置的收益。VRR 修正、队列深度调整应作为独立变量比较，避免一口气混在同一结果里。

需要同时看：新内容间隔、全部 Present 间隔、实际显示时间、源帧年龄／延迟，以及两进程的 GPU 等待。尤其不能让 UI 重绘刷高的 Present 频率替代新画面频率。PresentMon 可以按进程和交换链记录提交及显示指标，但可用字段取决于路径和版本；其 CPU 开始时间也不等于游戏真实输入采样时间。[PresentMon 官方字段说明](https://github.com/GameTechDev/PresentMon/blob/main/README-ConsoleApplication.md)

当前没有这轮 RTSS 开关前后的成对逐帧记录，因此本报告确认了机制和实现缺口，尚未给出实测增益比例。建议先实现普通输出节奏控制并单独处理显式 VRR 缺口，再按上述口径判断 1% Low、体感和延迟是否共同改善。
