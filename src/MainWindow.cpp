#include "MainWindow.h"
#include "FfmpegProcess.h"

#include <QtWidgets>
#include <QProcess>
#include <QKeyEvent>
#include <QDesktopServices>
#include <QSettings>
#include <QUrl>
#include <QLibraryInfo>
#include <QDir>
#include <QFile>

// 顶部信息常量
#ifndef APP_VERSION
#define APP_VERSION "0.0"
#endif
static const QString kAppVersion   = APP_VERSION;
static const QString kRepoUrl      = "https://github.com/padhialuo0426/qt-ffconv";
static const QString kFfmpegDocUrl = "https://ffmpeg.org/documentation.html";

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
    setWindowTitle("ffconv");
    setAcceptDrops(true);
    resize(960, 620);

    // 转码工作进程按需创建成一个「池」：每个任务一个 FfmpegProcess，
    // 最多 m_concurrency 个同时运行(见 startJob/pump)。

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

    auto *queueBox = new QGroupBox(tr("转码队列"));
    auto *queueLay = new QVBoxLayout(queueBox);
    queueLay->addWidget(m_table);
    queueLay->addLayout(queueBtns);

    // ===== 参数面板 =====
    m_codecCombo = new QComboBox;
    m_codecCombo->addItem("H.264 / AVC", int(VideoCodec::H264));
    m_codecCombo->addItem("H.265 / HEVC", int(VideoCodec::HEVC));
    m_codecCombo->addItem("AV1", int(VideoCodec::AV1));

    // 后端下拉根据本机 `ffmpeg -encoders` 动态生成：只列出当前编码格式
    // 真正可用的后端；切换编码格式时重建。
    detectEncoders();
    detectDevices();
    m_hwCombo = new QComboBox;
    populateHwCombo();
    connect(m_codecCombo, qOverload<int>(&QComboBox::currentIndexChanged),
            this, [this] { populateHwCombo(); });

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

    // 并发：默认(按 GPU 估计) / 1 / 2 / 4 / 8。data=0 表示「默认」，其余为具体并发数。
    m_autoConcurrency = autoNvencConcurrency();
    m_concurrencyCombo = new QComboBox;
    m_concurrencyCombo->addItem(tr("默认 (%1)").arg(m_autoConcurrency), 0);
    for (int n : {1, 2, 4, 8})
        m_concurrencyCombo->addItem(QString::number(n), n);
    m_concurrencyCombo->setToolTip(tr(
        "同时运行的转码任务数。多个 NVENC 引擎的高端卡(如 RTX 4090=2、"
        "RTX 6000 Ada/L40=3)可调高以提升吞吐；普通单引擎卡保持 1 即可。\n"
        "软件编码会强制为 1，避免占满 CPU。"));

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
    form->addRow(tr("并发"), m_concurrencyCombo);
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

    createMenus();
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
    if (isBusy()) return;
    QList<int> rows = selectedRows();
    std::sort(rows.begin(), rows.end(), std::greater<int>());
    for (int r : rows) {
        m_table->removeRow(r);
        m_jobs.remove(r);
    }
}

void MainWindow::clearQueue()
{
    if (isBusy()) return;
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
// ---------- 后端探测 ----------
void MainWindow::detectEncoders()
{
    m_encoders.clear();
    const QString out = runCapture(m_ffmpegPath, {"-hide_banner", "-encoders"});
    // 每行形如：" V....D h264_nvenc           NVIDIA NVENC H.264 encoder"
    // 前 6 列为标志位，其后第一个 token 即编码器名。
    static const QRegularExpression re(R"(^\s*[A-Z.]{6}\s+(\S+))");
    const QStringList lines = out.split('\n');
    for (const QString &ln : lines) {
        const auto m = re.match(ln);
        if (m.hasMatch())
            m_encoders.insert(m.captured(1));
    }
}

void MainWindow::detectDevices()
{
    // 只读地判断「图形/VPU 设备是否在场」，不采集任何机器标识信息。
    // 编码器编进 ffmpeg ≠ 硬件存在，这一层用设备节点/厂商 ID 精确挡掉死选项。
    m_hwPresent.clear();
    m_hwPresent.insert(int(HwAccel::Software));   // 软件后端永远可用

#if defined(Q_OS_MACOS)
    m_hwPresent.insert(int(HwAccel::VideoToolbox));
#elif defined(Q_OS_WIN)
    // Windows 无 sysfs / 设备节点可读的厂商信息：设备层放行 Windows 相关后端，
    // 由编码器层(ffmpeg -encoders)负责过滤——官方 ffmpeg 构建里有对应编码器
    // 且驱动/显卡具备时才会真正可用。VAAPI/RKMPP/V4L2M2M 是 Linux-only，其编码器
    // 不会出现在 Windows ffmpeg 中，编码器层自然挡掉，无需在此列出。
    m_hwPresent.insert(int(HwAccel::QSV));     // Intel 核显
    m_hwPresent.insert(int(HwAccel::NVENC));   // NVIDIA 独显
    m_hwPresent.insert(int(HwAccel::AMF));     // AMD 显卡
#else
    // GPU 厂商：遍历 DRM render 节点读 vendor ID
    bool intel = false, amd = false, nvidia = false;
    const QDir drm("/sys/class/drm");
    for (const QString &n : drm.entryList({"renderD*"}, QDir::Dirs | QDir::NoDotAndDotDot)) {
        QFile f("/sys/class/drm/" + n + "/device/vendor");
        if (!f.open(QIODevice::ReadOnly))
            continue;
        const QString v = QString::fromUtf8(f.readAll()).trimmed().toLower();
        if      (v == "0x8086") intel  = true;   // Intel
        else if (v == "0x1002") amd    = true;   // AMD
        else if (v == "0x10de") nvidia = true;   // NVIDIA
    }
    if (QFile::exists("/dev/nvidia0"))            // 专有驱动可能不出 DRM 节点
        nvidia = true;

    if (intel)  { m_hwPresent.insert(int(HwAccel::QSV));
                  m_hwPresent.insert(int(HwAccel::VAAPI)); }
    if (amd)    { m_hwPresent.insert(int(HwAccel::AMF));
                  m_hwPresent.insert(int(HwAccel::VAAPI)); }
    if (nvidia)   m_hwPresent.insert(int(HwAccel::NVENC));

    // Rockchip VPU
    if (QFile::exists("/dev/mpp_service"))
        m_hwPresent.insert(int(HwAccel::RKMPP));

    // V4L2 M2M 编码设备：按 video4linux 节点名匹配编码器（排除普通摄像头）
    const QDir v4l("/sys/class/video4linux");
    for (const QString &n : v4l.entryList({"video*"}, QDir::Dirs | QDir::NoDotAndDotDot)) {
        QFile f("/sys/class/video4linux/" + n + "/name");
        if (!f.open(QIODevice::ReadOnly))
            continue;
        const QString name = QString::fromUtf8(f.readAll()).trimmed().toLower();
        if (name.contains("enc") || name.contains("venus")
            || name.contains("codec") || name.contains("vpu")) {
            m_hwPresent.insert(int(HwAccel::V4L2M2M));
            break;
        }
    }
#endif
}

int MainWindow::autoNvencConcurrency() const
{
    // 「默认」并发的自动取值(A 方案)：NVENC 物理引擎数无法由 nvidia-smi 直接查出，
    // 只能按 GPU 型号名对照 NVIDIA 支持矩阵估计。未知/无 N 卡一律回退 1。
    const QString name =
        runCapture("nvidia-smi", {"--query-gpu=name", "--format=csv,noheader"})
            .trimmed().toLower();
    if (name.isEmpty())
        return 1;
    if (name.contains("rtx 6000 ada") || name.contains("l40"))
        return 3;                       // AD102 全开三 NVENC
    if (name.contains("4090"))
        return 2;                       // 消费版 4090 开两 NVENC
    return 1;                           // 其余按单引擎处理
}

QString MainWindow::hwDisplayName(HwAccel hw) const
{
    switch (hw) {
    case HwAccel::Software:     return tr("软件 (libx264/x265/SVT-AV1)");
    case HwAccel::QSV:          return tr("Intel QSV");
    case HwAccel::VAAPI:        return tr("VAAPI (Intel / AMD)");
    case HwAccel::NVENC:        return tr("NVIDIA NVENC");
    case HwAccel::AMF:          return tr("AMD AMF");
    case HwAccel::RKMPP:        return tr("Rockchip RKMPP");
    case HwAccel::VideoToolbox: return tr("Apple VideoToolbox");
    case HwAccel::V4L2M2M:      return tr("V4L2 M2M (高通 / ARM VPU)");
    }
    return QString();
}

void MainWindow::populateHwCombo()
{
    const VideoCodec codec = VideoCodec(m_codecCombo->currentData().toInt());
    // 记住当前选择，重建后尽量保留
    const int prev = m_hwCombo->count() ? m_hwCombo->currentData().toInt()
                                        : int(HwAccel::Software);

    QSignalBlocker block(m_hwCombo);
    m_hwCombo->clear();

    static const HwAccel order[] = {
        HwAccel::Software, HwAccel::QSV, HwAccel::VAAPI, HwAccel::NVENC,
        HwAccel::AMF, HwAccel::RKMPP, HwAccel::VideoToolbox, HwAccel::V4L2M2M,
    };
    // 一个后端要同时满足：① ffmpeg 编进了该编码器；② 本机有对应硬件在场。
    // 探测失败(未找到 ffmpeg 或 -encoders 无输出)时退回经典四后端，避免整表为空。
    const bool detected = !m_encoders.isEmpty();
    for (HwAccel hw : order) {
        const QString enc = EncodeSettings::encoderFor(codec, hw);
        if (enc.isEmpty())
            continue;
        const bool encOk = detected
            ? m_encoders.contains(enc)
            : (hw == HwAccel::Software || hw == HwAccel::QSV
               || hw == HwAccel::VAAPI || hw == HwAccel::NVENC);
        const bool devOk = m_hwPresent.contains(int(hw));
        if (encOk && devOk)
            m_hwCombo->addItem(hwDisplayName(hw), int(hw));
    }

    if (m_hwCombo->count() == 0) {
        m_hwCombo->addItem(tr("无可用编码器"), int(HwAccel::Software));
        m_hwCombo->setEnabled(false);
    } else {
        m_hwCombo->setEnabled(true);
        const int idx = m_hwCombo->findData(prev);
        m_hwCombo->setCurrentIndex(idx >= 0 ? idx : 0);
    }
}

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
int MainWindow::resolveConcurrency() const
{
    const int sel = m_concurrencyCombo->currentData().toInt(); // 0 = 默认(自动)
    int n = (sel <= 0) ? m_autoConcurrency : sel;
    // 软件编码强制单路，避免并发打满 CPU
    if (HwAccel(m_hwCombo->currentData().toInt()) == HwAccel::Software)
        n = 1;
    return qMax(1, n);
}

void MainWindow::startQueue()
{
    if (isBusy())
        return;

    // 只转码被勾选的视频（含此前已完成、重新勾选想换格式重转的）
    m_runRows.clear();
    for (int i = 0; i < m_jobs.size(); ++i)
        if (isRowChecked(i))
            m_runRows << i;

    if (m_runRows.isEmpty()) {
        QMessageBox::information(this, tr("提示"),
                                tr("请先勾选要转码的视频"));
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
    m_concurrency = resolveConcurrency();
    if (m_concurrency > 1)
        appendLog(tr("==== 并发 %1 路 ====").arg(m_concurrency));
    setUiRunning(true);
    pump();
}

void MainWindow::startJob(int row)
{
    auto *worker = new FfmpegProcess(this);
    worker->setBinaries(m_ffmpegPath, m_ffprobePath);
    m_active.insert(worker, row);

    connect(worker, &FfmpegProcess::logLine, this, &MainWindow::appendLog);
    connect(worker, &FfmpegProcess::progress, this, [this, worker](int pct) {
        const int r = m_active.value(worker, -1);
        if (r >= 0 && r < m_jobs.size()) {
            m_jobs[r].progress = pct;
            if (auto *it = m_table->item(r, 2))
                it->setText(QString::number(pct) + "%");
        }
        updateOverall();
    });
    connect(worker, &FfmpegProcess::finished, this, [this, worker](bool ok) {
        const int r = m_active.value(worker, -1);
        // 已被取消的行保留「已取消」，不要覆盖成失败
        if (r >= 0 && m_jobs[r].status != JobStatus::Cancelled) {
            setRowStatus(r, ok ? JobStatus::Done : JobStatus::Failed);
            // 转码成功后自动取消勾选：避免再次"开始"时被重复转码；
            // 若选错格式想重转，重新勾选该视频即可。
            if (ok)
                setRowChecked(r, false);
        }
        m_active.remove(worker);
        worker->deleteLater();
        if (!m_cancelling)
            pump();   // 补位后继任务，并在全部结束时收尾
    });

    setRowStatus(row, JobStatus::Running);
    const auto &job = m_jobs[row];
    appendLog(tr("→ 开始：%1  →  %2").arg(job.inputPath, job.outputPath));
    worker->start(job.inputPath, job.outputPath, job.settings);
}

void MainWindow::pump()
{
    // 补足并发槽位：把 Pending 行陆续投入运行，直到达到并发上限
    while (m_active.size() < m_concurrency) {
        int next = -1;
        for (int i = 0; i < m_jobs.size(); ++i)
            if (m_jobs[i].status == JobStatus::Pending) { next = i; break; }
        if (next < 0)
            break;
        startJob(next);   // 内部立即置为 Running，下一轮扫描不会重复选中
    }
    updateOverall();

    // 无活跃进程且无待处理 → 队列结束
    if (m_active.isEmpty()) {
        bool anyPending = false;
        for (const auto &j : m_jobs)
            if (j.status == JobStatus::Pending) { anyPending = true; break; }
        if (!anyPending) {
            setUiRunning(false);
            appendLog(tr("==== 队列处理完成 ===="));
            m_overallBar->setValue(100);
        }
    }
}

void MainWindow::updateOverall()
{
    if (m_runRows.isEmpty()) { m_overallBar->setValue(0); return; }
    double sum = 0;
    for (int r : m_runRows) {
        if (r < 0 || r >= m_jobs.size()) continue;
        switch (m_jobs[r].status) {
        case JobStatus::Done:
        case JobStatus::Failed:
        case JobStatus::Cancelled: sum += 100; break;        // 已结算
        case JobStatus::Running:   sum += m_jobs[r].progress; break;
        default: break;                                       // Pending = 0
        }
    }
    m_overallBar->setValue(int(sum / m_runRows.size()));
}

void MainWindow::cancelQueue()
{
    if (!isBusy()) return;
    m_cancelling = true;   // cancel() 同步触发 finished 回调，先设标志阻止其补位

    // 先把尚未启动的 Pending 行标记为取消，防止 pump 再投放
    for (int r : m_runRows)
        if (r >= 0 && r < m_jobs.size() && m_jobs[r].status == JobStatus::Pending)
            setRowStatus(r, JobStatus::Cancelled);

    // 取消所有活跃工作进程
    const auto workers = m_active.keys();
    for (FfmpegProcess *w : workers) {
        const int r = m_active.value(w, -1);
        if (r >= 0 && r < m_jobs.size())
            setRowStatus(r, JobStatus::Cancelled);
        w->cancel();   // 同步：其 finished 回调会在此期间发生
    }

    appendLog(tr("✗ 已取消"));
    m_cancelling = false;
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
        const QRegularExpression re(
            "(vaapi|qsv|nvenc|nvdec|cuvid|_amf|rkmpp|videotoolbox|v4l2m2m|vulkan)",
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

// ---------- 顶部菜单 ----------
void MainWindow::createMenus()
{
    // 文件
    QMenu *fileMenu = menuBar()->addMenu(tr("文件(&F)"));
    fileMenu->addAction(tr("添加文件…"), this, &MainWindow::addFiles);
    fileMenu->addAction(tr("清空队列"), this, &MainWindow::clearQueue);
    fileMenu->addSeparator();
    fileMenu->addAction(tr("退出"), this, &QWidget::close);

    // 设置 → 语言
    QMenu *settingsMenu = menuBar()->addMenu(tr("设置(&S)"));
    QMenu *langMenu = settingsMenu->addMenu(tr("语言"));

    QSettings settings;
    const QString cur = settings.value("language", "zh_CN").toString();

    auto *langGroup = new QActionGroup(this);
    langGroup->setExclusive(true);
    struct { QString name; QString code; } langs[] = {
        { tr("简体中文"), "zh_CN" },
        { "English",     "en"    },
    };
    for (const auto &l : langs) {
        QAction *act = langMenu->addAction(l.name);
        act->setCheckable(true);
        act->setChecked(cur == l.code);
        const QString code = l.code;
        connect(act, &QAction::triggered, this, [this, code] { changeLanguage(code); });
        langGroup->addAction(act);
    }

    // 帮助
    QMenu *helpMenu = menuBar()->addMenu(tr("帮助(&H)"));
    helpMenu->addAction(tr("使用说明"), this, &MainWindow::showUsage);
    helpMenu->addSeparator();
    helpMenu->addAction(tr("FFmpeg 官方文档"), this, [] {
        QDesktopServices::openUrl(QUrl(kFfmpegDocUrl));
    });
    helpMenu->addAction(tr("项目仓库"), this, [] {
        QDesktopServices::openUrl(QUrl(kRepoUrl));
    });
    helpMenu->addAction(tr("开源许可"), this, &MainWindow::showLicense);
    helpMenu->addSeparator();
    helpMenu->addAction(tr("关于"), this, &MainWindow::showAbout);
}

void MainWindow::changeLanguage(const QString &code)
{
    QSettings settings;
    if (settings.value("language", "zh_CN").toString() == code)
        return;   // 未变化
    settings.setValue("language", code);
    QMessageBox::information(this, tr("语言"),
                             tr("语言已切换，重启程序后生效。"));
}

void MainWindow::showUsage()
{
    QMessageBox::information(this, tr("使用说明"),
        tr("1. 通过「添加文件」或直接拖拽，把视频加入队列。\n"
           "2. 在列表中勾选要转码的视频（可用「全选」，或鼠标框选后按空格批量勾选）。\n"
           "3. 在右侧选择视频编码、加速后端、质量、音频与输出格式。\n"
           "4. 点击「开始转码」，仅勾选的视频会被处理。\n"
           "5. 转码完成的视频会自动取消勾选；若想换个格式重转，重新勾选它即可。"));
}

void MainWindow::showLicense()
{
    QMessageBox::about(this, tr("开源许可"),
        tr("本程序以 GNU 通用公共许可证 v3 或更新版本（GPL-3.0-or-later）发布，"
           "与本机所用的 FFmpeg（启用 GPL 组件构建）保持一致。\n\n"
           "本程序通过外部进程调用系统中的 FFmpeg，FFmpeg 及其各编解码库"
           "遵循各自的许可证。\n\n"
           "完整许可证见随附的 LICENSE 文件。"));
}

void MainWindow::showAbout()
{
    const QString ffver = runCapture(m_ffmpegPath, {"-hide_banner", "-version"})
                              .section('\n', 0, 0);
    QMessageBox::about(this, tr("关于"),
        tr("<h3>ffconv</h3>"
           "<p>版本 %1</p>"
           "<p>一个基于 Qt %2 的本机视频转码前端，通过调用系统 FFmpeg 完成转码。</p>"
           "<p>%3</p>"
           "<p>许可证：GPL-3.0-or-later</p>")
            .arg(kAppVersion, QT_VERSION_STR,
                 ffver.isEmpty() ? tr("未检测到 FFmpeg") : ffver.toHtmlEscaped()));
}
