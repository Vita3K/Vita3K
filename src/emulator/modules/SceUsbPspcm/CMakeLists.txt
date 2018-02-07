add_library(SceUsbPspcm STATIC include/SceUsbPspcm/exports.h src/SceUsbPspcm.cpp)
target_include_directories(SceUsbPspcm PUBLIC include)
target_link_libraries(SceUsbPspcm PRIVATE module)
