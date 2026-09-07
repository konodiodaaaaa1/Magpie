# Magpie 0.6.5 r9 local

落实 [r9 调查报告](experimental/reviews/20260905-v0.6.5-r9-dlssnr-switching-stutter-REVIEW.md) 中的 A、B、C、D，保留此前 r8 功能与默认参数。

- **A：捕获输入有效性。** Graphics Capture 同时检查每帧内容尺寸、实际纹理尺寸和裁剪框；拒绝不完整区域及未前进的捕获时间戳。恢复期间保留最后有效画面，暂停新帧编号与效果推理。恢复首帧即使像素相同也送达时序消费者。
- **B：捕获恢复和错误。** 重启、取帧、关闭异常统一清理，并进入现有捕获错误详情。启动或明确中断后等待有效帧最多约 5 秒；窗口尺寸变化继续使用现有自动重建流程。正常静止画面没有这个超时。普通恢复不弹提示。
- **C：恢复后的呈现节奏。** 捕获重启、无效帧恢复、捕获时间戳长间隔会开启新的捕获序列；首个有效帧同步请求光流和 FG 历史重置，清除旧队列等待，重建帧间隔基线。恢复期间沿用最后健康间隔，初次启动使用 60 FPS 的源帧回退并遵守限帧，避免无间隔突发呈现。仅有焦点变化不会清空帧间隔估计。
- **D：非阻塞性能采集。** 使用三个固定查询槽位，未完成结果留待后续读取；槽位忙时跳过采样。查询创建失败、读取失败、超时或无效时间戳停止本次 GPU 耗时采集并记录原因，效果处理继续。界面锁内仅发布完整结果，不等待 GPU。
- 增加捕获序列、会话重启、拒绝帧、恢复首帧及 CPU 读回／纹理发布／fence 等待统计，帮助分析后续日志。

这些修改修复了已确认的代码缺陷；尚未通过实际复现确认日志 18 的异常画面和所有卡顿都已解决。实机测试由用户进行。具体实现与交付记录见 [r9 实施记录](experimental/todos/20260905-v0.6.5-r9-ABCD.md)。

## English

Implements items A–D from the r9 investigation, preserving r8 features and defaults.

- **A: Validate capture input.** Graphics Capture checks content bounds, surface dimensions and the crop, and rejects non-advancing capture timestamps. Recovery retains the last valid image without advancing frame IDs or re-entering temporal effects. The first restored frame reaches consumers even if its pixels are unchanged.
- **B: Handle capture recovery failures.** Start, acquire and close failures clean up partial resources and use existing capture error details. Explicit recovery waits approximately five seconds for valid input. Existing window-geometry handling owns automatic rebuilds; healthy static content has no recovery timeout. Normal recovery shows no notification.
- **C: Restore presentation cadence.** A new capture sequence requests temporal history resets on the first valid frame and discards stale queue-wait accounting and interval samples. The last healthy interval remains a fallback; initial startup uses a 60 FPS source fallback subject to the configured frame-rate cap. Focus changes alone do not reset the cadence estimate.
- **D: Collect GPU timings without blocking.** Three bounded query slots retain unfinished samples for later polling. Busy slots skip sampling. Query failures, timeouts and invalid timestamps disable that profiling session while effects continue. GPU queries run outside the UI timing lock.
- Capture lifecycle and CPU readback/publication/fence timing logs provide evidence for subsequent diagnosis.

These are confirmed code fixes, not confirmation that every reported corruption or stutter has been resolved. Hardware and UI validation remains with the user.

## Capture delivery and presentation scheduling update

- WGC now uses `CreateFreeThreaded` and a session-owned event to wake the backend directly. The backend waits for either a captured frame or control messages, including during startup and minimum-FPS waits. Maximum-FPS limiting remains independent. Callbacks never render or access the capture source, and old-session callbacks cannot signal a replacement session.
- New content has priority over re-presenting an old background for an overlay update. Continuous cursor/overlay updates are coalesced at the destination display refresh rate; button, wheel, cancel and explicit toolbar actions remain immediate. Periodic cursor checks no longer default to 500 Hz on lower-refresh displays.
- Shared-layer content frames satisfy the overlay deadline too. XeSS comparison images and background-dependent cursors still update with new content. DLSSFG content keeps its existing FIFO and pacing.
- GPU scheduling priority remains REALTIME, as requested. The optimization reduces unnecessary presentation work without changing that policy.
- This is the normal `0.6.5-r9-local` Release build with `EnableFrameTrace=false`. The separate r9 debug package remains available as the previous diagnostic baseline.

Release compilation and CPU-only event/deadline, overlay-pacing and FG-cadence checks passed. Whether these changes resolve the reported visible stutter and 1% Low requires user testing; no GPU or GUI validation was performed by the assistant.
