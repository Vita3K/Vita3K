option(COMPILE_MODERN_CPP "Compile ${PROJECT_NAME} as a C++17 or C++14 program" ON)
option(COMPILE_CPP17 "Force compilation of ${PROJECT_NAME} as C++17" OFF)
option(COMPILE_CPP14 "Force compilation of ${PROJECT_NAME} as C++14" OFF)
mark_as_advanced(FORCE COMPILE_CPP17)
mark_as_advanced(FORCE COMPILE_CPP14)

# Assign the proper standard and compilation to the project based on the flags available.
# This tests for C++17 and C++14 compatibility, while applying C++11 as a fallback.
# Based on https://stackoverflow.com/a/44964919
function(get_standard_for_build)
	unset(STANDARD_FLAG PARENT_SCOPE)

	if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
		set(COMPILER_TEST "-std=c++1z;-std=c++1y;-std=c++0x")
	elseif(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
		# VS 2015.3+ does not have a C++11 option
		set(COMPILER_TEST "/std:c++17;/std:c++14;INVALID")
	elseif(CMAKE_CXX_COMPILER_ID MATCHES "^(Apple)?Clang$")
		if(MSVC) ##for msvc-clang
			set(COMPILER_TEST "/std:c++17;/std:c++14;/std:c++11")
		else()
			set(COMPILER_TEST "-std=c++17;-std=c++14;-std=c++11")
		endif()
	else()
		set(COMPILER_TEST "-std:c++17;-std:c++14;-std:c++11")
	endif()

	if(COMPILE_MODERN_CPP)
		if(NOT COMPILE_CPP17 AND NOT COMPILE_CPP14)
			include(CheckCXXCompilerFlag)
			list(GET COMPILER_TEST 0 STANDARD_FLAG)
			check_cxx_compiler_flag(${STANDARD_FLAG} HAS_CPP17_FLAG)

			include(CheckIncludeFileCXX)
			check_include_file_cxx("filesystem" HAS_CPP17_INCLUDE)
			if(HAS_CPP17_FLAG AND HAS_CPP17_INCLUDE)
				set(COMPILE_CPP17 ON)
			else()
				list(GET COMPILER_TEST 1 STANDARD_FLAG)
				check_cxx_compiler_flag(${STANDARD_FLAG} HAS_CPP14_FLAG)
				check_include_file_cxx("experimental/filesystem" HAS_EXPERIMENTAL)
				if(HAS_CPP14_FLAG AND HAS_EXPERIMENTAL)
					set(COMPILE_CPP14 ON)
				else()
					message(FATAL_ERROR "Your compiler does not support C++14 or C++17. "
					"Call\ncmake -DCOMPILE_MODERN_CPP=OFF ..\n"
					"to build ${PROJECT_NAME}.")
				endif()
			endif()
		endif()

		if(COMPILE_CPP17)
			set(CPP17_SUPPORTED ON PARENT_SCOPE)
			list(GET COMPILER_TEST 0 STANDARD_FLAG)
		elseif(COMPILE_CPP14)
			set(CPP14_SUPPORTED ON PARENT_SCOPE)
			list(GET COMPILER_TEST 1 STANDARD_FLAG)
		endif()
	else()
		list(GET COMPILER_TEST 2 STANDARD_FLAG)
	endif()

	set(STANDARD_FLAG ${STANDARD_FLAG} PARENT_SCOPE)
endfunction()
