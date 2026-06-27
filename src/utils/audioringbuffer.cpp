#include "audioringbuffer.h"

AudioPcmRingBuffer::AudioPcmRingBuffer(size_t capacity_samples, int sample_rate)
    : buffer_(capacity_samples == 0 ? 1 : capacity_samples, 0.0f),
    capacity_(capacity_samples == 0 ? 1 : capacity_samples),
    head_(0),
    tail_(0),
    size_(0),
    sample_rate_(sample_rate > 0 ? sample_rate : 16000),
    head_time_sec_(0.0),
    has_head_time_(false) {}

size_t AudioPcmRingBuffer::push(const float* data, size_t samples, double first_sample_time_sec)
{
    if (!data || samples == 0) {
        return 0;
    }

    std::lock_guard<std::mutex> lock(mutex_);

    // 写入数据超过容量，仅保留最后一段
    if (samples >= capacity_) {
        size_t skip = samples - capacity_;
        data += skip;
        samples = capacity_;

        head_ = 0;
        tail_ = 0;
        size_ = 0;

        if (std::isfinite(first_sample_time_sec)) {
            first_sample_time_sec += static_cast<double>(skip) / static_cast<double>(sample_rate_);
            head_time_sec_ = first_sample_time_sec;
            has_head_time_ = true;
        }
    }

    // 空间不足，丢弃最老数据
    const size_t free = capacity_ - size_;
    if (samples > free) {
        const size_t need_drop = samples - free;
        dropOldestLocked(need_drop);
    }

    // 空缓冲时设置时间戳
    if (size_ == 0 && std::isfinite(first_sample_time_sec)) {
        head_time_sec_ = first_sample_time_sec;
        has_head_time_ = true;
    }

    // 分两段写入环形缓冲区
    const size_t first_part = std::min(samples, capacity_ - tail_);
    std::memcpy(buffer_.data() + tail_, data, first_part * sizeof(float));

    const size_t remain = samples - first_part;
    if (remain > 0) {
        std::memcpy(buffer_.data(), data + first_part, remain * sizeof(float));
    }

    tail_ = (tail_ + samples) % capacity_;
    size_ += samples;

    return samples;
}

size_t AudioPcmRingBuffer::peek(float* out, size_t samples) const
{
    if (!out || samples == 0) {
        return 0;
    }

    std::lock_guard<std::mutex> lock(mutex_);
    if (size_ == 0) {
        return 0;
    }

    const size_t n = std::min(samples, size_);
    const size_t first_part = std::min(n, capacity_ - head_);
    std::memcpy(out, buffer_.data() + head_, first_part * sizeof(float));

    const size_t remain = n - first_part;
    if (remain > 0) {
        std::memcpy(out + first_part, buffer_.data(), remain * sizeof(float));
    }

    return n;
}

size_t AudioPcmRingBuffer::consume(size_t samples)
{
    if (samples == 0) {
        return 0;
    }

    std::lock_guard<std::mutex> lock(mutex_);
    if (size_ == 0) {
        return 0;
    }

    const size_t n = std::min(samples, size_);
    head_ = (head_ + n) % capacity_;
    size_ -= n;

    // 更新时间戳
    if (has_head_time_) {
        head_time_sec_ += static_cast<double>(n) / static_cast<double>(sample_rate_);
    }

    // 空缓冲重置指针
    if (size_ == 0) {
        head_ = 0;
        tail_ = 0;
    }

    return n;
}

size_t AudioPcmRingBuffer::available() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return size_;
}

size_t AudioPcmRingBuffer::free_space() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return capacity_ - size_;
}

void AudioPcmRingBuffer::clear()
{
    std::lock_guard<std::mutex> lock(mutex_);
    head_ = 0;
    tail_ = 0;
    size_ = 0;
    head_time_sec_ = 0.0;
    has_head_time_ = false;
}

void AudioPcmRingBuffer::reset_time(double time_sec)
{
    std::lock_guard<std::mutex> lock(mutex_);
    head_time_sec_ = time_sec;
    has_head_time_ = std::isfinite(time_sec);
}

double AudioPcmRingBuffer::head_time_sec() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return head_time_sec_;
}

int AudioPcmRingBuffer::sample_rate() const
{
    return sample_rate_;
}

size_t AudioPcmRingBuffer::capacity() const
{
    return capacity_;
}

void AudioPcmRingBuffer::dropOldestLocked(size_t n)
{
    if (n == 0 || size_ == 0) {
        return;
    }

    const size_t drop = std::min(n, size_);
    head_ = (head_ + drop) % capacity_;
    size_ -= drop;

    if (has_head_time_) {
        head_time_sec_ += static_cast<double>(drop) / static_cast<double>(sample_rate_);
    }

    if (size_ == 0) {
        head_ = 0;
        tail_ = 0;
    }
}
