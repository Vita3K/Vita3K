add_library(
	util
	STATIC
	include/util/overloaded.h
	include/util/align.h
	include/util/bytes.h
	include/util/safe_time.h
	include/util/exit_code.h
	include/util/exec.h
	include/util/find.h
	include/util/fs.h
	include/util/function_info.h
	include/util/instrset_detect.h
	include/util/semaphore.h
	include/util/lock_and_find.h
	include/util/log.h
	include/util/preprocessor.h
	include/util/pool.h
	include/util/string_utils.h
	include/util/system.h
	include/util/types.h
	include/util/vector_utils.h
	src/util.cpp
	src/instrset_detect.cpp
)

set(INCLUDE_BEGIN "#pragma once

/*
    THIS IS AN AUTOMATICALLY GENERATED FILE. DO NOT EDIT!
*/")

if(${STANDARD_NUM} GREATER 14)
	set(INCLUDE_HEADER "optional")
	set(NAMESPACE "std::optional")
else()
	set(INCLUDE_HEADER "boost/optional.hpp")
	set(NAMESPACE "boost::optional")
endif()

set(INCLUDE_COMPLETE "${INCLUDE_BEGIN}

#include <${INCLUDE_HEADER}>
using ${NAMESPACE}\;
")

file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/include/util/optional.h" ${INCLUDE_COMPLETE})

target_include_directories(util PUBLIC include ${CMAKE_CURRENT_BINARY_DIR}/include)
target_link_libraries(util PUBLIC ${Boost_LIBRARIES} fmt spdlog)
