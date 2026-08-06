#ifndef ONNXRUNTIMEUTIL_H
#define ONNXRUNTIMEUTIL_H

#include <onnxruntime_cxx_api.h>
#include <string>
#include <vector>
#include <QDebug>

// ONNX Runtime 公共工具：单例 Env + 通用辅助函数
class OrtUtil {
public:
    static OrtUtil& instance() {
        static OrtUtil inst;
        return inst;
    }

    Ort::Env& env() { return env_; }

    // 创建 SessionOptions（CPU，指定线程数）
    static Ort::SessionOptions defaultSessionOptions(int num_threads = 2) {
        Ort::SessionOptions opts;
        opts.SetIntraOpNumThreads(num_threads);
        opts.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_EXTENDED);
        opts.SetExecutionMode(ExecutionMode::ORT_SEQUENTIAL);
        return opts;
    }

    // 读取模型输入/输出名称列表
    static std::vector<std::string> getInputNames(const Ort::Session& session) {
        std::vector<std::string> names;
        size_t n = session.GetInputCount();
        Ort::AllocatorWithDefaultOptions alloc;
        for (size_t i = 0; i < n; ++i) {
            auto name = session.GetInputNameAllocated(i, alloc);
            names.push_back(name.get());
        }
        return names;
    }

    static std::vector<std::string> getOutputNames(const Ort::Session& session) {
        std::vector<std::string> names;
        size_t n = session.GetOutputCount();
        Ort::AllocatorWithDefaultOptions alloc;
        for (size_t i = 0; i < n; ++i) {
            auto name = session.GetOutputNameAllocated(i, alloc);
            names.push_back(name.get());
        }
        return names;
    }

    // 读取 am.mvn 文件（CMVN 归一化参数）
    // 格式: 第一行 mean，第二行 variance
    struct CmvnParams {
        std::vector<float> mean;
        std::vector<float> variance;
    };

    static CmvnParams loadCmvn(const std::string& mvn_path) {
        CmvnParams params;
        QFile file(QString::fromStdString(mvn_path));
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            qWarning() << "[OrtUtil] Cannot open am.mvn:" << QString::fromStdString(mvn_path);
            return params;
        }

        QTextStream in(&file);
        int line_idx = 0;
        while (!in.atEnd()) {
            QString line = in.readLine().trimmed();
            if (line.isEmpty()) continue;
            QStringList parts = line.split(' ', Qt::SkipEmptyParts);

            std::vector<float>* target = nullptr;
            if (line_idx == 0) target = &params.mean;
            else if (line_idx == 1) target = &params.variance;
            line_idx++;

            if (target) {
                for (const QString& p : parts) {
                    bool ok = false;
                    float v = p.toFloat(&ok);
                    if (ok) target->push_back(v);
                }
            }
        }
        qDebug() << "[OrtUtil] CMVN loaded: mean_dim=" << params.mean.size()
                 << "var_dim=" << params.variance.size();
        return params;
    }

    // 读取 JSON 配置文件
    static QString loadJsonFile(const std::string& path) {
        QFile file(QString::fromStdString(path));
        if (!file.open(QIODevice::ReadOnly)) return "";
        return file.readAll();
    }

private:
    OrtUtil() : env_(ORT_LOGGING_LEVEL_WARNING, "SmartPlayer") {}
    Ort::Env env_;
};

#endif // ONNXRUNTIMEUTIL_H
