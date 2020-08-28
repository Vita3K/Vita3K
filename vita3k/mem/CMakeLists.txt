add_library(
	mem
	STATIC
	src/mem.cpp
	include/mem/mempool.h
	include/mem/mem.h
	include/mem/ptr.h
)

target_include_directories(mem PUBLIC include)
target_link_libraries(mem PUBLIC util)
