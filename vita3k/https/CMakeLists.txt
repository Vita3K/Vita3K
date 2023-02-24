add_library(
	https
	STATIC
	include/https/functions.h
	src/https.cpp
)

target_include_directories(https PUBLIC include)
target_include_directories(https PRIVATE ${OPENSSL_INCLUDE_DIR})
target_link_libraries(https PUBLIC util)
