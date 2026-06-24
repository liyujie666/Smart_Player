#ifndef ASRMODELCACHE_H
#define ASRMODELCACHE_H

#include <QObject>
#include <QString>
#include <QThread>
#include <QMutex>
#include <atomic>
#include <memory>

struct whisper_context;

class AsrModelCache : public QObject {
    Q_OBJECT

public:
    enum class LoadState { Unloaded, Loading, Loaded, Failed };

    static AsrModelCache& instance();

    void setModelPath(const QString& path);
    QString modelPath() const;

    // 引用计数式获取/释放模型上下文
    // 返回 true  表示 ctx != nullptr caller 可以直接使用
    // 返回 false 表示模型未加载或加载失败，ctx_out 仍为 nullptr
    bool tryAcquire(whisper_context*& ctx_out);

    // 引用计数：acquire 返回 true 后必须配对 release
    void acquire();
    void release();

    LoadState state() const;
    bool isReady() const;
    whisper_context* peekContext() const;

Q_SIGNALS:
    void loaded(bool success, const QString& errorMsg);
    void loading();
    void unloaded();

private:
    AsrModelCache();
    ~AsrModelCache() override;

    AsrModelCache(const AsrModelCache&) = delete;
    AsrModelCache& operator=(const AsrModelCache&) = delete;

    void loadInThread(const QString& path);

    mutable QMutex mtx_;
    QString model_path_;
    std::atomic<LoadState> state_{LoadState::Unloaded};
    std::atomic<int> ref_count_{0};

    // 当前可用的 context（加载完成后写入，释放旧context后读取）
    whisper_context* ctx_ = nullptr;
    // 正在被替换的旧 context，等新 context 加载好后再 safe_free
    whisper_context* stale_ctx_ = nullptr;

    QThread worker_thread_;
    std::atomic<bool> cancel_requested_{false};
};

#endif // ASRMODELCACHE_H
