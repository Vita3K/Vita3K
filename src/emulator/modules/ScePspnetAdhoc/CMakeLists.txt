add_library(ScePspnetAdhoc STATIC include/ScePspnetAdhoc/exports.h src/ScePspnetAdhoc.cpp)
target_include_directories(ScePspnetAdhoc PUBLIC include)
target_link_libraries(ScePspnetAdhoc PRIVATE module)
