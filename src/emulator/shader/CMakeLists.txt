add_library(
	shader
	STATIC
	include/shader/profile.h
	include/shader/usse_types.h
    include/shader/types_imm.h
	include/shader/matcher.h
	include/shader/decoder_detail.h
	include/shader/usse_disasm.h
	include/shader/usse_decoder_helpers.h
	include/shader/usse_translator.h
	include/shader/usse_translator_entry.h
	include/shader/usse_translator_types.h
	include/shader/spirv_recompiler.h

	src/usse_disasm.cpp
	src/usse_decode_helpers.cpp
	src/usse_translator.cpp
	src/usse_translator_entry.cpp
	src/spirv_recompiler.cpp
)

target_include_directories(shader PUBLIC include)
target_link_libraries(shader PUBLIC util spirv-cross-glsl vita-headers)
target_link_libraries(shader PRIVATE gxm SPIRV microprofile)
