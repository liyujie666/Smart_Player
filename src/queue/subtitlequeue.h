#ifndef SUBTITLEQUEUE_H
#define SUBTITLEQUEUE_H

#include <vector>
#include <mutex>
#include <algorithm>
#include <string>
#include <queue>
#include <unordered_set>
#include <cmath>

struct SubtitleItem {
    double start_sec = 0.0;
    double end_sec = 0.0;
    std::string text;              // 原始识别文本
    std::string translated_text;   // 翻译后文本（为空则未翻译）
    std::string language;// 检测到的语言码
};

class SubtitleQueue {
public:
    enum class Mode {
        Live,
        Offline
    };

    SubtitleQueue() = default;

    void setMode(Mode mode) {
        std::lock_guard<std::mutex> lock(mtx_);
        // 切换模式强制清空所有数据，杜绝残留
        pending_ = {};
        cache_.clear();
        seen_.clear();
        cache_begin_ = 0;

        mode_ = mode;
        if (mode_ == Mode::Offline) {
            kStartTolerance = 0.0;
            kEndTolerance = 0.0;
            kMaxPending_ = 1000000;
        } else {
            kStartTolerance = 1.0;
            kEndTolerance = 3.0;
            kMaxPending_ = 50;
        }
    }

    void push(const SubtitleItem& item) {
        if (item.text.empty()) return;
        std::lock_guard<std::mutex> lock(mtx_);
        if (pending_.size() >= kMaxPending_) {
            if (mode_ == Mode::Live) pending_.pop();
        }
        pending_.push(item);
    }

    void clear() {
        std::lock_guard<std::mutex> lock(mtx_);
        pending_ = {};
        cache_.clear();
        seen_.clear();
        cache_begin_ = 0;
    }

    SubtitleItem getCurrent(double current_time) {
        std::lock_guard<std::mutex> lock(mtx_);
        flushPending();

        if (mode_ == Mode::Live) {
            purgeExpired(current_time);
        }

        if (cache_begin_ >= cache_.size()) return {};

        const double left = current_time - kStartTolerance;
        const double right = current_time + kEndTolerance;
        SubtitleItem best{};
        bool found = false;

        for (size_t i = cache_begin_; i < cache_.size(); ++i) {
            const auto& s = cache_[i];
            if (s.start_sec > right) break;
            if (s.end_sec < left) continue;

            if (!found || s.end_sec > best.end_sec ||
                (fabs(s.end_sec - best.end_sec) < 1e-6 && s.start_sec < best.start_sec)) {
                best = s;
                found = true;
            }
        }
        return found ? best : SubtitleItem{};
    }

private:
    void flushPending() {
        if (pending_.empty()) return;
        std::vector<SubtitleItem> batch;
        while (!pending_.empty()) {
            batch.push_back(pending_.front());
            pending_.pop();
        }
        mergeBatch(batch);
    }

    void mergeBatch(const std::vector<SubtitleItem>& batch) {
        for (const auto& sub : batch) {
            std::string key = std::to_string((int64_t)(sub.start_sec*1000)) + "|" +
                              std::to_string((int64_t)(sub.end_sec*1000)) + "|" + sub.text;
            if (seen_.count(key)) continue;
            seen_.insert(key);
            cache_.push_back(sub);
        }
        std::sort(cache_.begin(), cache_.end(), [](const SubtitleItem& a, const SubtitleItem& b) {
            return a.start_sec < b.start_sec;
        });
    }

    void purgeExpired(double current_time) {
        const double expire = current_time - 5.0;
        while (cache_begin_ < cache_.size() && cache_[cache_begin_].end_sec < expire) {
            cache_begin_++;
        }
    }

private:
    size_t kMaxPending_ = 50;
    double kStartTolerance = 1.0;
    double kEndTolerance = 3.0;
    Mode mode_ = Mode::Live;

    mutable std::mutex mtx_;
    std::queue<SubtitleItem> pending_;
    std::vector<SubtitleItem> cache_;
    std::unordered_set<std::string> seen_;
    size_t cache_begin_ = 0;
};

#endif
