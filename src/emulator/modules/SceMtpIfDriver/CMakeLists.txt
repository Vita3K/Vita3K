add_library(SceMtpIfDriver STATIC include/SceMtpIfDriver/exports.h src/SceMtpIf.cpp)
target_include_directories(SceMtpIfDriver PUBLIC include)
target_link_libraries(SceMtpIfDriver PRIVATE module)
