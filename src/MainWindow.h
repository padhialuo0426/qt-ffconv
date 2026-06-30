#pragma once
#include <QMainWindow>
#include <QVector>
#include "TranscodeJob.h"

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
    void showUsage();
    void showLicense();
    void showAbout();

private:
    void  createMenus();
    void  addPath(const QString &path);
    EncodeSettings currentSettings() const;
    QString buildOutputPath(const QString &input, const EncodeSettings &s) const;
    void  runNext();
    void  setRowStatus(int row, JobStatus st);
    void  appendLog(const QString &line);
    void  setUiRunning(bool running);
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
    QLineEdit      *m_outputDirEdit = nullptr;

    // 控制 / 输出
    QPushButton    *m_startBtn = nullptr;
    QPushButton    *m_cancelBtn = nullptr;
    QProgressBar   *m_overallBar = nullptr;
    QPlainTextEdit *m_log = nullptr;
    QLabel         *m_ffmpegLabel = nullptr;

    // 运行状态
    QVector<TranscodeJob> m_jobs;
    QList<int>     m_runRows;        // 本次运行勾选的行
    int            m_currentIndex = -1;
    FfmpegProcess *m_runner = nullptr;
    QString        m_ffmpegPath = "ffmpeg";
    QString        m_ffprobePath = "ffprobe";
};
