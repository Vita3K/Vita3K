add_library(
	shader
	STATIC
	include/shader/functions.h
	include/shader/profile.h
	src/spirv_recompiler.cpp
	src/parse_gxp_usse.cpp
)

target_include_directories(shader PUBLIC include)
target_link_libraries(shader PUBLIC util vita-headers)
target_link_libraries(shader PRIVATE gxm spirv-cross-glsl SPIRV microprofile)
