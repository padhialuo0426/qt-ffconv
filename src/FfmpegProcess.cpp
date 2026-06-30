#include "FfmpegProcess.h"
#include <QStringList>
#include <QVariant>

FfmpegProcess::FfmpegProcess(QObject *parent) : QObject(parent)
{
    m_proc = new QProcess(this);
    connect(m_proc, &QProcess::readyReadStandardOutput, this, &FfmpegProcess::onStdout);
    connect(m_proc, &QProcess::readyReadStandardError,  this, &FfmpegProcess::onStderr);
    connect(m_proc, qOverload<int, QProcess::ExitStatus>(&QProcess::finished),
            this, [this](int code, QProcess::ExitStatus) {
                bool ok = (code == 0) && !m_cancelled;
                emit progress(ok ? 100 : m_proc->property("lastPct").toInt());
                emit finished(ok);
            });
}

void FfmpegProcess::setBinaries(const QString &ffmpeg, const QString &ffprobe)
{
    m_ffmpeg = ffmpeg;
    m_ffprobe = ffprobe;
}

bool FfmpegProcess::isRunning() const
{
    return m_proc->state() != QProcess::NotRunning;
}

double FfmpegProcess::probeDurationSec(const QString &input)
{
    QProcess probe;
    probe.start(m_ffprobe, {"-v", "error",
                            "-show_entries", "format=duration",
                            "-of", "default=nokey=1:noprint_wrappers=1",
                            input});
    if (!probe.waitForFinished(5000))
        return 0.0;
    bool ok = false;
    double d = probe.readAllStandardOutput().trimmed().toDouble(&ok);
    return ok ? d : 0.0;
}

void FfmpegProcess::start(const QString &input, const QString &output, const EncodeSettings &s)
{
    m_cancelled = false;
    m_durationSec = probeDurationSec(input);

    const QStringList args = s.buildArgs(input, output);
    emit logLine(QStringLiteral("$ %1 %2").arg(m_ffmpeg, args.join(' ')));
    m_proc->start(m_ffmpeg, args);
}

void FfmpegProcess::cancel()
{
    if (isRunning()) {
        m_cancelled = true;
        m_proc->terminate();
        if (!m_proc->waitForFinished(2000))
            m_proc->kill();
    }
}

void FfmpegProcess::onStdout()
{
    // -progress pipe:1 输出形如 key=value 的行
    while (m_proc->canReadLine()) {
        const QString line = QString::fromUtf8(m_proc->readLine()).trimmed();
        if (m_durationSec <= 0.0)
            continue;
        // out_time_us / out_time_ms 在 ffmpeg 中均为微秒
        if (line.startsWith("out_time_us=") || line.startsWith("out_time_ms=")) {
            const double us = line.section('=', 1).toDouble();
            if (us > 0) {
                int pct = int((us / 1e6) / m_durationSec * 100.0);
                pct = qBound(0, pct, 99);
                m_proc->setProperty("lastPct", QVariant(pct));
                emit progress(pct);
            }
        }
    }
}

void FfmpegProcess::onStderr()
{
    const QByteArray data = m_proc->readAllStandardError();
    const QString text = QString::fromUtf8(data);
    for (const QString &l : text.split('\n', Qt::SkipEmptyParts))
        emit logLine(l.trimmed());
}
