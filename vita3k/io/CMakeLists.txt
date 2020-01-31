add_library(
	io
	STATIC
	include/io/device.h
	include/io/file.h
	include/io/filesystem.h
	include/io/functions.h
	include/io/io.h
	include/io/state.h
	include/io/types.h
	include/io/util.h
	include/io/vfs.h
	include/io/VitaIoDevice.h
	src/device.cpp
	src/file.cpp
	src/filesystem.cpp
	src/io.cpp
	src/state_functions.cpp
)

target_include_directories(io PUBLIC include)
target_link_libraries(io PUBLIC better-enums dirent mem rtc util)
