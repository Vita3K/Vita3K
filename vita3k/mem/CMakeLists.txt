add_library(
	mem
	STATIC
	include/mem/allocator.h
	include/mem/atomic.h
	include/mem/functions.h
	include/mem/mempool.h
	include/mem/block.h
	include/mem/ptr.h
	include/mem/state.h
	include/mem/util.h
	src/allocator.cpp
	src/mem.cpp
)

target_include_directories(mem PUBLIC include)

target_link_libraries(mem PUBLIC util)

if(TRACY_ENABLE_ON_CORE_COMPONENTS)
	target_link_libraries(mem PRIVATE tracy)
endif()

if(NOT ANDROID)
	add_executable(
		mem-tests
		tests/allocator_tests.cpp
	)

	target_include_directories(mem-tests PRIVATE include)
	target_link_libraries(mem-tests PRIVATE mem googletest util)
	add_test(NAME mem COMMAND mem-tests)
endif()
