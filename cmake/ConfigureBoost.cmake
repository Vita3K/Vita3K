if(NOT CI)
	# Only use this repo's Boost
	set(Boost_NO_SYSTEM_PATHS ON)

	# Set paths for use throughout.
	set(BOOST_ROOT "${PROJECT_SOURCE_DIR}/external/boost/")
	set(BOOST_BUILDDIR "${PROJECT_SOURCE_DIR}/external/boost-build/")
	set(BOOST_LIBRARYDIR "${BOOST_BUILDDIR}/lib")

	# Advanced settings
	option(BOOST_PRECOMPILE_ALL "Instead of compiling specific components, compile all Boost libraries" OFF)
	option(BOOST_ALWAYS_PRECOMPILE "Always compile Boost on every CMake reload" OFF)
	set(BOOST_CXX_STANDARD 11)

	mark_as_advanced(FORCE BOOST_ALWAYS_PRECOMPILE)
	mark_as_advanced(FORCE BOOST_PRECOMPILE_ALL)
endif()

function(pre_configure_boost)
	# Based on: https://github.com/maidsafe-archive/MaidSafe/blob/master/cmake_modules/add_boost.cmake#L154
	unset(B2_EXEC CACHE)
	if(WIN32)
		set(B2_NAME "b2.exe")
	else()
		set(B2_NAME "b2")
	endif()
	find_program(B2_EXEC NAMES ${B2_NAME} PATHS "${BOOST_ROOT}" NO_DEFAULT_PATH)

	if(NOT B2_EXEC)
		message(STATUS "Running Boost Bootstrap...")
		if(WIN32)
			set(BOOST_BOOTSTRAP "${BOOST_ROOT}/bootstrap.bat")
			list(APPEND BOOST_BOOTSTRAP "--with-toolset=msvc")
		else()
			set(BOOST_BOOTSTRAP "${BOOST_ROOT}/bootstrap.sh")
			if(CMAKE_CXX_COMPILER_ID MATCHES "^(Apple)?Clang$")
				list(APPEND BOOST_BOOTSTRAP "--with-toolset=clang")
			endif()
		endif()

		execute_process(COMMAND ${BOOST_BOOTSTRAP}
			WORKING_DIRECTORY ${BOOST_ROOT}
			RESULT_VARIABLE Result
			OUTPUT_VARIABLE Output
			ERROR_VARIABLE Error)

		if(NOT Result EQUAL "0")
			message(FATAL_ERROR "Boost Bootstrap failed: ${BOOST_ROOT}\n\n${Output}\n${Error}")
		else()
			message(STATUS "Boost Bootstrap completed successfully.")
			find_program(B2_EXEC NAMES ${B2_NAME} PATHS "${BOOST_ROOT}")
		endif()
	endif()

	message(STATUS "Now building Boost...")

	unset(dirList CACHE)
	if(BOOST_PRECOMPILE_ALL)
		file(GLOB dirList "${BOOST_ROOT}/libs/*")
	else()
		foreach(component ${BOOST_COMPONENTS})
			list(APPEND dirList "${BOOST_ROOT}/libs/${component}")
		endforeach()
	endif()

	foreach(component ${dirList})
		if(EXISTS "${component}/build") # compile each needed Boost library
			execute_process(COMMAND ${B2_EXEC}
				link=static
				threading=multi
				variant=release
				debug-symbols=on
				cxxstd=${BOOST_CXX_STANDARD}
				-j4
				--build-dir=${BOOST_BUILDDIR}
				stage
				--stagedir=${BOOST_BUILDDIR}
				WORKING_DIRECTORY "${component}/build"
				RESULT_VARIABLE Result)
			
			if(NOT Result EQUAL "0")
				message(FATAL_ERROR "Failed to compile Boost library ${component}")
			endif()
		endif()
	endforeach()

	message(STATUS "Boost pre-configuration complete.")
endfunction(pre_configure_boost)

function(setup_boost)
	if(NOT EXISTS "${BOOST_BUILDDIR}")
		file(MAKE_DIRECTORY "${BOOST_BUILDDIR}")
	endif()

	if(WIN32)
		set(LIBRARY_EXT "lib")
	else()
		set(LIBRARY_EXT "a")
	endif()
	unset(fileList CACHE)
	file(GLOB fileList "${BOOST_LIBRARYDIR}/*.${LIBRARY_EXT}")
	if(fileList STREQUAL "" OR BOOST_ALWAYS_PRECOMPILE)
		pre_configure_boost()
	endif()
endfunction()

function(configure_boost)
	set(Boost_USE_STATIC_LIBS ON)
	set(Boost_USE_MULTITHREADED ON)
	set(Boost_USE_RELEASE_LIBS ON)
	if(CI)
		set(Boost_USE_DEBUG_LIBS OFF)
	endif()
	set(Boost_NO_BOOST_CMAKE ON)

	find_package(Boost COMPONENTS ${BOOST_COMPONENTS} REQUIRED)

	add_compile_options(-DBOOST_NO_CXX11_SCOPED_ENUMS -DBOOST_NO_SCOPED_ENUMS)

	message(STATUS "Using Boost_VERSION: ${Boost_VERSION}")
	message(STATUS "Using Boost_INCLUDE_DIRS: ${Boost_INCLUDE_DIRS}")
	message(STATUS "Using Boost_LIBRARY_DIRS: ${Boost_LIBRARY_DIRS}")

	foreach(component ${BOOST_COMPONENTS})
		list(APPEND ${Boost_LIBRARIES} "Boost::${component}")
	endforeach()
	set(Boost_LIBRARIES ${Boost_LIBRARIES} PARENT_SCOPE)
endfunction(configure_boost)
