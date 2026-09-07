# r10：VRR 与呈现延迟复查

> 后续状态：用户决定暂缓 VRR。当前 r10 local 已隐藏并停用 VRR，撤回全屏焦点实验；容量等待优化与 Front Edge Sync 保留。下文记录此前实验与调查，当前使用说明见 ../../FRAME_SYNC_GUIDE.md。

## 当前结论

**现有 VRR 开关确实把请求传到了 DXGI，未发现漏传交换链标志这一类可以直接修好的错误。** 日志没有记录真实扫描输出模式，不能把「支持 tearing」或「Present 成功」当成显示器已经启用 VRR。

用户复现条件为：全屏效果组、目标 100 FPS、无 FG，其他程序可以触发 VRR，Magpie 未触发，以显示器实时刷新率为判断依据。

最值得验证的原因是**呈现窗口没有前台焦点**。Magpie 故意让源程序保持前台，输出窗口使用 WS_EX_NOACTIVATE，而且发现输出窗口意外获取焦点后会立即把焦点还给源程序。NVIDIA 开发者论坛有针对非焦点全屏覆盖窗口的直接复现报告，现象与此相符；这仍是候选原因，并非在本机通过受控对照证实的结论。[NVIDIA 论坛原始报告](https://forums.developer.nvidia.com/t/fix-vrr-for-overlays-always-on-top-windows/296168)

用户随后要求尝试全屏强制焦点，因此保留 VRR。按已告知的建议范围，**仅在 VRR 开启＋全屏效果组时尝试激活输出窗口**，窗口效果组与关闭 VRR 时保持原行为。本轮焦点代码已实现，实际显示器变频尚未验证。

## 本地证据

快照保存在工作区 .tools/r10-vrr-review/logs-before/。两份日志覆盖不同设置的多次运行，不能混为一个固定 100 FPS 场景。

- 17:45:53 的会话明确记录 VRR request: enabled=true tearingSupported=true path=DXGI。
- 同一会话记录 Front Edge Sync 实际启用，基础目标 100 FPS。
- 普通呈现为 DXGI FLIP_DISCARD；创建和 ResizeBuffers 均带 ALLOW_TEARING，开启 VRR 时 Present 使用 SyncInterval=0 和 ALLOW_TEARING。
- 此前会话记录输出尺寸 2560×1440，NVIDIA RTX 5070 Ti，输出显示器刷新率约 300 Hz。
- 交换链最大排队帧数为 1，尽管仍分配 4 或 8 个缓冲；缓冲分配数不等于实际排队帧数。
- 日志原有「可变刷新率支持」实际来自 DXGI_FEATURE_PRESENT_ALLOW_TEARING 查询。本轮改成准确的能力名称，避免误读为激活状态。

上述 API 组合满足微软给出的 VRR 请求要求，但 DirectFlip／Independent Flip 的实际采用仍受窗口与合成路径影响。可用 PresentMon 的 PresentMode 进一步区分；该字段也不能单独证明显示器物理变频已生效。[微软 VRR 要求](https://learn.microsoft.com/en-us/windows/win32/direct3ddxgi/variable-refresh-rate-displays)、[微软 Flip Model 指南](https://learn.microsoft.com/en-us/windows/win32/direct3ddxgi/for-best-performance--use-dxgi-flip-model)

## 强制焦点为什么不能只加一行

至少需要协调以下已有逻辑：

1. 全屏输出窗口的 NOACTIVATE 样式及 WM_MOUSEACTIVATE。
2. _UpdateSrcState 中自动还原源窗口焦点的逻辑。
3. 点击黑边／工具栏时激活源窗口的逻辑。
4. 3D 游戏模式下将其他前台窗口视为遮挡、停止效果组的检查。
5. 窗口置顶、鼠标捕获、Alt+Tab 与关闭效果组时的焦点恢复。

更关键的是，Windows 会把键盘输入交给获得焦点的窗口。输出窗口取得焦点后，部分游戏会失焦暂停，或不再处理键盘、Raw Input、DirectInput；转发普通 WM_KEYDOWN 不能普遍解决这些问题。跨进程 AttachThreadInput 也不是无代价修复，会把原本异步的窗口操作变成相互依赖，可能造成等待甚至挂起。[SetForegroundWindow 语义](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-setforegroundwindow)、[微软对输入队列合并的说明](https://devblogs.microsoft.com/oldnewthing/20130607-00/?p=4143)

本轮实现为可以通过关闭 VRR 并重新启用效果组退回的验证路径：

- VRR 全屏窗口去掉 NOACTIVATE，允许鼠标激活；进入全屏时，在源窗口仍为前台的条件下请求输出前台焦点。
- 输出获得焦点时，将其视为当前效果组会话仍在前台，以维持置顶、鼠标与 3D 游戏模式判断；这不表示源程序仍收到 Windows 前台输入。
- 切到其他程序时不抢焦点，切回源窗口后允许再次尝试；每次前台访问只自动尝试一次，拒绝或源程序夺回焦点后不逐帧重试。瞬间没有前台窗口不重置次数。
- 禁用源窗口／源窗口有启用的弹窗时不尝试；点击输出工具栏或黑边由系统正常激活输出，不再马上激活源窗口。
- 结束时仅当输出窗口仍在前台才尝试恢复源窗口焦点；保留原有跨线程输入队列分离，不加入输入转发或模拟按键绕过前台锁。
- 日志记录 Fullscreen VRR focus attempt 的请求返回值及实际前台状态，不将其命名为 VRR 激活结果。

源程序可能因失焦暂停、降后台帧率或丢失键盘输入。点击穿透后的源窗口也可能让焦点回到源程序，本轮不与它持续争抢。若实机验证确实使显示器变频，再决定是否投入输入兼容工作。不能在未验证的情况下宣称完整 VRR 修复。

## 延迟：本轮已经实现的有限优化

原来 BeginFrame 发现交换链尚无容量后，外层消息循环只等待消息或下一次轮询，通常约 1 ms。即使 DXGI 容量事件在中途到达，也不会直接唤醒这次等待。

本轮改为：

- 在外层消息循环同时等待交换链容量事件和输入／控制消息，事件到达就继续处理。
- 仅在 BeginFrame 已经报告容量不足时等待该事件，保留原有有限超时。
- 记录已经消费的容量令牌，下一次 BeginFrame 直接使用，防止自动复位事件被等待两次。
- 普通 DXGI 与 XeSS 采用相同的令牌状态逻辑；Front Edge Sync 截止时间优先，XeLL 的 xellSleep 和生成帧输出调度保留。
- DXGI ResizeBuffers 复用同一交换链的有效令牌；XeSS 重建等待对象时清除旧令牌。
- 从调整窗口大小的 DirectComposition 路径切回交换链时，若调整交换链失败，保留当前 DirectComposition 路径。

它削减的是容量已可用到下次轮询之间的额外等待，**不是保证端到端固定减少 1 ms**。容量一直充足时没有这项收益；操作系统调度、FES 截止时间和 GPU 工作仍影响实际输出。

## r9 的内容是否还需要

| 内容 | 判断 | 原因 |
| --- | --- | --- |
| WGC 自由线程新帧通知，有限取帧并保留最新帧 | 保留 | 减少交付延迟，防止旧帧积压 |
| 捕获中断、尺寸／时间戳校验、恢复后的历史重置 | 保留 | 针对切屏后 NR 历史污染；正常稳定帧不需要反复重置 |
| 工具栏／光标合并更新，内容优先 | 保留 | 避免重复呈现旧背景并争抢 GPU；FES 关闭及 FG 路径仍需要 |
| 有限消息处理、渲染退出窗口过程后执行 | 保留 | 避免拖动性能分析器时把后续输入堵在呈现等待后 |
| DLSSFG 有界输出队列及对应帧资源 | 保留 | 维持生成帧与真实帧次序；无 FG 时不运行 |
| 后端发布后的 GPU fence | 本轮保留 | 确保共享图像已可消费、DLSSFG 源纹理可安全复用；不能把 GPU 执行时间视为可直接删掉的空等待 |
| 交换链容量不足后的纯轮询 | 本轮改进 | 直接使用已经存在的容量事件唤醒 |
| GPU REALTIME 优先级 | 保留 | 用户明确要求保持 |

两份日志中有 509 个发布计时窗口，每个窗口标注 120 次采样；窗口均值的平均约 4.774 ms，记录到的最大等待 23.687 ms。它涵盖多个设置和运行阶段，且 fence 在当前 GPU 工作后触发，**不能推断删掉 fence 就能减少这些时长**。

更大幅度降低 FES 下图像年龄，可以研究在目标提交前按处理预算延后取最新捕获帧。当前在上一帧提交后就开始处理下一帧，提前处理完的图像可能等到下一次截止时间。修改这个调度需要验证 GPU 忙时的截止时间、源帧交付波动及 FG 输入连续性，超出本轮有限优化范围。

## 验证状态

容量令牌、自动复位信号、消息中断、超时与重置的 24 项 CPU 检查已通过。另有 22 项焦点策略检查通过：直接编译本轮生产方法，使用模拟的 Win32 调用覆盖拒绝、源程序夺回前台、切走／切回、瞬间 NULL、禁用／弹窗、窗口／VRR 开关范围与激活竞争。这些检查不调用真实前台激活 API，不验证 Windows 或游戏实际输入行为。

Release x64 编译通过，0 警告、0 错误；已覆盖部署 release/v0.6.5-r10-local/Magpie-v0.6.5-r10-local-x64。228 个包内文件、161 个效果器及 ZIP 所有条目的清单／哈希校验通过。旧部署完整备份在 .tools/r10-vrr-review/previous-deployment/，本地配置、缓存和日志保留，未加入分发 ZIP。

编译、源代码审计及包体校验记录保存在 .tools/r10-vrr-review/。没有运行 Magpie、没有使用 computer-use，也没有做 GUI／GPU 或显示器扫描输出测试。

VRR 焦点实验不能归类为纯文字小更新；需要确认显示器变频、源程序是否暂停、键鼠输入、Alt+Tab、停止效果组后焦点恢复。不能用编译通过替代这些检查。
