add_library(host_dialog STATIC
    src/filesystem.cpp
)

# Create alias for more friendly naming and avoid conflicts
# in global CMake scope
add_library(host::dialog ALIAS host_dialog)

target_include_directories(host_dialog PUBLIC include)

target_link_libraries(host_dialog PRIVATE NFDe::NFDe)
