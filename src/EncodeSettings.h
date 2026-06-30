#pragma once
#include <QString>
#include <QStringList>

// 视频编码格式
enum class VideoCodec { H264, HEVC, AV1 };
// 硬件加速后端
enum class HwAccel { Software, QSV, VAAPI, NVENC };
// 音频处理方式
enum class AudioMode { Copy, AAC, Opus };

struct EncodeSettings {
    VideoCodec codec = VideoCodec::H264;
    HwAccel    hw = HwAccel::Software;
    int        quality = 23;          // CRF / CQ / global_quality，数值越小质量越高
    AudioMode  audio = AudioMode::Copy;
    int        audioBitrateKbps = 192;
    QString    container = "mp4";     // 输出容器(扩展名)
    QString    vaapiDevice = "/dev/dri/renderD128"; // Intel 核显渲染节点

    // 返回选中的 ffmpeg 编码器名(如 h264_nvenc)
    QString encoderName() const;

    // 构造完整的 ffmpeg 参数列表(不含开头的 "ffmpeg")
    QStringList buildArgs(const QString &input, const QString &output) const;
};
