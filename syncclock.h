#ifndef SYNCCLOCK_H
#define SYNCCLOCK_H
#include <QDebug>
#include <QMutex>
extern "C" {
#include <libavutil/time.h>
}

#include <cmath>
#include <algorithm>

class AVSyncClock
{
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
        QMutexLocker lock(&m_mutex);
        m_audio_clock = 0;
        m_last_pts = 0;
        m_last_delay = 0;
        m_frame_timer = 0;
        m_lag_count = 0;
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
        QMutexLocker locker(&m_mutex);
        m_audio_clock = clock;
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
                m_last_pts    = video_pts_us;
                m_frame_timer = now;
                return MIN_REFRSH_US;
            }

            int64_t delay = video_pts_us - m_last_pts;
            if (delay <= 0 || delay > 1000000) delay = m_last_delay;

            delay /= m_speed;

            m_frame_timer += delay;
            if (m_frame_timer < now) m_frame_timer = now;

            m_last_pts   = video_pts_us;
            m_last_delay = delay;

            return std::max(m_frame_timer - now, MIN_REFRSH_US);
        }

        // 音视频
        if (m_last_pts == 0) {
            m_last_pts = video_pts_us;
            m_frame_timer = av_gettime();
            return MIN_REFRSH_US;
        }

        int64_t delay = video_pts_us - m_last_pts;
        if (delay <= 0 || delay > 1000000) delay = m_last_delay;

        int64_t diff = video_pts_us - m_audio_clock;
        int64_t sync_threshold = std::clamp(delay, MIN_SYNC_THRESHOLD, MAX_SYNC_THRESHOLD);

        if (std::abs(diff) < NOSYNC_THRESHOLD) {
            if (diff <= -sync_threshold) {
                delay = std::max(0LL, delay + diff);
                m_lag_count = (m_last_delay <= 0) ? (m_lag_count + 1) : 0;
            } else if (diff >= sync_threshold) {
                delay = (delay > SYNC_FRAMEDUP_THRESHOLD) ? (delay + diff) : (delay * 2);
                m_lag_count = 0;
            }
        }

        m_last_delay = delay;
        m_last_pts = video_pts_us;

        int64_t curr_time = av_gettime();
        if (m_frame_timer == 0) m_frame_timer = curr_time;
        m_frame_timer += delay;

        if (m_frame_timer < curr_time) m_frame_timer = curr_time;
        int64_t actual_delay = m_frame_timer - curr_time;
        return std::max(actual_delay, MIN_REFRSH_US);
    }

    bool need_force_catch_up() const {
        // 纯视频：永远不丢帧
        if (m_mode == SYSTEM_MASTER) return false;
        // 音视频：丢帧
        return (m_last_pts - m_audio_clock) < -LAG_THRESHOLD && m_lag_count >= LAG_CONTINUE_COUNT;
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

    int64_t get_diff() const { QMutexLocker locker(&m_mutex); return m_last_pts - m_audio_clock; }
    int64_t get_audio_clock() const { QMutexLocker locker(&m_mutex); return m_audio_clock; }
    int64_t get_last_pts() const { return m_last_pts; }
    double get_speed() const { return m_speed;}

private:
    int64_t m_audio_clock;
    int64_t m_last_pts;
    int64_t m_last_delay;
    int64_t m_frame_timer;
    int     m_lag_count;
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
