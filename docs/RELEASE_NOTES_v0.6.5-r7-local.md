# Magpie 0.6.5 r7 local

在 r6 local 基础上调整光流共享、补帧队列与性能显示。

**下拉框修复：**实时参数下拉框点击外部或失去焦点时正常收起，不修改未选择的参数值。输入路由记录最后一次实际呈现的弹窗状态，允许外部点击进入 ImGui 的正常关闭流程；焦点取消关闭非模态弹窗。

**r7 fix：**修复拖动性能分析器时 UI 重绘抢占 DLSS 呈现队列，以及队列等待被计入捕获周期、导致后续输出持续慢化的问题。UI 合入下一张待呈现内容帧，周期估计排除下游槽位等待；静态内容仍可重绘。输入密集时继续接收帧通知，日志新增输出间隔和已扣除等待。详见 [日志与实现排查](experimental/reviews/20260905-v0.6.5-r7-fix-dlssfg-profiler-REVIEW.md)。

- 光流整组统一选择一个提供者：优先 NVOF，使用实际 NVOF 申请中的较高档位；其次为实际 AMD OF 申请中的较高档位，再其次为内置光流。不会升至未申请的最高档。每个捕获帧只估算一次，所有启用光流的消费者共享，关闭光流的保持零向量。
- 申请不一致时，非报错提示列出相关效果器和目前统一使用的方法／档位。参数保存值不变，不再提示分别估算或额外资源开销。
- FSR2、FSR3／4、XeSS SR 的内置 OpticalFlow 接入共享池，统一基于捕获原图；仅内置申请时保留原算法，混用时消费优先来源。串联效果可能产生画质差异。
- 修复 XeSSFG 无外部光流时持续重置历史，以及资源复用、帧 ID、帧周期和错误码问题；保留 SDK 内部呈现调度。
- DLSSFG 每次检查 SDK 禁止插值标记；修复旧队列消息干扰、重置帧发布及同一捕获帧重复生成。无额外限帧效果器时也计算呈现间隔，长卡顿后不突发追赶。
- 存在 FG 时，工具栏和性能页显示总／真实帧率，例如 `120/60 FPS`。依据实际提交／SDK 报告计数，失败或跳过不按倍率虚增，未知总数显示 `—`。这不是显示器扫描输出的实测 FPS；对比开启时仍统计后台管线。

完整消费者清单、SDK 依据、改进说明与验收项目见 [光流与 FG review](experimental/reviews/20260905-v0.6.5-r7-motion-sharing-fg-REVIEW.md)。构建及产物校验见 [实施记录](experimental/todos/20260905-v0.6.5-r7-TODO.md)。

按用户要求，未启动 Magpie，未使用 computer-use；GPU 兼容性、画质、呈现节奏由用户实机验收。HDR 仍留到后续版本。

## English

One optical-flow provider is selected for the whole chain: NVOF first, then AMD, then built-in flow. Quality is the highest actually requested for the selected source, never an unrequested maximum. All enabled consumers share one estimate per captured frame; disabled consumers retain zero motion. A non-error notice names conflicting effects and the selected method/quality. Legacy FSR/XeSS SR joins this shared capture-source path.

XeSS FG history, frame IDs and input lifetime have been corrected. DLSS FG now respects the per-evaluation disable-interpolation flag and uses bounded, paced presentation without replaying stale queue messages. With FG, the overlay displays submitted total/real FPS using actual presentation counts rather than the configured multiplier. These counters do not measure physical display scanout. GPU behavior requires user validation.
