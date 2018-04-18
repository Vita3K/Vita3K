add_library(
	io
	STATIC
	src/io.cpp
	src/file.cpp
	include/io/io.h
	include/io/functions.h
	include/io/state.h
	include/io/file.h
	include/io/VitaIoDevice.def
)

target_include_directories(io PUBLIC include)
target_link_libraries(io PUBLIC miniz vita-headers dirent)
target_link_libraries(io PRIVATE util)
