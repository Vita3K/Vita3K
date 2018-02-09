add_library(SceSsl STATIC include/SceSsl/exports.h src/SceSsl.cpp)
target_include_directories(SceSsl PUBLIC include)
target_link_libraries(SceSsl PRIVATE module)
