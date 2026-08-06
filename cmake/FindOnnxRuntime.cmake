# FindOnnxRuntime.cmake
# Finds ONNX Runtime library
#
# Output:
#   OnnxRuntime_FOUND         - True if found
#   OnnxRuntime_INCLUDE_DIR   - Header location
#   OnnxRuntime_LIBRARY       - Library file location

set(ONNXRT_ROOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/dependencies")

# ========== 头文件 ==========
find_path(OnnxRuntime_INCLUDE_DIR
    NAMES onnxruntime_c_api.h
    PATHS
        ${ONNXRT_ROOT_DIR}/include
        ${ONNXRT_ROOT_DIR}/include/onnxruntime
    PATH_SUFFIXES onnxruntime
)

# ========== 库文件 ==========
find_library(OnnxRuntime_LIBRARY
    NAMES onnxruntime libonnxruntime
    PATHS
        ${ONNXRT_ROOT_DIR}/lib
    NO_DEFAULT_PATH
)

# ========== 检查 ==========
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(OnnxRuntime
    REQUIRED_VARS
        OnnxRuntime_LIBRARY
        OnnxRuntime_INCLUDE_DIR
)

#创建 INTERFACE target方便使用
if(OnnxRuntime_FOUND AND NOT TARGETonnxruntime::onnxruntime)
    add_library(onnxruntime::onnxruntime INTERFACE IMPORTED)
    set_target_properties(onnxruntime::onnxruntime PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "${OnnxRuntime_INCLUDE_DIR}"
        INTERFACE_LINK_LIBRARIES "${OnnxRuntime_LIBRARY}"
    )
endif()

mark_as_advanced(OnnxRuntime_INCLUDE_DIR OnnxRuntime_LIBRARY)
