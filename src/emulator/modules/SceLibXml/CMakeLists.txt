add_library(SceLibXml STATIC include/SceLibXml/exports.h src/SceLibXml.cpp)
target_include_directories(SceLibXml PUBLIC include)
target_link_libraries(SceLibXml PRIVATE module)
