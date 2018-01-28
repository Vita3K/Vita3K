add_library(SceCoredump STATIC include/SceCoredump/exports.h src/SceCoredump.cpp src/SceCoredumpNounlink.cpp)
target_include_directories(SceCoredump PUBLIC include)
target_link_libraries(SceCoredump PRIVATE module)
