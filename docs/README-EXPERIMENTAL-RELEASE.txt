Magpie Experimental v0.6.5 x64

安装或从旧版升级
建议先删除此前所有版本的 Magpie 程序目录（包括旧内测版），再安装本版，不要直接覆盖旧目录。
1. 如需保留配置或截图，先备份到程序目录之外。
2. 从托盘完全退出 Magpie，删除所有旧版程序目录。
3. 将完整主包解压到新目录，运行 Magpie.exe。
普通配置：%LOCALAPPDATA%\Magpie\config\v4\config.json。
便携配置：原程序目录内 config\config.json。
可保留或恢复配置，但不要复制旧版效果目录、DLL 或深度组件。

使用入口
- 效果组：选择或组合效果器；从工具栏打开“效果参数”边看画面边调节，修改自动保存。
- 对比：切换原图与处理后画面，切回无需重新加载效果。
- Front Edge Sync：默认开启、60 FPS，目标程序需配合限帧；启用补帧时控制真实帧输入，可能增加延迟。
- 全屏切屏：Alt+Tab／Win+Tab 会停用全屏效果组，返回后需手动启用。
- 性能与排错：在工具栏查看效果耗时；主页“最近一次问题”提供详情和日志入口。

完整更新、快捷键和可选附件用法见 RELEASE-NOTES.md；帧率设置见 FRAME_SYNC_GUIDE.md。
请保留 LICENSE-Magpie.txt、THIRD-PARTY-NOTICES.md、组件许可证和 build-manifest.json。

English

Install or upgrade
We recommend deleting all previous Magpie program folders, including older beta builds, before installing this version. Do not install over an old folder.
1. Back up any settings or screenshots you want to keep outside the program folders.
2. Fully exit Magpie from the system tray and delete all old program folders.
3. Extract the complete package into a new folder and run Magpie.exe.
Normal settings: %LOCALAPPDATA%\Magpie\config\v4\config.json.
Portable settings: config\config.json in the old program folder.
Keep or restore settings if needed, but do not copy old effects, DLLs or depth components.

Getting started
- Effect groups: select or combine effects; open Effect parameters from the toolbar to adjust the image with automatic saving.
- Comparison: switch between original and processed images without reloading effects.
- Front Edge Sync: enabled at 60 FPS by default; apply a matching source limiter. With FG it controls real input frames and may increase latency.
- Fullscreen switching: Alt+Tab / Win+Tab stop fullscreen effects; enable the group manually after returning.
- Performance and troubleshooting: inspect effect timings from the toolbar; Home's recent-issue card provides details and log access.

See RELEASE-NOTES.md for updates, shortcuts and optional assets, and FRAME_SYNC_GUIDE.md for frame-rate settings.
Retain LICENSE-Magpie.txt, THIRD-PARTY-NOTICES.md, component licenses and build-manifest.json.
