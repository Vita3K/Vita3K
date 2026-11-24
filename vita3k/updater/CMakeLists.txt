include(CheckCXXSourceCompiles)
set(_UPDATER_SAVED_REQUIRED_INCLUDES "${CMAKE_REQUIRED_INCLUDES}")
set(CMAKE_REQUIRED_INCLUDES ${Boost_INCLUDE_DIRS})
unset(VITA3K_UPDATER_HAS_BOOST_JSON CACHE)
check_cxx_source_compiles("#define BOOST_CONTAINER_NO_LIB\n#define BOOST_JSON_NO_LIB\n#include <boost/json/src.hpp>\nint main() { return 0; }" VITA3K_UPDATER_HAS_BOOST_JSON)
set(CMAKE_REQUIRED_INCLUDES "${_UPDATER_SAVED_REQUIRED_INCLUDES}")
unset(_UPDATER_SAVED_REQUIRED_INCLUDES)
if(NOT VITA3K_UPDATER_HAS_BOOST_JSON)
	message(FATAL_ERROR "The updater requires Boost.JSON headers (boost/json/src.hpp). Install Boost 1.82 or newer with Boost.JSON, or initialize the bundled Boost submodule.")
endif()

add_library(
    updater
    STATIC
    include/updater/functions.h
    include/updater/state.h
    src/common.cpp
    src/release.cpp
)

target_include_directories(updater PUBLIC include)
target_link_libraries(updater PUBLIC config fmt)
