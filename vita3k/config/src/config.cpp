// Vita3K emulator project
// Copyright (C) 2025 Vita3K team
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

#include <config/functions.h>
#include <config/state.h>
#include <config/version.h>
#include <yaml-cpp/yaml.h>

#include <util/fs.h>
#include <util/log.h>
#include <util/string_utils.h>
#ifdef TRACY_ENABLE
#include <util/tracy_module_utils.h>
#endif
#include <util/vector_utils.h>

#include <CLI11.hpp>

#include <algorithm>
#include <exception>
#include <iostream>
#include <vector>

namespace config {
// Update the members of the Config object based on the YAML node.
// Use this function when the YAML file is updated before the members.
static void update_members(Config &self, const YAML::Node &p_yaml_node) {
#define UPDATE_MEMBERS(option_type, option_name, option_default, member_name) \
    if (p_yaml_node[option_name].IsDefined()) {                               \
        self.member_name = p_yaml_node[option_name].as<option_type>();        \
    } else {                                                                  \
        self.member_name = option_default;                                    \
    }

    CONFIG_LIST(UPDATE_MEMBERS)
#undef UPDATE_MEMBERS
#ifdef TRACY_ENABLE
    tracy_module_utils::cleanup(self.tracy_advanced_profiling_modules);
    tracy_module_utils::load_from(self.tracy_advanced_profiling_modules);
#endif
}

static void update_members(Config &self, const Config &rhs) {
#define UPDATE_MEMBERS(option_type, option_name, option_default, member_name) \
    self.member_name = rhs.member_name;

    CONFIG_LIST(UPDATE_MEMBERS)
#undef UPDATE_MEMBERS
#ifdef TRACY_ENABLE
    tracy_module_utils::cleanup(self.tracy_advanced_profiling_modules);
    tracy_module_utils::load_from(self.tracy_advanced_profiling_modules);
#endif
}

// Perform comparisons with optional settings
static void check_members(Config &self, const Config &rhs) {
    if (rhs.content_path.has_value())
        self.content_path = rhs.content_path;
    if (rhs.run_app_path.has_value())
        self.run_app_path = rhs.run_app_path;
    if (rhs.recompile_shader_path.has_value())
        self.recompile_shader_path = rhs.recompile_shader_path;
    if (rhs.delete_title_id.has_value())
        self.delete_title_id = rhs.delete_title_id;
    if (rhs.pkg_path.has_value())
        self.pkg_path = rhs.pkg_path;
    if (rhs.pkg_zrif.has_value())
        self.pkg_zrif = rhs.pkg_zrif;

    if (!rhs.config_path.empty())
        self.config_path = rhs.config_path;

    self.overwrite_config = rhs.overwrite_config;
    self.load_config = rhs.load_config;
    self.fullscreen = rhs.fullscreen;
    self.console = rhs.console;
    self.app_args = rhs.app_args;
    self.load_app_list = rhs.load_app_list;
    self.self_path = rhs.self_path;
}

static YAML::Node get(const Config &self);

// Merge two Config nodes, respecting both options and preferring the left-hand side
static void merge(Config &self, const Config &rhs) {
    bool init = false;
    if (get(rhs) == get(Config{})) {
        init = true;
    }

    // if (log_imports != rhs.log_imports && (init || rhs.log_imports != false))
#define COMBINE_INDIVIDUAL(option_type, option_name, option_default, member_name)           \
    if (self.member_name != rhs.member_name && (init || rhs.member_name != option_default)) \
        self.member_name = rhs.member_name;

    CONFIG_INDIVIDUAL(COMBINE_INDIVIDUAL)
#undef COMBINE_INDIVIDUAL

#define COMBINE_VECTOR(option_type, option_name, option_default, member_name)      \
    if (self.member_name != rhs.member_name && (init || !rhs.member_name.empty())) \
        vector_utils::merge_vectors(self.member_name, rhs.member_name);

    CONFIG_VECTOR(COMBINE_VECTOR)
#undef COMBINE_VECTOR

    check_members(self, rhs);
}

// Generate a YAML node based on the current values of the members.
static YAML::Node get(const Config &self) {
    YAML::Node out;

#define GEN_VALUES(option_type, option_name, option_default, member_name) \
    out[option_name] = self.member_name;

    CONFIG_LIST(GEN_VALUES)
#undef GEN_VALUES

    return out;
}

// Load a function to the node network, and then update the members
static void load_new_config(Config &self, const fs::path &path) {
    fs::ifstream fin(path);
    YAML::Node yaml_node = YAML::Load(fin);
    update_members(self, yaml_node);
}

static std::set<std::string> get_file_set(const fs::path &loc, bool dirs_only = true) {
    std::set<std::string> cur_set{};
    if (!fs::exists(loc)) {
        return cur_set;
    }

    fs::directory_iterator it{ loc };
    while (it != fs::directory_iterator{}) {
        if (dirs_only) {
            if (!it->path().empty() && fs::is_directory(it->path())) {
                cur_set.insert(it->path().stem().string());
            }
        } else {
            cur_set.insert(it->path().stem().string());
        }

        boost::system::error_code err{};
        it.increment(err);
    }
    return cur_set;
}

static fs::path check_path(const fs::path &output_path) {
    if (output_path.filename() != "config.yml") {
        if (!output_path.has_extension()) // assume it is a folder
            return output_path / "config.yml";
        if (output_path.extension() != ".yml")
            return "";
    }
    return output_path;
}

static ExitCode parse(Config &cfg, const fs::path &load_path, const fs::path &root_pref_path) {
    const auto loaded_path = check_path(load_path);
    if (loaded_path.empty() || !fs::exists(loaded_path)) {
        LOG_ERROR("Config file input path invalid (did you name the extension \".yml\"?)");
        return FileNotFound;
    }

    try {
        load_new_config(cfg, loaded_path);
    } catch (YAML::Exception &exception) {
        LOG_ERROR("Config file can't be loaded: Error: {}", exception.what());
        return FileNotFound;
    }

    if (cfg.pref_path.empty())
        cfg.set_pref_path(root_pref_path);
    else {
        if (cfg.get_pref_path() != root_pref_path && !fs::exists(cfg.get_pref_path())) {
            LOG_ERROR("Cannot find preference path: {}", cfg.pref_path);
            return InvalidApplicationPath;
        }
    }

    return Success;
}

ExitCode serialize_config(Config &cfg, const fs::path &output_path) {
    const auto output = check_path(output_path);
    if (output.empty())
        return InvalidApplicationPath;
    fs::create_directories(output.parent_path());

    auto out_node = get(cfg);

    YAML::Emitter emitter;
    emitter << YAML::BeginDoc;
    emitter << out_node;
    emitter << YAML::EndDoc;

    fs::ofstream fo(output);
    if (!fo) {
        return InvalidApplicationPath;
    }

    fo << emitter.c_str();
    fo.close();

    return Success;
}

ExitCode init_config(Config &cfg, int argc, char **argv, const Root &root_paths) {
    // Always generate the default configuration file
    Config command_line{};
    // Load config path configuration by default; otherwise, move the default to the config path
    if (fs::exists(check_path(root_paths.get_config_path()))) {
        parse(cfg, root_paths.get_config_path(), root_paths.get_pref_path());
    } else {
        serialize_config(command_line, check_path(root_paths.get_config_path()));
    }

    // Declare all options
    CLI::App app{ "Vita3K Command Line Interface" }; // "--help,-h" is automatically generated
    app.allow_windows_style_options();
    app.allow_extras();
    app.enabled_by_default();
    app.get_formatter()->column_width(38);

    // clang-format off
    const auto ver = app.add_flag("--version,-v", "Print the version string")
        ->group("Options");

    // Positional options
    app.add_option("content-path", command_line.content_path, "Path to the app with a .vpk/.zip extension or folder of content to install & run")
        ->default_str({});

    // Grouped options
    auto input = app.add_option_group("Input", "Special options for Vita3K");
    input->add_flag("--console,-z", command_line.console, "Start the emulator in console mode.")
       ->default_val(false)->group("Input");
    input->add_option("--app-args,-Z", command_line.app_args, "Argument for app, use ', ' to separate arguments.")
        ->default_str("")->group("Input");
    input->add_option("--load-app-list,-a", command_line.load_app_list, "Starts the emulator with load app list.")
       ->default_val(false)->group("Input");
    input->add_option("--self,-S", command_line.self_path, "Path to the self to run inside Title ID")
        ->default_str("eboot.bin")->group("Input");
    input->add_option("--installed-path,-r", command_line.run_app_path, "Path to the installed app to run")
        ->default_str({})->group("Input");
    input->add_option("--recompile-shader,-s", command_line.recompile_shader_path, "Recompile the given PS Vita shader (GXP format) to SPIR_V / GLSL and quit")
        ->default_str({})->group("Input");
    input->add_option("--deleted-id,-d", command_line.delete_title_id, "Title ID of installed app to delete")
        ->default_str({})->check(CLI::IsMember(get_file_set(cfg.get_pref_path() / "ux0/app")))->group("Input");
    input->add_option("--firmware", command_line.pup_path, "Path to the firmware file (.pup extension) to install");
    auto input_pkg = input->add_option("--pkg", command_line.pkg_path, "Path to the app file (.pkg extension) to install")
        ->default_str({})->group("Input");
    auto input_zrif = input->add_option("--zrif", command_line.pkg_zrif, "zrif to decode the app (base64 format)")
        ->default_str({})->group("Input");
    input_pkg->needs(input_zrif);
    input_zrif->needs(input_pkg);

    auto config = app.add_option_group("Configuration", "Modify Vita3K's config.yml file");
    config->add_flag("--archive-log,-A", command_line.archive_log, "Make a duplicate of the log file with TITLE_ID and Game ID as title")
        ->group("Logging");
    config->add_option("--backend-renderer,-B", command_line.backend_renderer, "Renderer backend to use")
        ->ignore_case()->check(CLI::IsMember(std::set<std::string>{ "OpenGL", "Vulkan" }))->group("Vita Emulation");
    config->add_flag("--color-surface-debug,-C", command_line.color_surface_debug, "Save color surfaces")
        ->group("Vita Emulation");
    config->add_option("--config-location,-c", command_line.config_path, "Get a configuration file from a given location. If a filename is given, it must end with \".yml\", otherwise it will be assumed to be a directory. \nDefault loaded: <Vita3K>/config.yml \nDefaults: <Vita3K>/data/config/default.yml")
        ->group("YML");
    config->add_flag("!--keep-config,!-w", command_line.overwrite_config, "Do not modify the configuration file after loading.")
        ->group("YML");
    config->add_flag("--load-config,-f", command_line.load_config, "Load a configuration file. Setting --keep-config with this option preserves the configuration file.")
        ->group("YML");
    config->add_flag("--fullscreen,-F", command_line.fullscreen, "Start the emulator in fullscreen mode.")
        ->group("YML");

    std::vector<std::string> lle_modules{};
    config->add_option("--lle-modules,-m", lle_modules, "Load given (decrypted) OS modules from disk.\nSeparate by commas to specify multiple modules. Full path and extension should not be included, the following are assumed: vs0:sys/external/<name>.suprx\nExample: --lle-modules libscemp4,libngs")
        ->group("Modules");
    config->add_option("--log-level,-l", command_line.log_level, "Logging level:\nTRACE = 0\nDEBUG = 1\nINFO = 2\nWARN = 3\nERROR = 4\nCRITICAL = 5\nOFF = 6")
        ->check(CLI::Range( 0, 6 ))->group("Logging");
    config->add_flag("--log-active-shaders,-S", command_line.log_active_shaders, "Log Active Shaders")
        ->group("Logging");
    config->add_flag("--log-uniforms,-U", command_line.log_uniforms, "Log Uniforms")
        ->group("Logging");
    // clang-format on

    // Parse the inputs
    try {
        app.parse(argc, argv);
    } catch (CLI::ParseError &e) {
        if (e.get_exit_code() != 0) {
            std::cout << "CLI parsing error: " << e.what() << "\n";
            return InitConfigFailed;
        }
    }

    if (!app.get_help_ptr()->empty()) {
        std::cout << app.help() << std::endl;
        return QuitRequested;
    }
    if (!ver->empty()) {
        std::cout << window_title << std::endl;
        return QuitRequested;
    }
    if (command_line.run_app_path.has_value()) {
        std::string &app_path = *command_line.run_app_path;
        if (!fs::path(app_path).has_parent_path())
            app_path.insert(0, "ux0:app/");
        std::string app_parrent_path = fs::path(app_path).parent_path().string();
        string_utils::replace(app_parrent_path, ":", "/");
        std::set<std::string> exist_apps = get_file_set(fs::path(cfg.pref_path) / app_parrent_path);
        if (exist_apps.find(fs::path(app_path).stem().string()) == exist_apps.end()) {
            std::cout << "--installed-path: " << app_path << " no in {";
            for (auto &app : exist_apps) {
                std::cout << app;
                if (app != *exist_apps.rbegin())
                    std::cout << ", ";
            }
            std::cout << "}" << std::endl;
            return InitConfigFailed;
        }
    }
    if (command_line.recompile_shader_path.has_value()) {
        cfg.recompile_shader_path = std::move(command_line.recompile_shader_path);
        return QuitRequested;
    }
    if (command_line.delete_title_id.has_value()) {
        cfg.delete_title_id = std::move(command_line.delete_title_id);
        return QuitRequested;
    }
    if (command_line.pup_path.has_value()) {
        cfg.pup_path = std::move(command_line.pup_path);
        return QuitRequested;
    }
    if (command_line.pkg_path.has_value() && command_line.pkg_zrif.has_value()) {
        cfg.pkg_path = std::move(command_line.pkg_path);
        cfg.pkg_zrif = std::move(command_line.pkg_zrif);
        return QuitRequested;
    }
    if (command_line.load_config || command_line.config_path != root_paths.get_config_path()) {
        if (command_line.config_path.empty()) {
            command_line.config_path = root_paths.get_config_path();
        } else {
            if (parse(command_line, command_line.config_path, root_paths.get_pref_path()) != Success)
                return InitConfigFailed;
        }
    }

    if (cfg.console && (cfg.run_app_path || !cfg.content_path)) {
        LOG_ERROR("Command line version only supports .vpk for now.");
        return InitConfigFailed;
    }

    // Get LLE modules from the command line, otherwise get the modules from the YML file
    if (!lle_modules.empty()) {
        if (command_line.load_config) {
            command_line.lle_modules = vector_utils::merge_vectors(command_line.lle_modules, lle_modules);
        } else {
            command_line.lle_modules = std::move(lle_modules);
        }
    }

    // Merge configurations
    merge(cfg, command_line);
    if (cfg.pref_path.empty())
        cfg.set_pref_path(root_paths.get_pref_path());

    if (!cfg.console) {
        LOG_INFO_IF(cfg.load_config, "Custom configuration file loaded successfully.");

        logging::set_level(static_cast<spdlog::level::level_enum>(cfg.log_level));
        static constexpr std::array LIST_LOG_LEVEL = SPDLOG_LEVEL_NAMES;

        LOG_INFO_IF(cfg.content_path, "input-content-path: {}", cfg.content_path->string());
        LOG_INFO_IF(cfg.run_app_path, "input-installed-path: {}", *cfg.run_app_path);
        LOG_INFO("log-level: {}", LIST_LOG_LEVEL[cfg.log_level]);
        LOG_INFO_IF(cfg.log_active_shaders, "log-active-shaders: enabled");
        LOG_INFO_IF(cfg.log_uniforms, "log-uniforms: enabled");
    }
    // Save any changes made in command-line arguments
    if (cfg.overwrite_config || !fs::exists(check_path(cfg.config_path))) {
        if (serialize_config(cfg, cfg.config_path) != Success)
            return InitConfigFailed;
    }

    return Success;
}

} // namespace config

Config &Config::operator=(Config &&rhs) noexcept {
    config::check_members(*this, rhs);
    config::update_members(*this, rhs);
    return *this;
}

Config &Config::operator=(const Config &rhs) noexcept {
    if (this != &rhs) {
        config::check_members(*this, rhs);
        config::update_members(*this, rhs);
    }
    return *this;
}
