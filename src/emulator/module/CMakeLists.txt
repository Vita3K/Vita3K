add_library(
	module
	STATIC
	include/module/bridge.h
	include/module/bridge_args.h
	include/module/bridge_return.h
	include/module/module.h
	src/bridge_return.cpp
	src/module.cpp
)

target_include_directories(module PUBLIC include)
target_link_libraries(module PUBLIC host microprofile rpcs3)
