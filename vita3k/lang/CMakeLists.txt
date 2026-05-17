find_package(Python3 REQUIRED COMPONENTS Interpreter)

set(LANG_GENERATED_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
set(LANG_GENERATOR "${CMAKE_SOURCE_DIR}/tools/i18n/generate_lang_catalog.py")
set(LANG_STRINGS_DEF "${CMAKE_CURRENT_SOURCE_DIR}/include/lang/strings.def")
file(GLOB LANG_JSON_FILES CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/i18n/lang/overlay_*.json")

set(LANG_GENERATED_HEADER "${LANG_GENERATED_DIR}/generated_catalog.h")
set(LANG_GENERATED_SOURCE "${LANG_GENERATED_DIR}/generated_catalog.cpp")

add_custom_command(
    OUTPUT
        "${LANG_GENERATED_HEADER}"
        "${LANG_GENERATED_SOURCE}"
    COMMAND
        ${Python3_EXECUTABLE}
        "${LANG_GENERATOR}"
        --input-dir "${CMAKE_SOURCE_DIR}/i18n/lang"
        --strings-def "${LANG_STRINGS_DEF}"
        --output-header "${LANG_GENERATED_HEADER}"
        --output-source "${LANG_GENERATED_SOURCE}"
    DEPENDS
        "${LANG_GENERATOR}"
        "${LANG_STRINGS_DEF}"
        ${LANG_JSON_FILES}
    VERBATIM
)

add_library(
	lang
	STATIC
	include/lang/state.h
	include/lang/strings.def
	src/lang.cpp
	"${LANG_GENERATED_HEADER}"
	"${LANG_GENERATED_SOURCE}"
)

target_link_libraries(lang PRIVATE util)

target_include_directories(lang
	PUBLIC
		include
	PRIVATE
		"${LANG_GENERATED_DIR}"
)
