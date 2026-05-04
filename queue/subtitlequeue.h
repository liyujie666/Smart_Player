// #ifndef SUBTITLEQUEUE_H
// #define SUBTITLEQUEUE_H

// #include <queue>
// #include <mutex>
// #include <vector>
// #include <algorithm>
// #include <unordered_set>
// #include <cmath>
// #include <QDebug>
// #include "subtitle/asrworker.h"

// class SubtitleQueue {
// public:
//     enum class Mode {
//         Live,    // 直播/实时字幕：允许过期清理
//         Offline  // 离线全量字幕：保留完整历史
//     };

//     SubtitleQueue() = default;

//     void setMode(Mode mode) {
//         std::lock_guard<std::mutex> lock(mtx_);
//         mode_ = mode;
//         if(mode == Mode::Offline){
//             kStartTolerance = 0.0;
//             kEndTolerance = 0.0;
//         }else{
//             kStartTolerance = 1.0;
//             kEndTolerance = 3.0;
//         }
//     }

//     void push(const SubtitleItem& item) {
//         if (item.text.empty()) {
//             return;
//         }

//         std::lock_guard<std::mutex> lock(mtx_);

//         if (pending_.size() >= kMaxPending_) {
//             pending_.pop();
//         }
//         pending_.push(item);
//     }

//     std::vector<SubtitleItem> get_all() {
//         std::queue<SubtitleItem> tmp;
//         {
//             std::lock_guard<std::mutex> lock(mtx_);
//             std::swap(tmp, pending_);
//         }

//         std::vector<SubtitleItem> res;
//         res.reserve(tmp.size());

//         while (!tmp.empty()) {
//             if (!tmp.front().text.empty()) {
//                 res.push_back(std::move(tmp.front()));
//             }
//             tmp.pop();
//         }

//         sortByStart(res);
//         return res;
//     }

//     void clear() {
//         std::lock_guard<std::mutex> lock(mtx_);

//         std::queue<SubtitleItem> emptyQueue;
//         std::swap(pending_, emptyQueue);

//         cache_.clear();
//         seen_.clear();
//         cache_begin_ = 0;
//     }

//     SubtitleItem get_current_subtitle(double current_time) {
//         std::lock_guard<std::mutex> lock(mtx_);

//         flushPendingToCacheLocked();

//         // 只有实时模式才清理过期字幕
//         if (mode_ == Mode::Live) {
//             purgeExpiredLocked(current_time);
//         }

//         if (cache_begin_ >= cache_.size()) {
//             return SubtitleItem{};
//         }

//         const double left  = current_time - kStartTolerance;
//         const double right = current_time + kEndTolerance;

//         // 在候选字幕中选一个“最稳”的：优先保留 end 更晚的字幕，减少提前切段
//         SubtitleItem best{};
//         bool found = false;

//         for (size_t i = cache_begin_; i < cache_.size(); ++i) {
//             const auto& s = cache_[i];

//             if (s.start_sec > right) {
//                 break;
//             }
//             if (s.end_sec < left) {
//                 continue;
//             }

//             if (!found ||
//                 s.end_sec > best.end_sec ||
//                 (std::fabs(s.end_sec - best.end_sec) < 1e-6 && s.start_sec < best.start_sec)) {
//                 best = s;
//                 found = true;
//             }
//         }

//         return found ? best : SubtitleItem{};
//     }

//     void cache_subtitles(const std::vector<SubtitleItem>& subs) {
//         if (subs.empty()) {
//             return;
//         }

//         std::vector<SubtitleItem> batch;
//         batch.reserve(subs.size());

//         for (const auto& sub : subs) {
//             if (!sub.text.empty()) {
//                 batch.push_back(sub);
//             }
//         }

//         if (batch.empty()) {
//             return;
//         }

//         sortByStart(batch);

//         std::lock_guard<std::mutex> lock(mtx_);
//         mergeBatchIntoCacheLocked(batch);
//     }

//     size_t size() const {
//         std::lock_guard<std::mutex> lock(mtx_);
//         const size_t activeSize = (cache_begin_ < cache_.size()) ? (cache_.size() - cache_begin_) : 0;
//         return pending_.size() + activeSize;
//     }

//     void debugPrintCache() {
//         std::lock_guard<std::mutex> lock(mtx_);
//         qDebug() << "=== 字幕缓存总数：" << (cache_.size() >= cache_begin_ ? (cache_.size() - cache_begin_) : 0);

//         for (size_t i = cache_begin_; i < cache_.size(); ++i) {
//             const auto& s = cache_[i];
//             qDebug() << "缓存字幕："
//                      << QString::fromStdString(s.text)
//                      << "时间："
//                      << s.start_sec << "~" << s.end_sec;
//         }
//     }

// private:
//     size_t kMaxPending_ = 50;
//     double kStartTolerance = 1.0;
//     double kEndTolerance   = 3.0;
//     double kExpiredKeepSec  = 5.0;

//     mutable std::mutex mtx_;
//     std::queue<SubtitleItem> pending_;

//     std::vector<SubtitleItem> cache_;
//     size_t cache_begin_ = 0;

//     std::unordered_set<std::string> seen_;
//     Mode mode_ = Mode::Live;

// private:
//     static long long toMs(double sec) {
//         return static_cast<long long>(std::llround(sec * 1000.0));
//     }

//     static std::string makeKey(const SubtitleItem& item) {
//         return std::to_string(toMs(item.start_sec)) + "|" +
//                std::to_string(toMs(item.end_sec)) + "|" +
//                item.text;
//     }

//     static void sortByStart(std::vector<SubtitleItem>& vec) {
//         std::sort(vec.begin(), vec.end(),
//                   [](const SubtitleItem& a, const SubtitleItem& b) {
//                       if (a.start_sec != b.start_sec) {
//                           return a.start_sec < b.start_sec;
//                       }
//                       return a.end_sec < b.end_sec;
//                   });
//     }

//     void flushPendingToCacheLocked() {
//         if (pending_.empty()) {
//             return;
//         }

//         std::queue<SubtitleItem> tmp;
//         std::swap(tmp, pending_);

//         std::vector<SubtitleItem> batch;
//         batch.reserve(tmp.size());

//         while (!tmp.empty()) {
//             if (!tmp.front().text.empty()) {
//                 batch.push_back(std::move(tmp.front()));
//             }
//             tmp.pop();
//         }

//         if (batch.empty()) {
//             return;
//         }

//         sortByStart(batch);
//         mergeBatchIntoCacheLocked(batch);
//     }

//     void mergeBatchIntoCacheLocked(const std::vector<SubtitleItem>& batch) {
//         if (batch.empty()) {
//             return;
//         }

//         for (const auto& sub : batch) {
//             const std::string key = makeKey(sub);
//             if (!seen_.insert(key).second) {
//                 continue;
//             }

//             if (cache_begin_ >= cache_.size()) {
//                 cache_.clear();
//                 cache_begin_ = 0;
//                 cache_.push_back(sub);
//                 continue;
//             }

//             if (sub.start_sec >= cache_.back().start_sec) {
//                 cache_.push_back(sub);
//             } else {
//                 auto insertPos = std::lower_bound(
//                     cache_.begin() + static_cast<std::ptrdiff_t>(cache_begin_),
//                     cache_.end(),
//                     sub.start_sec,
//                     [](const SubtitleItem& item, double t) {
//                         return item.start_sec < t;
//                     }
//                     );
//                 cache_.insert(insertPos, sub);
//             }
//         }
//     }

//     void purgeExpiredLocked(double current_time) {
//         while (cache_begin_ < cache_.size()) {
//             if (cache_[cache_begin_].end_sec >= current_time - kExpiredKeepSec) {
//                 break;
//             }
//             ++cache_begin_;
//         }

//         if (cache_begin_ > 256 && cache_begin_ * 2 >= cache_.size()) {
//             cache_.erase(cache_.begin(), cache_.begin() + static_cast<std::ptrdiff_t>(cache_begin_));
//             cache_begin_ = 0;
//         }

//         if (cache_begin_ >= cache_.size()) {
//             cache_.clear();
//             cache_begin_ = 0;
//         }
//     }
// };

// #endif // SUBTITLEQUEUE_H
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
    std::string text;
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
