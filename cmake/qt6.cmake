# Qt6 discovery and configuration for Vita3K
# Provides the vita3k::qt6 INTERFACE target.
add_library(vita3k_qt6 INTERFACE)

set(VITA3K_QT_MIN_VER 6.7.0)

set(VITA3K_QT_COMPONENTS Core Gui Widgets Network Concurrent Svg LinguistTools)
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
	list(APPEND VITA3K_QT_COMPONENTS GuiPrivate)
endif()

find_package(Qt6 ${VITA3K_QT_MIN_VER} CONFIG COMPONENTS ${VITA3K_QT_COMPONENTS})

if(Qt6Widgets_FOUND)
	if(Qt6Widgets_VERSION VERSION_LESS ${VITA3K_QT_MIN_VER})
		message(FATAL_ERROR "Minimum supported Qt version is ${VITA3K_QT_MIN_VER}! "
			"You have version ${Qt6Widgets_VERSION}, please upgrade.")
	endif()
else()
	message("CMake was unable to find Qt6!")
	if(WIN32)
		message(FATAL_ERROR "Make sure the Qt6_ROOT environment variable has been set properly.\n"
			"Example: Qt6_ROOT=C:\\Qt\\${VITA3K_QT_MIN_VER}\\msvc2022_64\\")
	elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
		message(FATAL_ERROR "Make sure to install your distro's Qt6 development packages!")
	elseif(APPLE)
		message(FATAL_ERROR "Make sure to install Qt6 development packages.\n"
			"Set Qt6_ROOT to the installation path if CMake cannot find it.")
	else()
		message(FATAL_ERROR "You need Qt6 ${VITA3K_QT_MIN_VER} or later installed.\n"
			"Visit https://www.qt.io/download-open-source/ for instructions.")
	endif()
endif()

target_link_libraries(vita3k_qt6 INTERFACE
	Qt6::Core
	Qt6::Gui
	Qt6::Widgets
	Qt6::Network
	Qt6::Concurrent
	Qt6::Svg
)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND TARGET Qt6::GuiPrivate)
	target_link_libraries(vita3k_qt6 INTERFACE Qt6::GuiPrivate)
endif()

if(WIN32)
	target_compile_definitions(vita3k_qt6 INTERFACE WIN32_LEAN_AND_MEAN)
endif()

add_library(vita3k::qt6 ALIAS vita3k_qt6)

get_target_property(_qt6_qmake_executable Qt6::qmake IMPORTED_LOCATION)
get_filename_component(_qt6_bin_dir "${_qt6_qmake_executable}" DIRECTORY)

if(WIN32)
	find_program(WINDEPLOYQT_EXECUTABLE NAMES windeployqt6 windeployqt HINTS "${_qt6_bin_dir}" NO_DEFAULT_PATH)
	if(WINDEPLOYQT_EXECUTABLE)
		message(STATUS "Found windeployqt: ${WINDEPLOYQT_EXECUTABLE}")
	else()
		message(WARNING "windeployqt not found.")
	endif()
elseif(APPLE)
	find_program(MACDEPLOYQT_EXECUTABLE NAMES macdeployqt HINTS "${_qt6_bin_dir}" NO_DEFAULT_PATH)
	if(MACDEPLOYQT_EXECUTABLE)
		message(STATUS "Found macdeployqt: ${MACDEPLOYQT_EXECUTABLE}")
	else()
		message(WARNING "macdeployqt not found.")
	endif()
endif()
