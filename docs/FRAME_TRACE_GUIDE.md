# r9 debug 帧时间诊断版

这个版本用于调查 Endfield／视频中可以感受到的卡顿。它保持 r9 的渲染、光流、重复帧检测、兼容处理和效果参数行为，增加 CPU 时间线记录；仍使用 Release 优化构建。诊断记录本身有开销，不代表最终正式版的性能。

## 如何提供一次有效记录

1. 退出其他 Magpie 实例，运行本目录的 `Magpie.exe`。
2. 正常启用效果组，沿用出现问题的设置。无需打开性能分析器，诊断记录会自动开始。
3. 出现明显卡顿后，在方便时尽快停用效果组。每次停用后，会在本目录 `logs/frame-traces/` 导出一份 `trace-日期-时间-PID-序号.csv`。
4. 提供这份 CSV，以及 `logs/magpie.log`、如有则一并提供 `magpie.1.log`。补充源程序、是否移动鼠标／切屏，以及视频自身帧率即可。

导出发生在效果组停止、后端线程退出之后；较大文件可能让停止操作多花一点时间。运行期间不逐帧写入诊断文件。程序崩溃、强制结束进程或断电时，尚未导出的内存记录会丢失。正常停止后才出现 CSV，不表示运行期间没有采集。

## 保留哪些数据

前后端各有独立固定容量环缓冲：262144 条近期事件和 2048 条至少 50 ms 的长事件，合计约 24 MiB。容量按事件数限制，**不是固定保留多少秒**；高帧率和频繁输入会更快覆盖近期记录。两线程保留范围可能不同。

全会话的事件次数、最大值、平均值和至少 50 ms 的次数始终保留。超过容量的旧逐帧记录会被覆盖，CSV 中明确记录覆盖数量。50 ms 是诊断筛选阈值，不是对所有视频／源程序都适用的卡顿判据。

记录包括：

- WGC 原始时间戳、拒绝帧、关闭和重新启动捕获的完整调用耗时。
- 捕获返回状态、像素重复检查结果及读回等待。
- 后端消息处理、光流提交、效果器调用、参考图准备和复制、发布事务及 fence 等待。
- 前端消息处理、窗口状态／鼠标更新、源窗口响应探测、异步命中测试从请求到回调的时间。
- 共享槽获取失败、交换链容量不足、绘制、`Present` 调用时长和相邻成功调用的间隔。

这些是 CPU 墙钟时间。GPU 命令提交调用很短，并不能证明对应 GPU 工作也很短；嵌套事件的耗时相互包含，不能把它们相加。

## 离线分析

无需在游戏运行时启动分析工具。安装有 Python 3.9 或更高版本时，在诊断版目录运行：

```powershell
python .\Diagnostics\Analyze-FrameTrace.py .\logs\frame-traces\trace-实际文件名.csv
```

脚本仅使用 Python 标准库，会在 CSV 旁生成 `.analysis.md` 和 `.analysis.json`。也可以直接提供原始 CSV，由开发侧分析。

报告包括全会话阶段统计、保留片段的提交间隔分布、最长 20 个提交间隔以及重叠阶段和重复帧计数。长事件缓冲中的副本不会重复计入近期分布。不完整导出、记录数量不匹配等情况会拒绝生成报告。

**报告中的提交间隔 Low1 不是屏幕实测 FPS，也不能直接等同于 NVIDIA overlay。** 它仅以保留片段中最慢 1% 提交间隔的平均值换算，用来比较本诊断版的提交稳定性。遮挡／失败会断开提交间隔序列；第一帧之前的初始化不计入。XeSSFG 的代理交换链调用也不代表其全部生成帧的实际显示频率。

## 同一诊断版关闭采集作对照

如果需要排除新增记录的影响，完全退出 Magpie 后，通过下面的 PowerShell 命令启动。环境变量只影响这个终端随后启动的进程，不修改配置文件：

```powershell
$env:MAGPIE_FRAME_TRACE = '0'
Start-Process -FilePath .\Magpie.exe -WindowStyle Hidden
Remove-Item Env:MAGPIE_FRAME_TRACE
```

这样关闭的是本次新增的 FrameTrace，r9 原有 NR／NVOF 遥测仍保持原行为。普通启动默认采集；正式构建的 `EnableFrameTrace` 默认关闭。

## CSV 字段与关联方法

`#clock` 记录 QPC 频率、会话 QPC 起点和开始附近的 UTC FILETIME，便于与普通日志对齐；`start_us`、`duration_us` 使用同一个 QPC 时钟，单位微秒。`recent` 是近期环，`slow` 是独立长事件环。`#stat` 为全会话累计统计；`#complete,1` 表示写出尾标。

| 事件 | a | b |
| --- | --- | --- |
| WgcFrame | 原始捕获时间戳，100 ns | 捕获序列编号 |
| WgcNotificationWait | 自上次交换计数以来的通知次数 | 0；时长为待消费通知到取帧调用的等待，不与单张帧一一对应 |
| WgcDequeue | 此次实际取出的帧数，含跳过的旧帧 | 捕获会话代数 |
| CaptureWake | Windows 等待结果 | 等待对象位标记：1 到帧事件，2 定时器，3 两者；始终同时监听线程消息 |
| OverlayDeferred | 0 | 0；仅叠加层重绘等待显示周期 |
| WgcRejected | 1=内容范围无效，2=时间戳未前进 | 原始时间戳 |
| CaptureResult | 0=新帧，1=等待，2=错误 | 最近接受的 WGC 时间戳；等待时可能仍是旧值 |
| CaptureAccepted | 本次捕获时间戳 | 捕获序列编号 |
| DuplicateCheck | 1=像素相同，0=不同／检查失败后的默认结果 | Map HRESULT |
| DuplicateReadback | Map HRESULT | 1=像素相同 |
| NativeEffect | 效果器在组内的索引，从 0 开始 | 0 |
| NvofSubmit | 请求的 NVOF 档位枚举 | 0 |
| FrontendMessage | Windows 消息编号 | 0 |
| HitTestRequest／HitTestComplete | 请求编号 | 完成时为命中区域；耗时含线程池和回调排队 |
| FrontendAcquireBusy | 0=CPU 槽锁忙，1=GPU 纹理组获取失败 | CPU 锁忙时为槽号，否则为 HRESULT |
| Present | HRESULT；仅 0 为 S_OK | 交换链标识 |
| PresentGap | 0 | 交换链标识；区间从上次成功调用入口到本次入口 |
| ContentSubmit／OverlaySubmit | 是否生成帧 | 提交计数，-1=呈现器无法提供；不是实测显示计数 |
| RenderDecision | 位标记：1 强制、2 后端新版本、4 待呈现内容、8 对比启用 | 0 |

捕获检查阶段的 `frame_id` 是下一个候选编号：重复／等待时可以反复出现同一编号；以 `CaptureAccepted` 为实际接受点。前端获取新底图后改用其捕获编号，未获取前的状态检查可能仍带上一帧编号。普通 CSV 不包含截图或源程序画面。
