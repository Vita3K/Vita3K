add_library(
	shadergen
	STATIC
	include/shadergen/functions.h
	include/shadergen/profile.h
	src/shadergen_spirv.cpp
)

target_include_directories(shadergen PUBLIC include)
target_link_libraries(shadergen PUBLIC util vita-headers)
target_link_libraries(shadergen PRIVATE gxm spirv-cross-glsl SPIRV microprofile)
