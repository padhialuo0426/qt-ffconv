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

private slots:
    void addFiles();
    void removeSelected();
    void clearQueue();
    void chooseOutputDir();
    void startQueue();
    void cancelQueue();
    void showCapabilities();

private:
    void  addPath(const QString &path);
    EncodeSettings currentSettings() const;
    QString buildOutputPath(const QString &input, const EncodeSettings &s) const;
    void  runNext();
    void  setRowStatus(int row, JobStatus st);
    void  appendLog(const QString &line);
    void  setUiRunning(bool running);

    // 队列
    QTableWidget   *m_table = nullptr;
    QPushButton    *m_addBtn = nullptr;
    QPushButton    *m_removeBtn = nullptr;
    QPushButton    *m_clearBtn = nullptr;

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
    int            m_currentIndex = -1;
    FfmpegProcess *m_runner = nullptr;
    QString        m_ffmpegPath = "ffmpeg";
    QString        m_ffprobePath = "ffprobe";
};
