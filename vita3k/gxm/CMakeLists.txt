add_library(
	gxm
	STATIC
	include/gxm/functions.h
	include/gxm/state.h
	include/gxm/types.h
	src/attributes.cpp
	src/color.cpp
	src/gxp.cpp
	src/stream.cpp
	src/textures.cpp
	src/transfer.cpp
)

target_include_directories(gxm PUBLIC include)
target_link_libraries(gxm PUBLIC util)
target_link_libraries(gxm PRIVATE)

if(NOT ANDROID AND NOT MSVC AND CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    find_package(Python3 QUIET COMPONENTS Interpreter)
    if(Python3_Interpreter_FOUND)
        add_test(NAME gxm-fragment-linkage
            COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/fragment_linkage_test.py
                --cxx ${CMAKE_CXX_COMPILER} --openssl-include ${OPENSSL_INCLUDE_DIR}
                --crypto-library ${OPENSSL_CRYPTO_LIBRARY})
    endif()
endif()
