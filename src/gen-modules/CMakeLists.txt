set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(gen-modules gen-modules.cpp)
target_link_libraries(gen-modules PRIVATE yaml)
add_dependencies(gen-modules vita-headers)
