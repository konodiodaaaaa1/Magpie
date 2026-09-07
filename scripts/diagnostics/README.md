# NGX OTA 开关

`NGX_OTA_Switch.bat` 沿用 0.6.1 附件，是可选的系统设置工具，不是安装步骤。

运行后通过菜单查看状态、关闭／启用 OTA、清理 `nvngx_update.exe`，或 Restore default 恢复 NVIDIA 默认值。相关操作会请求管理员权限并影响其他 NGX 程序；删除 BAT 不会恢复设置。

只在怀疑 OTA 更新进程积累与异常内存占用有关时使用，不能将它视为所有卡顿或画面异常的通用修复。

---

# NGX OTA Switch

`NGX_OTA_Switch.bat` is the optional system-setting tool supplied with 0.6.1, not an installation step.

Its menu inspects status, disables/enables OTA, cleans up `nvngx_update.exe`, and restores NVIDIA defaults with Restore default. Relevant actions request administrator privileges and affect other NGX applications. Deleting the BAT does not restore settings.

Use only when accumulated OTA update processes are suspected in abnormal memory usage; it is not a universal fix for stalls or image artifacts.
