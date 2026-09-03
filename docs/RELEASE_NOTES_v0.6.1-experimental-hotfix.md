# Magpie Experimental v0.6.1 Hotfix

这是基于 Magpie Experimental v0.6.1 的 GPU 调度优先级热修复。

## 修复内容

- 将 Magpie 的 GPU 进程调度优先级从 `HIGH` 提升为 `REALTIME`，降低与游戏同时运行且 GPU 负载较高时，Magpie 被游戏持续抢占而造成的处理吞吐下降或帧时间波动。
- 此更改只影响 GPU 调度优先级，不会将 Magpie 的 Windows CPU 进程优先级设为“实时”。GPU 完全饱和时，它会改变游戏与 Magpie 之间的 GPU 时间分配。使用最小更新包前建议备份原 `Magpie.exe`；如遇兼容性或流畅度问题，请完全退出 Magpie 后恢复备份。

感谢 Bilibili：**Beadmoce 指出关键线索 GPU 优先级设置**。

## 应该下载哪个文件？

### 已安装 Experimental v0.6.1

下载最小更新包：

`Magpie-v0.6.1-Hotfix-x64.zip`

这是不完整的最小更新包，不能独立运行。请完全退出 Magpie，将包内的 `Magpie.exe` 替换到现有 v0.6.1 安装目录并允许覆盖。

### 首次安装或从更早版本更新

请直接前往 [Magpie Experimental v0.6.1 发布页](https://github.com/SAOG0721/Magpie/releases/tag/v0.6.1-experimental)，下载完整主包：

`Magpie-Experimental-x64.zip`

该发布页中的 v0.6.1 完整主包已替换为包含本 Hotfix 的版本，无需再下载最小更新包。将主包完整解压到新目录，再运行其中的 `Magpie.exe`。不要直接在压缩包内运行，也不要只替换旧版本的主程序。

---

# Magpie Experimental v0.6.1 Hotfix

This GPU scheduling-priority hotfix is based on Magpie Experimental v0.6.1.

## Fix

- Raised Magpie's GPU process scheduling priority from `HIGH` to `REALTIME`, reducing throughput loss or frame-time instability caused by games continuously preempting Magpie while both applications run under high GPU load.
- This change affects only GPU scheduling priority; it does not set Magpie's Windows CPU process priority to Realtime. When the GPU is fully saturated, it changes how GPU time is shared between the game and Magpie. Back up the existing `Magpie.exe` before applying the minimal update. If compatibility or smoothness issues occur, fully exit Magpie and restore that backup.

Thanks to Bilibili user **Beadmoce** for identifying the key lead: GPU priority configuration.

## Which File Should I Download?

### Experimental v0.6.1 is already installed

Download the minimal update:

`Magpie-v0.6.1-Hotfix-x64.zip`

This is an incomplete minimal update and cannot run by itself. Fully exit Magpie, then replace `Magpie.exe` in the existing v0.6.1 installation directory with the file from this package and allow overwrite.

### First installation or update from an earlier version

Go directly to the [Magpie Experimental v0.6.1 release](https://github.com/SAOG0721/Magpie/releases/tag/v0.6.1-experimental) and download the complete main package:

`Magpie-Experimental-x64.zip`

The complete v0.6.1 main package on that release has been replaced with a build that already includes this Hotfix, so the minimal update is not needed. Extract the complete package into a new directory, then run `Magpie.exe` from that directory. Do not run Magpie from inside the ZIP or replace only the executable of an earlier version.
