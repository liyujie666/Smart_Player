#ifndef SYNCCLOCK_H
#define SYNCCLOCK_H
#include <QDebug>
#include <QMutex>
extern "C" {
#include <libavutil/time.h>
}

#include <cmath>
#include <algorithm>
#include <atomic>

class AVSyncClock
{
    AVSyncClock(const AVSyncClock&) = delete;
    AVSyncClock& operator=(const AVSyncClock&) = delete;
    AVSyncClock(AVSyncClock&&) = delete;
    AVSyncClock& operator=(AVSyncClock&&) = delete;
public:
    enum SyncMode {
        AUDIO_MASTER,   // 音视频正常：音频为基准
        VIDEO_MASTER,   // 纯音频：无视频，无需同步
        SYSTEM_MASTER   // 纯视频：系统时钟为基准
    };

    // 微秒单位 (1ms = 1000us)
    static constexpr int64_t MIN_SYNC_THRESHOLD    = 10000;   // 10ms
    static constexpr int64_t MAX_SYNC_THRESHOLD    = 100000;  // 100ms
    static constexpr int64_t NOSYNC_THRESHOLD      = 10000000; // 10s
    static constexpr int64_t SYNC_FRAMEDUP_THRESHOLD = 100000; // 100ms
    static constexpr int64_t MIN_REFRSH_US          = 1000;   // 1ms
    static constexpr int64_t LAG_THRESHOLD         = 1000000; // 1s
    static constexpr int    LAG_CONTINUE_COUNT    = 10;

public:
    AVSyncClock() {
        reset();
    }

    void reset() {
        m_audio_clock.store(0, std::memory_order_relaxed);
        m_last_pts.store(0, std::memory_order_relaxed);
        m_last_delay.store(0, std::memory_order_relaxed);
        m_frame_timer.store(0, std::memory_order_relaxed);
        m_lag_count.store(0, std::memory_order_relaxed);
        m_system_base = 0;

        m_is_paused        = false;
        m_pause_start_time = 0;
        m_total_pause_us   = 0;
    }

    // 设置同步模式
    void setSyncMode(SyncMode mode, bool hasAudio, bool hasVideo) {
        m_mode = mode;
        m_has_audio = hasAudio;
        m_has_video = hasVideo;
        reset();
    }

    void pause()
    {
        if(m_mode != SYSTEM_MASTER || m_is_paused)
            return;

        m_is_paused        = true;
        m_pause_start_time = av_gettime();
    }


    void resume()
    {
        if(m_mode != SYSTEM_MASTER || !m_is_paused)
            return;

        int64_t pause_cost = av_gettime() - m_pause_start_time;
        m_total_pause_us  += pause_cost;
        m_is_paused        = false;
        m_pause_start_time = 0;
    }

    void setSpeedRatio(double ratio) {
        if (ratio <= 0 || ratio == m_speed) return;
        m_speed = ratio;
    }
    // 设置音频时钟
    void set_audio_clock(int64_t clock) {
        m_audio_clock.store(clock, std::memory_order_release);
    }

    int64_t calc_display_delay(int64_t video_pts_us) {

        // 纯音频：直接返回最小延迟
        if (!m_has_video || m_mode == VIDEO_MASTER) {
            return MIN_REFRSH_US;
        }

        // 异常PTS：直接返回最小延迟
        if (video_pts_us <= 0) {
            return MIN_REFRSH_US;
        }

        // 纯视频 → 系统时钟基准
        if (m_mode == SYSTEM_MASTER)
        {
            int64_t now = av_gettime();
            if (m_system_base == 0)
            {
                m_system_base = now - video_pts_us;
                m_last_pts.store(video_pts_us, std::memory_order_release);
                m_frame_timer.store(now, std::memory_order_release);
                return MIN_REFRSH_US;
            }

            int64_t last_pts = m_last_pts.load(std::memory_order_acquire);
            int64_t last_delay = m_last_delay.load(std::memory_order_acquire);
            int64_t delay = video_pts_us - last_pts;
            if (delay <= 0 || delay > 1000000) delay = last_delay;

            delay /= m_speed;

            int64_t new_timer = m_frame_timer.load(std::memory_order_acquire) + delay;
            if (new_timer < now) new_timer = now;

            m_frame_timer.store(new_timer, std::memory_order_release);
            m_last_pts.store(video_pts_us, std::memory_order_release);
            m_last_delay.store(delay, std::memory_order_release);

            return std::max(new_timer - now, MIN_REFRSH_US);
        }

        // 音视频
        int64_t last_pts = m_last_pts.load(std::memory_order_acquire);
        if (last_pts == 0) {
            m_last_pts.store(video_pts_us, std::memory_order_release);
            m_frame_timer.store(av_gettime(), std::memory_order_release);
            return MIN_REFRSH_US;
        }

        int64_t last_delay = m_last_delay.load(std::memory_order_acquire);
        int64_t delay = video_pts_us - last_pts;
        if (delay <= 0 || delay > 1000000) delay = last_delay;

        int64_t audio_clock = m_audio_clock.load(std::memory_order_acquire);
        int64_t diff = video_pts_us - audio_clock;
        int64_t sync_threshold = std::clamp(delay, MIN_SYNC_THRESHOLD, MAX_SYNC_THRESHOLD);

        int lag_count = m_lag_count.load(std::memory_order_acquire);
        if (std::abs(diff) < NOSYNC_THRESHOLD) {
            if (diff <= -sync_threshold) {
                delay = std::max(0LL, delay + diff);
                lag_count = (last_delay <= 0) ? (lag_count + 1) : 0;
            } else if (diff >= sync_threshold) {
                delay = (delay > SYNC_FRAMEDUP_THRESHOLD) ? (delay + diff) : (delay * 2);
                lag_count = 0;
            }
        }

        m_lag_count.store(lag_count, std::memory_order_release);
        m_last_delay.store(delay, std::memory_order_release);
        m_last_pts.store(video_pts_us, std::memory_order_release);

        int64_t curr_time = av_gettime();
        int64_t frame_timer = m_frame_timer.load(std::memory_order_acquire);
        if (frame_timer == 0) frame_timer = curr_time;
        frame_timer += delay;

        if (frame_timer < curr_time) frame_timer = curr_time;
        m_frame_timer.store(frame_timer, std::memory_order_release);
        int64_t actual_delay = frame_timer - curr_time;
        return std::max(actual_delay, MIN_REFRSH_US);
    }

    bool need_force_catch_up() const {
        // 纯视频：永远不丢帧
        if (m_mode == SYSTEM_MASTER) return false;
        // 音视频：丢帧
        int64_t last_pts = m_last_pts.load(std::memory_order_acquire);
        int64_t audio_clock = m_audio_clock.load(std::memory_order_acquire);
        int lag_count = m_lag_count.load(std::memory_order_acquire);
        return (last_pts - audio_clock) < -LAG_THRESHOLD && lag_count >= LAG_CONTINUE_COUNT;
    }

    bool hasAudio() const {  return m_has_audio; }
    bool hasVideo() const { return m_has_video; }

    int64_t getCurrentSystemClock() const
    {
        if (m_mode != SYSTEM_MASTER)
            return 0;

        int64_t now = av_gettime();
        int64_t valid_time = (now - m_system_base) - m_total_pause_us;

        return valid_time;
    }

    int64_t get_diff() const { return m_last_pts.load(std::memory_order_acquire) - m_audio_clock.load(std::memory_order_acquire); }
    int64_t get_audio_clock() const { return m_audio_clock.load(std::memory_order_acquire); }
    int64_t get_last_pts() const { return m_last_pts.load(std::memory_order_acquire); }
    double get_speed() const { return m_speed;}

private:
    std::atomic<int64_t> m_audio_clock{0};
    std::atomic<int64_t> m_last_pts{0};
    std::atomic<int64_t> m_last_delay{0};
    std::atomic<int64_t> m_frame_timer{0};
    std::atomic<int>     m_lag_count{0};
    mutable QMutex m_mutex;

    SyncMode m_mode = AUDIO_MASTER;  // 当前同步模式
    bool m_has_audio = false;         // 是否有音频流
    bool m_has_video = false;         // 是否有视频流
    int64_t m_system_base = 0;        // 纯视频：系统时钟基准时间
    double m_speed = 1.0;

    bool m_is_paused = false;
    int64_t m_pause_start_time= 0;
    int64_t m_total_pause_us  = 0;
};

#endif
