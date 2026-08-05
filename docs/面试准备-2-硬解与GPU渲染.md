# 面试准备：CUDA硬解 + GLSL 统一渲染

## 一、架构总览

```
┌────────────┐    ┌──────────────────┐    ┌─────────────────────┐
│  Demuxer   │───→│     Decoder      │───→│   OpenGLRenderer│
│ (解封装)   │    │ (硬解/软解统一)  │    │ (GPU YUV→RGB + 调色)│
└────────────┘    └──────────────────┘    └─────────────────────┘
                useHardware_ = true       renderMode: YUV420P / NV12
                → CUDA + get_format单fragment shader双分支
                   useHardware_ = false
                   → avcodec_find_decoder (软解)
```

---

## 二、硬解初始化流程

### 1. 选择最优硬件类型 `getBestHardwareType()`

优先级（编码在代码中）：
```
CUDA > QSV > D3D11VA > VAAPI > DXVA2
```

通过 `av_hwdevice_iterate_types()` 遍历系统可用类型，与优先级列表匹配。

### 2. 拼接硬解码器名称 `getHardwareDecoderName()`

支持编码：H264 / HEVC / VP9 / AV1

|硬件类型 | 解码器名 |
|----------|----------|
| CUDA | `h264_cuvid` / `hevc_cuvid` |
| QSV | `h264_qsv` / `hevc_qsv` |
| D3D11VA | `h264_d3d11va` |
| VAAPI | `h264_vaapi` |

### 3. 初始化流程`init()`

```
1. useHardware_ == true ?
   → getBestHardwareType() 获取硬件类型
   → getHardwareDecoderName() 获取解码器名
   → avcodec_find_decoder_by_name() 找硬解码器
   → initHardware(hwType):
       av_hwdevice_ctx_create(&hwDeviceCtx_)
   → 设置 codecCtx_->get_format = hwPixFmtCallback
   → 设置 codecCtx_->hw_device_ctx = hwDeviceCtx_
   
2. initHardware 失败 ?
   → useHardware_ = false
   → 清理已分配的 hwDeviceCtx_ / hwTmpFrame_
   → 递归调用 init()（此时走软解路径）
   
3. useHardware_ == false
   → avcodec_find_decoder(codecId) 通用软解
   → avcodec_open2()
```

### 4. `get_format` 回调 `hwPixFmtCallback()`

FFmpeg 在解码器初始化时调用此回调，询问"你要哪种输出格式"：

```cpp
// 根据硬件类型返回对应像素格式
CUDA→ AV_PIX_FMT_CUDA
D3D11VA → AV_PIX_FMT_D3D11
QSV     → AV_PIX_FMT_QSV
VAAPI   → AV_PIX_FMT_VAAPI
fallback → AV_PIX_FMT_YUV420P
```

---

## 三、解码流程 `decode()`

```cpp
int Decoder::decode(AVPacket *pkt, AVFrame *&outFrame) {
    avcodec_send_packet(codecCtx_, pkt);
    avcodec_receive_frame(codecCtx_, frame);
    
    if (isHardware()) {
        // GPU帧 → CPU 帧（NV12 格式）
        hwFrameTransfer(frame, hwTmpFrame_);
        outFrame = hwTmpFrame_;   // 输出 NV12
    } else {
        outFrame = frame;         // 输出 YUV420P
    }
}
```

**`hwFrameTransfer`**：调用 `av_hwframe_transfer_data()` 把 GPU 显存中的帧拷贝到 CPU 内存，输出格式为 NV12。

---

## 四、OpenGL 渲染管线

### 1. Fragment Shader（核心，必须能默写）

```glsl
uniform int renderMode;  // 0=YUV420P  1=NV12  2=RGBA

void main() {
    float y, u, v;
    if (renderMode == 0 || renderMode == 1) {
        y = texture2D(yTexture, vTexCoord).r;
        
        if (renderMode == 1) {  // NV12: UV交织在一张RG8纹理
            vec2 uv = texture2D(uvTexture, vTexCoord).rg;
            u = uv.r;v = uv.g;
        } else {  // YUV420P: U/V 各一张R8 纹理
            u = texture2D(uTexture, vTexCoord).r;
            v = texture2D(vTexture, vTexCoord).r;
        }
        
        // Studio range → Full range
        float Y = (y - 0.0625) / 0.91796875;
        float U = (u - 0.5) * 1.140625;
        float V = (v - 0.5) * 1.140625;
        
        // BT.601 矩阵
        R = Y + 1.5748 * V;
        G = Y - 0.1873 * U - 0.4681 * V;
        B = Y + 1.8556 * U;
    }
    
    // 色彩调节
    rgb = (rgb - 0.5) * contrast + 0.5;  // 对比度
    rgb += brightness;                     // 亮度
    float gray = dot(rgb, vec3(0.299, 0.504, 0.098));
    rgb = mix(vec3(gray), rgb, saturation); // 饱和度
}
```

### 2. 纹理上传差异

| | YUV420P | NV12 |
|---|---|---|
| **纹理数** | 3 (Y + U + V) | 2 (Y + UV) |
| **Y纹理** | `R8_UNorm`, W×H | `R8_UNorm`, W×H |
| **U 纹理** | `R8_UNorm`, W/2 × H/2 | — |
| **V 纹理** | `R8_UNorm`, W/2 × H/2 | — |
| **UV 纹理** | — | `RG8_UNorm`, W/2 × H/2 |
| **内存布局** | `[YYY...][UU...][VV...]` | `[YYY...][UVUVUV...]` |
| **来源** | 软解输出 | 硬解 `av_hwframe_transfer`输出 |
| **shader 采样** | 分别从 u/v 纹理取 `.r` | 从 uv 纹理一次取 `.rg` |

### 3. 色彩转换矩阵（BT.601 Studio Range）

预处理：
```
Y = (y_raw - 16/255) / (219/255)    // [16,235] → [0,1]
U = (u_raw - 128/255) * (256/224)// [16,240] → [-0.57, +0.57]
V = (v_raw - 128/255) * (256/224)
```

转换：
```
R = Y + 1.5748 * V
G = Y - 0.1873 * U - 0.4681 * V
B = Y + 1.8556 * U
```

> **注意**：面试时可以说"用的BT.601 参考标准的系数"，如果被追问具体数字不需要精确到小数点后四位。

---

## 五、色彩调节

```glsl
// 对比度：以 0.5 为中心缩放，默认 1.0
rgb = (rgb - 0.5) * contrast + 0.5;

// 亮度：直接偏移，默认 0.0
rgb += brightness;

// 饱和度：与灰度插值，默认 1.0
float gray = dot(rgb, vec3(0.299, 0.504, 0.098));
rgb = mix(vec3(gray), rgb, saturation);
```

---

## 六、字幕渲染

独立 shader + VAO，使用 **预乘 Alpha混合**：
```glsl
gl_FragColor = vec4(c.rgb * c.a, c.a);// premultiplied alpha
```

渲染流程：先画视频帧，再叠加字幕纹理（`QImage`渲染文字→ `uploadSubtitleTexture`）。

---

## 七、开发中遇到的问题

### 问题 1：CUDA 硬解在无NVIDIA显卡机器上崩溃

**现象**：在集显笔记本上启动直接 crash，`av_hwdevice_ctx_create` 返回 -1。

**原因**：`initHardware()` 失败后没有正确清理 `codecCtx_`（已经`avcodec_open2` 了半截），导致递归调用 `init()` 时 double-open。

**解决**：失败分支中先`avcodec_free_context(&codecCtx_)` 彻底释放，再递归。并在递归前将 `hwDeviceCtx_` 和 `hwTmpFrame_` 清空，避免后续误用。

### 问题 2：NV12 渲染偏绿

**现象**：硬解视频整体色调偏绿，软解正常。

**原因**：NV12 的 UV 交织是 `[U, V, U, V, ...]`，上传纹理时用 `RG8_UNorm` 格式，shader 中 `uv.r = U, uv.g = V`。但最初写反了 `u = uv.g; v = uv.r;` 导致 U/V 互换，表现为偏绿。

**解决**：修正 shader 中 `u = uv.r; v = uv.g;` 对应 NV12 内存布局的 UVUV 顺序。

### 问题 3：全屏时 OpenGL 窗口上的弹窗（QComboBox 下拉、Popup）不显示

**现象**：Windows DWM 合成问题，全屏 QOpenGLWidget 成为整个窗口的 surface，popup 类窗口无法合成到其上方。

**原因**：Windows DWM 对无边框全屏 HWND 走"独占合成路径"，拒绝合成 popup窗口。

**解决**：Qt 官方方案 —— 给全屏 HWND 加`WS_BORDER` 标志，走完整合成路径：
```cpp
// main.cpp
QWindowsWindow::setHasBorderInFullScreen(true);
// mainwindow.cpp event()
SetWindowLongPtr(hwnd, GWL_STYLE, style | WS_BORDER);
```

### 问题 4：纹理尺寸变化时不释放旧纹理导致显存泄漏

**现象**：反复切换不同分辨率视频后，显存占用持续上升。

**原因**：`uploadYUV420PTexture` 每次调用 `setSize()` + `setData()` 但旧纹理对象未`destroy()`，Qt 的 `QOpenGLTexture::setSize()` 不会自动释放旧 storage。

**解决**：在分辨率变化时先`destroy()` 旧纹理再重新 `allocateStorage()`，或判断尺寸相同时只`setData` 不重新分配。

---

## 八、面试必答题

### Q1：为什么用 OpenGL 做YUV→RGB 而不是 CPU的 sws_scale？
> `sws_scale` 是逐像素纯CPU 操作，1080P 每帧约 3MB数据要遍历转换，占15~20% CPU。GPU 的 fragment shader 并行处理每个像素，转换+调色一次完成，CPU 只需做纹理上传（一次 memcpy），占用降到 3~5%。

### Q2：YUV420P 和 NV12 的区别？为什么硬解输出 NV12？
> YUV420P 是 planar 格式（Y/U/V 三个独立平面），NV12 是 semi-planar（Y 独立 + UV 交织）。硬解器（CUDA/DXVA）内部管线按 NV12 排列显存，输出 NV12 避免一次额外的 planar 转换。

### Q3：get_format 回调的作用？
> FFmpeg 解码器初始化时通过此回调询问应用"你希望解码输出什么格式？"。返回 `AV_PIX_FMT_CUDA` 告诉解码器把帧留在 GPU 显存（减少 GPU→CPU 拷贝次数，只在需要渲染时 transfer）。

### Q4：软解回退怎么保证无感？
> `Decoder::init()` 中硬解失败后 `useHardware_ = false` + 递归调用自身走软解路径，上层 `PlayerCore` 不感知这个过程。渲染层通过 `frame->format` 判断像素格式（`AV_PIX_FMT_YUV420P` vs `AV_PIX_FMT_NV12`），自动走对应分支，无需额外切换逻辑。

### Q5：对比度/亮度/饱和度为什么放在 GPU 做？
> 如果在 CPU 做，需要先YUV→RGB（一次遍历），再做色彩调节（第二次遍历），再上传纹理。放在 shader 里，YUV→RGB 和色彩调节在同一个 fragment shader pass 中一次完成，零额外开销。

### Q6：BT.601 和 BT.709 什么区别？你用的哪个？
> BT.601 是标清标准（DVD时代），BT.709 是高清标准（1080P+）。我代码中用的系数偏向 BT.601。实际上对大部分 H264 内容差异不大（肉眼几乎看不出），后续可以根据视频`color_space` 元数据动态选择矩阵。
