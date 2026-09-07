# Frame Guidance / DLSSNR 测试矩阵

每次测试记录 Magpie commit/工作区、GPU、驱动、分辨率、捕获方式、光流方法/质量和日志路径。截图或 DDS 使用 `日期-场景-method-quality-frameId` 命名。

| 组别 | 方法/质量 | 预期 Provider | 关键检查 |
| --- | --- | --- | --- |
| Zero | None | 无 | 无 OF 执行日志；Motion 与 Depth 均为 Zero；消费者持续成功 |
| NVOF | Performance/Balanced/Quality/High Quality | 4F/4M/4S/2M | 日志档位精确；静止接近零；平移方向和幅度正确；遮挡置信度下降 |
| AMDOF | Performance/Quality | 50%/100% FidelityFX OF | 不按厂商硬限制；奇数尺寸缩放正确；输出为当前到前帧、原图像素单位 |
| Mixed | DLSS=NVOF、XeSSFG=AMDOF | 两提供器各一次 | frameId 一致且消费者不串线；同方法多消费者只执行最高请求档 |

## r4 适配器与档位矩阵

- Ada/Ampere：验证 None、4F、4M、4S、2M 的创建日志及 GPU avg/P95/P99/max。
- Turing 非 TU117：验证 4F/4M/4S；2M 应返回质量不支持，不得静默降档。
- TU117、非 NVIDIA 或 NVOF 不可用环境：选择 NVOF 应给出对应方法错误，不崩溃、不切换到 AMDOF。
- AMD/NVIDIA/Intel：在 D3D12 FL12、SM 6.2、Wave Ops 和 R16G16_SINT UAV 能力满足时分别运行 AMDOF 50%/100%；否则返回 AMDOF 专属能力错误。
- 1080p、1440p、4K 与奇数宽高：覆盖 resize、暂停/恢复、捕获中断、设备重建及连续 120 个真实输入帧。
- XeSSFG x2/Multi-FG：分别覆盖 None、AMDOF 两档、NVOF 四档，验证外部 motion 与基础帧 ID/尺寸/区域/同步严格一致。

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
- resize：重新分配 guidance 后必须使用最后一张真实捕获帧重建资源，不得生成无 Color 的伪帧；Zero Depth 的尺寸、有效区和同步点必须与消费者一致。
- PotPlayer：分别覆盖“先全屏再缩放”“缩放后切全屏”“暂停/恢复”和快速转场。关闭 Motion 只用于对照；运动视频没有有效运动矢量时仍可能产生时序拖影。

## 每组记录

- DLSSNR STATUS：创建路径、Feature 18、Evaluate result/计数、disabled 状态。
- Frame time：平均、P95、P99。
- Provider：NVOF 运行/跳过次数、CPU/GPU 时间、显存。
- 产物：Color、Motion、Confidence，以及固定 Zero Depth 的尺寸与同步信息。
- 结论：正确、性能不合格、画质不合格或无法判断，并附日志证据。
