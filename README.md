# ffmpeg-qt6

一个自用的视频转码小工具：Qt 6 Widgets 前端，通过 `QProcess` 调用本机的 `ffmpeg` / `ffprobe` 完成批量转码。

## 功能

- **批量队列**：拖拽文件或「添加文件」按钮加入多个视频，排队逐个转码，每行显示状态与进度，底部为总进度条。
- **转码参数**
  - 视频编码：H.264 / H.265(HEVC) / AV1
  - 加速后端：软件(libx264/x265/SVT-AV1) / Intel QSV / Intel VAAPI / NVIDIA NVENC
  - 质量：CRF / CQ / global_quality（数值越小质量越高），按后端自动映射到 `-crf` / `-global_quality` / `-cq` / `-qp`
  - 音频：直接复制 / AAC / Opus（可设码率）
  - 输出容器：mp4 / mkv / mov / webm（HEVC 进 mp4/mov 自动加 `hvc1` 标签）
  - 输出目录（留空 = 与源文件同目录；自动避免覆盖源文件）
- **本机编解码能力检测**：「检测本机编解码能力…」按钮，弹窗显示 ffmpeg 版本、过滤出的硬件编/解码器，以及全部编码器 / 解码器列表。窗口底部状态栏常驻显示 ffmpeg 版本。
- **实时日志**与**取消**。

## 依赖

- Qt 6（本机安装于 `~/Qt/6.11.1/gcc_64`）
- CMake ≥ 3.21、Ninja、g++
- 系统 `ffmpeg` / `ffprobe` 在 PATH 中（本机为 RPM Fusion 完整版，含 H.264/HEVC 硬件编解码）
- OpenGL 开发包：`libglvnd-devel`、`mesa-libGL-devel`

## 构建

```bash
./build.sh
# 或手动：
cmake -S . -B build -G Ninja -DCMAKE_PREFIX_PATH="$HOME/Qt/6.11.1/gcc_64"
cmake --build build
```

## 运行

```bash
./build/ffmpeg-qt6
```

二进制已嵌入 Qt 库的 RPATH，无需额外设置环境变量。

## 代码结构

| 文件 | 职责 |
|------|------|
| `src/main.cpp` | 程序入口 |
| `src/EncodeSettings.{h,cpp}` | 编码参数 → ffmpeg 命令行参数的映射 |
| `src/TranscodeJob.h` | 单个转码任务的数据结构与状态 |
| `src/FfmpegProcess.{h,cpp}` | QProcess 封装：ffprobe 取时长、ffmpeg 转码、`-progress` 进度解析 |
| `src/MainWindow.{h,cpp}` | 主界面：队列、参数面板、能力检测对话框、日志 |
