add_library(
	mem
	STATIC
	include/mem/atomic.h
	include/mem/functions.h
	include/mem/mempool.h
	include/mem/block.h
	include/mem/ptr.h
	include/mem/state.h
	include/mem/util.h
	src/mem.cpp
)

target_include_directories(mem PUBLIC include)
target_link_libraries(mem PUBLIC util)
