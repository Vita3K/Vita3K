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
	include/io/AbstractFS.h
	include/io/FilesytemHandlers.h
	include/io/ExternalFS.h
	include/io/UX0.h
	include/io/IGameFS.h
	src/device.cpp
	src/file.cpp
	src/filesystem.cpp
	src/io.cpp
	src/state_functions.cpp
	src/FilesystemHandlers.cpp
	src/AbstractFS.cpp
	src/ExternalFS.cpp
	src/UX0.cpp
	)

target_include_directories(io PUBLIC include)
target_link_libraries(io PUBLIC better-enums dirent mem rtc util)
