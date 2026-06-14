#include "semanticsegmenter.h"
#include "videosummarynetworkclient.h"
#include <QDebug>
#include <algorithm>
#include <numeric>

namespace {

QString normalizeText(const QString& text) {
    QString result = text.toLower();
    for (int i = result.size() - 1; i >= 0; --i) {
        QChar c = result[i];
        if (c.isLetterOrNumber() || c.isSpace() ||
            (c.unicode() >= 0x4E00 && c.unicode() <= 0x9FA5)) {
            continue;
        }
        result[i] = QChar(u' ');
    }
    result = result.simplified();
    return result;
}

QStringList tokenize(const QString& text) {
    QStringList tokens;
    QString currentEn;

    for (QChar c : text) {
        if (c.isLetterOrNumber()) {
            currentEn.append(c);
        } else {
            if (!currentEn.isEmpty()) {
                tokens.append(currentEn);
                currentEn.clear();
            }
            if (c.unicode() >= 0x4E00 && c.unicode() <= 0x9FA5) {
                tokens.append(QString(c));
            }
        }
    }
    if (!currentEn.isEmpty()) {
        tokens.append(currentEn);
    }

    return tokens;
}

}

QVector<double> SemanticSegmenter::tfidfVector(const QString& text) {
    QString normalized = normalizeText(text);
    if (normalized.isEmpty()) {
        return QVector<double>(512, 0.0);
    }

    QStringList tokens = tokenize(normalized);

    QMap<QString, int> tf;
    for (const QString& tok : tokens) {
        tf[tok]++;
    }

    static const int VEC_SIZE = 512;
    QVector<double> vec(VEC_SIZE, 0.0);
    for (auto it = tf.constBegin(); it != tf.constEnd(); ++it) {
        int idx = qHash(it.key()) % VEC_SIZE;
        vec[idx] = static_cast<double>(it.value());
    }

    double norm = 0.0;
    for (double v : vec) norm += v * v;
    norm = std::sqrt(norm);
    if (norm > 1e-10) {
        for (double& v : vec) v /= norm;
    }

    return vec;
}

double SemanticSegmenter::cosineSimilarity(const QVector<double>& a,
                                         const QVector<double>& b) {
    if (a.size() != b.size()) return 0.0;
    double dot = 0.0;
    for (int i = 0; i < a.size(); ++i) {
        dot += a[i] * b[i];
    }
    return dot;
}

QVector<double> SemanticSegmenter::computeSentenceSimilarity(
    const QList<SubtitleItem>& asrResults) {

    QVector<double> similarities;
    if (asrResults.size() < 2) return similarities;

    for (int i = 0; i < asrResults.size() - 1; ++i) {
        QString textA = QString::fromStdString(asrResults[i].text);
        QString textB = QString::fromStdString(asrResults[i + 1].text);

        if (textA.isEmpty() || textB.isEmpty()) {
            similarities.append(0.0);
            continue;
        }

        QVector<double> vecA = tfidfVector(textA);
        QVector<double> vecB = tfidfVector(textB);

        similarities.append(cosineSimilarity(vecA, vecB));
    }
    return similarities;
}

QList<qint64> SemanticSegmenter::textTiling(
    const QList<SubtitleItem>& asrResults,
    const QVector<double>& similarities) {

    if (similarities.isEmpty()) return {};

    const int WINDOW = 5;

    QVector<double> smoothed(similarities.size());
    for (int i = 0; i < similarities.size(); ++i) {
        double sum = 0.0;
        int cnt = 0;
        for (int j = qMax(0, i - WINDOW); j <= qMin(similarities.size() - 1, i + WINDOW); ++j) {
            sum += similarities[j];
            cnt++;
        }
        smoothed[i] = sum / cnt;
    }

    QVector<double> depthScores;
    for (int i = WINDOW; i < smoothed.size() - WINDOW; ++i) {
        double beforeMean = 0.0;
        for (int j = i - WINDOW; j < i; ++j) beforeMean += smoothed[j];
        beforeMean /= WINDOW;

        double afterMean = 0.0;
        for (int j = i + 1; j <= i + WINDOW; ++j) afterMean += smoothed[j];
        afterMean /= WINDOW;

        double depth = (beforeMean + afterMean) / 2.0 - smoothed[i];
        depthScores.append(depth);
    }

    if (depthScores.isEmpty()) return {};

    double meanDepth = 0.0;
    for (double d : depthScores) meanDepth += d;
    meanDepth /= depthScores.size();

    double varDepth = 0.0;
    for (double d : depthScores) varDepth += (d - meanDepth) * (d - meanDepth);
    varDepth /= depthScores.size();
    double stdDepth = std::sqrt(varDepth);

    double threshold = meanDepth + 0.3 * stdDepth + 0.005;

    QList<qint64> boundaries;
    for (int i = 1; i < depthScores.size() - 1; ++i) {
        if (depthScores[i] > threshold &&
            depthScores[i] > depthScores[i - 1] &&
            depthScores[i] > depthScores[i + 1]) {
            qint64 boundaryMs = qRound(asrResults[i].end_sec * 1000.0);
            boundaries.append(boundaryMs);
        }
    }

    QList<qint64> merged;
    for (qint64 b : boundaries) {
        if (merged.isEmpty() ||
            b - merged.last() >= m_config.minSegmentMs) {
            merged.append(b);
        }
    }

    qDebug() << "[SemanticSegmenter] === TextTiling 诊断 ===";
    qDebug() << "[SemanticSegmenter] ASR 条数:" << asrResults.size()
             << "→ 相似度数组长度:" << similarities.size();
    qDebug() << "[SemanticSegmenter] 相似度样本 (前5个):"
             << QVariant::fromValue(similarities.mid(0, qMin(5, similarities.size()))).toString();
    double simMean = similarities.isEmpty() ? 0.0 : std::accumulate(similarities.begin(), similarities.end(), 0.0) / similarities.size();
    qDebug() << "[SemanticSegmenter] 相似度均值:" << simMean;
    qDebug() << "[SemanticSegmenter] WINDOW:" << WINDOW
             << "depthScores 长度:" << depthScores.size()
             << "meanDepth:" << meanDepth << "stdDepth:" << stdDepth
             << "threshold:" << threshold;
    qDebug() << "[SemanticSegmenter] === TextTiling 边界详情 ===";
    for (int i = 1; i < depthScores.size() - 1; ++i) {
        bool isBoundary = depthScores[i] > threshold
                       && depthScores[i] > depthScores[i - 1]
                       && depthScores[i] > depthScores[i + 1];
        qDebug() << "  pos=" << (i + WINDOW)
                 << "depth=" << depthScores[i]
                 << "threshold=" << threshold
                 << (isBoundary ? "✓ 边界" : "✗");
    }
    qDebug() << "[SemanticSegmenter] TextTiling 原始边界数:" << boundaries.size();
    for (qint64 b : boundaries) {
        int idx = -1;
        for (int i = 0; i < asrResults.size() - 1; ++i) {
            if (qAbs(qRound(asrResults[i].end_sec * 1000.0) - b) < 1000) {
                idx = i;
                break;
            }
        }
        int depthIdx = qBound(0, idx - WINDOW, depthScores.size() - 1);
        qDebug() << "  TextTiling boundary at" << b << "ms (ASR index" << idx << ")"
                 << "depthScore:" << (depthIdx >= 0 && depthIdx < depthScores.size() ? depthScores[depthIdx] : -1);
    }
    qDebug() << "[SemanticSegmenter] TextTiling 合并后边界数:" << merged.size();
    qDebug() << "[SemanticSegmenter] === TextTiling 诊断结束 ===";

    return merged;
}

QList<SemanticBoundary> SemanticSegmenter::detectAudioBoundaries(
    const QList<SubtitleItem>& asrResults) {

    QList<SemanticBoundary> boundaries;

    if (asrResults.size() < 3) {
        return boundaries;
    }

    QVector<double> similarities = computeSentenceSimilarity(asrResults);
    QList<qint64> textTilingBoundaries = textTiling(asrResults, similarities);

    for (qint64 ts : textTilingBoundaries) {
        SemanticBoundary sb;
        sb.timestampMs = ts;
        sb.audioScore = 0.8;
        sb.audioReason = QStringLiteral(u"ASR 语义相似度骤降");
        boundaries.append(sb);
    }

    return boundaries;
}

QList<SemanticBoundary> SemanticSegmenter::detectVideoBoundaries(
    const QList<QPair<qint64, QString>>& sceneTags) {

    QList<SemanticBoundary> boundaries;

    if (sceneTags.size() < 2) return boundaries;

    for (int i = 1; i < sceneTags.size(); ++i) {
        const QString& prevTag = sceneTags[i - 1].second;
        const QString& currTag = sceneTags[i].second;

        if (!currTag.isEmpty() && !prevTag.isEmpty() && prevTag != currTag) {
            SemanticBoundary sb;
            sb.timestampMs = sceneTags[i].first;
            sb.videoScore = 0.9;
            sb.videoReason = QStringLiteral(u"场景切换: %1 → %2").arg(prevTag).arg(currTag);
            boundaries.append(sb);
        }
    }

    return boundaries;
}

QList<qint64> SemanticSegmenter::fuseAndSegment(
    const QList<SemanticBoundary>& audioBoundaries,
    const QList<SemanticBoundary>& videoBoundaries,
    qint64 totalDurationMs) {

    struct Candidate {
        qint64 ts;
        double score;
        QString reason;
    };
    QList<Candidate> candidates;

    for (const auto& sb : audioBoundaries) {
        candidates.append({sb.timestampMs, sb.audioScore * m_config.audioWeight,
                         sb.audioReason});
    }
    for (const auto& sb : videoBoundaries) {
        candidates.append({sb.timestampMs, sb.videoScore * m_config.videoWeight,
                         sb.videoReason});
    }

    if (candidates.isEmpty()) {
        return {0, totalDurationMs};
    }

    std::sort(candidates.begin(), candidates.end(),
               [](const Candidate& a, const Candidate& b) {
                   return a.ts < b.ts;
               });

    QList<Candidate> merged;
    for (const Candidate& c : candidates) {
        bool found = false;
        for (Candidate& m : merged) {
            if (qAbs(c.ts - m.ts) <= m_config.mergeWindowMs) {
                int weight = 2;
                m.ts = (m.ts * weight + c.ts) / (weight + 1);
                m.score = (m.score * weight + c.score) / (weight + 1);
                m.reason = m.reason + QStringLiteral(u" | ") + c.reason;
                found = true;
                break;
            }
        }
        if (!found) {
            merged.append(c);
        }
    }

    double totalScore = 0.0;
    for (const auto& c : merged) totalScore += c.score;
    double meanScore = merged.isEmpty() ? 0.0 : totalScore / merged.size();
    double scoreThreshold = meanScore * 0.3;

    QList<qint64> rawBoundaries = {0};
    for (const auto& c : merged) {
        if (c.score >= scoreThreshold) {
            rawBoundaries.append(c.ts);
        }
    }
    rawBoundaries.append(totalDurationMs);

    std::sort(rawBoundaries.begin(), rawBoundaries.end());

    QList<qint64> deduped;
    for (qint64 b : rawBoundaries) {
        if (deduped.isEmpty() || b != deduped.last()) {
            deduped.append(b);
        }
    }

    qDebug() << "[SemanticSegmenter] === 融合诊断 ===";
    qDebug() << "[SemanticSegmenter] 音频边界候选:" << audioBoundaries.size();
    for (const auto& sb : audioBoundaries) {
        qDebug() << "  [音频]" << sb.timestampMs << "ms score=" << sb.audioScore << sb.audioReason;
    }
    qDebug() << "[SemanticSegmenter] 视频边界候选:" << videoBoundaries.size();
    for (const auto& sb : videoBoundaries) {
        qDebug() << "  [视频]" << sb.timestampMs << "ms score=" << sb.videoScore << sb.videoReason;
    }

    if (candidates.isEmpty()) {
        qDebug() << "[SemanticSegmenter] 融合: 无任何候选，返回整段不分段";
        return {0, totalDurationMs};
    }

    qDebug() << "[SemanticSegmenter] 融合后候选(去重前):" << candidates.size();
    qDebug() << "[SemanticSegmenter] meanScore=" << meanScore << "scoreThreshold=" << scoreThreshold;
    qDebug() << "[SemanticSegmenter] 融合后边界数:" << rawBoundaries.size();
    qDebug() << "[SemanticSegmenter] 最终边界:" << rawBoundaries;
    qDebug() << "[SemanticSegmenter] === 融合诊断结束 ===";

    return adaptiveSegmentLength(deduped, totalDurationMs);
}

QList<qint64> SemanticSegmenter::adaptiveSegmentLength(
    const QList<qint64>& rawBoundaries,
    qint64 totalDurationMs) {

    if (rawBoundaries.size() <= 1) {
        return {0, totalDurationMs};
    }

    QList<qint64> result;
    result.append(rawBoundaries.value(0, 0));

    for (int i = 1; i < rawBoundaries.size(); ++i) {
        qint64 prev = result.last();
        qint64 curr = rawBoundaries[i];
        qint64 gap = curr - prev;

        if (gap < m_config.minSegmentMs) {
            continue;
        }

        while (gap > m_config.maxSegmentMs) {
            qint64 mid = prev + m_config.maxSegmentMs / 2;
            result.append(mid);
            prev = mid;
            gap = curr - prev;
        }

        result.append(curr);
    }

    if (result.last() != totalDurationMs) {
        result.append(totalDurationMs);
    }

    return result;
}

QList<qint64> SemanticSegmenter::computeSegments(
    const QList<SubtitleItem>& asrResults,
    qint64 totalDurationMs,
    const QList<QPair<qint64, QString>>& videoSceneTags) {

    QList<SemanticBoundary> audioBounds = detectAudioBoundaries(asrResults);
    QList<SemanticBoundary> videoBounds = detectVideoBoundaries(videoSceneTags);

    m_allBoundaries.clear();
    for (const auto& b : audioBounds) m_allBoundaries.append(b);
    for (const auto& b : videoBounds) m_allBoundaries.append(b);

    QList<qint64> boundaries = fuseAndSegment(audioBounds, videoBounds, totalDurationMs);

    qDebug() << "[SemanticSegmenter] boundaries:" << boundaries;
    qDebug() << "[SemanticSegmenter] audio boundaries:" << audioBounds.size()
             << "video boundaries:" << videoBounds.size();

    return boundaries;
}

void SemanticSegmenter::boundariesToSegments(
    const QList<qint64>& boundaries,
    QList<SummarySegment>& outSegments) {

    outSegments.clear();
    for (int i = 0; i < boundaries.size() - 1; ++i) {
        SummarySegment seg;
        seg.index = i;
        seg.startMs = boundaries[i];
        seg.endMs = boundaries[i + 1];
        seg.isAnalyzed = false;
        outSegments.append(seg);
    }
}

void SemanticSegmenter::aggregateSpeechText(
    QList<SummarySegment>& segments,
    const QList<SubtitleItem>& asrResults) {

    for (auto& seg : segments) {
        QStringList words;
        for (const auto& item : asrResults) {
            qint64 startMs = qRound(item.start_sec * 1000.0);
            qint64 endMs = qRound(item.end_sec * 1000.0);
            if (endMs > seg.startMs && startMs < seg.endMs) {
                words.append(QString::fromStdString(item.text));
            }
        }
        seg.speechText = words.join(QStringLiteral(u" "));
    }
}

QList<qint64> SemanticSegmenter::sampleTimestamps(
    qint64 startMs, qint64 endMs, int count) {

    QList<qint64> timestamps;
    if (count <= 0 || startMs >= endMs) return timestamps;

    double duration = (endMs - startMs) / 1000.0;
    for (int i = 1; i <= count; ++i) {
        double t = (startMs / 1000.0) + (duration * i / (count + 1));
        timestamps.append(qRound(t * 1000));
    }
    return timestamps;
}
