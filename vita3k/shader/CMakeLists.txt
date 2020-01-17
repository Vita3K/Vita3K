add_library(
	shader
	STATIC
	include/shader/profile.h
	include/shader/usse_types.h
    include/shader/types_imm.h
	include/shader/matcher.h
	include/shader/decoder_detail.h
	include/shader/usse_disasm.h
	include/shader/usse_program_analyzer.h
	include/shader/usse_decoder_helpers.h
	include/shader/usse_translator.h
	include/shader/usse_translator_entry.h
	include/shader/usse_translator_types.h
	include/shader/usse_utilities.h
	include/shader/spirv_recompiler.h

	src/translator/alu.cpp
	src/translator/branch_cond.cpp
	src/translator/data.cpp
	src/translator/special.cpp
	src/translator/texture.cpp
	src/translator/utils.cpp
	src/usse_disasm.cpp
	src/usse_program_analyzer.cpp
	src/usse_decode_helpers.cpp
	src/usse_translator_entry.cpp
	src/usse_utilities.cpp
	src/spirv_recompiler.cpp
)

target_include_directories(shader PUBLIC include)
target_link_libraries(shader PUBLIC features gxm util microprofile spirv-cross-glsl vita-headers)
target_link_libraries(shader PRIVATE SPIRV SPIRV-Tools-opt)

add_executable(
	shader-tests
	tests/usse_program_analyzer_test.cpp
)

target_include_directories(shader-tests PRIVATE include)
target_link_libraries(shader-tests PRIVATE googletest shader util)
add_test(NAME shader COMMAND shader-tests)
