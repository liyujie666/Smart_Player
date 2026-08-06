# FindCTranslate2.cmake
# Finds CTranslate2 library (用于 NLLB/MarianMT 本地翻译)
#
# Output:
#   CTranslate2_FOUND       - True if found
#   CTranslate2_INCLUDE_DIR    - Header location
#   CTranslate2_LIBRARY        - Library file

set(CT2_ROOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/dependencies")

find_path(CTranslate2_INCLUDE_DIR
    NAMES ctranslate2/translator.h
    PATHS
    ${CT2_ROOT_DIR}/include
    PATH_SUFFIXES ctranslate2
)

find_library(CTranslate2_LIBRARY
    NAMES ctranslate2 libctranslate2
    PATHS
   ${CT2_ROOT_DIR}/lib
    NO_DEFAULT_PATH
)

# SentencePiece（CTranslate2 的分词依赖）
find_library(SentencePiece_LIBRARY
    NAMES sentencepiece libsentencepiece
    PATHS
        ${CT2_ROOT_DIR}/lib
    NO_DEFAULT_PATH
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(CTranslate2
 REQUIRED_VARS
        CTranslate2_LIBRARY
        CTranslate2_INCLUDE_DIR
)

if(CTranslate2_FOUND AND NOT TARGET ctranslate2::ctranslate2)
    add_library(ctranslate2::ctranslate2 INTERFACE IMPORTED)
    set_target_properties(ctranslate2::ctranslate2 PROPERTIES
      INTERFACE_INCLUDE_DIRECTORIES "${CTranslate2_INCLUDE_DIR}"
        INTERFACE_LINK_LIBRARIES "${CTranslate2_LIBRARY};${SentencePiece_LIBRARY}"
    )
endif()

mark_as_advanced(CTranslate2_INCLUDE_DIR CTranslate2_LIBRARY SentencePiece_LIBRARY)
