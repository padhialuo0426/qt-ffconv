# qt-ffconv

**ffconv** —— 一个视频转码小工具：Qt 6 Widgets 前端，通过 `QProcess` 调用本机的 `ffmpeg` / `ffprobe` 完成批量转码。

## 功能

- **批量队列**：拖拽文件或「添加文件」按钮加入多个视频，排队逐个转码，每行显示状态与进度，底部为总进度条。
- **转码参数**
  - 视频编码：H.264 / H.265(HEVC) / AV1
  - 加速后端：软件(libx264/x265/SVT-AV1) / Intel QSV / Intel VAAPI / NVIDIA NVENC
  - 质量：CRF / CQ / global_quality（数值越小质量越高），按后端自动映射到 `-crf` / `-global_quality` / `-cq` / `-qp`
  - 音频：直接复制 / AAC / Opus（可设码率）
  - 输出容器：mp4 / mkv / mov / webm（HEVC 进 mp4/mov 自动加 `hvc1` 标签）
  - 输出目录（留空 = 与源文件同目录；自动避免覆盖源文件）
- **队列勾选**：每行带复选框，仅勾选的视频参与转码；支持「全选/全不选/反选」、鼠标框选 + 空格批量勾选。转码完成后自动取消勾选，重新勾选即可换格式重转。
- **本机编解码能力检测**：「检测本机编解码能力…」按钮，弹窗显示 ffmpeg 版本、过滤出的硬件编/解码器，以及全部编码器 / 解码器列表。窗口底部状态栏常驻显示 ffmpeg 版本。
- **顶部菜单**：文件 / 设置 / 帮助。
  - 设置 → 语言：**简体中文 / English**（基于 Qt Linguist，切换后重启生效，默认中文）。
  - 帮助：使用说明、FFmpeg 官方文档、项目仓库、开源许可、关于。
- **实时日志**与**取消**。

## 依赖

- Qt 6（本机安装于 `~/Qt/6.11.1/gcc_64`）
- CMake ≥ 3.21、Ninja、g++
- 系统 `ffmpeg` / `ffprobe` 在 PATH 中（本机为 RPM Fusion 完整版，含 H.264/HEVC 硬件编解码）
- OpenGL 开发包：`libglvnd-devel`、`mesa-libGL-devel`

## 构建

### CLion（推荐）

直接用 CLion 打开本项目目录，它会自动识别仓库中的 `CMakePresets.json`，
在 *Settings → Build → CMake* 里启用 **Debug** / **Release** 两个预设即可，
Qt 路径与 Ninja 生成器都已在预设里配好，无需额外设置。

### 命令行

```bash
cmake --preset debug      # 或 release
cmake --build --preset debug
```

可用预设：`debug`（输出到 `build/debug`）、`release`（输出到 `build/release`）。

## 运行

```bash
./build/debug/ffconv      # 或 build/release/ffconv
```

二进制已嵌入 Qt 库的 RPATH，无需额外设置环境变量。

## 代码结构

| 文件 | 职责 |
|------|------|
| `src/main.cpp` | 程序入口 |
| `src/EncodeSettings.{h,cpp}` | 编码参数 → ffmpeg 命令行参数的映射 |
| `src/TranscodeJob.h` | 单个转码任务的数据结构与状态 |
| `src/FfmpegProcess.{h,cpp}` | QProcess 封装：ffprobe 取时长、ffmpeg 转码、`-progress` 进度解析 |
| `src/MainWindow.{h,cpp}` | 主界面：菜单栏、队列、参数面板、能力检测对话框、日志 |
| `translations/ffconv_en.ts` | 英文翻译源（`lrelease` 后内嵌为 `:/i18n` 资源） |

## 国际化

界面字符串均包裹在 `tr()` 中，英文翻译位于 `translations/ffconv_en.ts`。
构建时由 `qt_add_translations` 自动 `lrelease` 成 `.qm` 并内嵌进程序。
新增/修改界面文案后，运行 `cmake --build build/debug --target update_translations`
即可用 `lupdate` 刷新 `.ts`，再补译。

## 许可证

本项目以 **GPL-3.0-or-later** 发布（见 `LICENSE`），与本机所用的 FFmpeg
（`--enable-gpl --enable-version3` 构建，含 x264/x265 等 GPL 组件）保持一致。
本程序通过外部进程调用系统 FFmpeg，FFmpeg 及其编解码库遵循各自许可证。
