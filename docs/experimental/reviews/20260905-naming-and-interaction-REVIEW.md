# 命名与交互 review：效果组、启用动作和显示模式

日期：2026-09-05。基于当前 0.6.5 r5 local / fix2 工作区；HEAD 为 `84d9f6abb8203a79f9a44d9a66984a293e68856d`，包含尚未提交的修改。本次仅调查和交付文档，未修改、编译、运行或部署程序。

## 结论

建议将面向用户的“缩放模式”改为“效果组”。当前一个组可以组合超分辨率、去噪、锐化、补帧、限帧等处理，名称不应暗示它只能改变图像尺寸。主要收益是把“处理配置”“启动动作”“全屏／窗口显示方式”和“尺寸缩放”分开。

建议定义为：**效果组是一组按顺序配置的效果器及其参数、尺寸设置；启用时按该配置处理目标窗口。** 可以只有一个效果器；编辑中的组可以为空，但空组不能启用。“包括多个效果器”容易让用户误以为至少需要两个。另需说明：帧生成等功能有特殊执行位置，列表顺序不代表每个后端阶段都严格在该行执行。

只修改用户可见文本时，配置兼容风险较低。不能把这次改名扩大为对 `scalingModes`、`scalingMode`、快捷键动作名或效果器 ID 的全局替换，否则会触及导入、配置恢复、程序配置引用和参数保存。

## 建议术语

| 对象 | 建议用语 | 用途和边界 |
| --- | --- | --- |
| 保存的效果组合 | 效果组 | 替代产品界面中的“缩放模式” |
| 组内单个处理单元 | 效果器 | 将“添加效果”等容易混用的组件名称统一为“添加效果器” |
| 启动／结束处理 | 启用／停用效果组 | 操作对象是某个目标窗口，不会禁用或删除整个组的配置 |
| 处理中的窗口 | 目标窗口／源窗口 | 选取和捕获对象；“前台窗口”只在描述启动时使用 |
| Magpie 展示的结果 | 输出窗口／输出画面 | 避免和源应用窗口混淆 |
| 输出显示方式 | 全屏模式／窗口模式 | 是 Magpie 的显示模式，和源应用是否全屏分开 |
| 尺寸运算 | 缩放、缩放倍数、输出尺寸、等比适应 | 保留数学和图像处理含义，不替换成“效果组” |
| 按程序保存的设置 | 程序配置／全局默认配置 | 程序配置选择效果组，同时保存捕获、光标、性能等设置 |
| 数值恢复 | 恢复效果器默认值 | 单个参数，和“恢复本次运行初始值”“恢复默认效果组”分开 |

英文可考虑 `Effect group`，技术说明中可补充 `ordered effect chain`；繁体中文可用“效果組”。这是后续用词建议，尚未要求同步修改所有语言。效果器文件和模型的已有英文名称无需跟着翻译或改名。

## 主要文案草案

下表为待实施的文案建议，不是已修改的资源。对快捷键保留“快捷键”字样，避免用户误认为设置行本身是启动按钮。

| 位置／当前文案 | 建议文案 | 说明 |
| --- | --- | --- |
| 导航、页面标题、程序配置中的“缩放模式” | 效果组 | 三处一致 |
| 新建缩放模式 | 新建效果组 | 空组允许编辑 |
| 添加效果 | 添加效果器 | 与组内对象名称一致 |
| 全屏模式缩放快捷键 | 效果组快捷键（全屏模式） | 描述：以全屏模式为目标窗口启用效果组 |
| 窗口模式缩放快捷键 | 效果组快捷键（窗口模式） | 描述：以窗口模式为目标窗口启用效果组 |
| 按下快捷键可缩放前台窗口或停止缩放。 | 按下快捷键可为前台窗口启用效果组，或停用当前正在运行的效果组。 | 下方补充模式切换行为，见下一节 |
| `{} 秒后缩放前台窗口` | `{} 秒后为前台窗口启用效果组` | 保留占位符，不能写死 3 秒 |
| 托盘：`{} 秒后缩放 (全屏模式)` | `{} 秒后启用效果组（全屏模式）` | 托盘空间较窄，可采用短句 |
| 停止缩放 | 停用效果组 | 工具栏结束当前运行；不删除组 |
| 切换到全屏模式缩放／窗口模式缩放 | 切换到全屏模式／窗口模式 | 已在运行，无需再次“启用” |
| 位于前台时自动缩放 | 位于前台时自动启用效果组 | 程序配置设置 |
| 窗口模式缩放（设置分区） | 窗口模式 | 初始缩放倍数仍保留原词 |
| 允许缩放最大化或全屏的窗口 | 允许为最大化或全屏窗口启用效果组 | 明确指源应用窗口状态 |
| 缩放时模拟独占全屏 | 运行效果组时模拟独占全屏 | 仍需注明仅用于 Magpie 全屏模式 |
| 工具栏位于缩放窗口顶部…… | 工具栏位于输出窗口顶部，提供帧率显示、截图等功能；窗口模式下还可拖动工具栏移动输出窗口。 | 区分输出和目标窗口 |
| 应用并重启缩放 | 应用并重新启用效果组 | 描述补充“针对当前目标窗口”；不是重启 Magpie |
| 恢复本次缩放初始值 | 恢复本次运行初始值 | 不能改成“恢复默认”，两者值不同 |
| 当前缩放模式没有效果器…… | 当前效果组没有效果器。请添加效果器，或选择其他效果组。 | 保留可操作建议 |
| 重置缩放模式配置？ | 恢复默认效果组？ | 这是批量替换操作，和参数重置分开 |
| 重置说明 | 这将删除当前所有效果组并恢复内置效果组。各程序配置将改为使用全局默认效果组。此操作无法撤销。 | 与当前 ResetScalingModes 行为一致 |
| 导入／导出缩放模式失败 | 导入／导出效果组失败 | JSON 技术字段仍显示原名 `scalingModes` |
| 切换全屏／窗口时的报错 | 请还原目标窗口或退出目标应用的全屏状态，再以窗口模式启用效果组；也可改用 Magpie 的全屏模式。 | 分开两种“全屏” |
| 高（缩放倍数）、宽（缩放倍数）、光标缩放系数 | 保留 | 都是尺寸运算 |
| 组内尺寸设置按钮“缩放” | 输出尺寸 | 可选改进；它实际打开尺寸规则设置，不会启动效果组 |

## 快捷键行为必须准确

源码的实际行为是：未运行时为前台窗口启动；启动中再次按下会取消；运行中按当前模式快捷键会停用；源窗口处于焦点且按另一模式快捷键时切换显示模式；源窗口没有焦点时则停用当前运行；停止中忽略重复操作。

因此长说明建议写成：“未运行时，为前台窗口启用效果组。运行时，按当前显示模式的快捷键可停用；目标窗口保持焦点时，按另一显示模式的快捷键可切换模式。”简短说明可以只介绍启停，但不要承诺“任何快捷键再次按下都会停用”。

依据：[ScalingService.cpp](<../../../src/Magpie/ScalingService.cpp#L114>)、[ScalingWindow.cpp](<../../../src/Magpie.Core/ScalingWindow.cpp#L369>)。

## 风险和处理建议

| 风险 | 程度 | 后续实施建议 |
| --- | --- | --- |
| 全局替换破坏配置、快捷键和导入 | 高，若跨入数据层 | 保留 `scalingModes`、`scalingMode`、`scalingType`、`Scale`、`WindowedModeScale`、效果器 ID、参数名和资源键；仅改资源值及说明 |
| 将尺寸“缩放”误改成启动动作 | 中 | 人工分类文案；光标、倍数、适应／填充、缩小图像等保留尺寸含义 |
| 用户误以为停用会禁用组供其他配置使用 | 中 | 文案说明“停用当前运行”，必要时显示目标窗口名 |
| 用户误以为效果组只是无顺序集合 | 中 | 初次说明写清执行顺序和参数配置；帧生成特殊执行位置另行解释 |
| 旧教程、截图和社区术语不一致 | 中 | 更新现行帮助；一版迁移说明“效果组（原称缩放模式）”；历史版本记录不批量重写 |
| 改名顺手改预设名称导致用户内容被覆盖 | 中 | 不批量重命名用户自己保存的组；内置品牌名如 Lanczos、DLSSNR 保留 |
| 新文本变长、换行、辅助功能名称遗漏 | 中 | 同步检查标题、Tooltip、快捷键编辑弹窗、托盘、错误详情和辅助功能名称；由用户做界面验收 |
| 语言资源覆盖不一致 | 中 | 当前 en-US / zh-Hans 各 601 项，zh-Hant 590 项；繁体缺少 11 个键，其中含效果组重置说明，后续不要只替换已有词 |
| 报错关键词和支持检索失联 | 低至中 | 稳定 `MP-XXX` 错误码和日志内部标识；发布说明给出新旧术语对照 |

核心证据：[数据结构](<../../../src/Magpie/ScalingMode.h#L7>)、[导入／导出](<../../../src/Magpie/ScalingModesService.cpp#L108>)、[快捷键序列化](<../../../src/Magpie/ShortcutHelper.cpp#L9>)、[配置恢复](<../../../src/Magpie/ConfigPersistence.h#L39>)。

## 建议实施边界与后续验收

第一批统一简体中文的配置对象名称、启动说明、错误提示和现行帮助；保留数据格式。其他语言是否同步改名可以在实施前决定，不阻塞本次调查。不得触发配置重置或迁移来完成纯文字改名。

后续应核对旧配置和旧导出文件仍可读取、新导出仍被原有版本识别、快捷键配置保持、用户自定义名称保持，以及全屏／窗口／源窗口已全屏这三个场景的说明。当前会话未执行这些程序测试。

## 文本覆盖盘点

当前简体资源中 90 个资源值包含“缩放”，其中 22 个包含“缩放模式”。下方清单覆盖这 90 项，供后续逐条分类使用；它不是批量替换指令。还应连带检查同分区不含该词的短标签，如“添加效果”“重置配置”。

| 资源键（链接到源码） | 当前文本 | 检查方向 |
| --- | --- | --- |
| [Root_ScalingModes.Content](<../../../src/Magpie/Resources.language-zh-Hans.resw#L189>) | 缩放模式 | 效果组对象 |
| [ScalingModes_PageFrame.Title](<../../../src/Magpie/Resources.language-zh-Hans.resw#L192>) | 缩放模式 | 效果组对象 |
| [ScalingModes_ResetDialog_Title](<../../../src/Magpie/Resources.language-zh-Hans.resw#L255>) | 重置缩放模式配置？ | 效果组对象 |
| [ScalingModes_ResetDialog_Content](<../../../src/Magpie/Resources.language-zh-Hans.resw#L258>) | 这将删除所有当前缩放模式并恢复默认缩放模式。自定义程序配置将改为使用全局默认缩放模式。此操作无法撤销。 | 效果组对象 |
| [ScalingModes_DeleteFlyout_Description.Text](<../../../src/Magpie/Resources.language-zh-Hans.resw#L345>) | 以下配置文件正在使用这个缩放模式: | 效果组对象 |
| [ScalingModes_DeleteFlyout_Title.Text](<../../../src/Magpie/Resources.language-zh-Hans.resw#L348>) | 确定删除这个缩放模式？ | 效果组对象 |
| [ScalingModes_NewScalingMode.Text](<../../../src/Magpie/Resources.language-zh-Hans.resw#L363>) | 新建缩放模式 | 效果组对象 |
| [ScalingModes_NewScalingModeFlyout_Title.Text](<../../../src/Magpie/Resources.language-zh-Hans.resw#L366>) | 新建缩放模式 | 效果组对象 |
| [ScalingModes_Scale.[using:Windows.UI.Xaml.Controls]ToolTipService.ToolTip](<source/src/Magpie/Resources.language-zh-Hans.resw:369>) | 缩放 | 启动动作／显示模式／运行状态 |
| [ScalingModes_ScaleFlyout_HeightFactor.Text](<../../../src/Magpie/Resources.language-zh-Hans.resw#L384>) | 高（缩放倍数） | 尺寸／光标语义，保留或仅改运行语境 |
| [ScalingModes_ScaleFlyout_WidthFactor.Text](<../../../src/Magpie/Resources.language-zh-Hans.resw#L390>) | 宽（缩放倍数） | 尺寸／光标语义，保留或仅改运行语境 |
| [ScalingModes_ScaleFlyout_Type_Absolute_Description](<../../../src/Magpie/Resources.language-zh-Hans.resw#L402>) | 指定缩放后的尺寸 | 尺寸／光标语义，保留或仅改运行语境 |
| [ScalingModes_ScaleFlyout_Type_Factor_Description](<../../../src/Magpie/Resources.language-zh-Hans.resw#L408>) | 指定相对于输入图像的缩放倍数 | 尺寸／光标语义，保留或仅改运行语境 |
| [ScalingModes_ScaleFlyout_Type_Fit_Description](<../../../src/Magpie/Resources.language-zh-Hans.resw#L420>) | 指定等比缩放到充满屏幕后的缩放倍数 | 尺寸／光标语义，保留或仅改运行语境 |
| [Profile_General_AutoScale.Header](<../../../src/Magpie/Resources.language-zh-Hans.resw#L429>) | 位于前台时自动缩放 | 启动动作／显示模式／运行状态 |
| [Profile_General_ScalingMode.Header](<../../../src/Magpie/Resources.language-zh-Hans.resw#L450>) | 缩放模式 | 效果组对象 |
| [Profile_Cursor_DrawCursor_AdjustCursorSpeed.Header](<../../../src/Magpie/Resources.language-zh-Hans.resw#L564>) | 缩放时调整光标速度 | 尺寸／光标语义，保留或仅改运行语境 |
| [Profile_Cursor_DrawCursor_ScaleFactor.Header](<../../../src/Magpie/Resources.language-zh-Hans.resw#L576>) | 缩放系数 | 尺寸／光标语义，保留或仅改运行语境 |
| [Profile_Cursor_DrawCursor_ScaleFactor_NoScaling.Content](<../../../src/Magpie/Resources.language-zh-Hans.resw#L582>) | 无缩放 | 尺寸／光标语义，保留或仅改运行语境 |
| [Home_Advanced_SimulateExclusiveFullscreen.Description](<../../../src/Magpie/Resources.language-zh-Hans.resw#L624>) | 仅适用于全屏模式缩放。启用后可以阻止某些应用的通知和弹窗 | 启动动作／显示模式／运行状态 |
| [Home_Advanced_SimulateExclusiveFullscreen.Header](<../../../src/Magpie/Resources.language-zh-Hans.resw#L627>) | 缩放时模拟独占全屏 | 启动动作／显示模式／运行状态 |
| [ScalingModes_HasUnkownEffects.Title](<../../../src/Magpie/Resources.language-zh-Hans.resw#L682>) | 此缩放模式包含未知效果，暂时无法使用。请删除未知效果，或恢复对应效果文件并重新启动 Magpie。 | 效果组对象 |
| [Home_Advanced_AllowScalingMaximized.Header](<../../../src/Magpie/Resources.language-zh-Hans.resw#L730>) | 允许缩放最大化或全屏的窗口 | 启动动作／显示模式／运行状态 |
| [Home_TouchSupport_Info.Title](<../../../src/Magpie/Resources.language-zh-Hans.resw#L790>) | 如果触控支持失效，Magpie 可能会在缩放前请求管理员权限以执行修复。 | 启动动作／显示模式／运行状态 |
| [Message_InvalidScalingMode](<../../../src/Magpie/Resources.language-zh-Hans.resw#L796>) | 当前配置没有可用的缩放模式。请选择其他缩放模式，或为当前模式添加并修复效果。 | 效果组对象 |
| [Message_InvalidSourceWindow](<../../../src/Magpie/Resources.language-zh-Hans.resw#L802>) | 无法缩放这个窗口。请确认它仍在运行、未停止响应、位于显示器内且窗口尺寸不小于 64×64。 | 错误处理，区分源窗口状态与输出模式 |
| [Message_Maximized](<../../../src/Magpie/Resources.language-zh-Hans.resw#L805>) | 当前禁止缩放最大化或全屏窗口。请先还原窗口，或在“主页 → 高级”中启用“允许缩放最大化或全屏的窗口”。 | 错误处理，区分源窗口状态与输出模式 |
| [Message_ScalingFailedGeneral](<../../../src/Magpie/Resources.language-zh-Hans.resw#L811>) | 缩放初始化失败。请退出缩放、重新打开目标窗口并重试；如果仍然失败，请重新启动 Magpie。 | 错误处理，区分源窗口状态与输出模式 |
| [Message_CreateFenceFailed](<../../../src/Magpie/Resources.language-zh-Hans.resw#L817>) | 所选显卡或驱动不支持缩放所需的同步功能。请在当前配置的“性能”中切换显卡，或更新显卡驱动后重试。 | 错误处理，区分源窗口状态与输出模式 |
| [Message_ScalingFailed](<../../../src/Magpie/Resources.language-zh-Hans.resw#L823>) | 缩放失败 | 错误处理，区分源窗口状态与输出模式 |
| [Message_BannedInWindowedMode](<../../../src/Magpie/Resources.language-zh-Hans.resw#L868>) | 当前窗口已最大化或处于全屏状态。请先还原窗口或退出全屏，也可以改用全屏模式缩放。 | 错误处理，区分源窗口状态与输出模式 |
| [Home_Activation.Description](<../../../src/Magpie/Resources.language-zh-Hans.resw#L874>) | 按下快捷键可缩放前台窗口或停止缩放。 | 启动动作／显示模式／运行状态 |
| [Home_Activation_FullscreenScaling.Header](<../../../src/Magpie/Resources.language-zh-Hans.resw#L877>) | 全屏模式缩放快捷键 | 启动动作／显示模式／运行状态 |
| [Home_Activation_FullscreenScaling_ShortcutControl.Title](<../../../src/Magpie/Resources.language-zh-Hans.resw#L883>) | 全屏模式缩放快捷键 | 启动动作／显示模式／运行状态 |
| [Home_Activation_WindowedScaling.Header](<../../../src/Magpie/Resources.language-zh-Hans.resw#L886>) | 窗口模式缩放快捷键 | 启动动作／显示模式／运行状态 |
| [Home_Activation_WindowedScaling_ShortcutControl.Title](<../../../src/Magpie/Resources.language-zh-Hans.resw#L889>) | 窗口模式缩放快捷键 | 启动动作／显示模式／运行状态 |
| [Home_Advanced_AllowScalingMaximized.Description](<../../../src/Magpie/Resources.language-zh-Hans.resw#L892>) | 仅适用于全屏模式缩放 | 启动动作／显示模式／运行状态 |
| [Home_Toolbar.Description](<../../../src/Magpie/Resources.language-zh-Hans.resw#L895>) | 工具栏位于缩放窗口顶部，提供帧率显示、截图等功能，窗口模式缩放时还可以用于拖拽缩放窗口。 | 启动动作／显示模式／运行状态 |
| [Message_ToolbarIn3DGameMode](<../../../src/Magpie/Resources.language-zh-Hans.resw#L919>) | 3D 游戏模式不支持工具栏。请停止缩放并关闭 3D 游戏模式，或使用缩放快捷键进行控制。 | 错误处理，区分源窗口状态与输出模式 |
| [Overlay_Toolbar_Close](<../../../src/Magpie/Resources.language-zh-Hans.resw#L925>) | 停止缩放 | 启动动作／显示模式／运行状态 |
| [Message_Windowed3DGameMode](<../../../src/Magpie/Resources.language-zh-Hans.resw#L940>) | 当前配置启用了 3D 游戏模式，无法使用窗口模式缩放。请关闭 3D 游戏模式，或改用全屏模式缩放。 | 错误处理，区分源窗口状态与输出模式 |
| [Profile_General_3DGameMode.Description](<../../../src/Magpie/Resources.language-zh-Hans.resw#L943>) | 针对 3D 游戏优化，不支持工具栏和窗口模式缩放 | 启动动作／显示模式／运行状态 |
| [Profile_WindowedScaling.Header](<../../../src/Magpie/Resources.language-zh-Hans.resw#L967>) | 窗口模式缩放 | 启动动作／显示模式／运行状态 |
| [Profile_WindowedScaling_InitialScaleFactor.Header](<../../../src/Magpie/Resources.language-zh-Hans.resw#L970>) | 初始缩放倍数 | 尺寸／光标语义，保留或仅改运行语境 |
| [Profile_General_AutoScale_Fullscreen.Content](<../../../src/Magpie/Resources.language-zh-Hans.resw#L985>) | 全屏模式缩放 | 启动动作／显示模式／运行状态 |
| [Profile_General_AutoScale_Windowed.Content](<../../../src/Magpie/Resources.language-zh-Hans.resw#L988>) | 窗口模式缩放 | 启动动作／显示模式／运行状态 |
| [Profile_General_Multimonitor.Description](<../../../src/Magpie/Resources.language-zh-Hans.resw#L991>) | 仅适用于全屏模式缩放 | 启动动作／显示模式／运行状态 |
| [Overlay_Toolbar_SwitchToFullscreen](<../../../src/Magpie/Resources.language-zh-Hans.resw#L1000>) | 切换到全屏模式缩放 | 启动动作／显示模式／运行状态 |
| [Overlay_Toolbar_SwitchToWindowed](<../../../src/Magpie/Resources.language-zh-Hans.resw#L1003>) | 切换到窗口模式缩放 | 启动动作／显示模式／运行状态 |
| [Home_Toolbar_InitialState_Fullscreen.Header](<../../../src/Magpie/Resources.language-zh-Hans.resw#L1006>) | 全屏模式缩放 | 启动动作／显示模式／运行状态 |
| [Home_Toolbar_InitialState_Windowed.Header](<../../../src/Magpie/Resources.language-zh-Hans.resw#L1009>) | 窗口模式缩放 | 启动动作／显示模式／运行状态 |
| [Message_ExportScalingModesFailed](<../../../src/Magpie/Resources.language-zh-Hans.resw#L1012>) | 导出缩放模式失败。请选择可写入的位置，并检查文件是否被占用以及磁盘空间是否充足。 | 效果组对象 |
| [Message_ImportScalingModesFailed](<../../../src/Magpie/Resources.language-zh-Hans.resw#L1015>) | 导入缩放模式失败。请选择有效且可读取的缩放模式 JSON 文件，并确认文件内容没有损坏。 | 效果组对象 |
| [Message_WindowedDesktopDuplication](<../../../src/Magpie/Resources.language-zh-Hans.resw#L1021>) | Desktop Duplication 不支持窗口模式缩放。请将捕获方式改为“默认”或 Graphics Capture，或改用全屏模式缩放。 | 错误处理，区分源窗口状态与输出模式 |
| [Profile_General_DesktopDuplicationWarning.Title](<../../../src/Magpie/Resources.language-zh-Hans.resw#L1024>) | Desktop Duplication 不支持窗口模式缩放。请改用“默认”或 Graphics Capture，或使用全屏模式缩放。 | 错误处理，区分源窗口状态与输出模式 |
| [Home_Activation_Timer_Description](<../../../src/Magpie/Resources.language-zh-Hans.resw#L1030>) | {} 秒后缩放前台窗口 | 启动动作／显示模式／运行状态 |
| [NotifyIcon_Timer_Fullscreen](<../../../src/Magpie/Resources.language-zh-Hans.resw#L1036>) | {} 秒后缩放 (全屏模式) | 启动动作／显示模式／运行状态 |
| [NotifyIcon_Timer_Windowed](<../../../src/Magpie/Resources.language-zh-Hans.resw#L1039>) | {} 秒后缩放 (窗口模式) | 启动动作／显示模式／运行状态 |
| [Profile_Advanced_DestAlignment.Description](<../../../src/Magpie/Resources.language-zh-Hans.resw#L1051>) | 仅适用于全屏模式缩放 | 启动动作／显示模式／运行状态 |
| [Home_Advanced_DeveloperOptions_DisableTopmost.Content](<../../../src/Magpie/Resources.language-zh-Hans.resw#L1084>) | 禁用缩放窗口置顶 | 启动动作／显示模式／运行状态 |
| [Settings_General_SmoothMotionCompatibilityMode.Description](<../../../src/Magpie/Resources.language-zh-Hans.resw#L1090>) | 每次缩放结束后重启 Magpie，释放 NVIDIA Smooth Motion 驱动驻留的显存。仅在已为 Magpie 启用 Smooth Motion 时开启。 | 启动动作／显示模式／运行状态 |
| [Overlay_EffectParameters_RestartPending](<../../../src/Magpie/Resources.language-zh-Hans.resw#L1098>) | 有 {} 项等待重新缩放 | 参数会话／重新启用 |
| [Overlay_EffectParameters_Revert](<../../../src/Magpie/Resources.language-zh-Hans.resw#L1100>) | 恢复本次缩放初始值 | 参数会话／重新启用 |
| [Overlay_EffectParameters_ApplyAndRestart](<../../../src/Magpie/Resources.language-zh-Hans.resw#L1101>) | 应用并重新缩放 | 参数会话／重新启用 |
| [Overlay_EffectParameters_SessionExpired](<../../../src/Magpie/Resources.language-zh-Hans.resw#L1105>) | 缩放会话已变化，参数未保存。 | 参数会话／重新启用 |
| [Overlay_EffectParameters_ScalingModeConflict](<../../../src/Magpie/Resources.language-zh-Hans.resw#L1107>) | 缩放期间模式结构已变化，未覆盖任何配置。 | 参数会话／重新启用 |
| [Message_ConflictingFrameGenerationEffects](<../../../src/Magpie/Resources.language-zh-Hans.resw#L1120>) | 同一缩放模式只能包含一个帧生成效果器。请移除 DLSSFG、XeSSFG x2 或 XeSS 多帧生成中的冲突项后重试。 | 效果组对象 |
| [Message_ScalingModeNotSelected](<../../../src/Magpie/Resources.language-zh-Hans.resw#L1334>) | 此配置尚未选择有效的缩放模式。请在配置中选择已有的缩放模式，然后重新开始缩放。 | 效果组对象 |
| [Message_ScalingModeEmpty](<../../../src/Magpie/Resources.language-zh-Hans.resw#L1335>) | 当前缩放模式没有效果器。请在缩放模式中添加效果器，或选择其他模式。 | 效果组对象 |
| [Message_ScalingModeUnknownEffect](<../../../src/Magpie/Resources.language-zh-Hans.resw#L1336>) | 缩放模式引用了不可用的效果器。请从完整发行包恢复对应的 effects 文件，或替换该效果器。 | 效果组对象 |
| [Message_PresentationInitFailed](<../../../src/Magpie/Resources.language-zh-Hans.resw#L1338>) | 无法创建画面输出。请尝试切换窗口或全屏缩放；若使用了补帧，请先尝试不含补帧的模式。 | 错误处理，区分源窗口状态与输出模式 |
| [Message_EffectCompileFailed](<../../../src/Magpie/Resources.language-zh-Hans.resw#L1339>) | 效果器编译失败。请在详情中查看名称，从对应发行包恢复该效果器，或将其移出缩放模式后重试。具体编译错误见日志。 | 效果组对象 |
| [Message_OverlayInitFailed](<../../../src/Magpie/Resources.language-zh-Hans.resw#L1343>) | 工具栏或光标叠加层初始化失败。请重新解压完整发行包后再次缩放；若仍失败，请提供详情和日志。 | 错误处理，区分源窗口状态与输出模式 |
| [Message_DlssNrUnavailable](<../../../src/Magpie/Resources.language-zh-Hans.resw#L1345>) | DLSSNR 初始化失败，当前画面仅透传，未进行 NR 处理。运行中参数暂不可用。请检查显卡支持情况和完整发行文件，然后重新启动缩放。 | 错误处理，区分源窗口状态与输出模式 |
| [Message_FrameGenerationDisabled](<../../../src/Magpie/Resources.language-zh-Hans.resw#L1346>) | 补帧连续失败，已在本次缩放中停用，原始帧会继续显示。请尝试降低倍率或更换光流设置，然后重新启动缩放。 | 错误处理，区分源窗口状态与输出模式 |
| [Message_EffectParameterConflict](<../../../src/Magpie/Resources.language-zh-Hans.resw#L1348>) | 同一参数已在其他位置被修改，本面板的修改未覆盖保存。请结束并重新开始缩放以载入最新保存值，再进行调整。 | 错误处理，区分源窗口状态与输出模式 |
| [Message_EffectParameterLiveFailed](<../../../src/Magpie/Resources.language-zh-Hans.resw#L1349>) | 效果器拒绝了本次实时更新，仍使用此前的运行值。已保存的设置可能与运行值不同；请重新启动缩放以应用，或恢复此前可用的参数值。 | 错误处理，区分源窗口状态与输出模式 |
| [Message_ScreenshotReadbackFailed](<../../../src/Magpie/Resources.language-zh-Hans.resw#L1353>) | 无法从显卡读取截图画面。请重新开始缩放后重试；若仍失败，可尝试降低输出尺寸、减少效果器，并提供详情和日志。 | 错误处理，区分源窗口状态与输出模式 |
| [Overlay_EffectParameters_BackendUnavailable](<../../../src/Magpie/Resources.language-zh-Hans.resw#L1355>) | DLSSNR 后端未初始化，当前仅透传。请查看主页 → 最近一次问题，处理后重新启动缩放。 | 参数会话／重新启用 |
| [Message_SourceWindowClosed](<../../../src/Magpie/Resources.language-zh-Hans.resw#L1356>) | 目标窗口已关闭。请重新打开，将其切到前台后再开始缩放。 | 错误处理，区分源窗口状态与输出模式 |
| [Message_SourceWindowUnresponsive](<../../../src/Magpie/Resources.language-zh-Hans.resw#L1357>) | 目标窗口暂时没有响应。请等待其恢复，或重新启动目标程序后再缩放。 | 错误处理，区分源窗口状态与输出模式 |
| [Message_SourceWindowUnsupported](<../../../src/Magpie/Resources.language-zh-Hans.resw#L1360>) | 不支持缩放这种透明窗口。请选择程序的主窗口，避开透明叠加层或悬浮组件。 | 错误处理，区分源窗口状态与输出模式 |
| [Message_SourceWindowGeometryFailed](<../../../src/Magpie/Resources.language-zh-Hans.resw#L1361>) | 无法读取或调整目标窗口的位置和尺寸。请还原窗口并移到单个屏幕内再试；全屏缩放失败时可尝试窗口缩放。 | 错误处理，区分源窗口状态与输出模式 |
| [Message_ScalingAlreadyActive](<../../../src/Magpie/Resources.language-zh-Hans.resw#L1362>) | 已存在其他缩放窗口。请先停止当前缩放，并关闭其他 Magpie 实例后重试。 | 错误处理，区分源窗口状态与输出模式 |
| [Message_ScalingWindowCreationFailed](<../../../src/Magpie/Resources.language-zh-Hans.resw#L1363>) | 无法创建缩放窗口。请重新运行 Magpie，并尝试窗口缩放；若仍失败，请提供详情和日志。 | 错误处理，区分源窗口状态与输出模式 |
| [Message_DisplayLayoutFailed](<../../../src/Magpie/Resources.language-zh-Hans.resw#L1364>) | 当前显示器布局无法用于全屏缩放。请改用最近的显示器或尝试窗口缩放，并确认所选显示器已连接。 | 错误处理，区分源窗口状态与输出模式 |
| [Message_ImportEmpty](<../../../src/Magpie/Resources.language-zh-Hans.resw#L1366>) | 所选文件为空或不包含任何缩放模式。请从 Magpie 导出模式，或取得完整的缩放模式文件后重试。 | 效果组对象 |
| [Message_ImportWrongFileType](<../../../src/Magpie/Resources.language-zh-Hans.resw#L1368>) | 此 JSON 不包含 scalingModes 数组。请选择由 Magpie 导出的缩放模式文件；将其他文件改名为 .json 不会转换其格式。 | 效果组对象 |
| [Message_ImportIncompatible](<../../../src/Magpie/Resources.language-zh-Hans.resw#L1369>) | 缩放模式条目的结构不兼容。请使用兼容的 Magpie 版本重新导出。本次未导入该文件中的任何模式。 | 效果组对象 |
| [Message_ExportWriteFailed](<../../../src/Magpie/Resources.language-zh-Hans.resw#L1370>) | 缩放模式文件未能完整写入。请检查路径、磁盘空间和写入权限，并重新导出到可写入的目录。 | 效果组对象 |
