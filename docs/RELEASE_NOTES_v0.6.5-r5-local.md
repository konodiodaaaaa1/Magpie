# Magpie 0.6.5 r5 local（fix2）

本地测试版，基于当前 r4 工作树继续修改。完整实机、画质和性能验收由用户进行。

## fix2（2026-09-05）

- 补充：光流估算的中文“极致质量”改为“高质量”（繁体为“高品質”），仅修改显示文本。主页“最近一次问题”支持右上角 × 关闭，切换页面不会重新显示；收到下一次问题报告时重新显示。
- 首页“工具栏”区域新增默认收起的“工具栏快捷键设置”，沿用现有 SettingsExpander，集中放置状态切换、性能监测、效果参数、截屏、固定工具栏五项快捷键。
- 增加简体中文、繁体中文和英文标题；保留原快捷键编辑控件及其行为。工具栏初始状态和截图保存目录继续独立显示。
- 完成 37 个效果器、144 个参数的 Live / Restart 源码核对；DLSSNR 初始化失败并透传时改为显示“不可用”，避免误标实时生效。Restart 指重新启动缩放。
- 主窗口效果参数页与运行面板增加简体/繁体中文标签、分组和选项，共 177 个资源键。只翻译显示副本，效果与参数标识不变。
- 新增 36 类问题诊断，覆盖缩放模式、目标窗口、效果/设备/补帧初始化、配置保存、参数冲突、截屏、导入/导出与文件选择器。主页新增“最近一次问题”：查看详情、复制诊断信息、打开日志目录。提示包含可操作建议，连续相同问题的弹窗限流 15 秒。
- 截图后台任务使用本次会话的目录和回调快照，区分读取、建目录、写入与编码失败；修复空目录编号和扫描失败的处理。导入拒绝不含模式的 JSON，报告语法位置；导出检查实际写入、刷新与关闭结果；正常取消文件选择器不报错。
- [完整参数分类及后续同步边界](experimental/todos/20260905-v0.6.5-r5-fix2-REVIEW.md)。自动重建时继承实时值、后端拒绝后的逐参数确认等 fix1 后续建议仍单列，未宣称已经修复。
- Release x64 构建通过；实际 PRI/显示层本地化共 1207 项无界面检查通过，XML、资源重名和诊断映射检查通过。未启动 Magpie 或使用 computer-use。
- 覆盖部署到原 `r5 local` 目录和 ZIP，程序版本字符串继续为 `0.6.5 r5 local`。界面验收由用户进行。

## fix1（2026-09-05）

- 首页效果参数图标与缩放工具栏统一为 E9E9。
- 删除保存按钮，参数修改自动保存：停止修改 300 ms 后写盘，持续拖动时约每秒保存；状态只在写盘完成后显示已保存。Restart 项仍需“应用并重新缩放”。恢复按钮改名为“恢复本次缩放初始值”，恢复也会自动保存。
- 移除保存成功后的错误默认资源查找，补上参数请求异常边界；已用旧包 PRI 独立复现默认查找异常。
- 连续请求不再丢弃；关闭面板/停止缩放后仍保存已提交修改；退出时同步完成最终保存。按参数合并并检查冲突，保留其他参数的并行编辑。
- 完整 review 和后续建议见 [r5 fix1 review](experimental/todos/20260905-v0.6.5-r5-fix1-REVIEW.md)。以下 r5 初始记录中的手动保存、保存后提示等行为以此 fix1 说明为准。

## r5 初始修改记录

- 实时参数面板英文标签改为 `Live` / `Restart`，去掉方括号。
- DLSSNR 输入分辨率调整使用可分离 Lanczos-2 AA 颜色降采样（先垂直再水平），残差使用 Catmull-Rom 4+4 升采样。复用同一张 FP16 中间纹理，不新增持久纹理；motion、confidence 和 inverse-depth 的降采样规则不变。
- 100% 输入比例保留 RGBA 精确复制或 BGRA 格式转换，同时继续执行残差控制；关闭输入分辨率调整时仍使用原有直接输出路径。
- `Shadow / Structure Control` 和 `Reflection/Glow Control` 改为按整像素的线性 Rec.709 亮度差选择一个倍率，作用于完整 RGB 残差。分类在方向控制和 HSL 调节前完成，以零为硬边界。Saturation / Lightness 仍各自缩放 HSL 差值，四项默认 1.0 时保持默认快速路径。
- 修复异步保存访问正在修改的缩放模式和界面状态的问题。配置先在界面线程序列化，后台只处理固定 JSON；保存按修订号排序，防止旧快照覆盖新配置。参数编辑的延迟保存改用界面线程计时器。
- 配置通过同目录临时文件、完整写入、FlushFileBuffers 和原子替换保存，并保留上一份有效 `config.json.bak`。参数面板等待保存成功后才提示成功或重启；失败时回滚本次保存对设置的修改。
- 配置已损坏时先保留原文件为 `config.json.corrupt-<时间标识>`，优先读取有效备份；无备份时仅恢复完整顶层条目和完整缩放模式/配置项。无法恢复的缺失内容使用默认值，不再因截断 JSON 无法启动。

## 新增快捷键

沿用现有全局快捷键注册、冲突提示和编辑控件，可在首页“工具栏”区域修改。只在有效缩放会话中执行。

| 功能 | 默认快捷键 |
| --- | --- |
| 性能监测开关 | Alt+Shift+P |
| 效果参数面板开关 | Alt+Shift+E |
| 截取最终效果输出 | Alt+Shift+S |
| 固定/取消固定工具栏 | Alt+Shift+F |

现有缩放、窗口化缩放及工具栏状态快捷键继续有效。3D 游戏模式延续现有面板限制；截图仍可调用。

## 自动检查与用户验收

- Release x64 完整编译及最终增量编译；产品 HLSL 六个入口按 `cs_5_0`、严格模式、全部资源绑定、优化级别 3、警告视为错误编译。
- 配置回归覆盖用户截断附件、全部截断位置、转义字符串、非法 JSON 拒绝、写入替换失败、有效备份、旧保存晚完成及 100 次并发保存。
- 用户附件恢复了 12 个完整缩放模式；截断点后的内容（包括 profiles）已不存在，不能恢复原值。原附件未修改，恢复文件单独提供。
- 按用户要求未执行 computer-use 界面操作、GPU 数值/性能测量或画质 A/B；不引用旧微基准作为本版本实测。建议用户重点检查实时调节后保存/重启、25%/中间比例/100% 的残差画面，以及四个新快捷键。

## 安装

完全退出旧版，将完整 ZIP 解压到新目录后运行 `Magpie.exe`。不要单独替换 EXE；必须使用同包的 `resources.pri` 和运行时。

---

# Magpie 0.6.5 r5 local（fix2） — English

Local build on top of the existing r4 worktree. Hardware, visual quality and performance acceptance are left to the user.

Fix2 groups the five toolbar shortcuts in the existing collapsed settings expander. All 144 parameters across 37 effects were reviewed against runtime paths; unavailable DLSSNR pass-through controls no longer claim Live support. Both parameter pages now localize labels, groups and choices in Simplified and Traditional Chinese. A Home issue card provides actionable guidance, diagnostic details, copy and logs access for 36 newly classified failure paths. Screenshot tasks own their session data; import/export and file-picker failures now distinguish actionable causes. Release x64 compilation and 1207 resource/localization checks passed without launching the application.

- English parameter badges now read `Live` / `Restart` without brackets.
- DLSSNR uses vertical-first separable Lanczos-2 AA for color input reduction and Catmull-Rom 4+4 for residual reconstruction, reusing the same FP16 intermediate. Guidance reduction is unchanged. The 100% identity/conversion path still applies residual controls; disabling input resolution adjustment retains the original direct output path.
- Shadow/Glow selects one multiplier for the entire RGB residual using the sign of the linear Rec.709 luminance difference before directional and HSL controls. The boundary is exactly zero. Saturation and Lightness continue to scale their respective HSL differences; all-one defaults preserve the fast path.
- Settings are serialized on the UI thread before background writing. Revision ordering prevents stale saves from winning. Parameter-save debouncing uses a UI dispatcher timer. Writes stage, flush and atomically replace the configuration while retaining a valid `.bak`. Parameter-panel save/restart reports success only after persistence succeeds and rolls back settings on failure.
- Damaged configurations are preserved as `.corrupt-<timestamp>` before recovery. A valid backup is preferred; otherwise only complete entries are salvaged, with defaults for missing data.
- Configurable global shortcuts: Alt+Shift+P (profiler), E (parameters), S (final-output screenshot), F (pin toolbar). Existing shortcut controls and conflict reporting are reused. Actions require an active scaling session; existing 3D-game panel restrictions remain.
- Automated checks cover Release x64 compilation, all six production shader entry points, configuration recovery, invalid serialization, failed replacement, backup preservation, out-of-order saves and 100 concurrent saves. The supplied truncated file yields 12 complete scaling modes; content beyond the truncation cannot be recovered. The original attachment is untouched.
- No computer-use interactions, GPU numerical/performance tests or visual A/B were performed, as requested. Exit the old version and extract the complete package into a new folder, keeping EXE, PRI and runtimes together.
