add_library(
	module
	STATIC
	src/load_module.cpp
	src/read_arg.cpp
	src/write_return_value.cpp
)

target_include_directories(module PUBLIC include)
target_link_libraries(module PUBLIC config cpu emuenv kernel util)
if(TRACY_ENABLE_ON_CORE_COMPONENTS)
	target_link_libraries(module PUBLIC tracy)
endif()

if(NOT ANDROID)
	add_executable(
		module-tests
		tests/arg_layout_tests.cpp
	)

target_include_directories(module-tests PRIVATE include)
target_link_libraries(module-tests PRIVATE googletest util)
add_test(NAME module COMMAND module-tests)
endif()
