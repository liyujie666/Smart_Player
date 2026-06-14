#include "summarypanel.h"
#include "summarysettingsdialog.h"
#include "configmanager.h"
#include <QListWidgetItem>
#include <QMovie>
#include <QScrollBar>
#include <QFileDialog>
#include <QMessageBox>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QRegularExpression>
#include <QFile>
#include <QEvent>
#include <QMouseEvent>

namespace {
QString formatDuration(qint64 ms) {
    int totalSec = ms / 1000;
    int h = totalSec / 3600;
    int m = (totalSec % 3600) / 60;
    int s = totalSec % 60;
    if (h > 0) return QString::asprintf("%d:%02d:%02d", h, m, s);
    return QString::asprintf("%02d:%02d", m, s);
}

QString formatMetaTime(qint64 ms) {
    int totalSec = ms / 1000;
    int h = totalSec / 3600;
    int m = (totalSec % 3600) / 60;
    int s = totalSec % 60;
    if (h > 0) return QString::asprintf("%d:%02d:%02d", h, m, s);
    return QString::asprintf("%d:%02d", m, s);
}

QLabel* makeSectionHeader(const QString& text, QWidget* parent) {
    QLabel* lbl = new QLabel(text, parent);
    lbl->setStyleSheet(QString::fromLatin1(
        "QLabel { color: #1F2937; font-size: 11px; font-family: Microsoft YaHei, PingFang SC, sans-serif; "
        "font-weight: 600; padding: 16px 0 6px 0; letter-spacing: 0.5px; }"));
    return lbl;
}

void setWidgetBackground(QWidget* w, const QString& color) {
    w->setStyleSheet(QString::fromLatin1("QWidget#%1 { background-color: %2; border-radius: 10px; }").arg(w->objectName()).arg(color));
}

void clearFlowLayout(QLayout* layout) {
    if (!layout) return;
    QLayoutItem* child;
    while ((child = layout->takeAt(0)) != nullptr) {
        if (QWidget* w = child->widget()) {
            w->deleteLater();
        }
        delete child;
    }
}
}

SummaryPanel::SummaryPanel(QWidget* parent)
    : QWidget(parent)
{
    setObjectName("SummaryPanel");
    setAutoFillBackground(true);
    QPalette pal = palette();
    pal.setColor(QPalette::Window, QColor("#FFFFFF"));
    setPalette(pal);
    buildUI();
}

void SummaryPanel::buildUI() {
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(10, 10, 10, 10);
    mainLayout->setSpacing(4);

    // ===== Toolbar =====
    QWidget* toolbarWidget = new QWidget(this);
    toolbarWidget->setObjectName("toolbarWidget");
    QHBoxLayout* toolbarLayout = new QHBoxLayout(toolbarWidget);
    toolbarLayout->setContentsMargins(0, 0, 0, 0);
    toolbarLayout->setSpacing(8);

    m_cmbModel = new QComboBox(this);
    m_cmbModel->setFixedWidth(130);
    m_cmbModel->addItem(QStringLiteral(u"qwen-vl-plus"), "qwen-vl-plus");
    m_cmbModel->addItem(QStringLiteral(u"qwen-vl-max"), "qwen-vl-max");
    m_cmbModel->addItem(QStringLiteral(u"qwen-vl-flash"), "qwen-vl-flash");
    m_cmbModel->setCurrentIndex(0);

    m_btnStart = new QPushButton(QIcon(QString::fromLatin1(":/SmartPlayer-icon/start_pink.png")), QString(), this);
    m_btnStart->setObjectName("btnStart");
    m_btnStart->setToolTip(QStringLiteral(u"\u5f00\u59cb\u5206\u6790"));
    m_btnStart->setIconSize(QSize(25,25));
    m_btnStop = new QPushButton(QIcon(QString::fromLatin1(":/SmartPlayer-icon/stop_blue.png")), QString(), this);
    m_btnStop->setObjectName("btnStop");
    m_btnStop->setToolTip(QStringLiteral(u"\u505c\u6b62"));
    m_btnStop->setIconSize(QSize(25,25));
    m_btnRerun = new QPushButton(QIcon(QString::fromLatin1(":/SmartPlayer-icon/reset_dark.png")), QString(), this);
    m_btnRerun->setObjectName("btnRerun");
    m_btnRerun->setToolTip(QStringLiteral(u"\u91cd\u65b0\u5206\u6790"));
    m_btnRerun->setIconSize(QSize(25,25));
    m_btnExport = new QPushButton(QStringLiteral(u"\U0001f4cb \u5bfc\u51fa"), this);
    m_btnExport->setObjectName("btnExport");

    // 设置按钮 (打开 SummarySettingsDialog)
    m_btnSettings = new QPushButton(
        QIcon(QString::fromLatin1(":/SmartPlayer-icon/setting_dark.png")),
        QString(), this);
    m_btnSettings->setObjectName("btnSettings");
    m_btnSettings->setToolTip(QStringLiteral(u"AI 总结设置"));
    m_btnSettings->setIconSize(QSize(22, 22));

    m_btnStop->setEnabled(false);
    m_btnRerun->setEnabled(false);
    m_btnExport->setEnabled(false);

    toolbarLayout->addWidget(m_cmbModel, 0, Qt::AlignVCenter);
    toolbarLayout->addWidget(m_btnStart, 0, Qt::AlignVCenter);
    toolbarLayout->addWidget(m_btnStop, 0, Qt::AlignVCenter);
    toolbarLayout->addWidget(m_btnRerun, 0, Qt::AlignVCenter);
    toolbarLayout->addWidget(m_btnExport, 0, Qt::AlignVCenter);
    toolbarLayout->addStretch();
    toolbarLayout->addWidget(m_btnSettings, 0, Qt::AlignVCenter);
    mainLayout->addWidget(toolbarWidget);

    // ===== Meta bar =====
    m_lblMeta = new QLabel(this);
    m_lblMeta->setStyleSheet(QString::fromLatin1(
        "QLabel { color: #9CA3AF; font-size: 11px; font-family: Microsoft YaHei, PingFang SC, sans-serif; }"));
    mainLayout->addWidget(m_lblMeta);

    // ===== TL;DR =====
    mainLayout->addWidget(makeSectionHeader(QStringLiteral(u"\u2605 TL;DR"), this));

    m_lblTldlr = new QLabel(this);
    m_lblTldlr->setStyleSheet(QString::fromLatin1(
        "QLabel { color: #374151; font-size: 14px; font-family: Microsoft YaHei, PingFang SC, sans-serif; "
        "font-weight: 500; padding: 4px 0 10px 0; line-height: 1.6; }"));
    m_lblTldlr->setWordWrap(true);
    m_lblTldlr->setText(QStringLiteral(u"\u672a\u751f\u6210\u5206\u6790"));
    mainLayout->addWidget(m_lblTldlr);

    // ===== Key Takeaways =====
    mainLayout->addWidget(makeSectionHeader(QStringLiteral(u"\u5173\u952e\u8981\u70b9"), this));

    m_keyTakeawaysWidget = new QWidget(this);
    m_keyTakeawaysWidget->setObjectName("keyTakeawaysWidget");
    m_keyTakeawaysLayout = new QVBoxLayout(m_keyTakeawaysWidget);
    m_keyTakeawaysLayout->setContentsMargins(4, 0, 4, 4);
    m_keyTakeawaysLayout->setSpacing(2);
    m_keyTakeawaysWidget->setStyleSheet(QString::fromLatin1(
        "QWidget#keyTakeawaysWidget { background-color: #F3F4F6; border-radius: 8px; padding: 8px; }"));
    mainLayout->addWidget(m_keyTakeawaysWidget);

    // ===== Chapter Timeline =====
    mainLayout->addWidget(makeSectionHeader(QStringLiteral(u"\u7ae0\u8282"), this));

    m_segmentList = new QListWidget(this);
    m_segmentList->setAlternatingRowColors(false);
    m_segmentList->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_segmentList->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    mainLayout->addWidget(m_segmentList, 1);

    // ===== 实体(标签云形态,默认展开) =====
    m_entitiesWidget = new QWidget(this);
    m_entitiesWidget->setObjectName("entitiesWidget");
    QVBoxLayout* entitiesTopLayout = new QVBoxLayout(m_entitiesWidget);
    entitiesTopLayout->setContentsMargins(0, 0, 0, 0);
    entitiesTopLayout->setSpacing(4);

    m_entitiesTitle = new QLabel(QStringLiteral(u"\u63d0\u5230\u7684\u6982\u5ff5 (0)"), this);
    m_entitiesTitle->setObjectName("entitiesTitle");
    m_entitiesTitle->setStyleSheet(QString::fromLatin1(
        "QLabel { color: #1F2937; font-size: 11px; font-family: Microsoft YaHei, PingFang SC, sans-serif; "
        "font-weight: 600; padding: 12px 0 4px 0; letter-spacing: 0.5px; }"));
    entitiesTopLayout->addWidget(m_entitiesTitle);

    m_entitiesContent = new QWidget(this);
    m_entitiesContent->setObjectName("entitiesContent");
    m_entitiesContentLayout = new FlowLayout(m_entitiesContent, 6, 6, 6);
    m_entitiesContentLayout->setContentsMargins(0, 0, 0, 0);
    entitiesTopLayout->addWidget(m_entitiesContent);

    mainLayout->addWidget(m_entitiesWidget);

    // ===== Progress area =====
    m_progressBar = new QProgressBar(this);
    m_progressBar->setRange(0, 100);
    m_progressBar->setValue(0);
    m_progressBar->setTextVisible(true);
    m_progressBar->setFixedHeight(14);
    mainLayout->addWidget(m_progressBar);

    m_lblStatus = new QLabel(QStringLiteral(u"\u72b6\u6001: \u7a7a\u95f2"), this);
    m_lblStatus->setStyleSheet(QString::fromLatin1(
        "QLabel { color: #9CA3AF; font-size: 11px; font-family: Microsoft YaHei, PingFang SC, sans-serif; }"));
    mainLayout->addWidget(m_lblStatus);

    // ===== Connections =====
    connect(m_btnStart, &QPushButton::clicked, this, &SummaryPanel::onStartClicked);
    connect(m_btnStop, &QPushButton::clicked, this, &SummaryPanel::onStopClicked);
    connect(m_btnRerun, &QPushButton::clicked, this, &SummaryPanel::onRerunClicked);
    connect(m_btnExport, &QPushButton::clicked, this, &SummaryPanel::onExportClicked);
    connect(m_btnSettings, &QPushButton::clicked, this, &SummaryPanel::onSettingsClicked);
    connect(m_segmentList, &QListWidget::itemClicked, this, &SummaryPanel::onSegmentClicked);

    // ===== Stylesheet =====
    setStyleSheet(QString::fromLatin1(R"(
        SummaryPanel {
            background-color: #FFFFFF;
            border-left: 1px solid #E5E7EB;
        }
        QWidget#toolbarWidget {
            background-color: transparent;
            padding: 4px 0;
        }
        QPushButton#btnStart, QPushButton#btnStop, QPushButton#btnRerun {
            background-color: transparent;
            border: none;
            border-radius: 6px;
            padding: 4px;
            min-width: 30px;
            max-width: 30px;
            min-height: 30px;
            max-height: 30px;
            icon-size: 18px;
        }
        QPushButton#btnExport {
            background-color: transparent;
            color: #6366F1;
            border: 1px solid #6366F1;
            border-radius: 6px;
            padding: 5px 14px;
            font-size: 12px;
            font-family: Microsoft YaHei, PingFang SC, sans-serif;
            font-weight: 500;
        }
        QPushButton#btnExport:hover {
            background-color: #EEF2FF;
        }
        QPushButton#btnExport:disabled {
            background-color: transparent;
            color: #D1D5DB;
            border-color: #D1D5DB;
        }
        QPushButton#btnSettings {
            background-color: transparent;
            border: none;
            border-radius: 6px;
            padding: 4px;
            min-width: 30px; max-width: 30px;
            min-height: 30px; max-height: 30px;
        }
        QPushButton#btnSettings:hover { background-color: #F3F4F6; }
        QProgressBar {
            border: none;
            border-radius: 4px;
            text-align: center;
            background-color: #F3F4F6;
            color: #374151;
            font-size: 11px;
            font-family: Microsoft YaHei, PingFang SC, sans-serif;
        }
        QProgressBar::chunk {
            background: qlineargradient(spread:pad, x1:0, y1:0, x2:1, y2:0,
                stop:0 #6366F1, stop:1 #8B5CF6);
            border-radius: 4px;
        }
        QListWidget {
            background-color: #F9FAFB;
            color: #374151;
            border: 1px solid #E5E7EB;
            font-family: Microsoft YaHei, PingFang SC, sans-serif;
            font-size: 12px;
            border-radius: 8px;
            outline: none;
        }
        QListWidget::item {
            padding: 7px 10px;
            border-radius: 4px;
            border-bottom: 1px solid #F3F4F6;
        }
        QListWidget::item:hover {
            background-color: #EEF2FF;
            color: #4338CA;
        }
        QListWidget::item:selected {
            background-color: #E0E7FF;
            color: #4338CA;
        }
        QListWidget::item:selected:!active {
            background-color: #E0E7FF;
            color: #4338CA;
        }
        QTextEdit {
            background-color: #F9FAFB;
            color: #374151;
            border: 1px solid #E5E7EB;
            font-family: Microsoft YaHei, PingFang SC, sans-serif;
            font-size: 12px;
            border-radius: 8px;
        }
        QComboBox {
            background-color: #F9FAFB;
            color: #374151;
            border: 1px solid #E5E7EB;
            border-radius: 6px;
            padding: 4px 10px 4px 10px;
            font-size: 12px;
            font-family: Microsoft YaHei, PingFang SC, sans-serif;
            min-width: 0;
        }
        QComboBox:hover {
            border-color: #6366F1;
        }
        QComboBox::drop-down {
            border: none;
            subcontrol-origin: padding;
            subcontrol-position: right center;
            width: 16px;
        }
        QComboBox::down-arrow {
            image: none;
            border-left: 4px solid transparent;
            border-right: 4px solid transparent;
            border-top: 5px solid #9CA3AF;
            margin-right: 2px;
        }
        QComboBox QAbstractItemView {
            background-color: white;
            color: #374151;
            border: 1px solid #E5E7EB;
            border-radius: 6px;
            selection-background-color: #E0E7FF;
            font-family: Microsoft YaHei, PingFang SC, sans-serif;
            font-size: 12px;
            padding: 4px;
        }
        QWidget#entitiesContent {
            background-color: transparent;
        }
        /* 概念标签 chip */
        QPushButton#entityChip {
            background-color: #EEF2FF;
            color: #4338CA;
            border: 1px solid #C7D2FE;
            border-radius: 12px;
            padding: 4px 10px;
            font-size: 12px;
            font-family: Microsoft YaHei, PingFang SC, sans-serif;
            font-weight: 500;
            text-align: center;
        }
        QPushButton#entityChip:hover {
            background-color: #E0E7FF;
            border-color: #818CF8;
            color: #3730A3;
        }
        QPushButton#entityChip:pressed {
            background-color: #C7D2FE;
        }
        /* 类型小标签(如 "概念" / "人物") */
        QLabel#entityTypeTag {
            background-color: transparent;
            color: #6B7280;
            font-size: 10px;
            font-family: Microsoft YaHei, PingFang SC, sans-serif;
            padding: 0 2px;
        }
    )"));
}

void SummaryPanel::bindManager(VideoSummaryManager* mgr) {
    m_manager = mgr;
    if (!m_manager) return;

    connect(m_manager, &VideoSummaryManager::stateChanged,
            this, &SummaryPanel::onStateChanged);
    connect(m_manager, &VideoSummaryManager::segmentAnalyzed,
            this, &SummaryPanel::onSegmentAnalyzed);
    connect(m_manager, &VideoSummaryManager::progressUpdated,
            this, &SummaryPanel::onProgressUpdated);
    connect(m_manager, &VideoSummaryManager::progressDetailChanged,
            this, &SummaryPanel::onProgressDetailChanged);
    connect(m_manager, &VideoSummaryManager::fullReportReady,
            this, &SummaryPanel::onFullReportReady);
    connect(m_manager, &VideoSummaryManager::structuredReportReady,
            this, &SummaryPanel::onStructuredReportReady);
    connect(m_manager, &VideoSummaryManager::errorOccurred,
            this, &SummaryPanel::onErrorOccurred);
}

void SummaryPanel::setVideoPath(const QString& path) {
    // 路径未变 → 不动（避免拖动进度条等场景误清空面板）
    if (path == m_currentVideoPath) return;
    m_currentVideoPath = path;
    // 路径变了 → 重置面板到初始占位态。
    // 旧分析的中断由调用方（MainWindow）负责，此处只负责 UI 状态清理。
    resetPanelForNewVideo();
}

void SummaryPanel::resetPanelForNewVideo() {
    m_analysisInProgress = false;
    m_currentPositionMs = -1;
    m_highlightedSegment = -1;

    // TL;DR
    m_lblTldlr->setText(QStringLiteral(u"未生成分析"));
    m_lblMeta->clear();

    // 关键要点
    {
        QLayoutItem* child;
        while ((child = m_keyTakeawaysLayout->takeAt(0)) != nullptr) {
            delete child->widget();
            delete child;
        }
        QLabel* empty = new QLabel(QStringLiteral(u"(\u6682\u65e0)"), this);
        empty->setStyleSheet(QString::fromLatin1(
            "QLabel { color: #9CA3AF; font-size: 12px; font-family: Microsoft YaHei, PingFang SC, sans-serif; padding: 8px 4px; }"));
        m_keyTakeawaysLayout->addWidget(empty);
    }

    // 章节列表
    m_segmentList->clear();

    // 实体标签云
    clearFlowLayout(m_entitiesContentLayout);
    {
        QLabel* empty = new QLabel(QStringLiteral(u"(\u6682\u65e0)"), m_entitiesContent);
        empty->setStyleSheet(QString::fromLatin1(
            "QLabel { color: #9CA3AF; font-size: 12px; font-family: Microsoft YaHei, PingFang SC, sans-serif; padding: 4px; }"));
        m_entitiesContentLayout->addWidget(empty);
    }
    if (m_entitiesTitle) {
        m_entitiesTitle->setText(QStringLiteral(u"\u63d0\u5230\u7684\u6982\u5ff5 (0)"));
    }

    // 进度 / 状态
    m_progressBar->setValue(0);
    m_lblStatus->setText(QStringLiteral(u"\u72b6\u6001: \u7a7a\u95f2"));

    // 按钮重置（与 onStateChanged(Idle) 的逻辑保持一致）
    m_btnStart->setEnabled(true);
    m_btnStop->setEnabled(false);
    m_btnRerun->setEnabled(false);
    m_btnExport->setEnabled(false);
    m_cmbModel->setEnabled(true);
}

void SummaryPanel::onStartClicked() {
    if (!m_manager) return;
    if (m_currentVideoPath.isEmpty()) {
        m_lblStatus->setText(QStringLiteral(u"\u9519\u8bef: \u8bf7\u5148\u6253\u5f00\u8981\u5206\u6790\u7684\u89c6\u9891"));
        return;
    }

    // 先查缓存:命中则直接显示结果,不重跑 LLM/VLM
    if (m_manager->tryLoadFromCache(m_currentVideoPath)) {
        populateFromReport(m_manager->report());
        const SummaryReport& r = m_manager->report();
        QString when = r.generatedAt.isValid()
            ? r.generatedAt.toString(QStringLiteral(u"yyyy-MM-dd HH:mm"))
            : QStringLiteral(u"\u672a\u77e5\u65f6\u95f4");
        m_lblStatus->setText(QStringLiteral(u"\u72b6\u6001: \u5df2\u4ece\u7f13\u5b58\u52a0\u8f7d (\u5206\u6790\u4e8e %1)").arg(when));
        if (m_lblMeta) {
            m_lblMeta->setText(QStringLiteral(u"\u4ece\u7f13\u5b58\u6062\u590d \u00b7 \u5206\u6790\u4e8e %1").arg(when));
        }
        m_btnStart->setEnabled(false);
        m_btnStop->setEnabled(false);
        m_btnRerun->setEnabled(true);
        m_btnExport->setEnabled(true);
        m_cmbModel->setEnabled(false);
        m_progressBar->setValue(100);
        m_analysisInProgress = false;
        return;
    }

    if (m_cmbModel->currentIndex() >= 0) {
        m_manager->setModel(m_cmbModel->currentData().toString());
    }
    m_manager->startSummary(m_currentVideoPath);
}

void SummaryPanel::onStopClicked() {
    if (!m_manager) return;
    m_manager->stopSummary();
}

void SummaryPanel::onRerunClicked() {
    onStartClicked();
}

void SummaryPanel::onExportClicked() {
    if (!m_manager) return;
    const SummaryReport& r = m_manager->report();
    if (!r.isValid) {
        QMessageBox::information(this, QStringLiteral(u"\u5bfc\u51fa"), QStringLiteral(u"\u6ca1\u6709\u53ef\u5bfc\u51fa\u7684\u5206\u6790\u7ed3\u679c"));
        return;
    }
    exportMarkdown(r);
}

void SummaryPanel::onSettingsClicked() {
    SummarySettingsDialog dlg(this);
    dlg.exec();
    // 配置改动不打断正在运行的分析,只在下次 startSummary 生效
    // 如果用户改的是 model,这里刷新一下 combobox
    if (m_manager) {
        QString model = ConfigManager::instance().getSummaryModel();
        m_manager->setModel(model);
    }
}

void SummaryPanel::onStateChanged(SummaryState state) {
    switch (state) {
        case SummaryState::Idle:
            m_btnStart->setEnabled(true);
            m_btnStop->setEnabled(false);
            m_btnRerun->setEnabled(false);
            m_btnExport->setEnabled(false);
            m_lblStatus->setText(QStringLiteral(u"\u72b6\u6001: \u7a7a\u95f2"));
            m_progressBar->setValue(0);
            m_analysisInProgress = false;
            break;
        case SummaryState::ExtractingFrames:
            m_btnStart->setEnabled(false);
            m_btnStop->setEnabled(true);
            m_cmbModel->setEnabled(false);
            m_lblStatus->setText(QStringLiteral(u"\u72b6\u6001: \u63d0\u53d6\u5173\u952e\u5e27..."));
            break;
        case SummaryState::RunningASR:
            m_lblStatus->setText(QStringLiteral(u"\u72b6\u6001: \u8bed\u97f3\u8bc6\u522b\u4e2d..."));
            break;
        case SummaryState::AnalyzingSegments:
            m_lblStatus->setText(QStringLiteral(u"\u72b6\u6001: \u5206\u6790\u65f6\u95f4\u6bb5..."));
            m_analysisInProgress = true;
            break;
        case SummaryState::Stopping:
            m_lblStatus->setText(QStringLiteral(u"\u72b6\u6001: \u6b63\u5728\u505c\u6b62..."));
            m_btnStop->setEnabled(false);
            break;
        case SummaryState::Finished:
            m_btnStart->setEnabled(true);
            m_btnStop->setEnabled(false);
            m_btnRerun->setEnabled(true);
            m_btnExport->setEnabled(true);
            m_cmbModel->setEnabled(true);
            m_lblStatus->setText(QStringLiteral(u"\u72b6\u6001: \u5206\u6790\u5b8c\u6210"));
            m_progressBar->setValue(100);
            break;
        case SummaryState::Error:
            m_btnStart->setEnabled(true);
            m_btnStop->setEnabled(false);
            m_cmbModel->setEnabled(true);
            m_lblStatus->setText(QStringLiteral(u"\u72b6\u6001: \u53d1\u751f\u9519\u8bef"));
            m_progressBar->setValue(0);
            m_analysisInProgress = false;
            break;
    }
}

void SummaryPanel::onSegmentAnalyzed(int index, const QString& desc) {
    if (m_analysisInProgress) return;
    updateSegmentItem(index, desc, true);
}

void SummaryPanel::onProgressUpdated(double progress) {
    m_progressBar->setValue(qRound(progress * 100));
}

void SummaryPanel::onProgressDetailChanged(const VideoSummaryManager::Progress& progress) {
    updateStatusLabel(progress);
    if (progress.totalSegments > 0 && progress.stage == SummaryState::AnalyzingSegments) {
        m_lblStatus->setText(QStringLiteral(u"\u5206\u6790\u4e2d %1/%2 \u6bb5")
            .arg(progress.currentSegment + 1)
            .arg(progress.totalSegments));
    }

    // 构建初始 segment 列表骨架（时间范围占位，无描述）
    // 列表在分析过程中保持空白/骨架态，直到 structuredReportReady 才填入章节标题
    if (m_segmentList->count() == 0 && progress.totalSegments > 0) {
        rebuildSegmentList();
    }

    if (progress.stage == SummaryState::Finished) {
        QString model = m_cmbModel->currentText();
        m_lblMeta->setText(QStringLiteral(u"%1 \u00b7 %2 \u6bb5 \u00b7 %3 \u00b7 \u5206\u6790\u5b8c\u6210")
            .arg(formatMetaTime(m_manager ? m_manager->progress().stageProgress * 0 : 0))
            .arg(progress.totalSegments)
            .arg(model));
    }
}

void SummaryPanel::onFullReportReady(const QString& reportJson) {
    Q_UNUSED(reportJson);
}

void SummaryPanel::onStructuredReportReady(const SummaryReport& report) {
    populateFromReport(report);
}

void SummaryPanel::onErrorOccurred(const QString& message) {
    m_lblStatus->setText(QStringLiteral(u"\u9519\u8bef: %1").arg(message));
}

int SummaryPanel::findSegmentAtMs(qint64 ms) const {
    if (!m_manager) return -1;
    int count = m_manager->segmentCount();
    for (int i = 0; i < count; ++i) {
        const SummarySegment* seg = m_manager->segmentAt(i);
        if (seg && ms >= seg->startMs && ms < seg->endMs) {
            return i;
        }
    }
    return -1;
}

void SummaryPanel::onSegmentClicked(QListWidgetItem* item) {
    int row = m_segmentList->row(item);
    if (m_manager) {
        const SummarySegment* seg = m_manager->segmentAt(row);
        if (seg) {
            emit seekTo(seg->startMs * 1000);
        }
    }
}

void SummaryPanel::onEntityClicked(const QString& entityName, qint64 ms) {
    Q_UNUSED(entityName);
    emit seekTo(ms * 1000);
}

void SummaryPanel::onPositionChanged(qint64 ms) {
    m_currentPositionMs = ms;
    highlightCurrentSegment(ms);
}

void SummaryPanel::highlightCurrentSegment(qint64 ms) {
    int segIdx = findSegmentAtMs(ms);
    if (segIdx == m_highlightedSegment) return;
    m_highlightedSegment = segIdx;

    for (int i = 0; i < m_segmentList->count(); ++i) {
        QListWidgetItem* item = m_segmentList->item(i);
        if (!item) continue;
        if (i == segIdx) {
            item->setBackground(QBrush(QColor(238, 242, 255)));
            item->setForeground(QBrush(QColor(99, 102, 241)));
        } else {
            item->setBackground(QBrush(QColor(249, 250, 251)));
            item->setForeground(QBrush(QColor(55, 65, 81)));
        }
    }
    if (segIdx >= 0) {
        m_segmentList->scrollToItem(m_segmentList->item(segIdx), QAbstractItemView::PositionAtCenter);
    }
}

bool SummaryPanel::eventFilter(QObject* watched, QEvent* event) {
    if (event->type() == QEvent::MouseButtonPress) {
        // 实体 chip 点击 → 跳转到首次出现时间
        QPushButton* chip = qobject_cast<QPushButton*>(watched);
        if (chip && chip->objectName() == QStringLiteral("entityChip")) {
            qint64 ms = chip->property("entityMs").toLongLong();
            QString name = chip->property("entityName").toString();
            onEntityClicked(name, ms);
            return true;
        }
    }
    return QWidget::eventFilter(watched, event);
}

void SummaryPanel::updateSegmentItem(int index, const QString& desc, bool isAnalyzed) {
    if (index < 0 || index >= m_segmentList->count()) return;
    QListWidgetItem* item = m_segmentList->item(index);
    if (!item) return;

    if (m_manager) {
        const SummarySegment* seg = m_manager->segmentAt(index);
        if (seg) {
            QString timeRange = QStringLiteral(u"%1 - %2")
                .arg(formatDuration(seg->startMs))
                .arg(formatDuration(seg->endMs));
            QString marker = (index == m_highlightedSegment) ? QStringLiteral(u"\u25cf ") : QStringLiteral(u"\u25cb ");
            item->setText(QStringLiteral(u"%1%2  %3").arg(marker).arg(timeRange, -14).arg(desc));
        }
    }
}

void SummaryPanel::updateStatusLabel(const VideoSummaryManager::Progress& progress) {
    Q_UNUSED(progress);
}

void SummaryPanel::rebuildSegmentList() {
    rebuildSegmentList({});
}

void SummaryPanel::rebuildSegmentList(const QList<SummaryChapter>& chapters) {
    if (!m_manager) return;

    m_segmentList->clear();
    int count = m_manager->segmentCount();
    for (int i = 0; i < count; ++i) {
        const SummarySegment* seg = m_manager->segmentAt(i);
        if (!seg) continue;

        QString timeRange = QStringLiteral(u"%1 - %2")
            .arg(formatDuration(seg->startMs))
            .arg(formatDuration(seg->endMs));

        // 优先从 report 的 chapter 中取标题
        QString displayTitle;
        for (const SummaryChapter& ch : chapters) {
            if (ch.startMs < seg->endMs && ch.endMs > seg->startMs) {
                if (!ch.title.isEmpty()) {
                    displayTitle = ch.title;
                    break;
                }
            }
        }
        // 其次用 segment 自身的 description
        if (displayTitle.isEmpty() && seg->isAnalyzed && !seg->description.isEmpty()
            && !seg->description.contains(QStringLiteral(u"(无画面"))
            && !seg->description.contains(QStringLiteral(u"(分析失败"))) {
            displayTitle = seg->description;
        }

        QString text;
        if (!displayTitle.isEmpty()) {
            text = QStringLiteral(u"\u25cb %1  %2").arg(timeRange, -14).arg(displayTitle);
        } else if (seg->isAnalyzed) {
            text = QStringLiteral(u"\u25cb %1  %2").arg(timeRange, -14).arg(seg->description);
        } else {
            text = QStringLiteral(u"\u25cb %1  \u5206\u6790\u4e2d...").arg(timeRange);
            QFont f = m_segmentList->font();
            QListWidgetItem* item = new QListWidgetItem(text, m_segmentList);
            item->setForeground(QBrush(QColor(120, 120, 120)));
            continue;
        }
        m_segmentList->addItem(text);
    }
}

void SummaryPanel::populateFromReport(const SummaryReport& report) {
    // TL;DR
    m_lblTldlr->setText(report.tldr.isEmpty()
        ? QStringLiteral(u"\u672a\u83b7\u5f97\u7ed3\u8bba")
        : report.tldr);

    // Key Takeaways
    QLayoutItem* child;
    while ((child = m_keyTakeawaysLayout->takeAt(0)) != nullptr) {
        delete child->widget();
        delete child;
    }
    if (report.keyTakeaways.isEmpty()) {
        QLabel* empty = new QLabel(QStringLiteral(u"(\u6682\u65e0)"), this);
        empty->setStyleSheet(QString::fromLatin1(
            "QLabel { color: #9CA3AF; font-size: 12px; font-family: Microsoft YaHei, PingFang SC, sans-serif; padding: 8px 4px; }"));
        m_keyTakeawaysLayout->addWidget(empty);
    } else {
        for (const QString& pt : report.keyTakeaways) {
            QLabel* bullet = new QLabel(QStringLiteral(u"\u2022 ") + pt, this);
            bullet->setStyleSheet(QString::fromLatin1(
                "QLabel { color: #374151; font-size: 12px; font-family: Microsoft YaHei, PingFang SC, sans-serif; "
                "padding: 3px 4px; line-height: 1.5; }"));
            bullet->setWordWrap(true);
            m_keyTakeawaysLayout->addWidget(bullet);
        }
    }

    // 实体 (标签云:每个概念一个 chip,自动换行,默认展开)
    clearFlowLayout(m_entitiesContentLayout);
    if (report.entities.isEmpty()) {
        QLabel* empty = new QLabel(QStringLiteral(u"(\u6682\u65e0)"), m_entitiesContent);
        empty->setStyleSheet(QString::fromLatin1(
            "QLabel { color: #9CA3AF; font-size: 12px; font-family: Microsoft YaHei, PingFang SC, sans-serif; padding: 4px; }"));
        m_entitiesContentLayout->addWidget(empty);
    } else {
        for (const SummaryEntity& e : report.entities) {
            // 用 QPushButton 当 chip,自带 hover/pressed 效果
            QPushButton* chip = new QPushButton(e.name, m_entitiesContent);
            chip->setObjectName(QStringLiteral("entityChip"));
            chip->setCursor(Qt::PointingHandCursor);
            chip->setToolTip(QStringLiteral(u"%1 · %2 · \u9996\u6b21\u51fa\u73b0 %3")
                .arg(e.name, e.type, formatDuration(e.firstMentionMs)));
            chip->setProperty("entityName", e.name);
            chip->setProperty("entityMs", e.firstMentionMs);
            chip->installEventFilter(this);
            m_entitiesContentLayout->addWidget(chip);
        }
    }
    if (m_entitiesTitle) {
        m_entitiesTitle->setText(QStringLiteral(u"\u63d0\u5230\u7684\u6982\u5ff5 (%1)").arg(report.entities.size()));
    }

    // Rebuild segment list with chapter titles from report
    // 解除屏蔽后立即刷新,此时已有章节标题
    m_analysisInProgress = false;
    rebuildSegmentList(report.chapters);

    // Enable export
    m_btnExport->setEnabled(true);
}

void SummaryPanel::exportMarkdown(const SummaryReport& report) {
    QString fileName = QFileDialog::getSaveFileName(this,
        QStringLiteral(u"\u5bfc\u51fa\u4e3a Markdown"),
        QDir::homePath() + "/video_summary.md",
        QStringLiteral(u"Markdown (*.md)"));

    if (fileName.isEmpty()) return;

    QString md;
    md += "# \u89c6\u9891 AI \u5206\u6790\u62a5\u544a\n\n";

    if (!report.tldr.isEmpty()) {
        md += "## TL;DR\n\n" + report.tldr + "\n\n";
    }
    if (!report.keyTakeaways.isEmpty()) {
        md += "## \u5173\u952e\u8981\u70b9\n\n";
        for (const QString& pt : report.keyTakeaways) {
            md += "- " + pt + "\n";
        }
        md += "\n";
    }
    if (!report.chapters.isEmpty()) {
        md += "## \u7ae0\u8282\u65f6\u95f4\u8f74\n\n";
        for (const SummaryChapter& ch : report.chapters) {
            md += QStringLiteral(u"- [%1 - %2] %3\n")
                .arg(formatDuration(ch.startMs))
                .arg(formatDuration(ch.endMs))
                .arg(ch.title);
        }
        md += "\n";
    }
    if (!report.entities.isEmpty()) {
        md += "## \u5173\u952e\u5b57\u8bcd\n\n";
        for (const SummaryEntity& e : report.entities) {
            md += QStringLiteral(u"- **%1** (%2) \u2014 \u9996\u6b21\u51fa\u73b0: %3\n")
                .arg(e.name)
                .arg(e.type)
                .arg(formatDuration(e.firstMentionMs));
        }
        md += "\n";
    }
    if (!report.fullMarkdown.isEmpty()) {
        QString cleanMd = report.fullMarkdown;
        QRegularExpression chapterListRe(
            QStringLiteral(u"##\\s*章节列表[\\s\\S]*?(?=\\n##\\s|\\Z)"));
        cleanMd.remove(chapterListRe);
        cleanMd = cleanMd.trimmed();
        if (!cleanMd.isEmpty()) {
            md += "## \u8be6\u7ec6\u5185\u5bb9\n\n" + cleanMd + "\n\n";
        }
    }

    md += QStringLiteral(u"---\n*\u7531 AI \u81ea\u52a8\u751f\u6210 \u00b7 %1*")
        .arg(QDateTime::currentDateTime().toString(QStringLiteral(u"yyyy-MM-dd HH:mm")));

    QFile f(fileName);
    if (f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        f.write(md.toUtf8());
        f.close();
        QMessageBox::information(this, QStringLiteral(u"\u5bfc\u51fa\u6210\u529f"),
            QStringLiteral(u"\u5df2\u5b58\u5230: %1").arg(fileName));
    } else {
        QMessageBox::warning(this, QStringLiteral(u"\u5bfc\u51fa\u5931\u8d25"), f.errorString());
    }
}
