# Magpie 实验分支文档索引

当前准备版本：[0.6.5 Release Note](../RELEASE_NOTES_v0.6.5-experimental.md)。r1–r10 已完成本地开发整合，发布包仍待维护者检查。历史 TODO 中的阶段性行为以 [r10](../RELEASE_NOTES_v0.6.5-r10-local.md) 和正式准备说明为准。

- [r10 DLSSNR 参数延迟重新启用](reviews/20260905-r10-dlssnr-parameter-restart.md)
- [Front Edge Sync 使用说明](../FRAME_SYNC_GUIDE.md)


本目录只收纳实验功能的路线图、TODO、测试矩阵和运行记录。上游通用 Wiki 文档继续保留在 `docs/` 根目录，避免实验说明与用户文档混在一起。

## 当前工作

- [v0.6.5 r8：参数状态、光流降级与 SR 入口收敛](todos/20260905-v0.6.5-r8-TODO.md)。
- [r8 本地版本说明](../RELEASE_NOTES_v0.6.5-r8-local.md)。

下列旧版条目保留历史；与 r8 记录不一致的方案已被取代。

- [v0.6.5 r4：统一光流方法与运动向量质量下拉菜单](todos/20260904-v0.6.5-r4-optical-flow-selection-TODO.md)：为 DLSS SR/FG/NR 定义 NVOF 4F/4M/4S/2M 四档，为 XeSSFG 增加 None/AMDOF/NVOF 方法选择、提供器专属质量档、能力错误与多消费者共享合同。
- [v0.6.5 r3 re fix2：恢复工具栏式的同帧浮窗呈现](todos/20260904-v0.6.5-r3-re-fix2-TODO.md)：移除提前生成 DrawData 和浮窗计时门控，让工具栏、参数窗与光标在实际前端帧内生成、绘制并统一 Present。
- [v0.6.5 r3 re fix1：浮窗与光标统一刷新调度](todos/20260904-v0.6.5-r3-re-fix1-TODO.md)：输入边沿即时推进，高频移动合并到正常前端 tick，并在交互期间使用约 8 ms 的按需刷新。
- [v0.6.5 r3：缩放期间调节效果参数](todos/20260904-v0.6.5-r3-TODO.md)：参数级区分实时应用与重新缩放后生效，并提供保存、还原和一次性完整重启流程。
- [v0.6.5 r2 fix4：参数列间距](todos/20260904-v0.6.5-r2-fix4-TODO.md)：将参数列间距增至 24 DIP，并保持分隔线居中。
- [v0.6.5 r2 fix3：参数列分隔线](todos/20260904-v0.6.5-r2-fix3-TODO.md)：在相邻可见参数列之间增加淡色细竖线，并随列显隐更新。
- [v0.6.5 r2 fix2：Residual 命名与 NR Style 换行](todos/20260904-v0.6.5-r2-fix2-TODO.md)：补齐两个 HSL Multiplier 名称，并让显式多行数值标签的说明行使用整列宽度。
- [v0.6.5 r2 fix1：参数列、残差语义与 DLSS 参数收敛](todos/20260904-v0.6.5-r2-fix1-TODO.md)：修正 Flyout 宽度、残差处理顺序、长标签、NR 范围与 Motion 开关。
- [v0.6.5 r2：参数列直展与 DLSSNR 残差精细控制](todos/20260904-v0.6.5-r2-TODO.md)：参数 Flyout 直接展开所有列，重排 DLSSNR 两列并增加 HSL 与有符号残差控制。
- [v0.6.5 r1：移除深度估算与效果参数多列分组](todos/20260904-v0.6.5-r1-TODO.md)：保留 Zero Depth 合同，移除学习型深度链，并为效果参数增加可选 GROUP 多列布局。
- [v0.6.1 Feature 4：缩放模式交互修复与引导性错误提示](todos/20260903-v0.6.1-feature4-interaction-errors-TODO.md)：修复拖拽闪退、重复新建不弹自动命名，并系统整理可恢复的失败提示与后续错误码拆分。
- [Frame Guidance 测试矩阵](testing/FRAME_GUIDANCE-TEST-MATRIX.md)：统一记录 Zero/Motion 两态、场景、性能和日志证据。

## 已完成的路线图

- [DLSS5 Frame Guidance 阶段 1–4](todos/completed/20260829-164413-DLSS5-FrameGuidance-short-term-TODO.md)：工程实现已完成，GPU 运行时验收转入当前 TODO。

## 已被 v0.6.5 取代

- [DLSS 组合兼容性短期 TODO](todos/20260830-DLSS-combination-compatibility-short-term-TODO.md)
- [DLSSNR 参数与性能短期 TODO](todos/20260829-DLSSNR-parameters-performance-short-term-TODO.md)
- [DLSSNR 性能与可观测性 TODO](todos/20260829-DLSSNR-performance-observability-TODO.md)

## 交接、发布与授权

- [实验分支 Git 工作流](GIT-WORKFLOW.md)
- [实验版发布规范](RELEASE-WORKFLOW.md)：版本与标签、双语 Release Note、附件布局、Draft 审核门禁及发布检查。
- [实验分支交接](../EXPERIMENTAL_HANDOFF_ZH.md)
- [v0.6.1 Hotfix 说明](../RELEASE_NOTES_v0.6.1-experimental-hotfix.md)
- [v0.6.1 已发布实验版说明](../RELEASE_NOTES_v0.6.1-experimental.md)
- [v0.6.5 下一实验版说明（开发中）](../RELEASE_NOTES_NEXT.md)
- [v0.6.0 Hotfix 说明](../RELEASE_NOTES_v0.6.0-experimental-hotfix.md)
- [v0.6.0 实验版说明](../RELEASE_NOTES_v0.6.0-experimental.md)
- [v0.5.9 未发布历史草稿](../RELEASE_NOTES_v0.5.9-experimental.md)
- [v0.5.8 实验版说明](../RELEASE_NOTES_v0.5.8-experimental.md)
- [v0.5.7 实验版说明](../RELEASE_NOTES_v0.5.7-experimental.md)
- [v0.5.6 实验版说明](../RELEASE_NOTES_v0.5.6-experimental.md)
- [v0.5.3 实验版说明](../RELEASE_NOTES_v0.5.3-experimental.md)
- [v0.5.2 实验版说明](../RELEASE_NOTES_v0.5.2-experimental.md)
- [实验包 README](../README-EXPERIMENTAL-RELEASE.txt)
- [第三方组件与再分发](../THIRD_PARTY_AND_REDISTRIBUTION.md)

## 目录约定

- `todos/`：按日期命名的执行清单；完成的阶段性清单归档到 `todos/completed/`。
- `testing/`：可复用测试矩阵、素材约定、截图/DDS/日志记录格式。
- 后续若增加设计说明，放入 `design/`；若增加一次性测试结果，放入 `results/YYYYMMDD/`。
