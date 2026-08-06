cmake_minimum_required(VERSION 3.16)
project(Smart_Player LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# ========== Qt ==========
find_package(Qt6 COMPONENTS
    Core
    Gui
    Widgets
    OpenGL
    OpenGLWidgets
    Concurrent
    Network
    Multimedia
    MultimediaWidgets
    Svg
    SvgWidgets
    REQUIRED
)

# Windows + OpenGL 全屏修复需要用到 QNativeInterface::Private::QWindowsWindow
# （位于 qplatformwindow_p.h），它是 Qt 私有接口。Qt 6 通过 GuiPrivate target 暴露。
if(WIN32)
    find_package(Qt6 COMPONENTS GuiPrivate REQUIRED)
endif()

# ========== 项目源文件目录 ==========
set(SRC_DIR       ${CMAKE_CURRENT_SOURCE_DIR}/src)
set(APP_DIR        ${SRC_DIR}/app)
set(CORE_DIR       ${SRC_DIR}/core)
set(VIEWMODEL_DIR  ${SRC_DIR}/viewmodel)
set(SUMMARY_DIR    ${SRC_DIR}/summary)
set(SUBTITLE_DIR   ${SRC_DIR}/subtitle)
set(QUEUE_DIR      ${SRC_DIR}/queue)
set(RENDER_DIR     ${SRC_DIR}/render)
set(RESAMPLER_DIR  ${SRC_DIR}/resampler)
set(DEMUXER_DIR    ${SRC_DIR}/demuxer)
set(DECODER_DIR    ${SRC_DIR}/decoder)
set(FILTER_DIR     ${SRC_DIR}/filter)
set(CONVERTER_DIR  ${SRC_DIR}/converter)
set(POOL_DIR       ${SRC_DIR}/pool)
set(UTILS_DIR      ${SRC_DIR}/utils)

# ========== 第三方依赖 ==========
set(DEPS_DIR ${CMAKE_CURRENT_SOURCE_DIR}/dependencies)
set(DEPS_LIB_DIR  ${DEPS_DIR}/lib)
set(DEPS_BIN_DIR  ${DEPS_DIR}/bin)
set(DEPS_INCLUDE_DIR ${DEPS_DIR}/include)

# ========== 项目源文件目录（让 #include "xxx/yyy.h" 可以工作）==========
# src/ 下所有模块 + dependencies
set(PROJECT_INCLUDE_DIRS
    ${CMAKE_CURRENT_SOURCE_DIR}       # main.cpp, resources/
    ${SRC_DIR}                        # src/ 下所有子目录都以此为根
    ${APP_DIR}
    ${CORE_DIR}
    ${VIEWMODEL_DIR}
    ${SUMMARY_DIR}
    ${SUBTITLE_DIR}
    ${QUEUE_DIR}
    ${RENDER_DIR}
    ${RESAMPLER_DIR}
    ${DEMUXER_DIR}
    ${DECODER_DIR}
    ${FILTER_DIR}
    ${CONVERTER_DIR}
    ${POOL_DIR}
    ${UTILS_DIR}
    ${DEPS_INCLUDE_DIR}
)

include_directories(${PROJECT_INCLUDE_DIRS})

# 收集所有源文件
file(GLOB_RECURSE APP_SOURCES    ${APP_DIR}/*.cpp)
file(GLOB_RECURSE CORE_SOURCES   ${CORE_DIR}/*.cpp)
file(GLOB_RECURSE VM_SOURCES     ${VIEWMODEL_DIR}/*.cpp)
file(GLOB_RECURSE SUMMARY_SOURCES ${SUMMARY_DIR}/*.cpp)
file(GLOB_RECURSE SUBTITLE_SOURCES ${SUBTITLE_DIR}/*.cpp)
file(GLOB_RECURSE QUEUE_SOURCES   ${QUEUE_DIR}/*.cpp)
file(GLOB_RECURSE RENDER_SOURCES  ${RENDER_DIR}/*.cpp)
file(GLOB_RECURSE RESAMPLER_SOURCES ${RESAMPLER_DIR}/*.cpp)
file(GLOB_RECURSE DEMUXER_SOURCES ${DEMUXER_DIR}/*.cpp)
file(GLOB_RECURSE DECODER_SOURCES ${DECODER_DIR}/*.cpp)
file(GLOB_RECURSE FILTER_SOURCES  ${FILTER_DIR}/*.cpp)
file(GLOB_RECURSE CONVERTER_SOURCES ${CONVERTER_DIR}/*.cpp)
file(GLOB_RECURSE POOL_SOURCES    ${POOL_DIR}/*.cpp)
file(GLOB_RECURSE UTILS_SOURCES   ${UTILS_DIR}/*.cpp)

file(GLOB APP_HEADERS ${APP_DIR}/*.h)
file(GLOB CORE_HEADERS  ${CORE_DIR}/*.h)
file(GLOB VM_HEADERS   ${VIEWMODEL_DIR}/*.h)
file(GLOB SUMMARY_HEADERS ${SUMMARY_DIR}/*.h)
file(GLOB SUBTITLE_HEADERS ${SUBTITLE_DIR}/*.h)
file(GLOB QUEUE_HEADERS  ${QUEUE_DIR}/*.h)
file(GLOB RENDER_HEADERS ${RENDER_DIR}/*.h)
file(GLOB RESAMPLER_HEADERS ${RESAMPLER_DIR}/*.h)
file(GLOB DEMUXER_HEADERS ${DEMUXER_DIR}/*.h)
file(GLOB DECODER_HEADERS ${DECODER_DIR}/*.h)
file(GLOB FILTER_HEADERS ${FILTER_DIR}/*.h)
file(GLOB CONVERTER_HEADERS ${CONVERTER_DIR}/*.h)
file(GLOB POOL_HEADERS   ${POOL_DIR}/*.h)
file(GLOB UTILS_HEADERS  ${UTILS_DIR}/*.h)

# UI 文件
file(GLOB APP_FORMS    ${APP_DIR}/*.ui)

set(ALL_SOURCES
    ${APP_SOURCES} ${CORE_SOURCES} ${VM_SOURCES} ${SUMMARY_SOURCES}
    ${SUBTITLE_SOURCES} ${QUEUE_SOURCES} ${RENDER_SOURCES} ${RESAMPLER_SOURCES}
    ${DEMUXER_SOURCES} ${DECODER_SOURCES} ${FILTER_SOURCES} ${CONVERTER_SOURCES}
    ${POOL_SOURCES} ${UTILS_SOURCES}
)

set(ALL_HEADERS
    ${APP_HEADERS} ${CORE_HEADERS} ${VM_HEADERS} ${SUMMARY_HEADERS}
    ${SUBTITLE_HEADERS} ${QUEUE_HEADERS} ${RENDER_HEADERS} ${RESAMPLER_HEADERS}
    ${DEMUXER_HEADERS} ${DECODER_HEADERS} ${FILTER_HEADERS} ${CONVERTER_HEADERS}
    ${POOL_HEADERS} ${UTILS_HEADERS}
)

# ========== 自定义 Find 模块 ==========
list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

# Whisper
find_package(Whisper REQUIRED)

# ONNX Runtime (用于 FSMN-VAD / SenseVoice)
find_package(OnnxRuntime QUIET)
if(OnnxRuntime_FOUND)
    message(STATUS "ONNX Runtime found: ${OnnxRuntime_LIBRARY}")
    add_compile_definitions(HAS_ONNXRUNTIME=1)
else()
    message(STATUS "ONNX Runtime not found - VAD/SenseVoice will be disabled")
    add_compile_definitions(HAS_ONNXRUNTIME=0)
endif()

# CTranslate2 (用于 NLLB / MarianMT 本地翻译)
find_package(CTranslate2 QUIET)
if(CTranslate2_FOUND)
    message(STATUS "CTranslate2 found: ${CTranslate2_LIBRARY}")
    add_compile_definitions(HAS_CTRANSLATE2=1)
else()
    message(STATUS "CTranslate2 not found - NLLB/MarianMT will be disabled")
    add_compile_definitions(HAS_CTRANSLATE2=0)
endif()

# Whisper 的 .lib 是导入库（MinGW 风格），CMake 把它当作 target 链接时
# 会展开成 -lwhisper，但 cmake 是按文件名查的 (libwhisper.lib 或 whisper.lib)
# 这里强制使用绝对路径 + INTERFACE 库，把所有 whisper/ggml 库当作一个 target
if(Whisper_FOUND)
    add_library(whisper_import INTERFACE)
    target_link_libraries(whisper_import INTERFACE
        ${Whisper_LIBRARY}
        ${GGML_LIBRARY}
        ${GGML_BASE_LIBRARY}
        ${GGML_CPU_LIBRARY}
    )
    target_include_directories(whisper_import INTERFACE ${Whisper_INCLUDE_DIR})
endif()

# ========== 资源文件 ==========
set(RESOURCES_DIR  ${CMAKE_CURRENT_SOURCE_DIR}/resources)
set(RESOURCE_QRC   ${RESOURCES_DIR}/Resource.qrc)
set(APP_ICON_RC    ${RESOURCES_DIR}/app_icon.rc)

# ========== 可执行文件 ==========
add_executable(${PROJECT_NAME} WIN32
    main.cpp
    ${ALL_SOURCES}
    ${ALL_HEADERS}
    ${APP_FORMS}
    ${RESOURCE_QRC}
    ${APP_ICON_RC}
)

# ========== Qt 依赖 ==========
target_link_libraries(${PROJECT_NAME} PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::OpenGL
    Qt6::OpenGLWidgets
    Qt6::Concurrent
    Qt6::Network
    Qt6::Multimedia
    Qt6::MultimediaWidgets
    Qt6::Svg
    Qt6::SvgWidgets
)

# Windows + OpenGL 全屏修复：链接 Qt 私有模块以访问 QWindowsWindow 接口
if(WIN32)
    target_link_libraries(${PROJECT_NAME} PRIVATE Qt6::GuiPrivate)
endif()

# ========== 第三方库链接 ==========
# 使用绝对路径指向 dependencies/lib
set(DEPS_LIB_ABS ${CMAKE_CURRENT_SOURCE_DIR}/dependencies/lib)

# 注意：链接顺序很重要
#   - 由于定义了 SDL_MAIN_HANDLED，main() 来自用户代码，不需要 SDL2main.lib
#   - whisper.dll 的 import lib 需要在前面（它依赖 ggml）
# FFmpeg + SDL2 + Whisper
target_link_libraries(${PROJECT_NAME} PRIVATE
    ${DEPS_LIB_ABS}/avformat.lib
    ${DEPS_LIB_ABS}/avcodec.lib
    ${DEPS_LIB_ABS}/avdevice.lib
    ${DEPS_LIB_ABS}/avfilter.lib
    ${DEPS_LIB_ABS}/avutil.lib
    ${DEPS_LIB_ABS}/swscale.lib
    ${DEPS_LIB_ABS}/swresample.lib
    ${DEPS_LIB_ABS}/postproc.lib
    ${DEPS_LIB_ABS}/SDL2.lib
)

# MinGW 下 Qt6EntryPoint 依赖 C runtime（msvcrt），需启动分组链接
target_link_options(${PROJECT_NAME} PRIVATE
    "LINKER:--start-group"
    "-Wl,--start-group"
    "LINKER:--end-group"
    "-Wl,--end-group"
)

# Whisper +ggml：通过 INTERFACE target 链接（避免 -lwhisper 解析失败）
target_link_libraries(${PROJECT_NAME} PRIVATE whisper_import)

# ONNX Runtime（可选）
if(OnnxRuntime_FOUND)
    target_link_libraries(${PROJECT_NAME} PRIVATE onnxruntime::onnxruntime)
endif()

# CTranslate2（可选，用于本地翻译）
if(CTranslate2_FOUND)
    target_link_libraries(${PROJECT_NAME} PRIVATE ctranslate2::ctranslate2)
endif()

# Windows 系统库
target_link_libraries(${PROJECT_NAME} PRIVATE dbghelp)

# ========== 定义 ==========
target_compile_definitions(${PROJECT_NAME} PRIVATE SDL_MAIN_HANDLED)

# ========== 复制运行时 DLL 到输出目录 ==========
# 策略：把 dependencies/bin/*.dll 复制到构建输出目录
file(GLOB DEPS_DLLS "${DEPS_BIN_DIR}/*.dll")
foreach(dll ${DEPS_DLLS})
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${dll}
            $<TARGET_FILE_DIR:${PROJECT_NAME}>
    )
endforeach()

# 复制 Qt 插件到输出目录（直接放在 exe 同级的标准目录，Qt 才能找到）
file(GLOB_RECURSE QT_PLUGIN_DLLS "${DEPS_DIR}/plugins/*.dll")
foreach(plugin_dll ${QT_PLUGIN_DLLS})
    get_filename_component(plugin_subdir ${plugin_dll} DIRECTORY)
    string(REPLACE "${DEPS_DIR}/plugins/" "" plugin_dest_subdir ${plugin_subdir})
    # plugin_dest_subdir 例如 "platforms" "imageformats" "iconengines"
    set(plugin_dest_dir $<TARGET_FILE_DIR:${PROJECT_NAME}>/${plugin_dest_subdir})
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory ${plugin_dest_dir}
    )
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${plugin_dll}
            ${plugin_dest_dir}/
    )
endforeach()

# ========== 复制 Qt TLS 插件（来自 Qt 安装目录本身）==========
# Qt 安装目录里自带 tls/qschannelbackend.dll / qopensslbackend.dll /
# qcertonlybackend.dll，用于 HTTPS 连接。复制到 exe 同级的 tls/ 子目录。
# 用多种方式探测 Qt 安装路径，以适配不同环境。
set(_qt_tls_plugins_dir "")

# 方式 1：CMake 缓存的 Qt 安装根（find_package 时由 Qt6Config.cmake 设置）
if(DEFINED QT_INSTALL_PREFIX AND EXISTS "${QT_INSTALL_PREFIX}/mingw_64/plugins/tls")
    set(_qt_tls_plugins_dir "${QT_INSTALL_PREFIX}/mingw_64/plugins/tls")
endif()

# 方式 2：通过 Qt6_DIR 反推（Qt6_DIR = .../mingw_64/lib/cmake/Qt6）
if(NOT _qt_tls_plugins_dir AND DEFINED Qt6_DIR)
    get_filename_component(_qt_dir "${Qt6_DIR}" DIRECTORY)   # .../mingw_64/lib
    get_filename_component(_qt_root "${_qt_dir}" DIRECTORY)  # .../mingw_64
    if(EXISTS "${_qt_root}/plugins/tls")
        set(_qt_tls_plugins_dir "${_qt_root}/plugins/tls")
    endif()
endif()

# 方式 3：硬编码路径作为兜底（你的机器实测就是这个路径）
if(NOT _qt_tls_plugins_dir)
    set(_candidate "D:/Qt/6.9.0/mingw_64/plugins/tls")
    if(EXISTS "${_candidate}")
        set(_qt_tls_plugins_dir "${_candidate}")
    endif()
endif()

if(_qt_tls_plugins_dir)
    set(_tls_dest_dir "$<TARGET_FILE_DIR:${PROJECT_NAME}>/tls")
    file(GLOB _qt_tls_dlls "${_qt_tls_plugins_dir}/*.dll")
    foreach(tls_dll ${_qt_tls_dlls})
        add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory "${_tls_dest_dir}"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${tls_dll}"
                "${_tls_dest_dir}/"
        )
    endforeach()
    message(STATUS "Qt TLS plugins will be copied from: ${_qt_tls_plugins_dir}")
else()
    message(WARNING "Qt TLS plugins not found. HTTPS will fail.")
endif()

# ========== 安装（可选）==========
install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION bin)
