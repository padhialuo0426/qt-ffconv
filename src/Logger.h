#pragma once
#include <QObject>
#include <QDateTime>
#include <QFile>
#include <QString>
#include <QStringList>

// 日志级别（数值越小越严重）。用于窗口按阈值过滤；文件始终记全量。
enum class LogLevel { Fatal = 0, Error, Warn, Info, Debug };

// 单条日志。jobId < 0 表示会话级（不属于任何转码任务）。
struct LogEntry {
    QDateTime ts;
    LogLevel  level = LogLevel::Info;
    int       jobId = -1;
    QString   name;      // 任务源文件名（会话级为空）
    QString   text;
    bool      raw = false;   // 分隔行等：原样输出，不加时间戳/级别前缀
};

// 会话级文件日志：写到 XDG StateLocation（~/.local/state/ffconv/）。
// 每次启动一个独立文件 ffconv-<时间戳>.log，流式追加 + 会话起止标记、记录全量；
// 保留个数超过上限时自动清理最旧的历史文件。keepCount = 0 关闭文件写入。
//
// 延迟建文件：启动阶段的日志先缓存在内存，直到 activate()（本次会话首次真正开始
// 转码）才建文件并把缓存刷入。若整个会话没有转码，则文件从不创建，避免无意义的
// 开关把日志目录填满。
class Logger : public QObject {
    Q_OBJECT
public:
    explicit Logger(int keepCount, QObject *parent = nullptr);
    ~Logger() override;

    void    activate();                            // 首次转码时调用：建文件并落盘缓存
    void    writeLine(const QString &formatted);   // 已激活则追加+flush，否则先缓存
    QString dir() const { return m_dir; }           // 日志目录（供“打开日志目录”）
    bool    enabled() const { return m_enabled; }

    static QString levelName(LogLevel lv);

private:
    void cleanup(int keep);   // 把目录内 ffconv-*.log 清理到只剩最新 keep 个

    QString     m_dir;
    QFile       m_file;
    bool        m_enabled = false;   // keepCount>0 才写文件
    int         m_keep = 0;
    QString     m_stamp;             // 本会话文件名时间戳（程序打开时刻）
    QStringList m_pending;           // activate() 前缓存的行
};
