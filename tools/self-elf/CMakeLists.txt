add_executable(vita3k-self-elf EXCLUDE_FROM_ALL main.cpp)
target_include_directories(vita3k-self-elf PRIVATE ${Boost_INCLUDE_DIRS})
target_link_libraries(vita3k-self-elf PRIVATE packages vita-toolchain)
set_target_properties(vita3k-self-elf PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
