#ifndef CLOUDASRENGINE_H
#define CLOUDASRENGINE_H

#include "iasrengine.h"
#include <QNetworkAccessManager>
#include <memory>
#include <string>

// 云端 ASR 引擎配置（扩展 AsrEngineConfig）
struct CloudAsrConfig {
    std::string api_key;
    std::string api_endpoint;
    std::string provider = "tencent";  // "tencent", "aliyun", "azure"
    int timeout_ms = 10000;
};

// 云端 ASR 引擎
// 支持多家云ASR服务API：腾讯云ASR、阿里云、Azure Speech等
class CloudAsrEngine : public IAsrEngine {
public:
    CloudAsrEngine();
    ~CloudAsrEngine() override;

    bool init(const AsrEngineConfig& cfg) override;
    void release() override;
    bool isReady() const override { return ready_; }

    bool recognize(const std::vector<float>& pcm,
                   std::vector<SubtitleItem>& out,
                   double base_sec = 0.0) override;
    void reset() override;

    std::string name() const override { return "CloudASR"; }

    // 云端特有配置
    void setCloudConfig(const CloudAsrConfig& cloud_cfg) { cloud_cfg_ = cloud_cfg; }

private:
    // PCM → WAV/PCM base64 编码
    std::string encodePcm(const std::vector<float>& pcm) const;

    // 调用云API
    std::string callCloudApi(const std::string& audio_data);

    // 解析响应
    bool parseResponse(const std::string& response,
                       std::vector<SubtitleItem>& out,
                       double base_sec);

private:
    AsrEngineConfig cfg_;
    CloudAsrConfig cloud_cfg_;
    bool ready_ = false;
    std::unique_ptr<QNetworkAccessManager> network_;
};

#endif // CLOUDASRENGINE_H
