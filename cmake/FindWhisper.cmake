# FindWhisper.cmake
# Finds whisper library (MinGW import library)
#
# Input variables:
#   whisper_ROOT or Whisper_ROOT - root directory containing whisper
#
# Output:
#   Whisper_FOUND          - True if whisper library is found
#   Whisper_INCLUDE_DIR    - whisper.h location
#   Whisper_LIBRARY        - whisper.lib / libwhisper.a location
#   Whisper_LIBRARIES      - list of all whisper-related libraries

set(WHISPER_ROOT_DIR "${whisper_ROOT}" CACHE PATH "Whisper root directory")
if(NOT WHISPER_ROOT_DIR)
    # 尝试从项目依赖目录推断
    get_filename_component(WHISPER_ROOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/dependencies" ABSOLUTE)
endif()

# ========== 头文件 ==========
find_path(Whisper_INCLUDE_DIR
    NAMES whisper.h
    PATHS
        ${CMAKE_CURRENT_SOURCE_DIR}/dependencies/include
        ${WHISPER_ROOT_DIR}/include
        ${WHISPER_ROOT_DIR}
    PATH_SUFFIXES whisper
)

# ========== 库文件（MinGW 导入库 .lib/.a）==========
find_library(Whisper_LIBRARY
    NAMES whisper libwhisper
    PATHS
        ${CMAKE_CURRENT_SOURCE_DIR}/dependencies/lib
        ${WHISPER_ROOT_DIR}/lib
    NO_DEFAULT_PATH
)

find_library(GGML_LIBRARY
    NAMES ggml libggml
    PATHS
        ${CMAKE_CURRENT_SOURCE_DIR}/dependencies/lib
        ${WHISPER_ROOT_DIR}/lib
    NO_DEFAULT_PATH
)

find_library(GGML_BASE_LIBRARY
    NAMES ggml-base libggml-base
    PATHS
        ${CMAKE_CURRENT_SOURCE_DIR}/dependencies/lib
        ${WHISPER_ROOT_DIR}/lib
    NO_DEFAULT_PATH
)

find_library(GGML_CPU_LIBRARY
    NAMES ggml-cpu libggml-cpu
    PATHS
        ${CMAKE_CURRENT_SOURCE_DIR}/dependencies/lib
        ${WHISPER_ROOT_DIR}/lib
    NO_DEFAULT_PATH
)

# ========== 检查 ==========
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Whisper
    REQUIRED_VARS
        Whisper_LIBRARY
        Whisper_INCLUDE_DIR
        GGML_LIBRARY
        GGML_BASE_LIBRARY
        GGML_CPU_LIBRARY
)

if(Whisper_FOUND)
    set(Whisper_LIBRARIES
        ${Whisper_LIBRARY}
        ${GGML_LIBRARY}
        ${GGML_BASE_LIBRARY}
        ${GGML_CPU_LIBRARY}
    )
endif()

mark_as_advanced(
    Whisper_ROOT
    Whisper_LIBRARY
    Whisper_INCLUDE_DIR
    GGML_LIBRARY
    GGML_BASE_LIBRARY
    GGML_CPU_LIBRARY
)
