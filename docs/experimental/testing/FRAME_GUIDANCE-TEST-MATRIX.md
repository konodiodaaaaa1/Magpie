# Frame Guidance / DLSSNR 测试矩阵

每次测试记录 Magpie commit/工作区、GPU、驱动、分辨率、捕获方式、后端、Depth Interval 和日志路径。截图或 DDS 使用 `日期-场景-guidanceMode-frameId` 命名。

| 组别 | Guidance Mode | 预期 Provider | 关键检查 |
| --- | --- | --- | --- |
| Zero | Force Zero | 无 | 无 NVOF/DAV2 运行日志；DLSSNR Evaluate 持续成功 |
| Motion | Motion Only | NVOF | 静止接近零；水平/垂直方向和幅度正确；遮挡置信度下降 |
| Depth | Depth Only | DAV2 | 近 1 远 0；无 NVOF Execute；非推理帧 hold/reproject 行为明确 |
| Both | Available | NVOF + DAV2 | 深度 residual 稳定；无新增方向性拖尾 |

## 场景

- 静止画面
- 匀速水平平移
- 匀速垂直平移
- 快速转场
- 前景横穿
- 镜面或透明物体
- 固定 UI
- resize
- 暂停恢复
- 重复捕获帧

## DLSSNR 时序历史回归

- 冷启动：第一条 Frame Guidance/DLSSNR 帧记录必须为真实捕获的 `frameId=1`；不得在 `frameId=0` 运行 NVOF 或 DLSSNR Evaluate。
- 切出/切回源窗口：日志应出现 `Frame Guidance history reset: reason=CaptureInterrupted`，恢复后的第一张真实捕获帧不得继承切屏前历史。
- 捕获停顿：真实捕获间隔达到 500 ms 后，日志应出现 `reason=LongPause`，恢复帧重新建立历史。
- 重复捕获帧：允许出现 `DLSSNR duplicate capture reused`；同一 `frameId` 只能执行一次 Evaluate，`evaluateCount` 不得因最低帧率强制呈现而领先真实帧数。
- resize：重新分配 guidance 后必须使用最后一张真实捕获帧重建资源，不得生成无 Color 的伪帧。
- PotPlayer：分别覆盖“先全屏再缩放”“缩放后切全屏”“暂停/恢复”和快速转场。`Force Zero` 仅用于诊断；运动视频没有有效运动矢量时仍可能产生时序拖影，不作为视频推荐模式。

## 每组记录

- DLSSNR STATUS：创建路径、Feature 18、Evaluate result/计数、disabled 状态。
- Frame time：平均、P95、P99。
- Provider：运行/跳过次数、CPU/GPU 时间、显存。
- 产物：Color、Depth、Motion、Confidence、DepthResidual。
- 结论：正确、性能不合格、画质不合格或无法判断，并附日志证据。
