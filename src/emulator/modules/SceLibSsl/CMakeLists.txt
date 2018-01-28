add_library(SceLibSsl STATIC include/SceLibSsl/exports.h src/SceSsl.cpp)
target_include_directories(SceLibSsl PUBLIC include)
target_link_libraries(SceLibSsl PRIVATE module)
