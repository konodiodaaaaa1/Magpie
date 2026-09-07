# 实验分支 Git 工作流

本文约定本仓库实验功能的源码版本管理方式。Git 保存可复现的源码状态，GitHub Release 保存面向使用者的运行包和经授权分发的运行时文件。

## 分支

- `experimental` 是唯一默认分支，提交后必须能够完成 Release x64 构建。
- 独立功能使用 `feature/<name>`，问题修复使用 `fix/<name>`，完成并验证后合并回 `experimental`。
- 不直接改写已经推送的提交或移动已发布标签；需要修正时追加新提交。

## 跟踪边界

应提交：

- `src/` 中的源码、工程文件和效果定义。
- `scripts/` 中可复现的构建与发布脚本。
- `docs/`、README、TODO、测试矩阵和发布说明。
- 明确需要版本控制的配置文件。

不应提交：

- `bin/`、`obj/`、`logs/`、`publish/`、`release/` 等生成目录。
- 第三方 DLL、EXE、LIB、ONNX 模型和 `.addon64` 文件。
- 本地依赖、游戏文件、调试捕获、签名材料、密钥或包含本机绝对路径的记录。

运行包中的 NVIDIA/Intel/AMD DLL 由发布流程从本地依赖复制，并通过发布清单记录来源和哈希；它们不属于源码提交。

## 提交

- 一个提交只表达一个可说明、可回退的结果；相关文档和代码应在同一提交中保持一致。
- 使用 `type(scope): summary` 格式，例如：
  - `feat(dlss): share frame guidance with SR and NR`
  - `fix(dlssfg): show window after FIFO first frame`
  - `perf(dlssfg): remove duplicate presentation pacing`
  - `docs(experimental): update compatibility test matrix`
- 提交前检查暂存区，不使用未经审阅的全量添加：

```powershell
git status --short
git diff --check
git diff --cached --stat
git diff --cached
```

## 版本与发布

- `version.json` 表示当前公开版本；开发下一版时，版本计划写入 `docs/RELEASE_NOTES_NEXT.md`，正式发布时再一起更新。
- 验证中的候选版本可使用 `v0.5.7-experimental.1`、`.2`；确认发布后使用 `v0.5.7-experimental`。
- 标签只指向已经完成构建和运行验证的提交。GitHub Release 的源码必须与标签一致，二进制差异只允许来自发布时明确记录的运行时 DLL。
- Release 的命名、双语正文、附件布局、Draft 审核门禁和发布检查统一遵循 [实验版发布规范](RELEASE-WORKFLOW.md)。
- 准备文件、创建 Draft、上传附件和正式发布是相互独立的授权阶段；没有维护者对当前阶段的明确批准，不进入下一阶段。
- 所有实验版 Release 都保持 `prerelease: true`。未经明确“发布”指令，只能保留为 Draft。

## 提交前最低检查

1. Release x64 构建为 0 error。
2. 正常 Release 输出中的 `Magpie.exe` 被运行中进程占用时，先关闭 Magpie 再继续构建；不要另建替代输出目录绕过占用。
3. `git diff --check` 无空白错误。
4. `git status --short` 中没有构建产物、日志、第三方二进制或本机绝对路径。
5. 涉及 DLSS/Frame Guidance 时，更新对应短期 TODO，并保留仍需 GPU 验收的项目为未完成状态。
