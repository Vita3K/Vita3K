#[[
    This CMake script is intended to be run by a build system using CMake in script
    mode when compiling a project (build time) and allows to copy a shared library to another
    directory knowing the current build type, following the rules for location of
    files for imported libraries (https://cmake.org/cmake/help/latest/prop_tgt/IMPORTED_LOCATION.html).
    However, it can also be used to copy any file at build time.

    This script requires CMake 3.15 or greater because of the use of FOLLOW_SYMLINK_CHAIN
    in file(COPY ...).

    This needs to be done in a separate script than the main CMake script because
    it only gets run at configure time, but the build type is only known by the
    build system in use when compiling the project.

    CMake variables to specify when calling the script:
        - LOCATION: Location of the library. May be superseded by the LOCATION_<CONFIG> if it's specified.
        - DESTINATION: Destination directory in which the library should be copied.
        - BUILD_TYPE: Optional argument. Build type name (upper-case) being used to build the project. Can be an empty string.
        - LOCATION_<CONFIG>: Optional argument. Configuration-specific location of the shared library. Can be an empty string.
#]]

cmake_minimum_required(VERSION 3.15)

# Ensure necessary variables exist within the script
if(NOT DEFINED BUILD_TYPE)
    set(BUILD_TYPE "")
endif()
if(NOT DEFINED LOCATION)
    message(ERROR "LOCATION wasn't defined.")
endif()
if(NOT DEFINED DESTINATION)
    message(ERROR "DESTINATION wasn't defined.")
endif()

if(BUILD_TYPE NOT STREQUAL "")
    # Ensure definition of LOCATION_${BUILD_TYPE}
    if(NOT DEFINED LOCATION_${BUILD_TYPE})
        set(LOCATION_${BUILD_TYPE} "")
    endif()

    # Decide which location to use as the origin
    if(LOCATION_${BUILD_TYPE} STREQUAL "")
        # If the configuration-specific location isn't specified, then use the
        # configuration-agnostic location
        set(ORIGIN ${LOCATION})
    else()
        # If the configuration-specific location is specified, use it
        set(ORIGIN ${LOCATION_${BUILD_TYPE}})
    endif()
else()
    set(ORIGIN ${LOCATION})
endif()

# Copy library file
file(COPY ${ORIGIN} DESTINATION ${DESTINATION} FOLLOW_SYMLINK_CHAIN)
