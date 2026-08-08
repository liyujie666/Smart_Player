#ifndef FBANK_H
#define FBANK_H

#include <vector>
#include <cmath>
#include <complex>
#include <cstring>
#include <cstdlib>

// FBank（Mel Filterbank）特征提取器
// 用于将 PCM 音频转换为 80 维 Mel 频谱特征
// 参数: 16kHz, 25ms 窗口, 10ms 步进, 80 维
class FbankExtractor {
public:
    struct Config {
        int sample_rate = 16000;
        int frame_length = 400;     // 25ms @ 16kHz
        int frame_shift = 160;      // 10ms @ 16kHz
        int n_mels = 80;
    float preemphasis = 0.97f;
    float dither = 1.0f;         // 添加抖动，避免静音段特征全零
    bool use_power = true;      // 用功率谱而非幅度谱
    float energy_floor = 1.0f;
    };

    FbankExtractor() = default;
    ~FbankExtractor() = default;

    void init(const Config& cfg = {}) {
        cfg_ = cfg;
        n_fft_ = 512;   // FFT 点数补零到 2 的幂次（标准做法），frame_length=400 时补 112 个零
        // 计算 Mel 滤波器组
        computeMelFilterbank();
        // 预计算汉明窗
        hamming_window_.resize(cfg_.frame_length);
        for (int i = 0; i < cfg_.frame_length; ++i) {
            hamming_window_[i] = 0.54f - 0.46f * std::cos(2.0f * M_PI * i / (cfg_.frame_length - 1));
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
        std::vector<float> power_spectrum(n_fft_ / 2 + 1);  // 257 bins @ n_fft=512

        for (int f = 0; f < num_frames; ++f) {
            int start = f * cfg_.frame_shift;

            // 1. 取帧 + dither（添加微弱随机噪声，避免静音段特征全零）
            for (int i = 0; i < cfg_.frame_length; ++i) {
                frame[i] = pcm[start + i];
                if (cfg_.dither > 0) {
                    // 简单均匀分布 dither，范围 [-dither, +dither]
                    float r = ((float)rand() / RAND_MAX - 0.5f) * 2.0f * cfg_.dither;
                    frame[i] += r;
                }
            }

            // 2. 预加重（第一帧不做预加重，保持原值）
            if (cfg_.preemphasis > 0) {
                float prev = frame[0];
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
        int n_filters = cfg_.n_mels;
        int n_fft_bins = n_fft_ / 2 + 1;
        float mel_min = hzToMel(0);
        float mel_max = hzToMel(cfg_.sample_rate / 2);

        // 标准 HTK/Kaldi 方式：生成 n_filters+2 个等间距 Mel 点
        // 第 m 个滤波器: left=points[m], center=points[m+1], right=points[m+2]
        std::vector<float> mel_points(n_filters + 2);
        for (int i = 0; i < n_filters + 2; ++i) {
            mel_points[i] = mel_min + (mel_max - mel_min) * i / (n_filters + 1);
        }

        mel_filters_.resize(n_filters);

        for (int m = 0; m < n_filters; ++m) {
            float hz_left = melToHz(mel_points[m]);
            float hz_center = melToHz(mel_points[m + 1]);
            float hz_right = melToHz(mel_points[m + 2]);

            int bin_left = (int)std::round(hz_left * n_fft_ / cfg_.sample_rate);
            int bin_center = (int)std::round(hz_center * n_fft_ / cfg_.sample_rate);
            int bin_right = (int)std::round(hz_right * n_fft_ / cfg_.sample_rate);

            // 确保至少有一个 bin 的宽度
            if (bin_right <= bin_left) bin_right = bin_left + 1;
            if (bin_center <= bin_left) bin_center = bin_left + 1;
            if (bin_center >= bin_right) bin_center = bin_right - 1;

            for (int k = bin_left; k <= bin_right && k < n_fft_bins; ++k) {
                float weight = 0.0f;
                if (k < bin_center) {
                    weight = (float)(k - bin_left) / (bin_center - bin_left);
                } else if (k <= bin_right) {
                    weight = (float)(bin_right - k) / (bin_right - bin_center);
                }
                if (weight > 0) {
                    mel_filters_[m].push_back({k, weight});
                }
            }
        }
    }

    float hzToMel(float hz) { return 1127.0f * std::log1p(hz / 700.0f); }
    float melToHz(float mel) { return 700.0f * (std::exp(mel / 1127.0f) - 1.0f); }

    // DFT 计算功率谱（补零到 n_fft，只需正频率部分）
    void computePowerSpectrum(const std::vector<float>& frame, std::vector<float>& power) {
        int n = n_fft_;
        int half = n / 2 + 1;
        int frame_len = (int)frame.size();

        for (int k = 0; k < half; ++k) {
            std::complex<float> sum(0, 0);
            // 只遍历 frame 的实际长度（补零部分贡献为 0，跳过）
            for (int i = 0; i < frame_len; ++i) {
                float angle = -2.0f * (float)M_PI * k * i / n;
                sum += std::complex<float>(frame[i] * std::cos(angle),
                                           frame[i] * std::sin(angle));
            }
            power[k] = std::norm(sum) / n;
            if (k > 0 && k < half - 1) power[k] *= 2;
        }
    }

private:
    Config cfg_;
    int n_fft_ = 512;
    std::vector<float> hamming_window_;
    std::vector<std::vector<std::pair<int, float>>> mel_filters_; // [mel_idx][bin_idx] = {fft_bin, weight}
};

#endif // FBANK_H
