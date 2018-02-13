add_library(
	io
	STATIC
        src/io.cpp
        src/vfs.cpp
	include/io/functions.h
        include/io/state.h
        include/io/vfs.h
)

target_include_directories(io PUBLIC include)
target_link_libraries(io PUBLIC miniz vita-headers dirent)
target_link_libraries(io PRIVATE util)

if (CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
   target_link_libraries(io PRIVATE stdc++fs)
endif()

