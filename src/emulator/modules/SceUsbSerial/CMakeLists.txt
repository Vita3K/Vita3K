add_library(SceUsbSerial STATIC include/SceUsbSerial/exports.h src/SceUsbSerial.cpp)
target_include_directories(SceUsbSerial PUBLIC include)
target_link_libraries(SceUsbSerial PRIVATE module)
