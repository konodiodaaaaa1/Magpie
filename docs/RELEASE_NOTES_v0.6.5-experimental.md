# Magpie Experimental v0.6.5

## Magpie 本体更新

- **效果组管理**：“缩放模式”更名为“效果组”，用于组合多个效果器；默认提供 Lanczos、FSR、RTX Video VSR Ultra、DLSSFG、XeSSFG 和 DLSSNR。已有自定义效果组会保留。

开发者注：我注意到对于这个功能的命名，实际上会在用户与用户间的沟通中造成误解和混淆，比如：“使用全屏缩放缩放全屏” ，这会增加沟通时的成本。

- **参数调节**：可在工具栏中边看画面边调节参数，修改自动保存，并提供中文翻译和分组显示。双击滑条，可恢复该效果器的默认值。
- **工具栏与对比**：性能监测、效果参数、截屏、固定工具栏和对比均可设置快捷键。一键切换原图与处理后画面。
- **Front Edge Sync**：默认开启并限制为 60 FPS，用于稳定画面节奏；启用补帧时控制补帧前的真实帧率。目标程序需要配合限帧，开启后可能增加延迟；主页和参数面板顶部均可设置。

开发者注：由于这个版本提供了更高的光流估算档位，帧时间可能会不太稳定，所以我非常推荐在使用时通过这个功能来稳定帧间隔。比如，当你的游戏运行在65-70的时候，你应该将帧同步设置为60，这样你会获得平滑的多的帧间隔，但代价是一点延迟。如果你还希望更高的视觉帧率，你可以在对应的效果组尾部增加fg类效果，我会比较推荐使用xessfg，它不这么依赖深度，所以在这里工作的更好。

- **性能监测**：可查看各效果器的处理耗时，方便判断哪些效果占用较多性能。启用 FG 后，以 `120/60` 这样的形式显示“输出帧率／真实帧率”。
- **[暂缓] VRR 与 HDR**：本版暂不提供 VRR 开关和 HDR 功能。

开发者注：VRR和HDR已经做了一些基础的实验，但暂未添加到正式版本中。HDR在magpie中很可能无法完美兼容。

## 效果器更新

- **DLSSNR 画面调整**：新增残差饱和度、亮度、阴影／结构、反射／辉光控制，并改善降低输入分辨率后的细节表现。残差参数可实时调整。

开发者注：现在你可以完全控制dlssnr的输出效果。
如果你觉得dlssnr添加的阴影太多了？那就减少，甚至可以只保留反射和发光效果。
如果你觉得dlssnr添加的反射和发光还不够强烈？那你可以继续增加发光效果。

- **DLSSNR 分辨率采样**：降低输入分辨率时，输入降采样改用 Lanczos-2 AA，输出残差升采样改用 Catmull-Rom，用于改善缩小后的抗锯齿与细节回填。残差总倍率、阴影／结构与反射／辉光、饱和度与亮度调整均在升采样前完成。

- **超分辨率效果**：DLSS SR、FSR 2/3/4、XeSS SR 各保留一个入口，在参数中选择光流方法即可。**旧 Zero MV、jitter、Optical Flow 独立版本及内置光流已移除**，旧配置会自动迁移。

开发者注：这里的SR效果器实际上由于缺失抖动渲染，实际上都没什么用。不过dlsssr的LM模型能力非常强大，可以展现出一点抗锯齿的效果。

- **光流质量与共享**：SR、NR、FG 可选择各自支持的 AMD OF／NVOF，并共用光流估算以减少重复开销。NVOF 新增“最高质量（极高开销）”，不同效果器设置不一致时会提示实际采用的方法。

开发者注：更高的光流档位开销会变得非常大，但提升很小。如果你用来看视频，你可以尝试一下更高档位。如果用于游戏，我建议使用性能/平衡。

- **帧率过滤器**：FrameRate Filter 默认跟随 Front Edge Sync，开启同步时无需单独设置。关闭同步后可选择“自定义”，使用滑条调整帧率。
- **[已移除] 深度估算**：移除深度估算效果及配套 DirectML／TensorRT 组件，旧深度选项会自动清理。

开发者注：经过对深度估算的反复评估，我决定彻底放弃深度输入，虽然深度输入在目前的dll中是生效的，但是基于 Nvidia官方文档 [NVIDIA ADLR：DLSS 5 Generative Neural Rendering](https://research.nvidia.com/labs/adlr/DLSS5/) 中的介绍：“在推理阶段，模型以当前渲染帧、引擎运动矢量、承载的时间状态和艺术指导值为条件。” 我决定放弃深度推理功能，将原本用于深度推理的资源分配给更高质量的运动估计，反而获得了更高的稳定性。


## 错误与稳定性修复

- **全屏切屏**：使用 Alt+Tab、Alt+Shift+Tab 或 Win+Tab 时自动停用全屏效果组，返回后需手动再次启用，以避开切屏花屏问题。窗口效果组不受此规则影响。
- **浮窗与工具栏**：增加输入缓冲区，改善低帧率和静止画面下的参数窗口操作。
- **故障提示**：报错增加原因说明和处理建议；主页“最近一次问题”可查看详情、复制诊断信息、打开日志目录，也可手动关闭。

## 使用说明

### 安装或从旧版升级

## **建议删除此前所有版本的 Magpie 程序目录（包括旧内测版），再安装本版；不要直接覆盖旧目录。**

1. 如需保留设置或截图，先备份到程序目录之外。
2. 从托盘完全退出 Magpie，删除所有旧版程序目录。
3. 将 `Magpie-Experimental-x64.zip` 完整解压到新目录，运行其中的 `Magpie.exe`。

普通配置位于 `%LOCALAPPDATA%\Magpie\config\v4\config.json`；便携配置位于原程序目录的 `config\config.json`。需要沿用设置时可保留或恢复配置，**不要复制旧版效果目录、DLL 或深度组件到新目录**。

### 参数与帧率设置

- **参数生效方式**：Live／实时立即生效；Restart／重启需点击“应用并重新启用”；Auto restart／自动重启会在编辑结束后重新启用效果组。
- **DLSSNR 残差控制**：先开启“调整输入分辨率”，即使比例为 100% 也能使用残差调整。降低比例可减轻性能压力，但会损失部分画面信息。
- **帧同步设置**：可在主页或工具栏的参数面板顶部修改开关和目标 FPS；修改自动保存，点击“应用并重新启用”后统一生效，不需要重启 Magpie。
- **Front Edge Sync**：手动目标表示补帧前帧率，`0` 表示按显示器刷新率自动设置，有 FG 时按倍率折算。例如 80 FPS 配合 2 倍补帧，名义输出为 160 FPS；请同步限制目标程序帧率。
- **补帧搭配**：一个效果组只使用一种 FG；出现延迟或性能压力时，可降低光流质量，或关闭 Front Edge Sync 比较表现。

## 附件的作用与使用

| 附件                                    | 用途与使用方法                                                                                                                                                               |
| --------------------------------------- | ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `Magpie-Experimental-x64.zip`           | **必选主包**，包含程序和所需运行组件；按上面的安装步骤完整解压使用。其余附件均为可选。                                                                                       |
| `DLSSNR-DLL-Options-310.8.0.0.zip`      | 提供 NVIDIA 官方版和 RTX 40/50 社区兼容版 DLL 选项；仅需切换版本时下载。完全退出 Magpie 并备份现有 `nvngx_dlssnr.dll`，再按包内说明选择一个版本放到 `Magpie.exe` 旁。        |
| `NGX_OTA_Switch.bat`                    | 用于查看、开关 NVIDIA NGX OTA 更新及清理更新进程，常规安装无需运行。相关操作需管理员权限且影响系统级 NGX 设置；恢复时使用菜单的 **Restore default**，删除 BAT 不会撤销设置。 |

---

# Magpie Experimental v0.6.5

## Magpie Application Updates

- **Effect groups**: Scaling modes are renamed to Effect groups for combining multiple effects; defaults include Lanczos, FSR, RTX Video VSR Ultra, DLSSFG, XeSSFG and DLSSNR. Existing custom groups are preserved.

Developer's note: I noticed that the old name could cause confusion in conversations between users. Phrases such as “use fullscreen scaling to scale fullscreen” make it harder to explain what you mean.

- **Parameter editing**: adjust parameters from the toolbar while viewing the result, with automatic saving, Chinese translations and grouped controls. Double-click a slider to restore the effect's default value.
- **Toolbar and comparison**: assign shortcuts to the profiler, parameters, screenshots, toolbar pinning and comparison. Switch between original and processed images with one action.
- **Front Edge Sync**: enabled by default at 60 FPS to stabilize frame pacing, controlling real frames before FG when frame generation is active. Apply a matching limiter to the source application; synchronization may increase latency, and its settings are available on Home and at the top of the parameter panel.

Developer's note: This version offers higher optical-flow quality levels that may make frame times less consistent, so I strongly recommend using this feature to stabilize frame intervals. For example, if your game runs at 65–70 FPS, set frame sync to 60 for much smoother intervals at the cost of some latency. For a higher visual frame rate, add an FG effect at the end of the group. I recommend XeSSFG here because it relies less on depth and works better in this context.

- **Performance monitoring**: view each effect's processing time to identify costly effects. With FG, readings such as `120/60` show output FPS / real FPS.
- **[Deferred] VRR and HDR**: this version does not provide the VRR switch or HDR features.

Developer's note: Some initial VRR and HDR experiments have been carried out, but neither is included in the release. Full HDR compatibility in Magpie may not be possible.

## Effect Updates

- **DLSSNR image controls**: new residual saturation, lightness, shadow/structure and reflection/glow controls accompany improved detail at reduced input resolution. Residual controls work live.

Developer's note: You can now fully control the look of DLSSNR's output.
If DLSSNR adds too much shadow, reduce it or keep only reflections and glow.
If its reflections and glow are not strong enough, increase the glow further.

- **DLSSNR resolution sampling**: when reducing input resolution, input downsampling now uses Lanczos-2 AA and output residual upsampling uses Catmull-Rom, to improve antialiasing during reduction and detail reconstruction. The overall residual multiplier, shadow/structure and reflection/glow controls, and saturation/lightness adjustments all run before upsampling.

- **Super resolution**: DLSS SR, FSR 2/3/4 and XeSS SR each have a single entry with optical-flow selection in its parameters. **Separate Zero MV, jitter and Optical Flow variants, plus the old built-in flow, are removed**, with automatic migration of old settings.

Developer's note: Without jittered rendering, these SR effects are of little practical use here. However, DLSS SR's LM model is powerful enough to provide some antialiasing.

- **Optical-flow quality and sharing**: SR, NR and FG can select their supported AMD OF / NVOF options and share estimation to reduce duplicate work. NVOF adds Highest Quality (Very High Cost), and differing effect settings produce a notice showing the selected method.

Developer's note: Higher optical-flow quality levels become very expensive for only a small improvement. You can try higher levels for video; for games, I recommend Performance or Balanced.

- **FrameRate Filter**: follows Front Edge Sync by default, requiring no separate cap while synchronization is enabled. Disable synchronization to choose Custom and adjust the frame-rate slider.
- **[Removed] Depth estimation**: depth effects and their DirectML/TensorRT components are removed, with automatic cleanup of legacy depth settings.

Developer's note: After repeatedly evaluating depth estimation, I decided to drop depth input entirely. Although depth input works in the current DLL, NVIDIA's [DLSS 5 Generative Neural Rendering](https://research.nvidia.com/labs/adlr/DLSS5/) documentation describes inference as being conditioned on the current rendered frame, engine motion vectors, carried temporal state and art-direction values. I decided to remove depth inference and use those resources for higher-quality motion estimation instead, which gave me greater stability.

## Error and Stability Fixes

- **Fullscreen task switching**: Alt+Tab, Alt+Shift+Tab and Win+Tab stop fullscreen effects to avoid task-switching artifacts; enable the group manually after returning. This rule does not affect windowed groups.
- **Overlays and toolbar**: added input buffering to improve parameter-window interaction at low frame rates and with static images.
- **Helpful errors**: messages include causes and suggested actions; Home's recent-issue card offers details, diagnostic copying, log access and manual dismissal.

## Usage

### Install or Upgrade

## **We recommend deleting all previous Magpie program folders, including older beta builds, before installing this version; do not install over an old folder.**

1. If you want to keep settings or screenshots, back them up outside the program folders first.
2. Fully exit Magpie from the system tray and delete all old program folders.
3. Extract `Magpie-Experimental-x64.zip` completely into a new folder and run its `Magpie.exe`.

Normal settings are stored at `%LOCALAPPDATA%\Magpie\config\v4\config.json`; portable settings are in the old program folder's `config\config.json`. Keep or restore the configuration if needed, but **do not copy old effects, DLLs or depth components into the new folder**.

### Parameters and Frame Rates

- **Applying parameters**: Live takes effect immediately; Restart requires Apply and restart; Auto restart re-enables the group after editing finishes.
- **DLSSNR residual controls**: enable Adjust Input Resolution first, even when using 100%. Lowering the percentage reduces processing pressure but loses some image information.
- **Frame-sync settings**: edit the switch and target FPS on Home or at the top of the toolbar's parameter panel. Changes are saved automatically and take effect together with Apply and restart; restarting Magpie is unnecessary.
- **Front Edge Sync**: a manual target is the rate before FG; `0` selects a rate from the display refresh rate, divided by the FG multiplier when applicable. For example, 80 FPS with 2× FG nominally outputs 160 FPS; apply a matching source-application limiter.
- **Combining FG**: use only one FG per group; if latency or GPU pressure is high, lower optical-flow quality or compare with Front Edge Sync disabled.

## Assets: Purpose and Instructions

| Asset | Purpose and Instructions |
| --- | --- |
| `Magpie-Experimental-x64.zip` | **Required main package**, containing the application and its runtime components; extract it completely as described above. All other assets are optional. |
| `DLSSNR-DLL-Options-310.8.0.0.zip` | Contains official NVIDIA and community RTX 40/50-compatible DLL choices; download only when switching versions. Fully exit Magpie, back up `nvngx_dlssnr.dll`, then follow the archive instructions to place one choice beside `Magpie.exe`. |
| `NGX_OTA_Switch.bat` | Inspects or toggles NVIDIA NGX OTA updates and cleans up update processes; normal installation does not require it. Relevant actions require administrator privileges and affect system-wide NGX settings; use **Restore default** to undo changes, since deleting the BAT does not restore settings. |
