add_executable(updater updater.cpp)
set_target_properties(updater PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
target_sources(updater PRIVATE resource.h updater.ico updater.rc)
target_link_libraries(updater PRIVATE boost util)
