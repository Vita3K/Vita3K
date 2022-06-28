add_library(package STATIC
            src/license.cpp
            src/pkg.cpp
            src/pup.cpp
            src/sce_utils.cpp
            src/sfo.cpp
            include/package/functions.h
            include/package/pkg.h
            include/package/sce_types.h
            include/package/sfo.h
)
target_include_directories(package PUBLIC include)

target_link_libraries(package PUBLIC host miniz crypto FAT16 vita-toolchain psvpfsparser io)
