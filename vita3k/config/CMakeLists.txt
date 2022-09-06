add_library(
	config
	STATIC
	include/config/config.h
	include/config/functions.h
	include/config/state.h
	include/config/version.h
	src/config.cpp
	version.cpp
)

configure_file(src/version.cpp.in version.cpp)

target_include_directories(config PUBLIC include)
target_link_libraries(config PUBLIC util)
target_link_libraries(config PRIVATE CLI11 yaml-cpp)
if(TRACY_ENABLE_ON_CORE_COMPONENTS)
	target_link_libraries(config PRIVATE tracy)
endif()
