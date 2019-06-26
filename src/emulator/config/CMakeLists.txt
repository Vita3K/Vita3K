add_library(
	config
	STATIC
	include/config/config.h
	include/config/config_func.h
	include/config/version.h
	src/config.cpp
	version.cpp
)

configure_file(src/version.cpp.in version.cpp)

target_include_directories(config PUBLIC include)
target_link_libraries(config PUBLIC util vita-headers)
target_link_libraries(config PRIVATE yaml-cpp)
