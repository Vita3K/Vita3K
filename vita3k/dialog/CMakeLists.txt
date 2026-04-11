add_library(
    dialog
    INTERFACE
)

target_include_directories(dialog INTERFACE include)
target_link_libraries(dialog INTERFACE lang io)
