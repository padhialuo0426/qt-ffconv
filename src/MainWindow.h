#pragma once
#include <QMainWindow>
#include <QVector>
#include <QSet>
#include <QHash>
#include <QString>
#include "TranscodeJob.h"
#include "EncodeSettings.h"
#include "Logger.h"

class QTableWidget;
class QComboBox;
class QSpinBox;
class QLineEdit;
class QPlainTextEdit;
class QProgressBar;
class QPushButton;
class QLabel;
class FfmpegProcess;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);

protected:
    void dragEnterEvent(QDragEnterEvent *e) override;
    void dropEvent(QDropEvent *e) override;
    bool eventFilter(QObject *obj, QEvent *ev) override;
    void closeEvent(QCloseEvent *e) override;

private slots:
    void addFiles();
    void removeSelected();
    void clearQueue();
    void chooseOutputDir();
    void startQueue();
    void cancelQueue();
    void showCapabilities();
    void checkAll();
    void checkNone();
    void invertCheck();
    void changeLanguage(const QString &code);
    void configureLogging();
    void showUsage();
    void showLicense();
    void showAbout();

private:
    void  createMenus();
    void  addPath(const QString &path);
    void  detectEncoders();                       // 解析 `ffmpeg -encoders` 填充 m_encoders
    void  detectDevices();                         // 探测本机图形/VPU 设备填充 m_hwPresent
    void  populateHwCombo();                       // 按 编码格式 × 编码器存在 × 硬件在场 重建后端下拉
    QString hwDisplayName(HwAccel hw) const;       // 后端的界面显示名
    EncodeSettings currentSettings() const;
    QString buildOutputPath(const QString &input, const EncodeSettings &s) const;
    void  pump();                                  // 补足并发槽位 / 判断队列是否完成
    void  startJob(int row);                        // 为某行起一个 ffmpeg 工作进程
    void  updateOverall();                          // 汇总所有本次任务的整体进度
    int   resolveConcurrency() const;               // 解析并发下拉(默认→自动值；软件强制 1)
    int   autoNvencConcurrency() const;             // 按 GPU 型号估计 NVENC 引擎数
    bool  isBusy() const { return !m_active.isEmpty(); }
    void  setRowStatus(int row, JobStatus st);
    void  setUiRunning(bool running);

    // 日志（5 级；文件记全量，窗口按级别 + 当前所选任务过滤）
    void    logSession(LogLevel lv, const QString &msg);          // 会话级事件
    void    logJob(int row, LogLevel lv, const QString &msg);     // 归属某任务
    void    logSeparator();                                       // 插入 === 分隔行
    void    addEntry(const LogEntry &e);                          // 存储 + 落盘 + 增量显示
    void    rebuildLogView();                                     // 作用域/级别变化时整体重绘
    void    setLogScope(int jobId);                              // -1 = 总览
    bool    entryVisible(const LogEntry &e) const;
    QString formatEntry(const LogEntry &e, bool forFile) const;
    bool  isRowChecked(int row) const;
    void  setRowChecked(int row, bool checked);
    QList<int> selectedRows() const;

    // 队列
    QTableWidget   *m_table = nullptr;
    QPushButton    *m_addBtn = nullptr;
    QPushButton    *m_removeBtn = nullptr;
    QPushButton    *m_clearBtn = nullptr;
    QPushButton    *m_checkAllBtn = nullptr;
    QPushButton    *m_checkNoneBtn = nullptr;
    QPushButton    *m_invertBtn = nullptr;

    // 参数面板
    QComboBox      *m_codecCombo = nullptr;
    QComboBox      *m_hwCombo = nullptr;
    QSpinBox       *m_qualitySpin = nullptr;
    QComboBox      *m_audioCombo = nullptr;
    QSpinBox       *m_audioBitrateSpin = nullptr;
    QComboBox      *m_containerCombo = nullptr;
    QComboBox      *m_concurrencyCombo = nullptr;
    QLineEdit      *m_outputDirEdit = nullptr;

    // 控制 / 输出
    QPushButton    *m_startBtn = nullptr;
    QPushButton    *m_cancelBtn = nullptr;
    QProgressBar   *m_overallBar = nullptr;
    QPlainTextEdit *m_log = nullptr;
    QComboBox      *m_logLevelCombo = nullptr;
    QLabel         *m_logScopeLabel = nullptr;
    QLabel         *m_ffmpegLabel = nullptr;

    // 运行状态
    QVector<TranscodeJob> m_jobs;
    QList<int>     m_runRows;        // 本次运行勾选的行
    QHash<FfmpegProcess*, int> m_active;  // 活跃工作进程 → 对应行
    int            m_concurrency = 1;     // 本次运行的并发槽位数
    int            m_autoConcurrency = 1; // 「默认」解析出的自动并发数(按 GPU 估计)
    bool           m_cancelling = false;  // 取消进行中：阻止 pump 补位/覆盖状态
    QString        m_ffmpegPath = "ffmpeg";
    QString        m_ffprobePath = "ffprobe";
    QSet<QString>  m_encoders;        // 本机 ffmpeg 支持的编码器名集合
    QSet<int>      m_hwPresent;       // 硬件在场的后端集合 (HwAccel 转 int)

    // 日志状态
    Logger           *m_logger = nullptr;
    QVector<LogEntry> m_entries;              // 本次会话全部日志（作显示过滤的数据源）
    LogLevel          m_visLevel = LogLevel::Info;  // 窗口显示阈值（≤ 此级别才显示）
    int               m_scopeJobId = -1;      // 当前显示作用域：-1=总览，否则某任务 id
    int               m_nextJobId = 0;        // 任务稳定 id 分配计数
};
