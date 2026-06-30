#include "MainWindow.h"
#include "FfmpegProcess.h"

#include <QtWidgets>
#include <QProcess>
#include <QKeyEvent>

// ---- 小工具：同步执行命令并返回 stdout ----
static QString runCapture(const QString &prog, const QStringList &args, int timeoutMs = 8000)
{
    QProcess p;
    p.setProcessChannelMode(QProcess::MergedChannels);
    p.start(prog, args);
    if (!p.waitForFinished(timeoutMs))
        return QString();
    return QString::fromUtf8(p.readAll());
}

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent)
{
    setWindowTitle(tr("FFmpeg 转码器"));
    setAcceptDrops(true);
    resize(960, 620);

    m_runner = new FfmpegProcess(this);
    m_runner->setBinaries(m_ffmpegPath, m_ffprobePath);
    connect(m_runner, &FfmpegProcess::logLine, this, &MainWindow::appendLog);
    connect(m_runner, &FfmpegProcess::progress, this, [this](int pct) {
        if (m_currentIndex >= 0) {
            m_jobs[m_currentIndex].progress = pct;
            if (auto *it = m_table->item(m_currentIndex, 2))
                it->setText(QString::number(pct) + "%");
        }
        // 整体进度 = (本次已完成 + 当前任务比例) / 本次勾选总数
        int done = 0;
        for (int r : m_runRows)
            if (r >= 0 && r < m_jobs.size() && m_jobs[r].status == JobStatus::Done)
                ++done;
        double overall = m_runRows.isEmpty() ? 0
            : (done * 100.0 + pct) / m_runRows.size();
        m_overallBar->setValue(int(overall));
    });
    connect(m_runner, &FfmpegProcess::finished, this, [this](bool ok) {
        if (m_currentIndex >= 0) {
            setRowStatus(m_currentIndex, ok ? JobStatus::Done : JobStatus::Failed);
            // 转码成功后自动取消勾选：避免再次"开始"时被重复转码；
            // 若选错格式想重转，重新勾选该视频即可。
            if (ok)
                setRowChecked(m_currentIndex, false);
        }
        runNext();
    });

    // ===== 队列表格 =====
    m_table = new QTableWidget(0, 3, this);
    m_table->setHorizontalHeaderLabels({tr("文件"), tr("状态"), tr("进度")});
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::ExtendedSelection); // 支持鼠标框选/Ctrl/Shift
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->installEventFilter(this);  // 拦截空格键：批量勾选选中行

    m_addBtn    = new QPushButton(tr("添加文件…"));
    m_removeBtn = new QPushButton(tr("移除"));
    m_clearBtn  = new QPushButton(tr("清空"));
    connect(m_addBtn,    &QPushButton::clicked, this, &MainWindow::addFiles);
    connect(m_removeBtn, &QPushButton::clicked, this, &MainWindow::removeSelected);
    connect(m_clearBtn,  &QPushButton::clicked, this, &MainWindow::clearQueue);

    m_checkAllBtn  = new QPushButton(tr("全选"));
    m_checkNoneBtn = new QPushButton(tr("全不选"));
    m_invertBtn    = new QPushButton(tr("反选"));
    connect(m_checkAllBtn,  &QPushButton::clicked, this, &MainWindow::checkAll);
    connect(m_checkNoneBtn, &QPushButton::clicked, this, &MainWindow::checkNone);
    connect(m_invertBtn,    &QPushButton::clicked, this, &MainWindow::invertCheck);

    auto *queueBtns = new QHBoxLayout;
    queueBtns->addWidget(m_addBtn);
    queueBtns->addWidget(m_removeBtn);
    queueBtns->addWidget(m_clearBtn);
    queueBtns->addStretch();
    queueBtns->addWidget(m_checkAllBtn);
    queueBtns->addWidget(m_checkNoneBtn);
    queueBtns->addWidget(m_invertBtn);

    auto *queueBox = new QGroupBox(tr("转码队列（拖拽文件到此；勾选要转码的视频，框选+空格可批量勾选）"));
    auto *queueLay = new QVBoxLayout(queueBox);
    queueLay->addWidget(m_table);
    queueLay->addLayout(queueBtns);

    // ===== 参数面板 =====
    m_codecCombo = new QComboBox;
    m_codecCombo->addItem("H.264 / AVC", int(VideoCodec::H264));
    m_codecCombo->addItem("H.265 / HEVC", int(VideoCodec::HEVC));
    m_codecCombo->addItem("AV1", int(VideoCodec::AV1));

    m_hwCombo = new QComboBox;
    m_hwCombo->addItem(tr("软件 (libx264/x265/SVT-AV1)"), int(HwAccel::Software));
    m_hwCombo->addItem(tr("Intel QSV"),  int(HwAccel::QSV));
    m_hwCombo->addItem(tr("Intel VAAPI"), int(HwAccel::VAAPI));
    m_hwCombo->addItem(tr("NVIDIA NVENC"), int(HwAccel::NVENC));

    m_qualitySpin = new QSpinBox;
    m_qualitySpin->setRange(0, 63);
    m_qualitySpin->setValue(23);
    m_qualitySpin->setToolTip(tr("CRF/CQ/global_quality，数值越小质量越高、体积越大"));

    m_audioCombo = new QComboBox;
    m_audioCombo->addItem(tr("直接复制"), int(AudioMode::Copy));
    m_audioCombo->addItem("AAC", int(AudioMode::AAC));
    m_audioCombo->addItem("Opus", int(AudioMode::Opus));

    m_audioBitrateSpin = new QSpinBox;
    m_audioBitrateSpin->setRange(32, 512);
    m_audioBitrateSpin->setValue(192);
    m_audioBitrateSpin->setSuffix(" kbps");
    m_audioBitrateSpin->setEnabled(false);
    connect(m_audioCombo, qOverload<int>(&QComboBox::currentIndexChanged), this, [this] {
        m_audioBitrateSpin->setEnabled(m_audioCombo->currentData().toInt() != int(AudioMode::Copy));
    });

    m_containerCombo = new QComboBox;
    m_containerCombo->addItems({"mp4", "mkv", "mov", "webm"});

    m_outputDirEdit = new QLineEdit;
    m_outputDirEdit->setPlaceholderText(tr("留空 = 与源文件同目录"));
    auto *browseBtn = new QPushButton(tr("浏览…"));
    connect(browseBtn, &QPushButton::clicked, this, &MainWindow::chooseOutputDir);
    auto *outDirLay = new QHBoxLayout;
    outDirLay->addWidget(m_outputDirEdit);
    outDirLay->addWidget(browseBtn);

    auto *form = new QFormLayout;
    form->addRow(tr("视频编码"), m_codecCombo);
    form->addRow(tr("加速后端"), m_hwCombo);
    form->addRow(tr("质量"), m_qualitySpin);
    form->addRow(tr("音频"), m_audioCombo);
    form->addRow(tr("音频码率"), m_audioBitrateSpin);
    form->addRow(tr("输出格式"), m_containerCombo);
    form->addRow(tr("输出目录"), outDirLay);

    auto *paramBox = new QGroupBox(tr("转码参数"));
    paramBox->setLayout(form);

    auto *capsBtn = new QPushButton(tr("检测本机编解码能力…"));
    connect(capsBtn, &QPushButton::clicked, this, &MainWindow::showCapabilities);

    auto *rightLay = new QVBoxLayout;
    rightLay->addWidget(paramBox);
    rightLay->addWidget(capsBtn);
    rightLay->addStretch();

    // ===== 控制 + 日志 =====
    m_startBtn  = new QPushButton(tr("开始转码"));
    m_cancelBtn = new QPushButton(tr("取消"));
    m_cancelBtn->setEnabled(false);
    connect(m_startBtn,  &QPushButton::clicked, this, &MainWindow::startQueue);
    connect(m_cancelBtn, &QPushButton::clicked, this, &MainWindow::cancelQueue);

    m_overallBar = new QProgressBar;
    m_overallBar->setRange(0, 100);

    m_log = new QPlainTextEdit;
    m_log->setReadOnly(true);
    m_log->setMaximumBlockCount(5000);
    m_log->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));

    m_ffmpegLabel = new QLabel;
    {
        const QString ver = runCapture(m_ffmpegPath, {"-hide_banner", "-version"})
                                .section('\n', 0, 0);
        m_ffmpegLabel->setText(ver.isEmpty() ? tr("⚠ 未找到 ffmpeg") : ver);
    }

    auto *ctrlLay = new QHBoxLayout;
    ctrlLay->addWidget(m_startBtn);
    ctrlLay->addWidget(m_cancelBtn);
    ctrlLay->addWidget(m_overallBar, 1);

    // ===== 总体布局 =====
    auto *topSplit = new QSplitter(Qt::Horizontal);
    topSplit->addWidget(queueBox);
    auto *rightW = new QWidget;
    rightW->setLayout(rightLay);
    topSplit->addWidget(rightW);
    topSplit->setStretchFactor(0, 3);
    topSplit->setStretchFactor(1, 2);

    auto *central = new QWidget;
    auto *mainLay = new QVBoxLayout(central);
    mainLay->addWidget(topSplit, 1);
    mainLay->addLayout(ctrlLay);
    mainLay->addWidget(new QLabel(tr("日志：")));
    mainLay->addWidget(m_log, 1);
    setCentralWidget(central);

    statusBar()->addWidget(m_ffmpegLabel);
}

// ---------- 拖拽 ----------
void MainWindow::dragEnterEvent(QDragEnterEvent *e)
{
    if (e->mimeData()->hasUrls())
        e->acceptProposedAction();
}

void MainWindow::dropEvent(QDropEvent *e)
{
    for (const QUrl &u : e->mimeData()->urls())
        if (u.isLocalFile())
            addPath(u.toLocalFile());
}

// ---------- 队列管理 ----------
void MainWindow::addFiles()
{
    const QStringList files = QFileDialog::getOpenFileNames(
        this, tr("选择视频文件"), QString(),
        tr("视频文件 (*.mp4 *.mkv *.mov *.avi *.webm *.flv *.ts *.m4v *.wmv);;所有文件 (*)"));
    for (const QString &f : files)
        addPath(f);
}

void MainWindow::addPath(const QString &path)
{
    TranscodeJob job;
    job.inputPath = path;
    m_jobs.append(job);

    const int row = m_table->rowCount();
    m_table->insertRow(row);
    auto *fileItem = new QTableWidgetItem(QFileInfo(path).fileName());
    fileItem->setFlags(fileItem->flags() | Qt::ItemIsUserCheckable);
    fileItem->setCheckState(Qt::Checked);   // 默认勾选，方便"添加即转码"
    fileItem->setToolTip(path);
    m_table->setItem(row, 0, fileItem);
    m_table->setItem(row, 1, new QTableWidgetItem(tr("等待")));
    m_table->setItem(row, 2, new QTableWidgetItem("0%"));
}

void MainWindow::removeSelected()
{
    if (m_runner->isRunning()) return;
    QList<int> rows = selectedRows();
    std::sort(rows.begin(), rows.end(), std::greater<int>());
    for (int r : rows) {
        m_table->removeRow(r);
        m_jobs.remove(r);
    }
}

void MainWindow::clearQueue()
{
    if (m_runner->isRunning()) return;
    m_table->setRowCount(0);
    m_jobs.clear();
    m_overallBar->setValue(0);
}

void MainWindow::chooseOutputDir()
{
    const QString d = QFileDialog::getExistingDirectory(this, tr("选择输出目录"));
    if (!d.isEmpty())
        m_outputDirEdit->setText(d);
}

// ---------- 设置 / 路径 ----------
EncodeSettings MainWindow::currentSettings() const
{
    EncodeSettings s;
    s.codec = VideoCodec(m_codecCombo->currentData().toInt());
    s.hw    = HwAccel(m_hwCombo->currentData().toInt());
    s.quality = m_qualitySpin->value();
    s.audio = AudioMode(m_audioCombo->currentData().toInt());
    s.audioBitrateKbps = m_audioBitrateSpin->value();
    s.container = m_containerCombo->currentText();
    return s;
}

QString MainWindow::buildOutputPath(const QString &input, const EncodeSettings &s) const
{
    QFileInfo fi(input);
    const QString dir = m_outputDirEdit->text().isEmpty()
                        ? fi.absolutePath() : m_outputDirEdit->text();
    QString out = dir + "/" + fi.completeBaseName() + "." + s.container;
    if (QFileInfo(out) == fi)   // 避免覆盖源文件
        out = dir + "/" + fi.completeBaseName() + "_out." + s.container;
    return out;
}

// ---------- 运行队列 ----------
void MainWindow::startQueue()
{
    if (m_runner->isRunning())
        return;

    // 只转码被勾选的视频（含此前已完成、重新勾选想换格式重转的）
    m_runRows.clear();
    for (int i = 0; i < m_jobs.size(); ++i)
        if (isRowChecked(i))
            m_runRows << i;

    if (m_runRows.isEmpty()) {
        QMessageBox::information(this, tr("提示"),
                                tr("请先勾选要转码的视频（可用「全选」或框选+空格）。"));
        return;
    }

    const EncodeSettings s = currentSettings();
    for (int i : m_runRows) {
        m_jobs[i].settings = s;
        m_jobs[i].outputPath = buildOutputPath(m_jobs[i].inputPath, s);
        m_jobs[i].progress = 0;
        m_jobs[i].status = JobStatus::Pending;
        setRowStatus(i, JobStatus::Pending);
        if (auto *it = m_table->item(i, 2)) it->setText("0%");
    }
    m_overallBar->setValue(0);
    setUiRunning(true);
    m_currentIndex = -1;
    runNext();
}

void MainWindow::runNext()
{
    // 找下一个待处理任务
    int next = -1;
    for (int i = 0; i < m_jobs.size(); ++i)
        if (m_jobs[i].status == JobStatus::Pending) { next = i; break; }

    if (next < 0) {            // 队列结束
        m_currentIndex = -1;
        setUiRunning(false);
        appendLog(tr("==== 队列处理完成 ===="));
        m_overallBar->setValue(100);
        return;
    }

    m_currentIndex = next;
    setRowStatus(next, JobStatus::Running);
    const auto &job = m_jobs[next];
    appendLog(tr("→ 开始：%1  →  %2").arg(job.inputPath, job.outputPath));
    m_runner->start(job.inputPath, job.outputPath, job.settings);
}

void MainWindow::cancelQueue()
{
    if (!m_runner->isRunning()) return;
    if (m_currentIndex >= 0)
        m_jobs[m_currentIndex].status = JobStatus::Cancelled;
    m_runner->cancel();
    appendLog(tr("✗ 已取消"));
    setUiRunning(false);
}

void MainWindow::setRowStatus(int row, JobStatus st)
{
    if (row < 0 || row >= m_table->rowCount()) return;
    m_jobs[row].status = st;
    QString text;
    switch (st) {
    case JobStatus::Pending:   text = tr("等待"); break;
    case JobStatus::Running:   text = tr("转码中"); break;
    case JobStatus::Done:      text = tr("完成"); break;
    case JobStatus::Failed:    text = tr("失败"); break;
    case JobStatus::Cancelled: text = tr("已取消"); break;
    }
    if (auto *it = m_table->item(row, 1)) it->setText(text);
    if (st == JobStatus::Done)
        if (auto *it = m_table->item(row, 2)) it->setText("100%");
}

void MainWindow::appendLog(const QString &line)
{
    m_log->appendPlainText(line);
}

void MainWindow::setUiRunning(bool running)
{
    m_startBtn->setEnabled(!running);
    m_cancelBtn->setEnabled(running);
    m_addBtn->setEnabled(!running);
    m_removeBtn->setEnabled(!running);
    m_clearBtn->setEnabled(!running);
}

// ---------- 勾选（候选框） ----------
bool MainWindow::isRowChecked(int row) const
{
    auto *it = m_table->item(row, 0);
    return it && it->checkState() == Qt::Checked;
}

void MainWindow::setRowChecked(int row, bool checked)
{
    if (auto *it = m_table->item(row, 0))
        it->setCheckState(checked ? Qt::Checked : Qt::Unchecked);
}

QList<int> MainWindow::selectedRows() const
{
    QList<int> rows;
    for (const QModelIndex &it : m_table->selectionModel()->selectedRows())
        rows << it.row();
    return rows;
}

void MainWindow::checkAll()
{
    for (int i = 0; i < m_table->rowCount(); ++i)
        setRowChecked(i, true);
}

void MainWindow::checkNone()
{
    for (int i = 0; i < m_table->rowCount(); ++i)
        setRowChecked(i, false);
}

void MainWindow::invertCheck()
{
    for (int i = 0; i < m_table->rowCount(); ++i)
        setRowChecked(i, !isRowChecked(i));
}

// 空格键：把当前选中的行批量勾选/取消（框选后一起选择）
bool MainWindow::eventFilter(QObject *obj, QEvent *ev)
{
    if (obj == m_table && ev->type() == QEvent::KeyPress) {
        auto *ke = static_cast<QKeyEvent *>(ev);
        if (ke->key() == Qt::Key_Space) {
            const QList<int> rows = selectedRows();
            if (!rows.isEmpty()) {
                // 只要有未勾选的就全部勾上，否则全部取消
                bool anyUnchecked = false;
                for (int r : rows)
                    if (!isRowChecked(r)) { anyUnchecked = true; break; }
                for (int r : rows)
                    setRowChecked(r, anyUnchecked);
                return true;  // 消费事件，避免默认只切换当前单元格
            }
        }
    }
    return QMainWindow::eventFilter(obj, ev);
}

// ---------- 本机编解码能力检测 ----------
void MainWindow::showCapabilities()
{
    QApplication::setOverrideCursor(Qt::WaitCursor);

    const QString version  = runCapture(m_ffmpegPath, {"-hide_banner", "-version"});
    const QString encoders = runCapture(m_ffmpegPath, {"-hide_banner", "-encoders"});
    const QString decoders = runCapture(m_ffmpegPath, {"-hide_banner", "-decoders"});

    QApplication::restoreOverrideCursor();

    // 概览：版本 + 硬件相关编/解码器
    QString overview;
    overview += tr("=== FFmpeg 版本 ===\n");
    overview += version.section('\n', 0, 0) + "\n\n";

    auto hwLines = [](const QString &all) {
        QStringList out;
        const QRegularExpression re("(vaapi|qsv|nvenc|cuvid|_amf|vulkan)",
                                    QRegularExpression::CaseInsensitiveOption);
        for (const QString &l : all.split('\n'))
            if (re.match(l).hasMatch())
                out << l.trimmed();
        return out.join('\n');
    };

    overview += tr("=== 硬件编码器 ===\n");
    overview += hwLines(encoders) + "\n\n";
    overview += tr("=== 硬件解码器 ===\n");
    overview += hwLines(decoders) + "\n";

    // 对话框 (选项卡：概览 / 全部编码器 / 全部解码器)
    QDialog dlg(this);
    dlg.setWindowTitle(tr("本机编解码能力"));
    dlg.resize(820, 640);

    auto *tabs = new QTabWidget(&dlg);
    auto mono = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    auto makeTab = [&](const QString &text) {
        auto *e = new QPlainTextEdit;
        e->setReadOnly(true);
        e->setFont(mono);
        e->setLineWrapMode(QPlainTextEdit::NoWrap);
        e->setPlainText(text);
        return e;
    };
    tabs->addTab(makeTab(overview),  tr("概览(硬件)"));
    tabs->addTab(makeTab(encoders),  tr("全部编码器"));
    tabs->addTab(makeTab(decoders),  tr("全部解码器"));

    auto *closeBtn = new QPushButton(tr("关闭"));
    connect(closeBtn, &QPushButton::clicked, &dlg, &QDialog::accept);

    auto *lay = new QVBoxLayout(&dlg);
    lay->addWidget(tabs);
    auto *btnLay = new QHBoxLayout;
    btnLay->addStretch();
    btnLay->addWidget(closeBtn);
    lay->addLayout(btnLay);

    dlg.exec();
}
