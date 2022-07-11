add_library(packages STATIC
            src/license.cpp
            src/pkg.cpp
            src/pup.cpp
            src/sce_utils.cpp
            src/sfo.cpp
            include/packages/functions.h
            include/packages/pkg.h
            include/packages/sce_types.h
            include/packages/sfo.h
)
target_include_directories(packages PUBLIC include)

target_link_libraries(packages PUBLIC emuenv miniz crypto FAT16 vita-toolchain psvpfsparser io)
