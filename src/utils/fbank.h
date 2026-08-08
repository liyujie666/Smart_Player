#ifndef FBANK_H
#define FBANK_H

#include <vector>
#include <cmath>
#include <complex>
#include <algorithm>

// FBank（Mel Filterbank）特征提取器
// 用于将 PCM 音频转换为 80 维 Mel 频谱特征
// 参数: 16kHz, 25ms 窗口, 10ms 步进, 80 维
class FbankExtractor {
public:
    static constexpr float kPi = 3.14159265358979323846f;

    struct Config {
        int sample_rate = 16000;
        int frame_length = 400;     // 25ms @ 16kHz
        int frame_shift = 160;      // 10ms @ 16kHz
        int n_mels = 80;
        int fft_size = 512;
        float preemphasis = 0.97f;
        float dither = 0.0f;
        bool remove_dc_offset = true;
        bool use_power = true;
        float pcm_scale = 32768.0f;
        float energy_floor = 1.0e-10f;
    };

    FbankExtractor() = default;
    ~FbankExtractor() = default;

    void init() {
        init(Config{});
    }

    void init(const Config& cfg) {
        cfg_ = cfg;
        //n_fft_ = std::max(cfg_.fft_size, cfg_.frame_length);
        n_fft_ = 512;
        // 计算 Mel 滤波器组
        computeMelFilterbank();
        // 预计算汉明窗
        hamming_window_.resize(cfg_.frame_length);
        for (int i = 0; i < cfg_.frame_length; ++i) {
            hamming_window_[i] = 0.54f - 0.46f * std::cos(2.0f * kPi * i / (cfg_.frame_length - 1));
        }
    }

    // 提取 FBank 特征
    // pcm: 16kHz mono float32
    // 返回: [num_frames][n_mels] 的二维向量
    std::vector<std::vector<float>> extract(const std::vector<float>& pcm) {
        int num_frames = 0;
        if ((int)pcm.size() < cfg_.frame_length) {
            num_frames = 0;
        } else {
            num_frames = 1 + ((int)pcm.size() - cfg_.frame_length) / cfg_.frame_shift;
        }

        std::vector<std::vector<float>> features(num_frames);
        std::vector<float> frame(cfg_.frame_length);
        std::vector<float> power_spectrum(n_fft_ / 2 + 1);

        for (int f = 0; f < num_frames; ++f) {
            int start = f * cfg_.frame_shift;

            // 1. 转换到 FunASR/Kaldi 使用的 int16 幅值范围
            for (int i = 0; i < cfg_.frame_length; ++i) {
                frame[i] = pcm[start + i] * cfg_.pcm_scale;
            }

            // 2. 去直流分量并预加重
            if (cfg_.remove_dc_offset) {
                float mean = 0.0f;
                for (float sample : frame) mean += sample;
                mean /= static_cast<float>(frame.size());
                for (float& sample : frame) sample -= mean;
            }

            if (cfg_.preemphasis > 0) {
                float prev = frame[0];
                frame[0] *= (1.0f - cfg_.preemphasis);
                for (int i = 1; i < cfg_.frame_length; ++i) {
                    float cur = frame[i];
                    frame[i] = cur - cfg_.preemphasis * prev;
                    prev = cur;
                }
            }

            // 3. 加窗
            for (int i = 0; i < cfg_.frame_length; ++i) {
                frame[i] *= hamming_window_[i];
            }

            // 4. FFT → 功率谱
            computePowerSpectrum(frame, power_spectrum);

            // 5. Mel 滤波器组 → FBank
            features[f].resize(cfg_.n_mels);
            for (int m = 0; m < cfg_.n_mels; ++m) {
                float sum = 0.0f;
                const auto& filter = mel_filters_[m];
                for (int k = 0; k < (int)filter.size(); ++k) {
                    sum += power_spectrum[filter[k].first] * filter[k].second;
                }
                // 取 log
                features[f][m] = (sum > cfg_.energy_floor) ? std::log(sum) : std::log(cfg_.energy_floor);
            }
        }

        return features;
    }

    // 对特征做 CMVN 归一化
    void applyCmvn(std::vector<std::vector<float>>& features,
                   const std::vector<float>& mean,
                   const std::vector<float>& variance) {
        if (mean.size() != cfg_.n_mels || variance.size() != cfg_.n_mels) return;
        for (auto& frame : features) {
            for (int m = 0; m < cfg_.n_mels; ++m) {
                frame[m] = (frame[m] + mean[m]) * variance[m];
            }
        }
    }

    int frameLength() const { return cfg_.frame_length; }
    int frameShift() const { return cfg_.frame_shift; }
    int numMels() const { return cfg_.n_mels; }

private:
    void computeMelFilterbank() {
        const int n_filters = cfg_.n_mels;
        const int n_fft_bins = n_fft_ / 2 + 1;
        const float mel_min = hzToMel(0.0f);
        const float mel_max = hzToMel(cfg_.sample_rate / 2.0f);

        mel_filters_.assign(n_filters, {});
        std::vector<float> mel_points(n_filters + 2);
        for (int i = 0; i < n_filters + 2; ++i) {
            mel_points[i] = mel_min + (mel_max - mel_min) * i / (n_filters + 1);
        }

        for (int m = 0; m < n_filters; ++m) {
            const float left = mel_points[m];
            const float center = mel_points[m + 1];
            const float right = mel_points[m + 2];

            for (int k = 0; k < n_fft_bins; ++k) {
                const float hz = static_cast<float>(k) * cfg_.sample_rate / n_fft_;
                const float mel = hzToMel(hz);
                float weight = 0.0f;
                if (mel > left && mel <= center) {
                    weight = (mel - left) / (center - left);
                } else if (mel > center && mel < right) {
                    weight = (right - mel) / (right - center);
                }
                if (weight > 0.0f) mel_filters_[m].push_back({k, weight});
            }
        }
    }

    float hzToMel(float hz) { return 1127.0f * std::log1p(hz / 700.0f); }
    float melToHz(float mel) { return 700.0f * (std::exp(mel / 1127.0f) - 1.0f); }

    // 计算零填充到 n_fft 后的正频率功率谱
    void computePowerSpectrum(const std::vector<float>& frame, std::vector<float>& power) {
        std::vector<std::complex<float>> spectrum(n_fft_, {0.0f, 0.0f});
        for (size_t i = 0; i < frame.size(); ++i) spectrum[i] = frame[i];

        for (int i = 1, j = 0; i < n_fft_; ++i) {
            int bit = n_fft_ >> 1;
            for (; j & bit; bit >>= 1) j ^= bit;
            j ^= bit;
            if (i < j) std::swap(spectrum[i], spectrum[j]);
        }

        for (int len = 2; len <= n_fft_; len <<= 1) {
            const float angle = -2.0f * kPi / len;
            const std::complex<float> step(std::cos(angle), std::sin(angle));
            for (int i = 0; i < n_fft_; i += len) {
                std::complex<float> w(1.0f, 0.0f);
                for (int j = 0; j < len / 2; ++j) {
                    const auto even = spectrum[i + j];
                    const auto odd = spectrum[i + j + len / 2] * w;
                    spectrum[i + j] = even + odd;
                    spectrum[i + j + len / 2] = even - odd;
                    w *= step;
                }
            }
        }

        for (int k = 0; k <= n_fft_ / 2; ++k) {
            power[k] = cfg_.use_power ? std::norm(spectrum[k]) : std::abs(spectrum[k]);
        }
    }

private:
    Config cfg_;
    int n_fft_ = 512;
    std::vector<float> hamming_window_;
    std::vector<std::vector<std::pair<int, float>>> mel_filters_; // [mel_idx][bin_idx] = {fft_bin, weight}
};

#endif // FBANK_H
