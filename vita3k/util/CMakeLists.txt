add_library(
	util
	STATIC
	src/android_driver.cpp
	src/arm.cpp
	src/float_to_half.cpp
	src/fs_utils.cpp
	src/hash.cpp
	src/instrset_detect.cpp
	src/logging.cpp
	src/net_utils.cpp
	src/string_utils.cpp
	src/sysinfo.cpp
	src/tracy.cpp
	src/vita_theme_utils.cpp
)

# vc_runtime_checker.cpp is directly added from the main CMakeList (for some reason adding it here doesn't work)

target_include_directories(util PUBLIC include)
target_link_libraries(util PUBLIC fmt spdlog http mem)
target_link_libraries(util PRIVATE libcurl crypto SDL3::SDL3)
if(ANDROID)
	target_link_libraries(util PUBLIC emuenv android xxHash::xxhash)
	target_link_libraries(util PRIVATE adrenotools)
endif()
target_compile_definitions(util PRIVATE $<$<CONFIG:Debug,RelWithDebInfo>:TRACY_ENABLE>)

if(NOT ANDROID)
    find_package(Python3 3.8 QUIET COMPONENTS Interpreter)
    if(Python3_Interpreter_FOUND)
        find_package(Threads REQUIRED)
        set(TERMINATE_TEST_SOURCE ${CMAKE_CURRENT_BINARY_DIR}/terminate_test.cpp)
        add_custom_command(OUTPUT ${TERMINATE_TEST_SOURCE}
            COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/generate_terminate_test.py
                --source ${CMAKE_CURRENT_SOURCE_DIR}/src/logging.cpp --output ${TERMINATE_TEST_SOURCE}
            DEPENDS tests/generate_terminate_test.py src/logging.cpp
            VERBATIM)
        add_executable(util-terminate-tests ${TERMINATE_TEST_SOURCE})
        target_link_libraries(util-terminate-tests PRIVATE Threads::Threads)
        add_test(NAME util-terminate-handler
            COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_terminate_tests.py $<TARGET_FILE:util-terminate-tests>)
    endif()
endif()
