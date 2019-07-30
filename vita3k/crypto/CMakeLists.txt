add_library(
	crypto
	STATIC
	include/crypto/hash.h
	src/hash.cpp
)

target_include_directories(crypto PUBLIC include)
target_link_libraries(crypto PRIVATE crypto-algorithms)
