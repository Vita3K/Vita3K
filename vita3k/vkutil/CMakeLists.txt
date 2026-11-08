add_library(
	vkutil
	STATIC
	include/vkutil/apple_loader.h
	include/vkutil/native_surface.h
	include/vkutil/objects.h
	include/vkutil/vkutil.h

	src/apple_loader.cpp
	src/objects.cpp
	src/vkutil.cpp
)

target_include_directories(vkutil PUBLIC include)
target_link_libraries(vkutil PUBLIC vulkan vma)
target_link_libraries(vkutil PRIVATE util)
