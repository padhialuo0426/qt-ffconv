#include "Logger.h"
#include <QStandardPaths>
#include <QFileInfo>
#include <QDir>

Logger::Logger(int keepCount, QObject *parent) : QObject(parent)
{
    // 目录始终确定（即便关闭写入，“打开日志目录”仍需它）
    m_dir = QStandardPaths::writableLocation(QStandardPaths::StateLocation);
    m_enabled = keepCount > 0;
    m_keep = keepCount;
    if (!m_enabled)
        return;

    // 文件名与 session start 时间戳都取“程序打开”时刻；文件本身延迟到 activate() 才建
    m_stamp = QDateTime::currentDateTime().toString("yyyyMMdd-HHmmss");
    const QString ts = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss");
    m_pending << QStringLiteral("==== session start %1 ====").arg(ts);
}

Logger::~Logger()
{
    if (m_file.isOpen()) {   // 只有 activate 过（发生过转码）才有文件需要收尾
        const QString ts = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss");
        writeLine(QStringLiteral("==== session end %1 ====").arg(ts));
        m_file.close();
    }
}

void Logger::activate()
{
    if (!m_enabled || m_file.isOpen())
        return;   // 已关闭写入 或 已激活

    QDir().mkpath(m_dir);
    m_file.setFileName(m_dir + "/ffconv-" + m_stamp + ".log");
    if (!m_file.open(QIODevice::Append | QIODevice::Text)) {
        m_enabled = false;      // 打不开就彻底降级
        m_pending.clear();
        return;
    }
    for (const QString &l : std::as_const(m_pending)) {   // 刷入启动阶段缓存
        m_file.write(l.toUtf8());
        m_file.write("\n");
    }
    m_pending.clear();
    m_file.flush();
    cleanup(m_keep);   // 含刚建的本会话文件在内，保留最新 keep 个
}

void Logger::cleanup(int keep)
{
    QDir d(m_dir);
    // 文件名含时间戳，按名升序即“最旧在前”；超出的从最旧开始删
    const QFileInfoList files =
        d.entryInfoList({QStringLiteral("ffconv-*.log")}, QDir::Files, QDir::Name);
    const int excess = int(files.size()) - keep;
    for (int i = 0; i < excess; ++i)
        QFile::remove(files.at(i).absoluteFilePath());
}

void Logger::writeLine(const QString &formatted)
{
    if (!m_enabled)
        return;                      // 关闭写入
    if (!m_file.isOpen()) {          // 尚未 activate：先缓存，等首次转码再落盘
        m_pending << formatted;
        return;
    }
    m_file.write(formatted.toUtf8());
    m_file.write("\n");
    m_file.flush();                  // 崩溃/被 kill 也不丢已发生的日志
}

QString Logger::levelName(LogLevel lv)
{
    switch (lv) {
    case LogLevel::Fatal: return QStringLiteral("FATAL");
    case LogLevel::Error: return QStringLiteral("ERROR");
    case LogLevel::Warn:  return QStringLiteral("WARN");
    case LogLevel::Info:  return QStringLiteral("INFO");
    case LogLevel::Debug: return QStringLiteral("DEBUG");
    }
    return QStringLiteral("?");
}
