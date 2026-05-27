add_library(gui-qt STATIC
    src/gui_application.cpp
    src/main_window.cpp
    src/about_dialog.cpp
    src/archive_install_dialog.cpp
    src/controls_dialog.cpp
    src/ctrl_keyboard_filter.cpp
    src/debug_widget.cpp
    src/firmware_install_dialog.cpp
    src/game_compatibility.cpp
    src/apps_list.cpp
    src/apps_list_context_menu.cpp
    src/apps_list_delegate.cpp
    src/apps_list_table.cpp
    src/game_window.cpp
    src/gui_settings.cpp
    src/gui_settings_base.cpp
    src/ime_keyboard_filter.cpp
    src/license_install_dialog.cpp
    src/live_area_widget.cpp
    src/log_widget.cpp
    src/persistent_settings.cpp
    src/pkg_install_dialog.cpp
    src/physical_key_qt.cpp
    src/qt_utils.cpp
    src/settings_dialog.cpp
    src/settings_dialog_tooltips.cpp
    src/theme_manager.cpp
    src/theme_surface.cpp
    src/vita_theme.cpp
    src/vita_themes_dialog.cpp
    src/trophy_collection_dialog.cpp
    src/update_manager.cpp
    src/gui_language.cpp
    src/user_management_dialog.cpp
    src/welcome_dialog.cpp

    src/main_window.ui
    src/about_dialog.ui
    src/controls_dialog.ui
    src/settings_dialog.ui
    src/welcome_dialog.ui

    include/gui-qt/gui_application.h
    include/gui-qt/main_window.h
    include/gui-qt/about_dialog.h
    include/gui-qt/archive_install_dialog.h
    include/gui-qt/controls_dialog.h
    include/gui-qt/ctrl_keyboard_filter.h
    include/gui-qt/custom_dock_widget.h
    include/gui-qt/debug_widget.h
    include/gui-qt/firmware_install_dialog.h
    include/gui-qt/game_compatibility.h
    include/gui-qt/app_icon_item.h
    include/gui-qt/apps_list.h
    include/gui-qt/apps_list_columns.h
    include/gui-qt/apps_list_context_menu.h
    include/gui-qt/apps_list_delegate.h
    include/gui-qt/apps_list_table.h
    include/gui-qt/game_window.h
    include/gui-qt/gui_save.h
    include/gui-qt/gui_settings.h
    include/gui-qt/gui_settings_base.h
    include/gui-qt/ime_keyboard_filter.h
    include/gui-qt/license_install_dialog.h
    include/gui-qt/live_area_widget.h
    include/gui-qt/log_widget.h
    include/gui-qt/persistent_settings.h
    include/gui-qt/pkg_install_dialog.h
    include/gui-qt/physical_key_qt.h
    include/gui-qt/qt_utils.h
    include/gui-qt/settings_dialog.h
    include/gui-qt/settings_dialog_tooltips.h
    include/gui-qt/theme_manager.h
    include/gui-qt/theme_surface.h
    include/gui-qt/vita_theme.h
    include/gui-qt/vita_themes_dialog.h
    include/gui-qt/trophy_collection_dialog.h
    include/gui-qt/update_manager.h
    include/gui-qt/gui_language.h
    include/gui-qt/user_management_dialog.h
    include/gui-qt/welcome_dialog.h

    "../resources.qrc"
)

set_target_properties(gui-qt PROPERTIES
    AUTOMOC ON
    AUTOUIC ON
    AUTORCC ON
)

target_include_directories(gui-qt
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)
target_link_libraries(gui-qt PUBLIC
    vita3k::qt6

    config
    ctrl
    dialog
    emuenv
    app
    compat
    io
    np
    kernel
    motion
    packages
    renderer
    touch
    gxm
    audio
    ime

    miniz
    libzRIF
    psvpfsparser
    pugixml::pugixml
    cppcommon
    updater
    input
)

target_include_directories(gui-qt PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/..)
