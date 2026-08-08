#ifndef ONNXRUNTIMEUTIL_H
#define ONNXRUNTIMEUTIL_H

#include <string>
#include <vector>
#include <QDebug>
#include <QFile>
#include <QTextStream>
#include <QStringList>
#include <QRegularExpression>

#if HAS_ONNXRUNTIME
#include <onnxruntime_cxx_api.h>

// 跨平台 ONNX Runtime 模型路径转换宏
// Windows 下 Ort::Session 构造函数要求 const wchar_t*，其他平台用 const char*
#ifdef _WIN32
#define ORT_PATH(qstr) ((qstr).toStdWString().c_str())
#else
#define ORT_PATH(qstr) ((qstr).toUtf8().constData())
#endif

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
    // 支持两种格式：
    //  1. Kaldi nnet 文本格式：<AddShift> ... [ mean... ]  <Rescale> ... [ var... ]
    //  2. 简单格式：第一行 mean，第二行 variance
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

        QString content = QTextStream(&file).readAll();

        if (content.contains("<AddShift>") || content.contains("<Rescale>")) {
            // Kaldi 格式：按标记定位随后的 [ ... ] 数值块
            QString spaced = content;
            spaced.replace('[', " [ ").replace(']', " ] ");
            const QStringList tokens = spaced.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);

            std::vector<float>* target = nullptr;   // 当前标记指向的目标
            std::vector<float>* collecting = nullptr;

            for (const QString& tok : tokens) {
                if (tok == "<AddShift>") {
                    target = &params.mean;
                } else if (tok == "<Rescale>") {
                    target = &params.variance;
                } else if (tok == "<Splice>" || tok == "<Nnet>" || tok == "</Nnet>") {
                    target = nullptr;               // 这些标记后的 [ ] 不是CMVN 数据
                } else if (tok == "[") {
                    collecting = target;
                    if (collecting) collecting->clear();
                } else if (tok == "]") {
                    collecting = nullptr;
                    target = nullptr;
                } else if (collecting) {
                    bool ok = false;
                    float v = tok.toFloat(&ok);
                    if (ok) collecting->push_back(v);
                }
            }
        } else {
            // 简单格式：第一行 mean，第二行 variance
            const QStringList lines = content.split('\n', Qt::SkipEmptyParts);
            for (int i = 0; i < lines.size() && i < 2; ++i) {
                std::vector<float>* target = (i == 0) ? &params.mean : &params.variance;
                const QStringList parts = lines[i].trimmed().split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
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

#endif // HAS_ONNXRUNTIME

#endif // ONNXRUNTIMEUTIL_H
