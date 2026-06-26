#include "summaryviewmodel.h"
#include "app/configmanager.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QRegularExpression>

namespace {
QString formatDurationMs(qint64 ms) {
    int totalSec = int(ms / 1000);
    int h = totalSec / 3600;
    int m = (totalSec % 3600) / 60;
    int s = totalSec % 60;
    if (h > 0) return QString::asprintf("%d:%02d:%02d", h, m, s);
    return QString::asprintf("%02d:%02d", m, s);
}
}

SummaryViewModel::SummaryViewModel(QObject* parent)
    : IViewModel(parent)
{
    m_manager = new VideoSummaryManager(this);

    // 同步 model 缓存：VM 启动后取 ConfigManager 当前值，避免初始为空
    m_model = ConfigManager::instance().getSummaryModel();
    if (!m_model.isEmpty()) {
        m_manager->setModel(m_model);
    }

    connect(m_manager, &VideoSummaryManager::stateChanged,
            this, &SummaryViewModel::onMgrStateChanged);
    connect(m_manager, &VideoSummaryManager::segmentAnalyzed,
            this, &SummaryViewModel::onMgrSegmentAnalyzed);
    connect(m_manager, &VideoSummaryManager::progressUpdated,
            this, &SummaryViewModel::onMgrProgressUpdated);
    connect(m_manager, &VideoSummaryManager::progressDetailChanged,
            this, &SummaryViewModel::onMgrProgressDetail);
    connect(m_manager, &VideoSummaryManager::structuredReportReady,
            this, &SummaryViewModel::onMgrStructuredReportReady);
    connect(m_manager, &VideoSummaryManager::fullReportReady,
            this, &SummaryViewModel::onMgrFullReportReady);
    connect(m_manager, &VideoSummaryManager::asrCompleted,
            this, &SummaryViewModel::onMgrAsrCompleted);
    connect(m_manager, &VideoSummaryManager::errorOccurred,
            this, &SummaryViewModel::onMgrError);
}

SummaryViewModel::~SummaryViewModel() = default;

const QList<SubtitleItem>& SummaryViewModel::asrResults() const {
    static const QList<SubtitleItem> kEmpty;
    return m_manager ? m_manager->asrResults() : kEmpty;
}

bool SummaryViewModel::tryLoadFromCache(const QString& videoPath) {
    if (!m_manager) return false;
    bool ok = m_manager->tryLoadFromCache(videoPath);
    if (ok) {
        m_report = m_manager->report();
        emit reportChanged();
    }
    return ok;
}

void SummaryViewModel::saveToCache() {
    if (!m_manager) return;
    m_manager->saveToCache(m_videoPath);
}

// ============================================================
// Commands
// ============================================================
void SummaryViewModel::setVideoPath(const QString& path) {
    if (path == m_videoPath) return;
    m_videoPath = path;

    // 路径变化 → 若 manager 仍在工作，先打断
    if (m_manager &&
        m_state != SummaryState::Idle &&
        m_state != SummaryState::Finished &&
        m_state != SummaryState::Error) {
        m_manager->stopSummary();
    }

    // 重置 VM 端缓存（避免 hasReport 仍是上一个视频的 true）
    m_report = SummaryReport{};
    m_lastError.clear();
    m_overall = m_stageProgress = 0.0;
    m_currentSegment = m_totalSegments = 0;

    emit videoPathChanged(m_videoPath);
    emit reportChanged();
    emit progressChanged();
}

void SummaryViewModel::start() {
    if (!m_manager || m_videoPath.isEmpty()) return;

    // 先尝试缓存命中
    if (m_manager->tryLoadFromCache(m_videoPath)) {
        m_report = m_manager->report();
        emit reportChanged();
        // manager 在 tryLoadFromCache 内部不会 emit stateChanged，
        // 由 VM 主动同步状态到 Finished，方便 View 收到统一的"已完成"信号
        if (m_state != SummaryState::Finished) {
            m_state = SummaryState::Finished;
            emit stateChanged(m_state);
        }
        // 同步发出 asrCompleted（缓存场景下 manager 内部已写入 m_asrResults，但不会 emit）
        if (!m_report.asrResults.isEmpty()) {
            emit asrCompleted(m_report.asrResults);
        }
        return;
    }

    m_manager->startSummary(m_videoPath);
}

void SummaryViewModel::stop() {
    if (!m_manager) return;
    m_manager->stopSummary();
}

void SummaryViewModel::rerun() {
    // 直接 start：start() 内部会先查缓存命中；如果用户期望"强制重跑"
    // 当前流程是先 stop 再 startSummary（绕过缓存的能力由 manager 控制）。
    // 与旧 SummaryPanel::onRerunClicked 语义保持一致。
    if (!m_manager || m_videoPath.isEmpty()) return;
    m_manager->startSummary(m_videoPath);
}

void SummaryViewModel::setModel(const QString& model) {
    if (model == m_model) return;
    m_model = model;
    if (m_manager) m_manager->setModel(model);
    emit modelChanged(m_model);
}

void SummaryViewModel::exportMarkdownTo(const QString& filePath) {
    if (filePath.isEmpty()) return;
    if (!m_report.isValid) {
        m_lastError = QStringLiteral(u"没有可导出的分析结果");
        emit errorOccurred(m_lastError);
        return;
    }

    QString md;
    md += QStringLiteral(u"# 视频 AI 分析报告\n\n");

    if (!m_report.tldr.isEmpty()) {
        md += QStringLiteral(u"## TL;DR\n\n") + m_report.tldr + "\n\n";
    }
    if (!m_report.keyTakeaways.isEmpty()) {
        md += QStringLiteral(u"## 关键要点\n\n");
        for (const QString& pt : m_report.keyTakeaways) {
            md += "- " + pt + "\n";
        }
        md += "\n";
    }
    if (!m_report.chapters.isEmpty()) {
        md += QStringLiteral(u"## 章节时间轴\n\n");
        for (const SummaryChapter& ch : m_report.chapters) {
            md += QStringLiteral(u"- [%1 - %2] %3\n")
                .arg(formatDurationMs(ch.startMs))
                .arg(formatDurationMs(ch.endMs))
                .arg(ch.title);
        }
        md += "\n";
    }
    if (!m_report.entities.isEmpty()) {
        md += QStringLiteral(u"## 关键字词\n\n");
        for (const SummaryEntity& e : m_report.entities) {
            md += QStringLiteral(u"- **%1** (%2) — 首次出现: %3\n")
                .arg(e.name)
                .arg(e.type)
                .arg(formatDurationMs(e.firstMentionMs));
        }
        md += "\n";
    }
    if (!m_report.fullMarkdown.isEmpty()) {
        QString cleanMd = m_report.fullMarkdown;
        QRegularExpression chapterListRe(
            QStringLiteral(u"##\\s*章节列表[\\s\\S]*?(?=\\n##\\s|\\Z)"));
        cleanMd.remove(chapterListRe);
        cleanMd = cleanMd.trimmed();
        if (!cleanMd.isEmpty()) {
            md += QStringLiteral(u"## 详细内容\n\n") + cleanMd + "\n\n";
        }
    }

    md += QStringLiteral(u"---\n*由 AI 自动生成 · %1*")
        .arg(QDateTime::currentDateTime().toString(QStringLiteral(u"yyyy-MM-dd HH:mm")));

    QFile f(filePath);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        m_lastError = f.errorString();
        emit errorOccurred(m_lastError);
        return;
    }
    f.write(md.toUtf8());
    f.close();
}

// ============================================================
// Manager → VM 信号翻译
// ============================================================
void SummaryViewModel::onMgrStateChanged(SummaryState s) {
    if (s == m_state) return;
    m_state = s;
    emit stateChanged(m_state);
}

void SummaryViewModel::onMgrSegmentAnalyzed(int index, const QString& description) {
    emit segmentAnalyzed(index, description);
}

void SummaryViewModel::onMgrProgressUpdated(double progress) {
    m_overall = progress;
    emit progressChanged();
}

void SummaryViewModel::onMgrProgressDetail(const VideoSummaryManager::Progress& p) {
    m_overall         = p.overallProgress;
    m_stageProgress   = p.stageProgress;
    m_currentSegment  = p.currentSegment;
    m_totalSegments   = p.totalSegments;
    if (p.stage != m_state) {
        m_state = p.stage;
        emit stateChanged(m_state);
    }
    emit progressChanged();
}

void SummaryViewModel::onMgrStructuredReportReady(const SummaryReport& report) {
    m_report = report;
    emit reportChanged();
}

void SummaryViewModel::onMgrFullReportReady(const QString& reportJson) {
    emit fullReportReady(reportJson);
}

void SummaryViewModel::onMgrAsrCompleted(const QList<SubtitleItem>& items) {
    emit asrCompleted(items);
}

void SummaryViewModel::onMgrError(const QString& msg) {
    m_lastError = msg;
    emit errorOccurred(msg);
}
