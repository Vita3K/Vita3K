add_library(
	kernel
	STATIC
	include/kernel/functions.h
	include/kernel/state.h
	include/kernel/thread_functions.h
	include/kernel/thread_state.h
	src/kernel.cpp
	src/thread.cpp
)

target_include_directories(kernel PUBLIC include)
target_link_libraries(kernel PUBLIC cpu sdl2 vita-headers util)
