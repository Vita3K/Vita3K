add_library(
	module
	STATIC
	include/module/args_layout.h
	include/module/bridge.h
	include/module/bridge_types.h
	include/module/lay_out_args.h
	include/module/module.h
	include/module/read_arg.h
	include/module/write_return_value.h
	src/module.cpp
	src/write_return_value.cpp
)

target_include_directories(module PUBLIC include)
target_link_libraries(module PUBLIC host microprofile rpcs3)
