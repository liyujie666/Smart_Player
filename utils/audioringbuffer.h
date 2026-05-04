#ifndef AUDIORINGBUFFER_H
#define AUDIORINGBUFFER_H

#include <vector>
#include <mutex>
#include <cstring>
#include <cmath>
#include <algorithm>
#include <cstddef>

class AudioPcmRingBuffer {
public:
    explicit AudioPcmRingBuffer(size_t capacity_samples = 160000, int sample_rate = 16000);

    AudioPcmRingBuffer(const AudioPcmRingBuffer&) = delete;
    AudioPcmRingBuffer& operator=(const AudioPcmRingBuffer&) = delete;

    size_t push(const float* data, size_t samples, double first_sample_time_sec = NAN);
    size_t peek(float* out, size_t samples) const;
    size_t consume(size_t samples);
    size_t available() const;
    size_t free_space() const;
    void clear();
    void reset_time(double time_sec);
    double head_time_sec() const;
    int sample_rate() const;
    size_t capacity() const;

private:

    void dropOldestLocked(size_t n);

private:
    std::vector<float> buffer_;
    size_t capacity_;
    size_t head_;
    size_t tail_;
    size_t size_;
    int sample_rate_;
    double head_time_sec_;
    bool has_head_time_;
    mutable std::mutex mutex_;
};

#endif // AUDIORINGBUFFER_H
