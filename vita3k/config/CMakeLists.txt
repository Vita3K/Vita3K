add_library(
	config
	STATIC
	include/config/config.h
	include/config/functions.h
	include/config/state.h
	include/config/version.h
	include/config/yaml.h
	src/config.cpp
	version.cpp
)

configure_file(src/version.cpp.in version.cpp)

target_include_directories(config PUBLIC include)
target_link_libraries(config PUBLIC util yaml-cpp)
target_link_libraries(config PRIVATE CLI11)

if(${STANDARD_NUM} GREATER 11)
	if(${STANDARD_NUM} GREATER 14)
		target_compile_definitions(config PRIVATE CLI11_STD_OPTIONAL=1)
	else()
		target_compile_definitions(config PRIVATE CLI11_STD_OPTIONAL=0 CLI11_STD_EXPERIMENTAL_OPTIONAL=1)
	endif()
else()
	target_link_libraries(config PRIVATE ${Boost_LIBRARIES})
	target_compile_definitions(config PRIVATE CLI11_STD_OPTIONAL=0 CLI11_STD_EXPERIMENTAL_OPTIONAL=0 CLI11_BOOST_OPTIONAL=1)
endif()
