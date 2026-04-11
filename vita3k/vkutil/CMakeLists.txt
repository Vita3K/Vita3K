add_library(
	vkutil
	STATIC
	include/vkutil/objects.h
	include/vkutil/vkutil.h

	src/objects.cpp
	src/vkutil.cpp
)

target_include_directories(vkutil PUBLIC include)
target_link_libraries(vkutil PUBLIC vulkan vma)
target_link_libraries(vkutil PRIVATE util)
