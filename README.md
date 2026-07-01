# qt-ffconv

**ffconv** —— 一个视频转码小工具：Qt 6 Widgets 前端，通过 `QProcess` 调用本机的 `ffmpeg` / `ffprobe` 完成批量转码。跨平台(Linux x86_64/aarch64、Windows、macOS),硬件后端按平台自动探测。

## 功能

- **批量队列**：拖拽文件或「添加文件」按钮加入多个视频，每行显示状态与进度，底部为总进度条。
- **转码参数**
  - 视频编码：H.264 / H.265(HEVC) / AV1
  - 加速后端(**动态探测,仅显示本机可用项**)：软件(libx264/x265/SVT-AV1) / Intel QSV / VAAPI / NVIDIA NVENC / AMD AMF / Rockchip RKMPP / Apple VideoToolbox / V4L2 M2M
  - 质量：CRF / CQ / global_quality（数值越小质量越高），按后端自动映射到对应参数
  - 音频：直接复制 / AAC / Opus（可设码率）
  - 输出容器：mp4 / mkv / mov / webm（HEVC 进 mp4/mov 自动加 `hvc1` 标签）
  - 输出目录（留空 = 与源文件同目录；自动避免覆盖源文件）
- **两层后端探测**：先解析 `ffmpeg -encoders` 得知编译进了哪些编码器,再探测硬件是否在场(GPU 厂商 ID、VPU 设备节点等),**两者都满足**的后端才出现在下拉里,避免选到编不出的死选项。
- **并发转码**：「并发」下拉可选 默认 / 1 / 2 / 4 / 8,多路 ffmpeg 进程并行(适合多编码引擎的显卡);「默认」按 GPU 型号自动估计引擎数,软件后端强制单路以免拖垮 CPU。
- **队列勾选**：每行带复选框，仅勾选的视频参与转码；支持「全选/全不选/反选」、鼠标框选 + 空格批量勾选。转码完成后自动取消勾选，重新勾选即可换格式重转。
- **本机编解码能力检测**：弹窗显示 ffmpeg 版本、过滤出的硬件编/解码器,以及全部编码器 / 解码器列表。窗口底部状态栏常驻显示 ffmpeg 版本。
- **软件编码守卫**：若本机有可用硬件编码器却仍选择了软件后端(常是忘了选加速),点「开始转码」前弹窗确认——继续将由 CPU 软件编码,速度较慢并可能卡顿;默认按钮为「取消」防误点。仅在有硬件可选时提示。
- **分级日志**：5 级(FATAL / ERROR / WARN / INFO / DEBUG)。
  - 窗口按级别过滤(默认 INFO 清爽;ffmpeg 原始命令行/输出为 DEBUG),**点击队列某行只看该视频的日志**,未选为总览;右侧「打开日志目录」直达文件。
  - 落盘到系统标准用户目录(见下方「日志文件位置」):**一次开→关程序 = 一个 `ffconv-<时间戳>.log`**,流式写入(崩溃不丢)、始终记全量;整个会话没转码则不建文件。
  - 设置 → 日志…:本地保留个数(0 = 关闭写入,默认 10,上限 255),超限自动清理最旧文件。
- **多语言**：设置 → 语言 可切换 **简体中文 / English**（基于 Qt Linguist，切换后重启生效，默认中文）。
- **取消**：随时终止进行中的转码。

## 依赖

通用:**Qt 6**(Widgets + LinguistTools)、**CMake ≥ 3.21**、**Ninja**、C++17 编译器,以及 PATH 中编译进所需编解码器的系统 **`ffmpeg` / `ffprobe`**。硬件后端还需对应**驱动**在场(NVIDIA / Intel / AMD 驱动、Rockchip MPP 等)。

### 各发行版安装命令

**Debian / Ubuntu**

```bash
sudo apt install build-essential cmake ninja-build \
    qt6-base-dev qt6-tools-dev qt6-tools-dev-tools libgl1-mesa-dev
sudo apt install ffmpeg
```

**Fedora / RHEL**

```bash
sudo dnf install gcc-c++ cmake ninja-build \
    qt6-qtbase-devel qt6-qttools-devel libglvnd-devel mesa-libGL-devel
# 完整版 ffmpeg 走 RPM Fusion(含 x264/x265 及硬件编解码)
sudo dnf install https://mirrors.rpmfusion.org/free/fedora/rpmfusion-free-release-$(rpm -E %fedora).noarch.rpm
sudo dnf install ffmpeg
```

**Arch Linux**

```bash
sudo pacman -S base-devel cmake ninja qt6-base qt6-tools ffmpeg
```

**Windows(x86_64)**:用在线安装器装 Qt 6(MSVC 版)+ VS 2022 Build Tools;ffmpeg 用 [gyan.dev](https://www.gyan.dev/ffmpeg/builds/) `ffmpeg-release-full` 或 [BtbN](https://github.com/BtbN/FFmpeg-Builds) gpl 版(含 nvenc/qsv/amf),放进 PATH。

### 自行编译:Rockchip(aarch64,RKMPP)

发行版 ffmpeg 不含 `rkmpp`,需自编 **MPP** + **ffmpeg-rockchip**。先装基础依赖(Debian 系,含 `libdrm-dev`、`pkg-config`):

```bash
sudo apt install build-essential cmake ninja-build pkg-config libdrm-dev \
    qt6-base-dev qt6-tools-dev qt6-tools-dev-tools
```

**① Rockchip MPP**(装到 `/usr`):

```bash
git clone -b jellyfin-mpp https://github.com/nyanmisaka/mpp.git
cmake -S mpp -B mpp-build -G Ninja \
      -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr
cmake --build mpp-build
sudo cmake --install mpp-build
```

> 用独立的 `mpp-build` 目录,别在仓库自带的 `build/` 内构建——那里放着 `version.in`、`merge_objects.cmake` 等 cmake 辅助文件,清掉会导致配置失败。

**② ffmpeg-rockchip**(装到 `/usr/local`):

```bash
git clone https://github.com/nyanmisaka/ffmpeg-rockchip.git
cd ffmpeg-rockchip
./configure --prefix=/usr/local --enable-gpl --enable-version3 \
            --enable-rkmpp --enable-libdrm --disable-vulkan
make -j$(nproc)
sudo make install
```

> `--disable-vulkan` 必需:板上 Vulkan 头偏旧会在编译时报错,且 RK3588 硬件编解码走 rkmpp 而非 Vulkan。

**③ 运行时权限**:确保普通用户能访问 `/dev/mpp_service` —— 加 udev 规则 `KERNEL=="mpp_service", MODE="0660", GROUP="video"` 并把用户加入 `video` 组(`sudo usermod -aG video $USER`,重新登录生效),否则 rkmpp 会报 "Generic error in an external library"。

## 构建

命令行 + `CMakePresets.json`。共三个预设:

| 预设 | 用途 | Qt 来源 |
|------|------|---------|
| `debug` | 开发调试(输出 `build/debug`) | 预设中 `CMAKE_PREFIX_PATH` 指定的 Qt |
| `release` | 本地发布(输出 `build/release`) | 同上 |
| `release-system` | 分发用(输出 `build/release-system`) | **系统 Qt**(无 RPATH) |

`debug` / `release` 用的 Qt 路径写在 `CMakePresets.json` 的 `base` 预设里(默认 `~/Qt/6.11.1/gcc_64`),按你本机 Qt 安装位置改;`release-system` 链接系统 Qt,不依赖该路径。

```bash
# 开发构建
cmake --preset debug
cmake --build --preset debug

# 分发构建(系统 Qt)
cmake --preset release-system
cmake --build --preset release-system
```

**Windows(x86_64)**:装 Qt 6(MSVC 版)+ VS 2022 Build Tools,配置时用 `-DCMAKE_PREFIX_PATH=<Qt路径>`,构建后用 Qt 自带 `windeployqt` 收集 DLL:

```powershell
cmake -S . -B build\win -G Ninja -DCMAKE_BUILD_TYPE=Release ^
      -DCMAKE_PREFIX_PATH="C:\Qt\6.8.2\msvc2022_64"
cmake --build build\win
windeployqt --release build\win\ffconv.exe
```

## 运行

```bash
./build/debug/ffconv          # 或 build/release/ffconv、build/release-system/ffconv
```

`debug` / `release` 二进制已嵌入 Qt 库 RPATH,直接运行;`release-system` 依赖系统 Qt 运行库。Linux 可 `cmake --install build/release-system --prefix ~/.local` 装入桌面菜单(带图标)。运行前确保 `ffmpeg` / `ffprobe` 在 PATH 中。

## 日志文件位置

日志目录取 Qt 的 `QStandardPaths`(跨平台,三端无需改代码),子目录固定为 `ffconv/ffconv/`,文件名 `ffconv-<时间戳>.log`。`StateLocation` 为 Qt 6.7 新增,对更旧的 Qt 编译时自动退回 `AppLocalDataLocation`,故目录随 Qt 版本略有不同:

| 系统 | Qt ≥ 6.7（`StateLocation`） | Qt < 6.7（退回 `AppLocalDataLocation`） |
|------|------------------------------|------------------------------------------|
| **Linux** | `~/.local/state/ffconv/ffconv/` | `~/.local/share/ffconv/ffconv/` |
| **macOS** | `~/Library/Application Support/ffconv/ffconv/` | 同左（一致） |
| **Windows** | `C:\Users\<用户>\AppData\Local\ffconv\ffconv\` | 同左（一致） |

> macOS / Windows 没有独立的 "state" 目录概念,两种 Qt 版本解析到同一位置;仅 Linux 在 6.7 以下从 `state` 变为 `share`。界面里「打开日志目录」按钮会直接定位到当前实际目录(Finder / 资源管理器 / 文件管理器)。

## 许可证

本项目以 **GPL-3.0-or-later** 发布（见 `LICENSE`），与常用的 FFmpeg
（`--enable-gpl --enable-version3` 构建，含 x264/x265 等 GPL 组件）保持一致。
本程序通过外部进程调用系统 FFmpeg，FFmpeg 及其编解码库遵循各自许可证。
