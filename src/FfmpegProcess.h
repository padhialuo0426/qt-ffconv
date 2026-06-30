#pragma once
#include <QObject>
#include <QProcess>
#include "EncodeSettings.h"

// 封装一次 ffmpeg 转码调用：先用 ffprobe 取时长，再跑 ffmpeg 并解析进度。
class FfmpegProcess : public QObject {
    Q_OBJECT
public:
    explicit FfmpegProcess(QObject *parent = nullptr);

    void setBinaries(const QString &ffmpeg, const QString &ffprobe);
    void start(const QString &input, const QString &output, const EncodeSettings &s);
    void cancel();
    bool isRunning() const;

signals:
    void progress(int percent);          // 0-100
    void logLine(const QString &line);
    void finished(bool ok);

private:
    double probeDurationSec(const QString &input);
    void   onStdout();
    void   onStderr();

    QProcess *m_proc = nullptr;
    QString   m_ffmpeg  = "ffmpeg";
    QString   m_ffprobe = "ffprobe";
    double    m_durationSec = 0.0;
    bool      m_cancelled = false;
};
