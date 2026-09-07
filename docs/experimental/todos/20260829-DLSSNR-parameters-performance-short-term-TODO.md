# DLSSNR 参数与性能短期 TODO

> 已由 [v0.6.5 r1 TODO](20260904-v0.6.5-r1-TODO.md) 取代。以下内容保留为历史实施记录，未完成项不代表当前路线。

> 创建时间：2026-08-29（Asia/Shanghai）
> 分支：`experimental`
> 状态：P0–P3、P5、P6 已完成并经用户实测暂未发现问题；P7 的 DLSS SR/DLSS FG 接线已实现；P8 的 v0.5.7 参数实现和 Release x64 构建已完成，等待实机运行验收
> 目标：先修正可见参数合同与 Indicator 位置，再隔离并消除主要帧时间尖峰。

## 当前结论

1. Signed Snippet 主路径已经实际工作。最新日志中 `created=true path=signed-snippet`，连续 Evaluate 返回 `result=0x1`，`failures=0`；性能差不能直接归因于 DLSSNR 未触发。
2. 异步化后的 2560×1440 实测表明，首要瓶颈已经转为 NGX Evaluate GPU：两组样本平均约 13.4–15.6 ms，P95 约 19.7–20.0 ms，P99 约 27.6–29.2 ms；这会吃满 60 FPS 的单帧预算。
3. CPU bridge 已不再是瓶颈：四槽 `slotWait` P99 约 0.001 ms，Evaluate CPU 稳态约 0.36–0.40 ms，submit 约 0.07–0.08 ms。DirectML ORT 稳态约 13–16 ms，个别达到 41.5 ms，虽不阻塞 Renderer CPU，但会与 NGX 争用 GPU 并放大 P95/P99。
4. `NR Reset` 不具有可预期的性能收益。可用项目把它作为创建、尺寸变化和历史不连续后的内部一次性标志，而不是画质风格参数；短期不提供用户控制，也不把它当作性能优化手段。
5. v0.5.7 以 SHA256 `E1C28FDE0922B12FC10734E58C3D24A36808E575247F4FD4F36226540D7EE023` 的新 `renodx-dlss5.addon64` 为准，覆盖 0.5.6 的旧参数决定：
   - `NR Preset`：`0 Default`、`1 Preset #1`、`2 Preset #2`、`3 Preset #3`。
   - `NR Style`：`0 Default`、`1 Natural`、`2 Cinematic`。
   - `NR Intensity`、`Local Tone Strength`、`Local Structure Strength`：`0.0–2.0`，默认 `1.0`。
   - `Skin Structure Strength`：`-1.0–2.0`，默认 `-1.0`。
   - `Automatic Mask`、`NR UI Correction`：默认关闭。
6. NVIDIA Indicator 在 Magpie 最终输出中的 Y 方向与参考项目的显示结果不同。以实机观察为准：左下角应使用 `X=0, Y=0`，不能继续照搬参考项目的 `Y=1`。
7. 下一阶段不再优先压缩 bridge CPU 开销。当前实现用 NGX GPU 最新值与 EMA 控制 DAV2 提交频率，并为 NVOF 增加 GPU 区间计时，用实测决定是否继续牺牲 Guidance 更新率或 NVOF 质量。

## P0：参数与可见行为修正（0.5.6 历史记录）

> 本节记录 0.5.6 当时已执行的决定；其中 Preset、强度范围和 Skin Structure 项已被 P8 的 0.5.7 合同覆盖。

- [x] 将 Indicator 参数改为 `X=0, Y=0`；实际位置仍需在 16:9、非 16:9 和缩放/裁剪场景验证。
- [x] 将 `NR Style` 的 UI 文案、范围、默认值和 native clamp 统一为 `0–2`：
  - `0 Default`
  - `1 Natural`
  - `2 Cinematic`
- [x] 将 `Local Tone Strength` 的 UI 范围和 native clamp 从 `0–2` 收紧为 `0–1`。
- [x] 将 `Local Structure Strength` 的 UI 范围和 native clamp 从 `0–2` 收紧为 `0–1`。
- [x] 在创建状态日志中打印最终 clamp 后的 style、local tone 和 local structure，避免 UI、配置与 NGX 实际值不一致。
- [x] 对旧配置执行安全 clamp；已有 `style=0` 保持数值不变，但其显示含义改为 `Default`。
- [x] 不再把 `Skin Structure Strength` 与 Local Structure 绑定；生产 UI 和默认 NGX 写入均已移除。
- [x] 移除 `NR Preset` UI/配置入口；native 固定使用已验证默认 `1`，旧配置中的 `preset` 不再生效。
- [x] 将 `Automatic Mask` 默认值改为关闭，同时保留显式用户选项。
- [x] 按当前实验要求将 `Frame Guidance` 默认值改为 `0 Available`。

验收：三种 Style 均可创建并连续 Evaluate；两个 Local Strength 无法从 UI 或旧配置向 NGX 传入区间外值；Indicator 稳定位于左下角。

## P1：收齐真实性能证据

- [x] 将每帧 DLSSNR 时间拆成：帧首槽位 fence 等待、输入准备、Guidance 准备、NGX Evaluate CPU 调用、GPU Evaluate、D3D11/D3D12 提交。
- [x] 使用每命令槽独立的 D3D12 timestamp query 测量 Evaluate GPU 时间；查询结果只在对应槽位 fence 完成后读取。
- [x] 保留 DAV2 的 capture/readback、ORT Run、percentile、postprocess/upload 和 Renderer `BeginFrame` 分项，并记录 120 样本滚动平均、P95、P99 和最大值。
- [ ] 在同一输入尺寸下完成四组对照：`Force Zero`、`Motion Only`、`Depth Only`、`Available`。
- [ ] 每组至少采样 120 个真实帧，同时确认 `result=0x1`、`failures=0`，避免把 fallback 的高帧率误判为优化成功。

验收：能独立回答“DAV2、NVOF、NGX Evaluate、桥接等待各占多少帧时间”，日志不再遗漏帧首 fence stall。

## P2：立即降低卡顿风险

- [x] DAV2 异步主体完成后，按当前实验要求将默认 Guidance 恢复为 `Available`；`Motion Only` 继续作为性能 A/B 对照。
- [x] 不再用固定 `Depth Inference Interval=8` 掩盖卡顿；异步路径按上一轮 worker 总耗时施加 2 倍自适应 cooldown，固定 interval 仅保留为最低帧间隔设置。
- [x] 初始化前检测 `cudnn64_9.dll`；缺失时直接选择 DirectML，避免每次会话重复 TensorRT 失败探测与误导性警告。
- [x] 保持 Provider 按 Guidance 需求惰性创建；`Motion Only` 下不得初始化或运行 DAV2。

验收：默认路径日志中没有 DAV2 初始化、ORT Run 或 staging readback；画面仍由 NVOF Motion Guidance + DLSSNR 处理，而不是 pass-through。

## P3：移植多槽命令资源，消除逐帧 CPU 等待

- [x] 参考已验证项目移植 4 槽 bridge frame ring；每槽持有独立 allocator、command list 和 completion fence value。
- [x] 只在复用仍在飞行的槽位时等待，不再在每帧 Evaluate 前无条件等待上一提交完成。
- [x] 保持同一 D3D12 queue 上的 Evaluate 顺序，以及 D3D11 wait/signal、输出资源所有权和关闭时 GPU drain 顺序不变。
- [x] resize、feature rebuild、异常降级与 shutdown 时通过队列 drain 排空全部在飞行槽位。

验收：稳态帧首 CPU fence wait 的 P95 小于 0.2 ms；无 allocator reset、资源状态、fence 和关闭时序错误；120/120 Evaluate 成功。

## P4：Reset 最小化验证

- [x] 不增加 `NR Reset` UI，也不允许每帧手动或固定置 1。
- [x] 短期仅在 Feature 首次创建、resize/rebuild 和明确的捕获历史不连续后发送一次 Reset。
- [x] 复用 Frame Guidance reset reason 日志；正常异步结果到达不额外触发 Reset。
- [ ] 做一次 A/B：内部一次性 Reset 与创建后始终 `Reset=0` 各运行 120 帧，比较快速转场、暂停恢复和 resize 后的拖影；不以 FPS 差异作为主要判断。
- [ ] 若 A/B 证明无可见收益且驱动不要求，再删除稳态之外的 Reset 传播；否则保留内部一次性语义。

验收：稳态无重复 Reset；无论最终保留与否，都不能把 Reset 作为 DAV2 卡顿的替代修复。

## P5：真正解决 Available 模式的周期性尖峰

此项是短期阶段的退出条件，但实现规模高于前四项：

- [x] 使用三槽 GPU 预处理/readback ring、`DO_NOT_WAIT` 轮询和 latest-only 单工作线程；ORT、百分位计算均不再阻塞 Renderer。
- [x] 新深度未完成时使用上一份有效深度，并由 NVOF 做时域重投影；Renderer 不等待推理结果。
- [x] 已评估 DirectML I/O Binding / D3D12 device tensor：当前先采用 DML1 应用持有队列与固定复用 OrtValue；完整跨 D3D11/D3D12 资源直连还需要双向共享缓冲、显式 fence 和 GPU 百分位路径，作为实测确认 CPU 往返仍是主瓶颈后的独立阶段，不混入本轮安全优化。
- [x] 对 resize、场景切换和 shutdown 实现 generation 丢弃、latest-only 覆盖和 worker join 生命周期。

实现记录（2026-08-29）：

- 模型长边由 518 先降为 392、本轮继续降为 336，输入尺寸按 patch 14 对齐；ORT session 使用 `batch_size/height/width` 固定维度覆盖。
- BGRA/RGBA 缩放、RGB 归一化迁移至 GPU compute shader；只读回约模型尺寸的 CHW 输入，不再读回全尺寸颜色。
- CPU 双份全尺寸上采样与上传已删除；worker 只计算小尺寸百分位，模型输出以一次小尺寸上传交给 GPU 完成归一化、双线性上采样和 temporal filter。
- DirectML 使用应用持有的 D3D12 compute queue；输入/输出 OrtValue 固定分配复用。完整 DML device tensor 直连仍保留为后续项。
- warmup、TensorRT/DirectML session 创建和推理全部位于 worker；根据上一轮 worker 时间加入 2 倍 cooldown，避免持续占满同一 GPU。
- DLSSNR 使用四槽 timestamp query 统计真实 NGX GPU Evaluate 时间，所有关键路径使用 120 样本滚动平均/P95/P99/最大值；fence 等待事件改为会话内复用，不再按等待次数创建内核对象。
- Release x64 `Magpie.Core` 与暂存目录完整 `Magpie.exe` 构建通过；嵌入的三个 DAV2 compute shader 均通过 `cs_5_0` 编译验证。
- 旧实例退出后，最新完整 Release 已成功原位部署为 `bin/x64/Release/Magpie.exe`；临时 `Magpie.next.exe` 已移除，打包脚本也会显式排除该名称。

## P6：NGX 预算感知调度与 NVOF GPU 遥测

本轮执行记录（2026-08-29 21:47）：

- [x] DLSSNR timestamp 完成后以无锁原子快照发布 NGX GPU 最新值、EMA 和样本数；初始化/重建时清零，DAV2 调度不等待 GPU、不读取未完成 query。
- [x] 当 `max(latest, EMA) >= 10.5 ms` 时延迟新的 DAV2 capture，但深度捕获最大年龄限制为 250 ms；当该值 `>= 16.0 ms` 时最大年龄放宽为 500 ms。达到最大年龄后强制允许一次更新并覆盖普通 cooldown，避免深度永久冻结。
- [x] DAV2 模型长边由 392 降至 336；NVOF 继续在两次推理之间重投影上一份有效深度。
- [x] NVOF 使用四槽 D3D11 timestamp/disjoint query 轮转，并以 `D3D11_ASYNC_GETDATA_DONOTFLUSH` 非阻塞读取 120 样本窗口。该指标覆盖 `nvOFExecute + Densify` 在 D3D11 GPU 时间线上的区间，不宣称是 Optical Flow Accelerator 的纯硬件耗时。
- [x] 日志增加 DAV2 预算阈值、NGX GPU latest/EMA、预算跳过次数，以及 NVOF GPU avg/P95/P99/max。
- [x] Release x64 完整解决方案构建通过并部署至 `bin/x64/Release/Magpie.exe`。
- [ ] 用 `Available` 连续运行至少 120 个真实帧，确认 `result=0x1`、`failures=0`，并记录 `gpuBudgetSkips`、深度结果频率、NGX GPU P95/P99 和 NVOF GPU 区间。
- [ ] 对 `Automatic Mask=0/1` 做同场景 A/B；若关闭能稳定降低 NGX Evaluate GPU，则保持默认关闭。
- [ ] 根据新增 NVOF 数据，再对 `MEDIUM + bidirectional` 与 `FAST + forward-only` 做画质/性能 A/B；forward-only 可保留 cost confidence，但会失去 forward-backward consistency confidence。
- [ ] TensorRT 路径因缺少 `cudnn64_9.dll` 被跳过；补齐运行库后再与 DirectML 的 GPU 时间和显存占用做同尺寸对照。

验收：`Available` 下 Renderer 不再出现 100 ms 级 ORT 同步 stall，P95/P99 帧时间没有固定的 interval 周期尖峰。

## P7：DLSS SR、DLSS FG 与 XeSS FG 复用 Frame Guidance

目标：让三个后续消费者复用 Renderer 级 NVOF/DAV2 输出，并由用户独立选择是否向 SDK 提供运动矢量和估算深度；不得为每个消费者重复运行 Provider。

### 公共合同

- [x] DLSS SR/DLSS FG 增加 `Use Motion Vectors` 与 `Use Estimated Depth` 两个独立参数；默认 Motion 开、Estimated Depth 关，关闭通道时向 SDK 绑定 Zero。
- [x] 将 native effects 与 DLSS FG 的需求统一汇总到 Renderer；每个真实捕获帧最多生产一次 Guidance。XeSS FG 的前台需求汇总按当前范围后置。
- [x] 后台消费者继续使用各自颜色输入，并共享带 `baseFrameId`、Motion、Depth、源尺寸、有效区和 reset 的 `FrameGuidanceView`；避免错误假设三个消费者使用同一颜色纹理。
- [ ] 为未来 XeSS FG 增加跨后台/前台的 `PresentedFramePacket` 与 Guidance 导出 ring；本轮不实现 XeSS FG。
- [x] 区分“Provider 内部依赖”和“传给 SDK 的通道”：Depth 时域稳定可以内部使用 NVOF，但 `Use Motion Vectors=0` 时只向消费者绑定 Zero MV。
- [x] DLSS SR/DLSS FG 在参数、尺寸和 Guidance availability 变化时重建/重置 feature；同一基础帧的所有 DLSS FG 中间帧复用同一 Guidance 和 `baseFrameId`。
- [x] DLSS SR/DLSS FG 日志记录 requested/produced/bound 通道、frameId 和 Zero fallback；XeSS FG 的跨线程日志后置。

### DLSS SR（第一优先级）

- [x] 非 Jitter native draw context 直接消费 D3D11 Guidance；旧 `DLSS_OpticalFlow` 作为兼容别名改为请求共享 Motion，不再重复运行私有光流。
- [x] Motion 使用当前帧到上一帧、源像素单位和低分辨率标志；Depth 使用稳定归一化逆深度并设置 `DepthInverted` 创建标志。
- [x] 第一版只在 DLSS 输入尺寸与 Guidance 源尺寸一致时启用真实输入；效果链位置或尺寸不匹配时回退 Zero 并记录日志。
- [ ] 分别验证 Zero、Motion、Depth、Motion+Depth 四种组合；固定平移素材确认 MV 符号与尺度。

### DLSS FG（第二优先级）

- [x] 扩展 `DLSSFrameGenerator` 接口以接收同一 `baseFrameId` 的 `FrameGuidanceView`；通过共享 D3D11/D3D12 资源与 fence 导入 Guidance。
- [x] Backbuffer 保持最终输出尺寸，Motion/Depth 保持捕获源分辨率，并设置 DLSSG render/subrect 尺寸，避免全分辨率放大与复制。
- [x] 绑定真实 NVOF Motion 时设置 `cameraMotionIncluded=true`，并按资源尺寸将源像素运动归一化；x2 与多帧生成画质仍由用户实测。
- [x] 估算深度标记为 Experimental、默认关闭；使用稳定逆深度与 identity/synthetic matrices，不把 SDK 成功返回等同于画质有效。

### XeSS FG（第三优先级）

当前决定：本轮不实现，保留现有 Zero-MV/平坦深度行为。

- [ ] 扩展 Backend→Frontend/Presenter 帧交接，使颜色和 Guidance 使用同一个基础帧 ID；Presenter 不从全局状态临时查询 Guidance。
- [ ] 使用 XeSS FG 推荐的低分辨率 `R16G16_FLOAT` 当前帧到上一帧像素运动；Depth 与 Motion 使用相同尺寸并按 inverted-depth 初始化。
- [ ] 使用 D3D12 resource tag、有效期和同步 fence 管理输入；先验证 x2，再验证 Multi Frame Generation。
- [ ] 记录当前前端 UI/光标没有对应 MV 的已知限制；HUD-less/UI 独立输入作为后续画质阶段，不与首轮 Guidance 接线混合。

### 待确认策略

- [x] 新的 DLSS SR/DLSS FG 默认采用“Motion 开、Estimated Depth 关”。
- [x] 用户可见名称使用 `DLSS SR_Experimental` 与 `DLSS FG_Experimental`；内部旧标识保留以兼容已有配置。
- [x] DLSS FG 的估算深度以 `Experimental` 参数暴露并默认关闭；XeSS FG 本轮不暴露。

验收：三个消费者均只复用一份 Guidance；关闭通道时对应 Provider 不因该消费者运行；颜色、Motion、Depth 的基础帧 ID 一致；任一通道失败都能无闪退回退 Zero；实测确认运动方向、尺度、遮挡边界和 P95/P99 帧时间。

## P8：v0.5.7 DLSSNR 参数暴露

- [x] 增加 `NR Preset`，使用独立配置键 `nrPreset`，范围 `0–3`、默认 `0`；创建 Feature 时写入 `DLSSNR.Hint.Render.Preset`。
- [x] 将 `NR Intensity`、`Local Tone Strength`、`Local Structure Strength` 的 UI 与 native clamp 统一扩展为 `0–2`。
- [x] 增加 `Skin Structure Strength`，范围 `-1–2`、默认 `-1`，逐帧写入 `DLSSNR.SkinStructureStrength`。
- [x] 增加 `NR UI Correction`，默认关闭，逐帧写入 `DLSSNR.UICorrection`。
- [x] 保持 `Automatic Mask` 默认关闭；`Reset` 与 `Enabled` 继续由内部生命周期控制。
- [x] 设置迁移版本升至 v2，清除旧的 `preset` 键，避免其在恢复 Preset 后意外生效。
- [x] 状态日志增加最终 preset、skin structure 与 UI correction 值。
- [x] Release x64 完整重建通过（0 警告、0 错误）；构建输出包含新版 HLSL 参数元数据，`Magpie.exe` 文件版本与产品版本均为 `0.5.7-experimental`。
- [ ] 实机启动后确认效果解析器显示全部新增参数，并完成边界值 Evaluate 验收。

验收：默认创建日志显示 `preset=0`、`skinStructure=-1`、`autoMask=false`、`uiCorrection=false`；全部数值参数在边界值上可创建并连续 Evaluate，超范围配置不会进入 NGX。

## 推荐执行顺序

1. 使用本轮 Release 在同一场景运行 `Available` 至少 120 帧，收集 P6 新日志。
2. 切换 `Motion Only` 做同场景对照，量化 DAV2 推理和预算调度对 NGX P95/P99 的净影响。
3. 在场景、分辨率和 Guidance 模式不变时，对 `Automatic Mask=0/1` 做 A/B。
4. 只有 NVOF GPU 区间占比显著时，才实现并测试 `FAST + forward-only` 诊断分支；否则继续保持 `MEDIUM + bidirectional`。
5. 补齐 cuDNN 后再评估 TensorRT；DML device tensor 直连继续后置，除非日志重新显示 CPU readback/upload 成为主要瓶颈。

## 本轮不做

- 不回退 Signed Snippet 生命周期或重新启用 Core `CreateFeature(18)` 前置步骤。
- 不通过关闭 DLSSNR、吞掉 Evaluate 失败或 pass-through 来制造性能改善。
- 不把提高 Depth Interval、强制 `Reset=0` 或补装 cuDNN 单独视为同步卡顿的最终解决方案。
