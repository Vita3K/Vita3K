add_library(
	kernel
	STATIC
	include/kernel/functions.h
	include/kernel/state.h
	include/kernel/types.h
	include/kernel/thread/thread_functions.h
	include/kernel/thread/thread_state.h
	include/kernel/thread/sync_primitives.h
	include/kernel/load_self.h
	include/kernel/relocation.h
	src/kernel.cpp
	src/thread/thread.cpp
	src/thread/sync_primitives.cpp
	src/load_self.cpp
	src/relocation.cpp
)

target_include_directories(kernel PUBLIC include)
target_link_libraries(kernel PUBLIC module modules cpu vita-headers util)
target_link_libraries(kernel PRIVATE sdl2 elfio miniz nids vita-toolchain)
