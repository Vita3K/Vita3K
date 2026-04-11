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

// taiHEN plugin configuration file parser for Vita3K.
//
// The taiHEN configuration file (typically at ux0:tai/config.txt or
// ur0:tai/config.txt) lists kernel and user-mode plugins to load at
// application startup. Its format is:
//
//   # Lines beginning with '#' are comments.
//
//   *KERNEL
//   ur0:tai/kubridge.skprx   <- kernel plugin (applies to all processes)
//
//   *ALL
//   ux0:tai/some_plugin.suprx  <- user plugin for ALL titles
//
//   PCSX00000                <- Title ID section
//   ux0:tai/game_plugin.suprx  <- plugin only for PCSX00000
//
// Section headers (*KERNEL, *ALL, or a title ID) introduce a new section.
// Plugin paths that follow belong to that section until the next header.

#include <string>
#include <unordered_map>
#include <vector>

namespace tai {

/// Parsed representation of a taiHEN config file.
struct PluginConfig {
    /// Plugins listed under the `*KERNEL` section.
    /// These are kernel plugins that should be "loaded" at startup.
    /// Vita3K cannot execute real kernel code, so only HLE-backed plugins
    /// (e.g. kubridge.skprx) will be usable.
    std::vector<std::string> kernel_plugins;

    /// Plugins listed under the `*ALL` section.
    /// These user-mode plugins are loaded for every title.
    std::vector<std::string> all_plugins;

    /// Plugins listed under per-title-ID sections.
    /// Key: uppercase title ID string (e.g. "PCSX00000").
    /// Value: list of plugin paths for that title.
    std::unordered_map<std::string, std::vector<std::string>> title_plugins;
};

/// Parse the contents of a taiHEN config file.
/// \param content  The full text of the config file (may use CR/LF or LF).
/// \return Parsed PluginConfig.  Empty structs are returned for sections that
///         do not appear in the file.
PluginConfig parse(const std::string &content);

/// Collect the ordered list of user-mode plugins that should be loaded for
/// the given title.  *ALL plugins come first, then title-specific ones.
/// \param config    Parsed config returned by parse().
/// \param title_id  Title ID of the running game (e.g. "PCSX00000").
/// \return Ordered vector of plugin path strings.
std::vector<std::string> plugins_for_title(const PluginConfig &config, const std::string &title_id);

} // namespace tai
