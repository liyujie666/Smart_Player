#include "openglrenderer.h"
#include <QDebug>
#include <cstring>
#include <QPainter>

const float OpenGLRenderer::texCoords_[] = {
    0.0f, 1.0f,
    1.0f, 1.0f,
    0.0f, 0.0f,
    1.0f, 0.0f
};

OpenGLRenderer::OpenGLRenderer(QWidget *parent) : QOpenGLWidget(parent)
{
    this->setAttribute(Qt::WA_PaintOnScreen, false);
    this->setAttribute(Qt::WA_OpaquePaintEvent, true);
}

OpenGLRenderer::~OpenGLRenderer()
{
    if (!isValid()) {
        return;
    }
    makeCurrent();

    if(yTexture_)   { yTexture_->destroy(); delete yTexture_; yTexture_ = nullptr; }
    if(uTexture_)   { uTexture_->destroy(); delete uTexture_; uTexture_ = nullptr; }
    if(vTexture_)   { vTexture_->destroy(); delete vTexture_; vTexture_ = nullptr; }
    if(uvTexture_)  { uvTexture_->destroy(); delete uvTexture_; uvTexture_ = nullptr; }
    if(rgbTexture_) { rgbTexture_->destroy(); delete rgbTexture_; rgbTexture_ = nullptr; }
    if(subtitleTexture_) { subtitleTexture_->destroy(); delete subtitleTexture_; subtitleTexture_ = nullptr; }

    delete program_;
    delete vao_;
    delete vbo_;

    doneCurrent();
}

void OpenGLRenderer::setupTexture(QOpenGLTexture* tex)
{
    tex->setWrapMode(QOpenGLTexture::ClampToEdge);
    tex->setMinMagFilters(QOpenGLTexture::Linear, QOpenGLTexture::Linear);
}

void OpenGLRenderer::calculateAspectRatioVertices(float* vertices)
{
    if (sizeMode_ == Stretch) {
        const float stretchVerts[] = {-1,-1, 1,-1, -1,1, 1,1};
        memcpy(vertices, stretchVerts, 8 * sizeof(float));
        return;
    }

    if (videoWidth_ <= 0 || videoHeight_ <= 0 || width_ <= 0 || height_ <= 0) {
        const float defaultVerts[] = {-1,-1, 1,-1, -1,1, 1,1};
        memcpy(vertices, defaultVerts, 8 * sizeof(float));
        return;
    }

    float videoAspect = (float)videoWidth_ / videoHeight_;
    float winAspect = (float)width_ / height_;
    float left, right, bottom, top;

    if (videoAspect > winAspect) {
        left = -1.0f; right = 1.0f;
        float scale = winAspect / videoAspect;
        bottom = -scale; top = scale;
    } else {
        bottom = -1.0f; top = 1.0f;
        float scale = videoAspect / winAspect;
        left = -scale; right = scale;
    }

    vertices[0] = left;  vertices[1] = bottom;
    vertices[2] = right; vertices[3] = bottom;
    vertices[4] = left;  vertices[5] = top;
    vertices[6] = right; vertices[7] = top;
}

void OpenGLRenderer::initializeGL()
{
    initializeOpenGLFunctions();
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);

    program_ = new QOpenGLShaderProgram(this);
    vao_ = new QOpenGLVertexArrayObject(this);
    vbo_ = new QOpenGLBuffer(QOpenGLBuffer::VertexBuffer);

    const char* vertexShader = R"(
        attribute vec2 aPos;
        attribute vec2 aTexCoord;
        varying vec2 vTexCoord;
        void main() {
            gl_Position = vec4(aPos, 0.0, 1.0);
            vTexCoord = aTexCoord;
        }
    )";

    // 双格式着色器：YUV420P + NV12

    const char* fragmentShader = R"(
    varying vec2 vTexCoord;
    uniform sampler2D yTexture;
    uniform sampler2D uTexture;
    uniform sampler2D vTexture;
    uniform sampler2D uvTexture;
    uniform sampler2D rgbTexture;
    uniform int renderMode;        // 0=YUV420P 1=NV12 2=RGBA

    uniform float brightness;
    uniform float contrast;
    uniform float saturation;

    void main() {
        vec3 rgb;
        float alpha = 1.0;
        if(renderMode == 0 || renderMode == 1) {
            float y = texture2D(yTexture, vTexCoord).r;
            float u = 0.0, v = 0.0;

            if(renderMode == 1) { // NV12 ✅ 修复这里！
                vec2 uv = texture2D(uvTexture, vTexCoord).rg;
                u = uv.r;  // 正确：U = R通道
                v = uv.g;  // 正确：V = G通道
            } else { // YUV420P
                u = texture2D(uTexture, vTexCoord).r;
                v = texture2D(vTexture, vTexCoord).r;
            }

            // Y:  16~235 → 0~1
            float Y = clamp((y - 0.0625) / 0.91796875, 0.0, 1.0);
            // UV: 16~240 → -1~1
            float U = (u - 0.5) * 1.140625;
            float V = (v - 0.5) * 1.140625;

            // BT.601 系数
            float R = Y + 1.5748  * V;
            float G = Y - 0.1873  * U - 0.4681 * V;
            float B = Y + 1.8556  * U;

            rgb = clamp(vec3(R, G, B), 0.0, 1.0);
        }
        else {
            vec4 rgba = texture2D(rgbTexture, vTexCoord);
            rgb = rgba.rgb;
            alpha = rgba.a;
        }

        // 画面调节
        rgb = (rgb - 0.5) * contrast + 0.5;
        rgb += brightness;
        float gray = dot(rgb, vec3(0.299, 0.504, 0.098));
        rgb = mix(vec3(gray), rgb, saturation);

        gl_FragColor = vec4(clamp(rgb, 0.0, 1.0), alpha);
    }
    )";


    program_->addShaderFromSourceCode(QOpenGLShader::Vertex, vertexShader);
    program_->addShaderFromSourceCode(QOpenGLShader::Fragment, fragmentShader);
    program_->link();

    vao_->create();
    vao_->bind();
    vbo_->create();
    vbo_->bind();
    vbo_->setUsagePattern(QOpenGLBuffer::DynamicDraw);
    vbo_->allocate(16 * sizeof(float));

    int aPos = program_->attributeLocation("aPos");
    glVertexAttribPointer(aPos, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
    glEnableVertexAttribArray(aPos);

    int aTex = program_->attributeLocation("aTexCoord");
    glVertexAttribPointer(aTex, 2, GL_FLOAT, GL_FALSE, 0, (void*)(8*sizeof(float)));
    glEnableVertexAttribArray(aTex);

    vbo_->release();
    vao_->release();

    // ========== 初始化所有纹理 ==========
    yTexture_  = new QOpenGLTexture(QOpenGLTexture::Target2D);
    uTexture_  = new QOpenGLTexture(QOpenGLTexture::Target2D);
    vTexture_  = new QOpenGLTexture(QOpenGLTexture::Target2D);
    uvTexture_ = new QOpenGLTexture(QOpenGLTexture::Target2D);
    rgbTexture_ = new QOpenGLTexture(QOpenGLTexture::Target2D);
    subtitleTexture_ = new QOpenGLTexture(QOpenGLTexture::Target2D);

    setupTexture(yTexture_);
    setupTexture(uTexture_);
    setupTexture(vTexture_);
    setupTexture(uvTexture_);
    setupTexture(rgbTexture_);
    setupTexture(subtitleTexture_);
}

void OpenGLRenderer::resizeGL(int w, int h)
{
    glViewport(0, 0, w, h);
    width_ = w;
    height_ = h;
    update();
}

void OpenGLRenderer::paintGL()
{
    if(isStopped){
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        return;
    }

    glClear(GL_COLOR_BUFFER_BIT);
    if (!program_ || videoWidth_ == 0) return;

    float vertices[8];
    calculateAspectRatioVertices(vertices);

    vao_->bind();
    vbo_->bind();
    vbo_->write(0, vertices, 8 * sizeof(float));
    vbo_->write(8 * sizeof(float), texCoords_, 8 * sizeof(float));
    vbo_->release();

    program_->bind();
    program_->setUniformValue("renderMode", (int)renderMode_);
    program_->setUniformValue("brightness", brightness_);
    program_->setUniformValue("contrast", contrast_);
    program_->setUniformValue("saturation", saturation_);
    if (renderMode_ == YUV420P) {
        program_->setUniformValue("yTexture", 0);
        program_->setUniformValue("uTexture", 1);
        program_->setUniformValue("vTexture", 2);
        yTexture_->bind(0);
        uTexture_->bind(1);
        vTexture_->bind(2);
    } else if (renderMode_ == NV12) {
        program_->setUniformValue("yTexture", 0);
        program_->setUniformValue("uvTexture", 1);
        yTexture_->bind(0);
        uvTexture_->bind(1);
    }
    else {
        program_->setUniformValue("rgbTexture", 0);
        rgbTexture_->bind(0);
    }

    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    // 绘制字幕
    if (!currentSubtitle_.isEmpty() && subtitleTexture_->isCreated() && subTitleWidth_ > 0) {
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, 0);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, 0);
        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_2D, 0);

        program_->setUniformValue("renderMode", (int)RGBA);
        program_->setUniformValue("brightness", 0.0f);
        program_->setUniformValue("contrast", 1.0f);
        program_->setUniformValue("saturation", 1.0f);

        float subHeightNDC = (float)subTitleHeight_ * 2.0f / height_;
        float subWidthNDC = (float)subTitleWidth_ * 2.0f / width_;
        float left = -subWidthNDC / 2.0f;
        float right = subWidthNDC / 2.0f;
        float bottom = -1.0f;
        float top = bottom + subHeightNDC;

        // 字幕顶点
        float subVertices[] = {
            left,  bottom,
            right, bottom,
            left,  top,
            right, top
        };
        // 纹理坐标
        float subTexCoords[] = {0.0f,1.0f, 1.0f,1.0f, 0.0f,0.0f, 1.0f,0.0f};

        vbo_->bind();
        vbo_->write(0, subVertices, 8 * sizeof(float));
        vbo_->write(8*sizeof(float), subTexCoords, 8*sizeof(float));
        vbo_->release();

        program_->setUniformValue("rgbTexture", 0);
        subtitleTexture_->bind(0);

        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        glDisable(GL_BLEND);
    }

    program_->release();
    vao_->release();
}

void OpenGLRenderer::uploadYUV420PTexture(const QByteArray &yuvData, int width, int height)
{
    if (yuvData.isEmpty() || currentSource_ != RenderSource::Video) return;
    makeCurrent();

    renderMode_ = YUV420P;
    videoWidth_ = width;
    videoHeight_ = height;

    const uint8_t* data = (const uint8_t*)yuvData.constData();
    int ySize = width * height;
    int uvSize = (width/2)*(height/2);

    yTexture_->destroy();
    yTexture_->setSize(width, height);
    yTexture_->setFormat(QOpenGLTexture::R8_UNorm);
    yTexture_->allocateStorage();
    yTexture_->setData(QOpenGLTexture::Red, QOpenGLTexture::UInt8, data);

    uTexture_->destroy();
    uTexture_->setSize(width/2, height/2);
    uTexture_->setFormat(QOpenGLTexture::R8_UNorm);
    uTexture_->allocateStorage();
    uTexture_->setData(QOpenGLTexture::Red, QOpenGLTexture::UInt8, data + ySize);

    vTexture_->destroy();
    vTexture_->setSize(width/2, height/2);
    vTexture_->setFormat(QOpenGLTexture::R8_UNorm);
    vTexture_->allocateStorage();
    vTexture_->setData(QOpenGLTexture::Red, QOpenGLTexture::UInt8, data + ySize + uvSize);

    doneCurrent();
    update();
}

void OpenGLRenderer::uploadNV12Texture(const QByteArray &nv12Data, int width, int height)
{
    if (nv12Data.isEmpty() || currentSource_ != RenderSource::Video) return;
    makeCurrent();

    renderMode_ = NV12;
    videoWidth_ = width;
    videoHeight_ = height;

    const uint8_t* data = (const uint8_t*)nv12Data.constData();
    int ySize = width * height;

    // Y纹理
    yTexture_->destroy();
    yTexture_->setSize(width, height);
    yTexture_->setFormat(QOpenGLTexture::R8_UNorm);
    yTexture_->allocateStorage();
    yTexture_->setData(QOpenGLTexture::Red, QOpenGLTexture::UInt8, data);

    uvTexture_->destroy();
    uvTexture_->setSize(width/2, height/2);
    uvTexture_->setFormat(QOpenGLTexture::RG8_UNorm);
    uvTexture_->allocateStorage();
    uvTexture_->setData(QOpenGLTexture::RG, QOpenGLTexture::UInt8, data + ySize);
    doneCurrent();
    update();
}

void OpenGLRenderer::uploadRGBATexture(const QByteArray &rgbData, int width, int height)
{
    if (rgbData.isEmpty()) return;
    makeCurrent();

    renderMode_ = RGBA;
    videoWidth_ = width;
    videoHeight_ = height;

    const uint8_t* data = (const uint8_t*)rgbData.constData();

    // 初始化RGBA纹理
    rgbTexture_->destroy();
    rgbTexture_->setSize(width, height);
    rgbTexture_->setFormat(QOpenGLTexture::RGBA8_UNorm);
    rgbTexture_->allocateStorage();
    rgbTexture_->setData(QOpenGLTexture::RGBA, QOpenGLTexture::UInt8, data);

    // 纹理参数
    rgbTexture_->setWrapMode(QOpenGLTexture::ClampToEdge);
    rgbTexture_->setMinMagFilters(QOpenGLTexture::Linear, QOpenGLTexture::Linear);

    doneCurrent();
    update();
}

void OpenGLRenderer::uploadSubtitleTexture(const QString& text)
{
    if (currentSubtitle_ == text) {
        return;
    }
    currentSubtitle_ = text;

    if (text.isEmpty() || width_ <= 0) {
        makeCurrent();
        if (subtitleTexture_->isCreated()) {
            subtitleTexture_->destroy();
        }
        doneCurrent();
        update();
        return;
    }

    // 计算文字实际宽度
    QFont font;
    font.setPointSize(26);
    font.setBold(true);
    font.setFamily("Microsoft YaHei");
    QFontMetrics fm(font);
    int textWidth = fm.horizontalAdvance(text);
    int subWidth = textWidth + 40;
    int subHeight = SUBTITLE_HEIGHT;

    // 自适应宽度的图片
    QImage img(subWidth, subHeight, QImage::Format_RGBA8888);
    img.fill(QColor(0, 0, 0, 150));

    // 绘制文字
    QPainter painter(&img);
    painter.setPen(Qt::white);
    painter.setFont(font);
    painter.setRenderHint(QPainter::TextAntialiasing);
    painter.drawText(img.rect(), Qt::AlignCenter, text);
    painter.end();

    // 上传自适应尺寸的字幕纹理
    makeCurrent();
    subtitleTexture_->destroy();
    subtitleTexture_->setSize(subWidth, subHeight);
    subtitleTexture_->setFormat(QOpenGLTexture::RGBA8_UNorm);
    subtitleTexture_->allocateStorage();
    subtitleTexture_->setData(QOpenGLTexture::RGBA, QOpenGLTexture::UInt8, img.bits());
    setupTexture(subtitleTexture_);

    subTitleWidth_ = subWidth;
    subTitleHeight_ = subHeight;

    doneCurrent();
    update();
}


void OpenGLRenderer::renderCoverImage(const QImage &image)
{
    if (image.isNull()) return;

    QImage rgbImage = image.convertToFormat(QImage::Format_RGBA8888);
    QByteArray rgbData((const char*)rgbImage.bits(), rgbImage.sizeInBytes());

    uploadRGBATexture(rgbData, rgbImage.width(), rgbImage.height());
}

void OpenGLRenderer::start()
{
    isStopped = false;
}

void OpenGLRenderer::stop()
{
    if (!isValid()) return;
    isStopped = true;
    clearSubtitle();
    update();
}

void OpenGLRenderer::clear()
{
    if (!isValid()) return;
    makeCurrent();
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    doneCurrent();
    update();
}

void OpenGLRenderer::clearSubtitle()
{
    currentSubtitle_.clear();
    subTitleWidth_ = 0;  // 重置尺寸
    subTitleHeight_ = 0;
    makeCurrent();
    if (subtitleTexture_->isCreated()) {
        subtitleTexture_->destroy();
    }
    doneCurrent();
    update();
}

void OpenGLRenderer::setRenderSource(RenderSource source)
{
    currentSource_ = source;
}

void OpenGLRenderer::setSizeMode(int mode)
{
    qDebug() << "mode" << mode;
    switch (mode) {
    case 0: sizeMode_ = Fit; break;
    case 1: sizeMode_ = Stretch; break;
    default: sizeMode_ = Fit; break;
    }
    update();
}

void OpenGLRenderer::setBrightness(float value)
{
    brightness_ = value;
    update();
}

void OpenGLRenderer::setContrast(float value)
{
    contrast_ = value;
    update();
}

void OpenGLRenderer::setSaturation(float value)
{
    saturation_ = value;
    update();
}
