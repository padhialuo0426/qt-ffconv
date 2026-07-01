#pragma once
#include <QString>
#include <QStringList>

// 视频编码格式
enum class VideoCodec { H264, HEVC, AV1 };
// 硬件加速后端
//   Software     : libx264 / libx265 / SVT-AV1
//   QSV / VAAPI  : Intel（VAAPI 在 Linux 上也覆盖 AMD Mesa 驱动）
//   NVENC        : NVIDIA
//   AMF          : AMD（需 ffmpeg --enable-amf）
//   RKMPP        : Rockchip VPU（RK3588 等，需 ffmpeg-rockchip / --enable-rkmpp）
//   VideoToolbox : Apple Silicon（macOS）
//   V4L2M2M      : 通用 V4L2 内存到内存编码（高通 Adreno VPU / ARM SoC）
enum class HwAccel { Software, QSV, VAAPI, NVENC, AMF, RKMPP, VideoToolbox, V4L2M2M };
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

    // (codec, hw) 组合对应的 ffmpeg 编码器名；该组合不存在时返回空字符串
    // (如 AV1×RKMPP)。供界面按 `ffmpeg -encoders` 动态筛选可用后端。
    static QString encoderFor(VideoCodec codec, HwAccel hw);

    // 构造完整的 ffmpeg 参数列表(不含开头的 "ffmpeg")
    QStringList buildArgs(const QString &input, const QString &output) const;
};
