add_library(
	io
	STATIC
	src/io.cpp
	include/io/functions.h
	include/io/state.h
)

target_include_directories(io PUBLIC include)
target_link_libraries(io PUBLIC miniz vita-headers)
target_link_libraries(io PRIVATE util)
