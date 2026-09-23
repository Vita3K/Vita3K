// Vita3K emulator project
// Copyright (C) 2026 Vita3K team
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License along
// with this program; if not, write to the Free Software Foundation, Inc.,
// 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

#pragma once

#include <gui-qt/gui_save.h>
#include <gui-qt/gui_settings_base.h>

#include <QString>

namespace gui {

const QString Settings = QStringLiteral("CurrentSettings");

const QString LightStylesheet = QStringLiteral("light");
const QString DarkStylesheet = QStringLiteral("dark");

const QString main_window = QStringLiteral("MainWindow");
const QString apps_list = QStringLiteral("GameList");
const QString logger = QStringLiteral("Logger");
const QString game_window = QStringLiteral("GameWindow");
const QString trophy = QStringLiteral("Trophy");
const QString controls = QStringLiteral("Controls");
const QString settings = QStringLiteral("Settings");
const QString user_mgmt = QStringLiteral("UserManagement");
const QString vita_themes = QStringLiteral("VitaThemes");
const QString meta = QStringLiteral("Meta");

const GuiSave m_currentStylesheet = GuiSave(meta, QStringLiteral("currentStylesheet"), DarkStylesheet);

const GuiSave mw_geometry = GuiSave(main_window, QStringLiteral("geometry"), QByteArray());
const GuiSave mw_windowState = GuiSave(main_window, QStringLiteral("windowState"), QByteArray());
const GuiSave mw_mwState = GuiSave(main_window, QStringLiteral("mwState"), QByteArray());
const GuiSave mw_loggerVisible = GuiSave(main_window, QStringLiteral("loggerVisible"), true);
const GuiSave mw_appsListVisible = GuiSave(main_window, QStringLiteral("gamelistVisible"), true);
const GuiSave mw_toolBarVisible = GuiSave(main_window, QStringLiteral("toolBarVisible"), true);
const GuiSave mw_titleBarsVisible = GuiSave(main_window, QStringLiteral("titleBarsVisible"), true);
const GuiSave mw_confirmExitApp = GuiSave(main_window, QStringLiteral("confirmExitApp"), true);
const GuiSave mw_warnAdminPrivileges = GuiSave(main_window, QStringLiteral("warnAdminPrivileges"), true);

const GuiSave gl_sortCol = GuiSave(apps_list, QStringLiteral("sortCol"), 1);
const GuiSave gl_sortAsc = GuiSave(apps_list, QStringLiteral("sortAsc"), true);
const GuiSave gl_headerState = GuiSave(apps_list, QStringLiteral("headerStateV2"), QByteArray());
const GuiSave gl_visibleColumns = GuiSave(apps_list, QStringLiteral("visibleColumnsV2"), QStringList());
const GuiSave gl_iconSize = GuiSave(apps_list, QStringLiteral("iconSize"), 25);
const GuiSave gl_iconCrop = GuiSave(apps_list, QStringLiteral("iconCrop"), static_cast<int>(0));

const GuiSave l_visible = GuiSave(logger, QStringLiteral("visible"), true);
const GuiSave l_bufferSize = GuiSave(logger, QStringLiteral("bufferSize"), 1000);
const GuiSave l_fontFamily = GuiSave(logger, QStringLiteral("fontFamily"), QString());

const GuiSave gw_geometry = GuiSave(game_window, QStringLiteral("geometry"), QByteArray());
const GuiSave gw_roundedCorners = GuiSave(game_window, QStringLiteral("roundedCorners"), false);

const GuiSave tr_geometry = GuiSave(trophy, QStringLiteral("geometry"), QByteArray());
const GuiSave tr_splitterState = GuiSave(trophy, QStringLiteral("splitterState"), QByteArray());
const GuiSave tr_appHeaderState = GuiSave(trophy, QStringLiteral("appHeaderStateV2"), QByteArray());
const GuiSave tr_trophyHeaderState = GuiSave(trophy, QStringLiteral("trophyHeaderStateV2"), QByteArray());
const GuiSave tr_iconHeight = GuiSave(trophy, QStringLiteral("iconHeight"), 64);
const GuiSave tr_showLocked = GuiSave(trophy, QStringLiteral("showLocked"), true);
const GuiSave tr_showUnlocked = GuiSave(trophy, QStringLiteral("showUnlocked"), true);
const GuiSave tr_showHidden = GuiSave(trophy, QStringLiteral("showHidden"), false);
const GuiSave tr_showBronze = GuiSave(trophy, QStringLiteral("showBronze"), true);
const GuiSave tr_showSilver = GuiSave(trophy, QStringLiteral("showSilver"), true);
const GuiSave tr_showGold = GuiSave(trophy, QStringLiteral("showGold"), true);
const GuiSave tr_showPlatinum = GuiSave(trophy, QStringLiteral("showPlatinum"), true);

const GuiSave cd_geometry = GuiSave(controls, QStringLiteral("geometry"), QByteArray());

const GuiSave sd_geometry = GuiSave(settings, QStringLiteral("geometry"), QByteArray());
const GuiSave sd_lastTab = GuiSave(settings, QStringLiteral("lastTab"), 0);

const GuiSave um_geometry = GuiSave(user_mgmt, QStringLiteral("geometry"), QByteArray());

const GuiSave vt_geometry = GuiSave(vita_themes, QStringLiteral("geometry"), QByteArray());
const GuiSave vt_appliedSelection = GuiSave(vita_themes, QStringLiteral("appliedSelection"), QString());
const GuiSave vt_bgmEnabled = GuiSave(vita_themes, QStringLiteral("bgmEnabled"), false);
const GuiSave vt_bgmVolume = GuiSave(vita_themes, QStringLiteral("bgmVolume"), 50);

} // namespace gui

class GuiSettings : public GuiSettingsBase {
    Q_OBJECT

public:
    explicit GuiSettings(const QString &settings_dir, const fs::path &static_themes_path, QObject *parent = nullptr);
};
