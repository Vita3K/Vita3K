add_library(
	vkutil
	STATIC
	include/vkutil/native_surface.h
	include/vkutil/objects.h
	include/vkutil/vkutil.h

	src/readback.cpp
	src/objects.cpp
	src/vkutil.cpp
)

target_include_directories(vkutil PUBLIC include)
target_link_libraries(vkutil PUBLIC vulkan vma)
target_link_libraries(vkutil PRIVATE util)

option(VITA3K_SURFACE_READBACK_TESTS "Build offscreen Vulkan/guest-memory readback fixture" OFF)
if(VITA3K_SURFACE_READBACK_TESTS)
  add_executable(surface-readback-tests tests/readback_tests.cpp)
  target_link_libraries(surface-readback-tests PRIVATE renderer vkutil mem)
  add_executable(readback-cpu-tests tests/readback_cpu_tests.cpp)
  target_link_libraries(readback-cpu-tests PRIVATE vkutil mem)
  add_test(NAME readback-cpu COMMAND readback-cpu-tests)
  if(APPLE)
    add_executable(readback-safety-tests ../mem/tests/gpu_readback_tests.cpp)
    target_link_libraries(readback-safety-tests PRIVATE mem)
    add_test(NAME readback-safety COMMAND readback-safety-tests)
  endif()
endif()
