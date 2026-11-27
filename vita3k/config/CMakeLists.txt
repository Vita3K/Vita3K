add_library(
	config
	STATIC
	include/config/config.h
	include/config/functions.h
	include/config/settings.h
	include/config/state.h
	include/config/version.h
	src/config.cpp
	src/settings.cpp
	${CMAKE_CURRENT_BINARY_DIR}/version.cpp
)
add_dependencies(config vita3k_version)

target_include_directories(config PUBLIC include)
target_link_libraries(config PUBLIC input util)
target_link_libraries(config PRIVATE CLI11 pugixml::pugixml yaml-cpp)
if(TRACY_ENABLE_ON_CORE_COMPONENTS)
	target_link_libraries(config PRIVATE tracy)
endif()
