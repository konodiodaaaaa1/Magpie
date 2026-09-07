# DLSS 组合兼容性短期 TODO（v0.5.7）

> 已由 [v0.6.5 r1 TODO](20260904-v0.6.5-r1-TODO.md) 取代。以下内容保留为历史实施记录，未完成项不代表当前路线。

> 创建时间：2026-08-30（Asia/Shanghai）
> 分支：`experimental`
> 目标版本：`0.5.7-experimental`
> 状态：第三轮代码修正完成，等待 GPU/游戏内验收
> 范围：DLSS SR + DLSSNR、DLSSNR + DLSS FG、NGX D3D12 生命周期、DLSS SR 命名迁移

## 已确认结论

1. DLSS SR → DLSSNR 的首帧失败来自 Guidance 尺寸不一致，而不是纹理被提前释放。
   - 捕获与源 Guidance 为 `1924x1177`。
   - DLSS SR 输出、DLSSNR 输入为 `2354x1440`。
   - 原实现要求 `metadata.sourceExtent == consumer extent`，因此 NR 在 `stage=guidance-interop` 失败。
2. 当前日志中的 NR + FG 组合只有 x3 样本；x4 样本为 Frame Rate Filter + DLSS FG，不应把它当作 NR + FG x4 的证据。
3. NVOF Motion 已经是 current-to-previous/source-pixels，FG 再设置 `1 / width`、`1 / height` 会把运动缩小约一千倍；像素单位输入的 `mvecScale` 必须是 `1,1`。
4. DAV2 的归一化相对逆深度不等于 DLSS FG 要求的硬件投影深度，也没有可匹配的真实投影矩阵、near/far/FOV；Estimated Depth 默认保持关闭，Motion-only 是当前可信测试路径。
5. x3/x4 的另一限制是显示刷新率与同步呈现容量。60 FPS 真实帧目标在 x4 下要求约 240 FPS；约 120 Hz 的显示链只能看到约 120 次呈现。
6. NR 与 FG 原先分别调用 D3D12 Core `Shutdown1`，退出/重建时会让后析构的参数块得到 `0xbad00007 (NotInitialized)`。
7. 主效果已能输入共享 Motion 与可选 Depth，`DLSS_ZeroMV` 不再是准确的用户可见名称。

## 固定实现原则

- 一帧只运行一份 NVOF/DAV2 源 Guidance，不因消费者尺寸不同重复推理。
- 不只修改 metadata 伪装尺寸；纹理内容、Motion 数值、有效区和同步点一起转换。
- 转换失败回退到消费者尺寸的完整 Zero 组，只在进入失败和恢复真实 Guidance 的边界 reset 一次。
- DLSSNR Feature 18 仍由 signed snippet 创建、Evaluate、Release；Core 只提供参数块和会话生命周期。
- 所有 D3D12 Core Feature/参数释放后，Renderer owner 才执行唯一一次 Core `Shutdown1`。
- 不把提交次数等同于视觉上存在不同生成帧；x3/x4 继续依赖运行日志和显示侧测试判断。
- FG 的 `BackbufferFrameID` 每个真实帧递增，同一真实帧的所有 `multiFrameIndex` 使用相同 ID。
- 多槽呈现必须有界、保持 generated1 → generated2 → generated3 → real 顺序，并在槽位耗尽时施加回压。

## P0：消费者尺寸 Frame Guidance 适配

- [x] 在 Renderer 的原生效果输入边界请求消费者尺寸视图；同尺寸保持零复制 fast path。
- [x] 仅在首次出现跨尺寸消费者时按需创建 D3D11 compute adapter，不增加同尺寸启动或逐帧开销。
- [x] 双线性重采样 `R16G16_FLOAT` Motion、`R32_FLOAT` relative inverse Depth 与 `R8_UNORM` Confidence。
- [x] Motion X/Y 分量分别乘以 `target/source` 比例，继续表示消费者像素单位的 current-to-previous 运动。
- [x] 按比例转换 `validRegion`，保留 `frameId`、reset reason、`requiresHistoryReset` 和通道一致性。
- [x] 目标纹理、UAV、SRV 与 Zero 组按目标尺寸复用，不逐帧创建 GPU 资源。
- [x] 发布 D3D11 fence/value；D3D12 消费者在打开和读取共享资源前等待转换完成。
- [x] 转换失败时回退目标尺寸 Zero；连续失败不逐帧 reset，恢复真实 Guidance 时 reset 一次。
- [ ] 运行确认 SR `1924x1177 → 2354x1440` 后，NR 前 8 帧均 `result=0x1`、`failures=0`，且不再出现 `stage=guidance-interop`。

## P1：统一 NGX D3D12 Core 生命周期

- [x] 新增 Renderer 会话级 `NgxD3D12Core` owner，统一创建 D3D12 Device、Core Init、参数分配/销毁和最终 Shutdown。
- [x] DLSSNR 与 DLSS FG 复用同一个 D3D12 Device；各自保留独立 command queue。
- [x] DLSSNR Core 路径只分配/销毁参数；Feature 18 继续全程调用 signed snippet 导出。
- [x] DLSS FG 析构只释放自己的 Feature/参数，不再调用全局 Core Shutdown。
- [x] Renderer 在 resize、Feature rebuild、禁用和退出前先统一 drain 所有 NR/FG queue，再释放消费者。
- [x] NR 继续遵守：drain → ReleaseFeature → DestroyParameters → snippet Shutdown1 → 恢复 IAT → FreeLibrary。
- [x] owner 记录 Core Init、consumer acquire/release、参数块计数和唯一 final Shutdown。
- [ ] 连续启动/停止至少 10 次，并覆盖一次 resize 与一次 FG rebuild；确认无 `0xbad00007`、SEH、重复 Shutdown 或退出延迟。

## P2：校准 DLSS FG x2/x3/x4 与显示容量

- [x] 将像素单位 NVOF Motion 的 `mvecScale` 从 `1/width,1/height` 修正为 `1,1`，初始化日志固定记录输入合同。
- [x] Estimated Depth 参数默认保持 `0`；显式启用时记录相对逆深度不符合投影深度合同的警告，推荐 Motion-only。
- [x] 每个真实帧设置 `BackbufferFrameID`，并在 x3/x4 的全部索引调用中保持相同 ID；设置过程包含 SEH 防护。
- [x] 创建 4 字节 `OutputDisableInterpolation` UAV 与低频 readback；每 120 个真实帧记录各索引 enabled/disabled/readback-failure，禁止逐帧 CPU readback。
- [x] 初始化日志记录显示刷新率、Frame Rate Filter 真实帧目标、请求倍率、SDK 最大倍率、实际倍率和理论输出 FPS。
- [x] 理论输出明显超过显示刷新率时记录一次容量警告。
- [x] 每 120 个真实帧统计每个 `multiFrameIndex` 的 Evaluate 成功/失败与生成帧 publish 成功/失败。
- [x] 每秒呈现日志区分 generated/real 入队成功与失败，并继续报告 captured/queued FPS。
- [x] 正常窗口退出导致的同步呈现中断不再进入普通 FG 恢复/禁用路径。
- [x] 将单共享纹理 + 同步 `SendMessage` + 后端 DWM wait 改为最多 4 槽的有界共享纹理 ring；使用专用 FIFO 窗口消息、槽位事件、keyed mutex 和重建 generation 防止覆盖、乱序与旧消息误消费。
- [x] D3D11 复制完成后才允许 D3D12 复用生成纹理；DWM pacing 移到前端消费侧，后端仅在 ring 满时回压。
- [x] 修正高刷新率下约 150 FPS 的固定呈现上限：Presenter 显式报告是否使用 DXGI frame-latency waitable object；SwapChain 路径不再叠加逐帧 DWM wait，DirectComposition 路径继续由 DWM 时钟节拍。
- [x] SwapChain 路径在 `BeginFrame` 前使用高精度 waitable timer 等待目标输出 deadline；DXGI waitable object 只负责交换链容量，长时间停顿后重置基准以避免追赶式突发呈现。
- [x] 每 120 个输出帧聚合记录 `paceWait`、`beginFrame`、前端绘制、`endFrame/Present` 与 ring 回压平均耗时，用于区分目标节拍、DXGI/DWM 和 GPU/复制瓶颈。
- [x] 修复 DLSS FG 专用 FIFO 绕过首帧启动的问题：专用消息复用普通路径的源窗口、焦点、光标与首帧后处理，首次成功提交后负责 `_Show()`。
- [x] 同步 DLSS FG 模式下普通 `Renderer::Render()` 不再消费 presentation ring；专用 FIFO 成为唯一消费者，保持 generated1 → generated2 → generated3 → real 顺序。
- [x] Presenter 的 `EndFrame` 返回是否已提交；`DXGI_STATUS_OCCLUDED` 仍视为已提交，使初始隐藏窗口可以在首帧后显示，真正的 Present/Commit 失败不会误触发 `_Show()`。
- [x] 显式检查 frame-latency wait 与 Present 返回值；timeout、`DXGI_STATUS_OCCLUDED` 和失败日志包含窗口可见性与 `firstFrame` 状态，并限制重复日志频率。
- [ ] 分别测试 x2/x3/x4：高刷新率容量足够、120 Hz + 60 FPS、以及降低真实帧目标使 x3/x4 不超过刷新率三组条件。
- [ ] 连续启动/停止 DLSS FG 至少 10 次，确认每次都有 `First frontend frame submitted by DLSSFG FIFO`，不再出现鼠标消失、窗口隐藏和约 1 秒一次的 frame-latency timeout。
- [ ] 检查 `interpolation[indexN=enabled/disabled/readback-failure]`；如果 SDK 未禁用插值但生成内容仍疑似相同，再增加低频 GPU 纹理差异度诊断。

## P3：`DLSS_ZeroMV` → `DLSS_SR`

- [x] 主效果文件与稳定标识迁移为 `DLSS\DLSS_SR`，显示名固定为 `DLSS SR_Experimental`。
- [x] 默认缩放模式名改为 `DLSS SR`。
- [x] AppSettings 对导入后的所有 scaling mode 执行一次性幂等迁移，原参数和 scaling type 保持不变。
- [x] native backend factory 保留隐藏的旧 ID 兼容入口，但新效果列表和默认配置不再暴露旧主 ID。
- [x] 内部类型、文件、宏与构建目标统一为 `DLSSSRUpscaler` / `MP_ENABLE_DLSS_SR` / `EnableDLSSSR`；旧 `EnableDLSSZeroMV` 仅作为本地/CI 构建覆盖的兼容输入。
- [x] 更新 Effects 工程、发布脚本、README、发布说明与实验文档索引。
- [x] 发布脚本会删除输出目录中残留的旧 `DLSS_ZeroMV.hlsl`，并检查 `DLSS_SR.hlsl` 存在。
- [x] `DLSS_ZeroMV_Jitter` 保留为独立诊断效果；`DLSS_OpticalFlow` 保留为旧兼容入口。

## P4：组合回归矩阵

- [ ] DLSS SR → DLSSNR：Zero、Motion、Depth、Motion+Depth 各运行至少 120 个真实帧。
- [ ] DLSSNR → DLSS FG：x2/x3/x4，分别关闭/开启 FG Motion；Depth 默认关闭，仅显式开启做对照，NR 保持连续 Evaluate。
- [ ] DLSS SR → DLSSNR → DLSS FG：覆盖同尺寸与 SR 后尺寸变化。
- [ ] 各组确认颜色、Motion、Depth 的 `frameId` 一致，适配纹理与消费者输入尺寸一致。
- [ ] 各组确认无永久 pass-through、重复 Provider、重复 Core Shutdown、allocator/resource-state/fence 错误。
- [ ] 覆盖启动、停止、resize、窗口模式切换和应用退出，关闭阶段日志保持干净。
- [x] Release x64 代码编译和链接通过，0 error；`git diff --check` 通过。
- [ ] 关闭当前占用 `publish\x64\Magpie.exe` 的旧 Magpie 进程后，再完成一次无文件占用警告的完整 Rebuild。

## 本轮不做

- 不实现 XeSS FG 的 Frame Guidance 接线。
- 不启用 Core `CreateFeature(18)` 作为 signed snippet 前置步骤。
- 不为每个消费者重复运行 NVOF/DAV2。
- 不把高于显示刷新率的提交目标包装成可见收益。
- 不用永久 Force Zero、吞错误或永久 pass-through 作为兼容性修复。
- 不在缺少 x3 与显示刷新率证据时重写 DLSS FG 多帧调用合同。
