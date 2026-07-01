#include "EncodeSettings.h"

QString EncodeSettings::encoderFor(VideoCodec codec, HwAccel hw)
{
    switch (codec) {
    case VideoCodec::H264:
        switch (hw) {
        case HwAccel::Software:     return "libx264";
        case HwAccel::QSV:          return "h264_qsv";
        case HwAccel::VAAPI:        return "h264_vaapi";
        case HwAccel::NVENC:        return "h264_nvenc";
        case HwAccel::AMF:          return "h264_amf";
        case HwAccel::RKMPP:        return "h264_rkmpp";
        case HwAccel::VideoToolbox: return "h264_videotoolbox";
        case HwAccel::V4L2M2M:      return "h264_v4l2m2m";
        }
        break;
    case VideoCodec::HEVC:
        switch (hw) {
        case HwAccel::Software:     return "libx265";
        case HwAccel::QSV:          return "hevc_qsv";
        case HwAccel::VAAPI:        return "hevc_vaapi";
        case HwAccel::NVENC:        return "hevc_nvenc";
        case HwAccel::AMF:          return "hevc_amf";
        case HwAccel::RKMPP:        return "hevc_rkmpp";
        case HwAccel::VideoToolbox: return "hevc_videotoolbox";
        case HwAccel::V4L2M2M:      return "hevc_v4l2m2m";
        }
        break;
    case VideoCodec::AV1:
        switch (hw) {
        case HwAccel::Software:     return "libsvtav1";
        case HwAccel::QSV:          return "av1_qsv";
        case HwAccel::VAAPI:        return "av1_vaapi";
        case HwAccel::NVENC:        return "av1_nvenc";
        case HwAccel::AMF:          return "av1_amf";
        case HwAccel::RKMPP:        return QString();   // Rockchip 无 AV1 编码
        case HwAccel::VideoToolbox: return QString();   // Apple 无 AV1 编码
        case HwAccel::V4L2M2M:      return QString();   // V4L2 M2M 一般无 AV1 编码
        }
        break;
    }
    return QString();
}

QString EncodeSettings::encoderName() const
{
    const QString n = encoderFor(codec, hw);
    return n.isEmpty() ? "libx264" : n;
}

QStringList EncodeSettings::buildArgs(const QString &input, const QString &output) const
{
    QStringList a;
    a << "-y" << "-hide_banner";

    // VAAPI 需要先指定设备，并在 -i 后通过 hwupload 把帧上传到 GPU
    if (hw == HwAccel::VAAPI)
        a << "-vaapi_device" << vaapiDevice;

    a << "-i" << input;

    if (hw == HwAccel::VAAPI)
        a << "-vf" << "format=nv12,hwupload";

    // 视频编码器
    a << "-c:v" << encoderName();

    // 质量参数：不同后端的控制旋钮不同
    const QString q = QString::number(quality);
    switch (hw) {
    case HwAccel::Software:
        if (codec == VideoCodec::AV1)
            a << "-crf" << q << "-preset" << "6";       // SVT-AV1 preset 0(慢)-13(快)
        else
            a << "-crf" << q << "-preset" << "medium";  // x264/x265
        break;
    case HwAccel::QSV:
        a << "-global_quality" << q << "-preset" << "medium";
        break;
    case HwAccel::NVENC:
        a << "-cq" << q << "-preset" << "p5";           // p1(快)-p7(慢)
        break;
    case HwAccel::VAAPI:
        a << "-qp" << q;
        break;
    case HwAccel::AMF:
        // AMD AMF：固定 QP 模式
        a << "-rc" << "cqp" << "-qp_i" << q << "-qp_p" << q << "-quality" << "balanced";
        break;
    case HwAccel::RKMPP:
        // Rockchip MPP：固定 QP 码控
        a << "-rc_mode" << "CQP" << "-qp_init" << q;
        break;
    case HwAccel::VideoToolbox:
        // VideoToolbox 的 -q:v 为 0-100 且越大越好，与 CRF 语义相反，做一次翻转映射
        a << "-q:v" << QString::number(qBound(1, 100 - quality, 100));
        break;
    case HwAccel::V4L2M2M:
        // V4L2 M2M 多为码率控制、无 CRF/QP；缺省给 5Mbps
        a << "-b:v" << "5M";
        break;
    }

    // HEVC 放进 mp4/mov 时打 hvc1 标签，提升 Apple 生态兼容性
    if (codec == VideoCodec::HEVC && (container == "mp4" || container == "mov"))
        a << "-tag:v" << "hvc1";

    // 音频
    switch (audio) {
    case AudioMode::Copy:
        a << "-c:a" << "copy";
        break;
    case AudioMode::AAC:
        a << "-c:a" << "aac" << "-b:a" << QString::number(audioBitrateKbps) + "k";
        break;
    case AudioMode::Opus:
        a << "-c:a" << "libopus" << "-b:a" << QString::number(audioBitrateKbps) + "k";
        break;
    }

    // 机器可读进度输出到 stdout
    a << "-progress" << "pipe:1" << "-nostats";
    a << output;
    return a;
}
