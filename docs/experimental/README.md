# Magpie 实验分支文档索引

本目录只收纳实验功能的路线图、TODO、测试矩阵和运行记录。上游通用 Wiki 文档继续保留在 `docs/` 根目录，避免实验说明与用户文档混在一起。

## 当前工作

- [v0.6.1 Feature 4：缩放模式交互修复与引导性错误提示](todos/20260903-v0.6.1-feature4-interaction-errors-TODO.md)：修复拖拽闪退、重复新建不弹自动命名，并系统整理可恢复的失败提示与后续错误码拆分。
- [DLSS 组合兼容性短期 TODO](todos/20260830-DLSS-combination-compatibility-short-term-TODO.md)：修复 SR→NR 跨尺寸 Guidance、统一 NR/FG 的 NGX D3D12 Core 生命周期、迁移 `DLSS_SR` 标识并补齐 x2/x3/x4 诊断。
- [DLSSNR 参数与性能短期 TODO](todos/20260829-DLSSNR-parameters-performance-short-term-TODO.md)：修正 Indicator/参数合同，跟踪异步 DAV2、四槽 bridge 与 GPU 验收。
- [DLSSNR 性能与可观测性 TODO](todos/20260829-DLSSNR-performance-observability-TODO.md)：处理严重卡顿、pass-through 仍运行 Guidance、DLSS Indicator 不可靠和 DAV2 同步瓶颈。
- [Frame Guidance 测试矩阵](testing/FRAME_GUIDANCE-TEST-MATRIX.md)：统一记录诊断视图、四组 Guidance、场景、性能和日志证据。

## 已完成的路线图

- [DLSS5 Frame Guidance 阶段 1–4](todos/completed/20260829-164413-DLSS5-FrameGuidance-short-term-TODO.md)：工程实现已完成，GPU 运行时验收转入当前 TODO。

## 交接、发布与授权

- [实验分支 Git 工作流](GIT-WORKFLOW.md)
- [实验版发布规范](RELEASE-WORKFLOW.md)：版本与标签、双语 Release Note、附件布局、Draft 审核门禁及发布检查。
- [实验分支交接](../EXPERIMENTAL_HANDOFF_ZH.md)
- [v0.6.1 Hotfix 说明](../RELEASE_NOTES_v0.6.1-experimental-hotfix.md)
- [v0.6.1 实验版发布候选说明（未发布）](../RELEASE_NOTES_v0.6.1-experimental.md)
- [v0.6.1 下一实验版说明（开发中）](../RELEASE_NOTES_NEXT.md)
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
