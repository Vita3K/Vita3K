add_library(
	crypto
	STATIC
	include/crypto/hash.h
        include/crypto/crypto_operations_factory.h
        include/crypto/crypto_operations_interface.h
        include/crypto/libtomcrypt_crypto_operations.h
        src/hash.cpp
        src/crypto_operations_factory.cpp
        src/libtomcrypt_crypto_operations.cpp
)

target_include_directories(crypto PUBLIC include)
target_link_libraries(crypto PRIVATE libtomcrypt)
