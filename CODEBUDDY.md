# CODEBUDDY.md This file provides guidance to CodeBuddy when working with code in this repository.

## Build Commands

### Prerequisites
- Qt 6.9.0 with MinGW 64-bit toolchain (installed at `D:\Qt\6.9.0\mingw_64`)
- FFmpeg development libraries (headers + .lib/.dll in `dependencies/`)
- SDL2, Whisper (whisper.cpp), CTranslate2, SentencePiece libraries in `dependencies/`

### Generate Makefile
```
D:\Qt\6.9.0\mingw_64\bin\qmake.exe -o Makefile Smart_Player.pro
```

### Build (Release)
```
mingw32-make release
```

### Build (Debug)
```
mingw32-make debug
```

### Clean
```
mingw32-make release-clean
```
or
```
mingw32-make debug-clean
```

### Full Rebuild
```
mingw32-make distclean && D:\Qt\6.9.0\mingw_64\bin\qmake.exe -o Makefile Smart_Player.pro && mingw32-make release
```

## Architecture Overview

Smart_Player is a Qt 6 / C++17 desktop video player built on top of FFmpeg with OpenGL hardware-accelerated rendering, SDL2 audio output, Whisper-based ASR (speech recognition), and an AI-powered video summary system. The project uses qmake (`.pro` file) as its build system.

### Core Pipeline: PlayerCore

`PlayerCore` is the central orchestrator. It owns and manages the entire media playback pipeline across **4 dedicated threads**:

1. **Demux Thread** (`demuxThreadFunc`) — Reads compressed packets from the media source via `Demuxer` and distributes them to audio/video packet queues.
2. **Audio Decode Thread** (`audioDecodeThreadFunc`) — Pulls from `AVPacketQueue`, decodes via `Decoder`, resamples via `Resampler`, applies tempo changes via `AudioFilter` (atempo), and pushes to audio frame queue. Also feeds PCM to `AsrManager` for real-time speech recognition.
3. **Video Decode Thread** (`videoDecodeThreadFunc`) — Pulls video packets, decodes (software or hardware/CUDA), converts pixel format via `VideoConverter`, and pushes to video frame queue.
4. **Video Render Thread** (`videoRenderThreadFunc`) — Pulls decoded frames, performs A/V sync against `AVSyncClock`, and emits signals (`frameYuv420pDecoded`, `frameNv12Decoded`, `frameRGBADecoded`) consumed by the OpenGL renderer.

Audio output uses an **SDL2 callback model** — `AudioOutput` registers a PCM fill callback that pulls from an `AudioRingBuffer`, which is fed by the audio decode thread.

### A/V Synchronization

`AVSyncClock` (header-only in `syncclock.h`) maintains a master clock. The video render thread compares each frame's PTS against the audio clock (`AudioOutput::getAudioClock()`) and sleeps/drops frames accordingly. Speed changes (0.5x–2.0x) are handled by `AudioFilter` (atempo graph swap) and `AVSyncClock` speed factor simultaneously.

### Thread-Safe Queues with Object Pooling

The pipeline uses bounded, blocking queues (`AVPacketQueue`, `AVFrameQueue`) that enforce backpressure — producers block when queues are full (configurable limits: 5 audio packets, 10 video packets, 8 audio frames, 15 video frames). `FramePool` and `PacketPool` (in `pool/`) provide lock-free object reuse via `GlobalPool` singletons, avoiding repeated `av_frame_alloc`/`av_packet_alloc` calls. `SubtitleQueue` is a specialized sorted container for timestamped subtitle items.

### Rendering Layer

`OpenGLRenderer` extends `QOpenGLWidget` and supports three pixel format paths — YUV420P (3-plane Y/U/V textures), NV12 (2-plane Y/UV), and RGBA (single texture). Shader programs apply brightness/contrast/saturation adjustments in the fragment shader. The renderer also composites subtitle text as a semi-transparent overlay texture. Display modes include aspect-fit and stretch-fill, controlled via `setSizeMode()`.

### ASR (Speech Recognition) Subsystem

`AsrManager` coordinates speech-to-text using the **Strategy pattern** (`IAsrStrategy` interface):
- `AsrRealtimeStrategy` — Processes audio in real-time during playback; accumulates PCM in a ring buffer and runs Whisper inference on segments.
- `AsrOfflineStrategy` — Processes the entire audio track offline (used by the video summary pipeline).

`AsrWorker` runs inference in a background `QThread`. The Whisper model path is user-configurable via `ConfigManager`. Subtitle results flow as `SubtitleItem` structs (start_sec, end_sec, text) emitted via signals.

### AI Video Summary System

The summary pipeline (triggered from `SummaryPanel` UI) runs in multiple stages:

1. **ASR** — `VideoSummaryManager` invokes `AsrOfflineStrategy` to transcribe the entire video.
2. **Segmentation** — `VideoSummarySegmenter` splits the video into time-based segments, extracting keyframes (decoded via `Decoder` + `VideoConverter`) and speech text per segment.
3. **Semantic Segmentation** — `SemanticSegmenter` optionally merges/splits segments based on audio energy (RMS) and visual similarity (histogram comparison), producing semantically coherent segments.
4. **LLM Analysis** — `VideoSummaryNetworkClient` sends segment data (keyframe images + speech text) to a configurable LLM API endpoint, receiving structured `SummaryReport` (title, summary, chapters, key points, highlights).
5. **Caching** — Results are cached per-video (keyed by path+size+mtime SHA256) in the user's AppData directory.

Data structures: `SummarySegment` (time range + keyframes + speech), `SummaryChapter` (AI-generated chapter title + time range), `SummaryReport` (full analysis output). All defined in `videosummarynetworkclient.h`.

### UI Architecture

`MainWindow` (Qt Designer `.ui` form) contains:
- **Center**: `OpenGLRenderer` widget for video display.
- **Bottom**: Control bar with playback controls, progress slider (`VideoSlider` with hover preview), volume, speed selector, and feature buttons.
- **Left**: Collapsible file list (`QListWidget`) with thumbnail previews (`VideoItemWidget`), supporting drag-add, directory scan, and playlist persistence.
- **Right Dock**: `QDockWidget` containing a `QTabWidget` with two tabs — `SummaryPanel` (AI summary dashboard) and `TranscriptPanel` (karaoke-style live transcript with chapter-based grouping).

`PreviewPlayer` is a lightweight secondary player instance that decodes single frames for the progress bar hover preview tooltip.

`SubtitlePopup` renders real-time ASR subtitles as a floating overlay on the video.

### Configuration

`ConfigManager` is a singleton managing two persistence layers:
- **QSettings (INI)** — Player preferences: hardware decode toggle, decoder format, brightness/contrast/saturation, screenshot path, ASR model path, AI summary API config (endpoint, API key, model name, segment duration, semantic segmentation weights).
- **JSON file** — Playlist state: video items (path, name, duration, thumbnail, last position), current index, play mode (ListLoop/SingleRepeat/Shuffle).

Thumbnails are stored in `AppData/SmartPlayer/thumbnails/` with filenames derived from video path hashes.

### Key Conventions

- FFmpeg C API is wrapped in C++ classes; all `av_*` resource management uses RAII patterns in destructors or explicit `close()`/`releaseResources()` methods.
- Inter-thread communication uses Qt signals/slots (queued connections across threads) and atomic variables for state flags.
- The translator module (`translator/`) is present in source but **commented out** in the .pro file — do not attempt to compile it without enabling those lines and providing CTranslate2/SentencePiece libraries.
- UI strings use `QStringLiteral(u"...")` for Chinese text literals.
- The `dependencies/` directory contains pre-built FFmpeg, SDL2, and Whisper headers and import libraries; do not modify this directory.
