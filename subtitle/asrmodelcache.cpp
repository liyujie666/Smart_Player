#include "asrmodelcache.h"
#include "whisper/whisper.h"
#include <QMutexLocker>
#include <QDebug>

AsrModelCache& AsrModelCache::instance() {
    static AsrModelCache inst;
    return inst;
}

AsrModelCache::AsrModelCache() {
    moveToThread(&worker_thread_);
    worker_thread_.start();
}

AsrModelCache::~AsrModelCache() {
    cancel_requested_ = true;
    worker_thread_.quit();
    worker_thread_.wait(5000);
    if (stale_ctx_) {
        whisper_free(stale_ctx_);
        stale_ctx_ = nullptr;
    }
    if (ctx_) {
        whisper_free(ctx_);
        ctx_ = nullptr;
    }
}

void AsrModelCache::setModelPath(const QString& path) {
    QMutexLocker lock(&mtx_);
    if (model_path_ == path) return;
    model_path_ = path;

    cancel_requested_ = true;
    state_ = LoadState::Unloaded;

    cancel_requested_ = false;
    QMetaObject::invokeMethod(this, [this]() { loadInThread(model_path_); }, Qt::QueuedConnection);
}

QString AsrModelCache::modelPath() const {
    QMutexLocker lock(&mtx_);
    return model_path_;
}

bool AsrModelCache::tryAcquire(whisper_context*& ctx_out) {
    ctx_out = nullptr;
    LoadState s = state_.load();

    if (s == LoadState::Loaded) {
        QMutexLocker lock(&mtx_);
        if (ctx_) {
            ref_count_++;
            ctx_out = ctx_;
            return true;
        }
    }

    if (s == LoadState::Unloaded && ref_count_ == 0) {
        QMutexLocker lock(&mtx_);
        cancel_requested_ = false;
        QMetaObject::invokeMethod(this, [this]() { loadInThread(model_path_); }, Qt::QueuedConnection);
    }

    return false;
}

void AsrModelCache::acquire() {
    LoadState s = state_.load();
    if (s == LoadState::Loaded) {
        QMutexLocker lock(&mtx_);
        ref_count_++;
    }
}

void AsrModelCache::release() {
    if (ref_count_ <= 0) return;
    int r = ref_count_.fetch_sub(1);
    if (r == 1) {
        QMutexLocker lock(&mtx_);
        state_ = LoadState::Unloaded;
        cancel_requested_ = false;
        Q_EMIT unloaded();
        QMetaObject::invokeMethod(this, [this]() { loadInThread(model_path_); }, Qt::QueuedConnection);
    }
}

AsrModelCache::LoadState AsrModelCache::state() const {
    return state_.load();
}

bool AsrModelCache::isReady() const {
    return state_.load() == LoadState::Loaded;
}

whisper_context* AsrModelCache::peekContext() const {
    QMutexLocker lock(&mtx_);
    return ctx_;
}

void AsrModelCache::loadInThread(const QString& path) {
    if (path.isEmpty()) {
        qDebug() << "[AsrModelCache] model path is empty, skip loading";
        state_ = LoadState::Failed;
        Q_EMIT loaded(false, QStringLiteral("模型路径为空"));
        return;
    }

    if (cancel_requested_) {
        state_ = LoadState::Unloaded;
        return;
    }

    state_ = LoadState::Loading;
    Q_EMIT loading();

    qDebug() << "[AsrModelCache] loading model:" << path;

    auto params = whisper_context_default_params();
    whisper_context* new_ctx = whisper_init_from_file_with_params(path.toLocal8Bit().constData(), params);

    if (cancel_requested_) {
        if (new_ctx) whisper_free(new_ctx);
        state_ = LoadState::Unloaded;
        return;
    }

    if (!new_ctx) {
        qDebug() << "[AsrModelCache] failed to load model:" << path;
        state_ = LoadState::Failed;
        Q_EMIT loaded(false, QStringLiteral("加载模型失败: %1").arg(path));
        return;
    }

    {
        QMutexLocker lock(&mtx_);
        // 保持式重载：先收走旧 context，等新 context 可用后再释放
        if (ctx_) {
            if (stale_ctx_) whisper_free(stale_ctx_);
            stale_ctx_ = ctx_;
        }
        ctx_ = new_ctx;
    }

    qDebug() << "[AsrModelCache] model loaded successfully, cleaning stale context";
    state_ = LoadState::Loaded;
    Q_EMIT loaded(true, QString());

    // 在持有锁的状态下同步释放旧 context（此时新 ctx 已安全可用）
    {
        QMutexLocker lock(&mtx_);
        if (stale_ctx_) {
            whisper_free(stale_ctx_);
            stale_ctx_ = nullptr;
        }
    }
}
