add_library(
	shader
	STATIC
	include/shader/glsl/code_writer.h
	include/shader/glsl/params.h
	include/shader/glsl/recompiler.h
	include/shader/glsl/translator.h
	include/shader/spirv/translator_entry.h
	include/shader/spirv/translator.h
	include/shader/spirv/utilities.h
	include/shader/constant_table.h
	include/shader/decoder_detail.h
	include/shader/decoder_helpers.h
	include/shader/disasm.h
	include/shader/gxp_parser.h
	include/shader/matcher.h
	include/shader/profile.h
	include/shader/program_analyzer.h
	include/shader/recompiler.h
	include/shader/translator_types.h
	include/shader/translator.h
    include/shader/types_imm.h
	include/shader/types.h
	
	src/glsl/translator/alu.cpp
	src/glsl/translator/ialu.cpp
	src/glsl/translator/branch_cond.cpp
	src/glsl/translator/data.cpp
	src/glsl/translator/special.cpp
	src/glsl/translator/texture.cpp
	src/glsl/code_writer.cpp
	src/glsl/params.cpp
	src/glsl/recompiler.cpp
	src/glsl/translator.cpp
	src/spirv/translator/alu.cpp
	src/spirv/translator/ialu.cpp
	src/spirv/translator/branch_cond.cpp
	src/spirv/translator/data.cpp
	src/spirv/translator/special.cpp
	src/spirv/translator/texture.cpp
	src/spirv/translator/utils.cpp
	src/spirv/recompiler.cpp
	src/spirv/translator.cpp
	src/spirv/utilities.cpp
	src/translator/alu.cpp
	src/translator/branch_cond.cpp
	src/translator/data.cpp
	src/translator/ialu.cpp
	src/translator/illegal.cpp
	src/translator/special.cpp
	src/translator/texture.cpp
	src/decode_helpers.cpp
	src/disasm.cpp
	src/gxp_parser.cpp
	src/program_analyzer.cpp
	src/translator.cpp
	src/types.cpp
)

target_include_directories(shader PUBLIC include)
target_link_libraries(shader PUBLIC features gxm util spirv-cross-glsl)
target_link_libraries(shader PRIVATE SPIRV)

# Marshmallow Tracy linking
if(TRACY_ENABLE_ON_CORE_COMPONENTS)
	target_link_libraries(shader PRIVATE tracy)
endif()

add_executable(
	shader-tests
	tests/usse_program_analyzer_test.cpp
)

target_include_directories(shader-tests PRIVATE include)
target_link_libraries(shader-tests PRIVATE googletest shader util)
add_test(NAME shader COMMAND shader-tests)
