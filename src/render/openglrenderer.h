#ifndef OPENGLRENDERER_H
#define OPENGLRENDERER_H

#include <QOpenGLWidget>
#include <QOpenGLShaderProgram>
#include <QOpenGLVertexArrayObject>
#include <QOpenGLBuffer>
#include <QOpenGLTexture>
#include <QOpenGLFunctions>
#include <QMutex>

extern"C"{
#include <libavformat/avformat.h>
}

class OpenGLRenderer : public QOpenGLWidget, protected QOpenGLFunctions
{
    Q_OBJECT
public:
    enum class RenderSource {
        None,
        Video,
        Cover
    };

    enum SizeMode {
        Fit = 0,      // 等比适应（默认）
        Stretch = 1,  // 拉伸填充
    };

    explicit OpenGLRenderer(QWidget *parent = nullptr);
    ~OpenGLRenderer() override;

    void uploadYUV420PTexture(const QByteArray &yuvData, int width, int height);
    void uploadNV12Texture(const QByteArray &nv12Data, int width, int height);
    void uploadRGBATexture(const QByteArray &rgbData, int width, int height);
    void uploadSubtitleTexture(const QString& text);
    void renderCoverImage(const QImage &image);
    void start();
    void stop();
    void clear();

    void clearSubtitle();
    void setRenderSource(RenderSource source);
    void setSizeMode(int mode);
    void setBrightness(float value);
    void setContrast(float value);
    void setSaturation(float value);
    void setSubtitleFontSize(int size);

protected:
    void initializeGL() override;
    void resizeGL(int w, int h) override;
    void paintGL() override;

private:
    void calculateAspectRatioVertices(float* vertices);
    void setupTexture(QOpenGLTexture* tex);

private:
    enum RenderMode {
        YUV420P = 0,
        NV12    = 1,
        RGBA    = 2,
        NONE = 3
    };

    QOpenGLShaderProgram* program_ = nullptr;
    QOpenGLShaderProgram* subProgram_ = nullptr;
    QOpenGLVertexArrayObject* vao_ = nullptr;
    QOpenGLVertexArrayObject* subVao_ = nullptr;
    QOpenGLBuffer* vbo_ = nullptr;
    QOpenGLBuffer* subVbo_ = nullptr;

    QOpenGLTexture* yTexture_ = nullptr;
    QOpenGLTexture* uTexture_ = nullptr;
    QOpenGLTexture* vTexture_ = nullptr;
    QOpenGLTexture* uvTexture_ = nullptr;
    QOpenGLTexture* rgbTexture_ = nullptr;
    QOpenGLTexture* subtitleTexture_ = nullptr;

    int width_ = 0;
    int height_ = 0;
    int videoWidth_ = 0;
    int videoHeight_ = 0;
    int subTitleWidth_ = 0;
    int subTitleHeight_ = 0;
    RenderMode renderMode_ = YUV420P;
    RenderSource currentSource_ = RenderSource::None;
    SizeMode sizeMode_ = Fit;

    // 色彩控制
    float brightness_ = 0.0f;
    float contrast_ = 1.0f;
    float saturation_ = 1.0f;

    static const float texCoords_[];
    std::atomic<bool> isStopped = false;

    // 字幕
    QString currentSubtitle_;
    static constexpr int SUBTITLE_HEIGHT = 60;
    static constexpr float SUBTITLE_BG_ALPHA = 0.7f;
    int subtitleFontSize_ = 26;
    bool subtitleDirty_ = false;
    QMutex subtitleMutex_;
};

#endif // OPENGLRENDERER_H
