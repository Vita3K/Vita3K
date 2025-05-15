set(SOURCE_LIST
	include/kernel/state.h
	include/kernel/types.h
	include/kernel/thread/thread_data_queue.h
	include/kernel/thread/thread_state.h
	include/kernel/cpu_protocol.h
	include/kernel/sync_primitives.h
	include/kernel/relocation.h
	include/kernel/object_store.h
	include/kernel/debugger.h
	include/kernel/load_self.h
	include/kernel/callback.h
	src/kernel.cpp
	src/thread.cpp
	src/debugger.cpp
	src/load_self.cpp
	src/cpu_protocol.cpp
	src/sync_primitives.cpp
	src/relocation.cpp
	src/callback.cpp
)

add_library(
	kernel
	STATIC
	${SOURCE_LIST}
)

target_include_directories(kernel PUBLIC include)
target_link_libraries(kernel PUBLIC rtc cpu mem util nids)
target_link_libraries(kernel PRIVATE patch SDL3::SDL3 miniz vita-toolchain)
if(TRACY_ENABLE_ON_CORE_COMPONENTS)
	target_link_libraries(kernel PRIVATE tracy)
endif()
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${SOURCE_LIST})