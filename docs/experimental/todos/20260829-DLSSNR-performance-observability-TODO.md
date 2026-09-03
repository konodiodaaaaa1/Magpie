# DLSSNR 性能与可观测性 TODO

> 创建时间：2026-08-29（Asia/Shanghai）
> 分支：`experimental`
> 来源：首次阶段 2–4 GPU 测试反馈
> 状态：P1–P3 与 Signed Snippet 主路径已实现；启用/禁用 Release x64 Core 构建及启用态完整应用重链接通过，待 GPU 运行时验收

## 现象与当前判断

1. 开启实验链后严重卡顿。
2. 未看到 NVIDIA DLSS Indicator，无法仅凭屏幕确认 Feature 18 是否持续 Evaluate。
3. 当前实现按效果名称提前初始化并逐帧运行 Frame Guidance；DLSSNR native backend 即使失败并回退 pass-through，也仍会支付 NVOF/DAV2 成本。
4. `Guidance Mode` 只替换 DLSSNR 消费资源，不会阻止未使用的 Provider 执行，因此现有四组对照不能隔离性能。
5. DAV2 当前每个真实帧执行 GPU→CPU staging readback、同步 ORT Run、CPU 预处理/百分位/源分辨率上采样和 GPU 上传，是首要性能瓶颈。

## 固定原则

- NVIDIA Indicator 只作为外部辅助信号；应用自己的创建结果、Evaluate 结果、运行路径和计数才是验收依据。
- pass-through 或 DLSSNR session disabled 时不得继续运行仅为 DLSSNR 服务的真实 Provider。
- Force Zero 不执行 NVOF/DAV2；Motion Only 不执行 DAV2；Depth Only 不执行 NVOF；Available 才执行两者。
- 诊断 Effect 只请求自身需要的数据，多个消费者取需求并集。
- 优化不能破坏 `frameId` 一致性、Zero 回退、reset 传播和重复帧缓存。

## P0：建立基线与可证伪假设

- [x] 代码审计确认 Frame Guidance 初始化早于 native backend 创建。
- [x] 代码审计确认 pass-through 后 Provider 仍按效果名运行。
- [x] 代码审计确认 Guidance Mode 当前不控制 Provider 生产。
- [x] 列出 DAV2 同步路径：readback、预处理、ORT、百分位、上采样、上传。
- [x] 保存首次运行日志：Core Feature 18 Create 返回 `0xbad0000b`，旧 direct `Init_Ext` 返回 `0xbad00002`，最终 `created=false` 并 pass-through，未进入 Evaluate。

## P1：按真实消费者需求执行 Provider

- [x] 为 native backend 增加只读 `FrameGuidanceRequirements`，默认不请求任何 Guidance。
- [x] DLSSNR 按 Guidance Mode 返回 Zero/Motion/Depth 需求；disabled 后返回空需求。
- [x] 四个诊断 backend 只返回自身所需需求。
- [x] 将 Frame Guidance 初始化移动到 native backend 创建之后；pass-through 不创建真实 Provider。
- [x] Renderer 每个真实帧合并活跃 backend 的需求，并传给 `FrameGuidanceService::BeginFrame`。
- [x] Service 对未请求 Provider 使用 Zero 输出，不调用其 `BeginFrame`。
- [x] resize 后 backend 失效时停止后续真实 Provider 执行。
- [x] 日志记录本次 session 的需求掩码以及 Provider skip 原因。

实现记录（2026-08-29）：真实 Provider 现在由创建成功的 native backend 需求并集决定。Force Zero 只初始化 Zero 资源；Motion Only 不创建 DAV2；Depth Only 不创建 NVOF；Feature 18 创建失败时不初始化 Frame Guidance。Evaluate 或互操作失败会将 DLSSNR 标记 disabled，下一真实帧需求降为空并停止真实 Provider。

验收：Force Zero 的日志中无 NVOF Execute 和 DAV2 Run；Motion Only 无 DAV2 Run；Depth Only 无 NVOF Execute；DLSSNR pass-through 时两者均不运行。

## P2：建立独立于 NVIDIA Indicator 的状态证据

- [x] 创建成功日志固定包含 `Feature=18`、`signed-snippet`/`core-diagnostic`、尺寸和 Guidance Mode。
- [x] Evaluate 日志固定包含 frameId、调用计数、NGX result、成功/失败累计和当前是否 disabled。
- [x] 初始化失败、首次 Evaluate 失败和 fallback 必须输出单一明确的 `DLSSNR STATUS` 行。
- [x] 将状态同时发送到 debugger output，便于 DebugView/VS 捕获。
- [ ] 评估在 Magpie overlay 中增加简短状态行，避免依赖 NVIDIA Indicator。

实现记录（2026-08-29）：`DLSSNR STATUS` 是当前内部真值。创建、前 8 次 Evaluate、每 120 次 Evaluate、NGX 失败和互操作失败均记录并发送到 debugger；NVIDIA Indicator 缺失不再阻断触发判断。

验收：仅凭 `magpie.log` 能区分“没有创建”“持续 Evaluate”“失败后 pass-through”三种状态。

## P2.1：Signed Snippet 生产主路径

- [x] 默认路径不再预先调用 Core `CreateFeature(18)`，也不再以 Core Feature 18 的失败作为 Snippet 前置条件。
- [x] 从应用目录加载当前 `nvngx_dlssnr.dll`，解析 `Init_Ext`、`CreateFeature`、`EvaluateFeature`、`ReleaseFeature` 与 `Shutdown1`。
- [x] 只修改该 DLL 自身的 `GetModuleFileNameW` IAT；仅对 Magpie 调用方模块报告 `nvngx.dll`，其他调用转发原函数。
- [x] IAT 兼容在 `Init_Ext` 前安装，并一直保留到 Snippet `Shutdown1` 返回后才恢复。
- [x] Snippet `Init_Ext` 固定使用 Application ID `0x0876232C`、应用目录和 `nullptr` 参数块。
- [x] Core Feature API 只承担独立参数块的 `AllocateParameters`/`DestroyParameters`；独立 Magpie 进程仍执行必要的 Core Init/Shutdown 参数系统引导，但不查询 capability、不创建或 Evaluate Feature 18。
- [x] 创建参数补齐一比一输入/输出尺寸、`Upscaling=0`、`Scale=1`、`ScalingRatio=1` 与 scaling-ratio callback，并保留 preset、quality 和 node mask。
- [x] Create/Evaluate/Release 全部走同一个 Signed Snippet 导出集合；Core Feature 18 仅保留为编译期默认关闭的诊断对照。
- [x] Init、Allocate、参数写入、Create、Evaluate、Release、Destroy 和 Shutdown 全部增加 SEH 边界；异常时 fail-closed。
- [x] 销毁顺序固定为 GPU drain → ReleaseFeature → DestroyParameters → Snippet Shutdown1 → 恢复 IAT → FreeLibrary；之后才结束参数 Core。
- [x] `EnableDLSSNR=true` 与 `EnableDLSSNR=false` 的 Release x64 `Magpie.Core` 构建通过。
- [x] 退出旧 Magpie 进程并完成启用态 Release x64 `Magpie.exe` 重链接。
- [ ] GPU 验证日志首先出现 `created=true path=signed-snippet`，随后前 8 帧 `result=0x1`、`failures=0` 且 Evaluate 计数持续增长。

实现记录（2026-08-29）：旧路径的问题不在 Guidance，而在 Signed Snippet 合同不完整：错误 AppID、错误数据目录、没有调用方 IAT 兼容，并且把 Core Feature 18 当作前置探测。本阶段已按已验证实现重建生命周期。NVIDIA Indicator 仍只作旁证；`DLSSNR STATUS` 是主要验收信号。

## P2.2：DLSSNR 时序历史生命周期修复

- [x] Frame Guidance 初始化阶段只分配资源，不再用捕获启动前内容未定义的输出纹理制造 `frameId=0`；第一张真实捕获帧负责建立 NVOF 历史并传播 Initialize reset。
- [x] resize 后使用最后一张真实捕获帧重新产出 guidance，不再构造无 Color 的伪帧。
- [x] 源窗口失焦/回焦、WGC 捕获重启和至少 500 ms 的真实捕获停顿会调用 `ResetHistory`，使下一张真实帧重建 NVOF/DLSSNR 历史。
- [x] DLSSNR 对相同 `frameId` 复用已有输出，最低帧率强制呈现不再重复执行 Evaluate 或推进时序状态。
- [x] Release x64 全解决方案构建通过。
- [ ] 在目标 NVIDIA GPU 上完成 PotPlayer 全屏启动、切出/切回、暂停/恢复和游戏切屏回归；核对 reset/reuse 日志与主观画质。

实现记录（2026-09-03）：旧日志中的 `evaluateCount > frameId` 及首帧 NVOF 历史污染已有代码级修复。`Force Zero` 是无运动矢量的诊断模式，运动视频仍可能表现出 DLSSNR 自身的时序拖影；视频回归以 Available/Motion Only 为主，同时保留 Force Zero 作为隔离对照。

## P3：DAV2 短期降载

- [x] DLSSNR 增加 `Depth Inference Interval`，默认每 4 个真实帧推理一次，可设 1–8。
- [x] 非推理帧使用 NVOF 将上一帧过滤深度重投影到当前帧，不执行 readback、ORT、百分位或源分辨率 CPU 上采样。
- [x] Depth Only 无 NVOF 时采用明确的 hold-last-depth 行为并记录一次提示。
- [x] 首帧、resize、切场、后端切换和长暂停强制立即推理并 reset。
- [x] raw depth 仅在真实推理帧有效；诊断视图不得把旧 raw depth 标记成当前 frameId。
- [x] 分开记录 DAV2 preprocess、ORT Run、postprocess/upload 和 skip/reproject 次数。

实现记录（2026-08-29）：默认 interval=4。推理帧保留同步 readback/ORT/postprocess；其余三帧只运行 GPU temporal shader。可用真实 NVOF 时按当前→上一帧矢量重投影，低置信度/越界区域回退同位置历史；Depth Only 使用 hold-last。该方案显著降低平均成本，但同步推理帧仍可能产生周期性尖峰，P4 异步/GPU 原生路径仍是彻底解决方案。

验收：默认模式下同步 DAV2 重负载频率不超过捕获帧率的 1/4，非推理帧无 staging `Map` 和 ORT `Run`。

## P4：DAV2 中期异步与 GPU 原生路径

- [ ] 使用 staging ring/query 或 GPU 原生输入避免在 Renderer immediate context 上立即阻塞 `Map`。
- [ ] 将 ORT Run 移到专用 worker，并用有界单槽/双槽队列丢弃过期任务。
- [ ] 使用 ORT I/O Binding、DirectML/D3D12 buffer 或 CUDA interop，减少 CPU tensor 往返。
- [ ] 将 resize、归一化、深度上采样与 temporal filter 保持在 GPU。
- [ ] 用 GPU histogram/分位数近似替代每帧 CPU `nth_element`。
- [ ] 为 D3D11/D3D12 DLSSNR 互操作增加至少双缓冲 allocator/resource，减少逐帧等待。

验收：Renderer 线程不等待 ORT 推理；异步结果过期时丢弃而不是阻塞；P95 frame time 不出现固定周期尖峰。

## P5：性能与画质验收

- [ ] 按测试矩阵完成 Zero、Motion Only、Depth Only、Available 四组。
- [ ] 记录平均、P95、P99 frame time，而不只记录 FPS。
- [ ] 分别记录 NVOF、DAV2 preprocess/ORT/postprocess、DLSSNR Evaluate 的 CPU/GPU 时间。
- [ ] 验证固定水平/垂直平移的方向、幅度和 `MVecScale`。
- [ ] 验证静止、快速转场、前景横穿、resize、暂停恢复和重复帧。
- [ ] 补齐 cuDNN 9 后单独完成 TensorRT 主路径与故障切换测试。

## 暂不在本轮处理

- 改写或强制开启 NVIDIA 驱动全局 Indicator 注册表设置。
- DLSS SR、DLSS FG 消费 Frame Guidance。
- 正式发布和第三方二进制再分发。
