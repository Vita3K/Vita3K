// Vita3K emulator project
// Copyright (C) 2018 Vita3K team
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

#include <config/config.h>
#include <config/version.h>

#include <util/log.h>
#include <util/string_utils.h>
#include <util/vector_utils.h>

#include <boost/optional/optional_io.hpp>
#include <boost/program_options.hpp>
#include <psp2/system_param.h>
#include <spdlog/spdlog.h>
#include <yaml-cpp/yaml.h>

#include <exception>
#include <iostream>

namespace po = boost::program_options;

namespace app {

template <typename T, typename Q = T>
static void get_yaml_value(YAML::Node &config_node, const char *key, T *target_val, Q default_val) {
    try {
        *target_val = config_node[key].as<Q>();
    } catch (...) {
        *target_val = std::move(default_val);
    }
}

template <typename T>
static void get_yaml_value_optional(YAML::Node &config_node, const char *key, boost::optional<T> *target_val, T default_val) {
    try {
        *target_val = config_node[key].as<T>();
    } catch (...) {
        *target_val = std::move(default_val);
    }
}

template <typename T>
static void get_yaml_value_optional(YAML::Node &config_node, const char *key, boost::optional<T> *target_val) {
    try {
        *target_val = config_node[key].as<T>();
    } catch (...) {
        *target_val = boost::none;
    }
}

template <typename T>
static void config_file_emit_single(YAML::Emitter &emitter, const char *name, T &val) {
    emitter << YAML::Key << name << YAML::Value << val;
}

template <typename T>
static void config_file_emit_optional_single(YAML::Emitter &emitter, const char *name, boost::optional<T> &val) {
    if (val) {
        config_file_emit_single(emitter, name, val.value());
    }
}

template <typename T>
static void config_file_emit_vector(YAML::Emitter &emitter, const char *name, std::vector<T> &values) {
    emitter << YAML::Key << name << YAML::BeginSeq;

    for (const T &value : values) {
        emitter << value;
    }

    emitter << YAML::EndSeq;
}

static fs::path check_path(fs::path output_path) {
    if (output_path.filename() != "config.yml") {
        if (!output_path.has_extension()) // assume it is a folder
            output_path /= "config.yml";
        else if (output_path.extension() != ".yml")
            return "";
    }
    return output_path;
}

static ExitCode parse(Config &cfg, fs::path load_path, const std::string &root_pref_path) {
    load_path = check_path(load_path);
    if (load_path.empty() || !fs::exists(load_path)) {
        LOG_ERROR("Config file input path invalid (did you make sure to name the extension \".yml\"?)");
        return FileNotFound;
    }

    YAML::Node config_node;
    try {
        config_node = YAML::LoadFile(load_path.generic_path().string());
    } catch (YAML::Exception &exception) {
        std::cerr << "Config file can't be loaded: Error: " << exception.what() << "\n";
        return FileNotFound;
    }

    get_yaml_value(config_node, "log-imports", &cfg.log_imports, false);
    get_yaml_value(config_node, "log-exports", &cfg.log_exports, false);
    get_yaml_value(config_node, "log-active-shaders", &cfg.log_active_shaders, false);
    get_yaml_value(config_node, "log-uniforms", &cfg.log_uniforms, false);
    get_yaml_value(config_node, "sync-rendering", &cfg.sync_rendering, false);
    get_yaml_value(config_node, "pstv-mode", &cfg.pstv_mode, false);
    get_yaml_value(config_node, "show-gui", &cfg.show_gui, false);
    get_yaml_value(config_node, "show-game-background", &cfg.show_game_background, true);
    get_yaml_value(config_node, "icon-size", &cfg.icon_size, static_cast<int>(64));
    get_yaml_value(config_node, "archive-log", &cfg.archive_log, false);
    get_yaml_value(config_node, "texture-cache", &cfg.texture_cache, true);
    get_yaml_value(config_node, "sys-button", &cfg.sys_button, static_cast<int>(SCE_SYSTEM_PARAM_ENTER_BUTTON_CROSS));
    get_yaml_value(config_node, "sys-lang", &cfg.sys_lang, static_cast<int>(SCE_SYSTEM_PARAM_LANG_ENGLISH_US));
    get_yaml_value(config_node, "background-image", &cfg.background_image, std::string{});
    get_yaml_value(config_node, "background-alpha", &cfg.background_alpha, 0.300f);
    get_yaml_value(config_node, "log-level", &cfg.log_level, static_cast<int>(spdlog::level::trace));
    get_yaml_value(config_node, "pref-path", &cfg.pref_path, root_pref_path);
    get_yaml_value(config_node, "discord-rich-presence", &cfg.discord_rich_presence, true);
    get_yaml_value_optional(config_node, "wait-for-debugger", &cfg.wait_for_debugger);

    if (!fs::exists(cfg.pref_path) && !cfg.pref_path.empty()) {
        LOG_ERROR("Cannot find preference path: {}", cfg.pref_path);
        return InvalidApplicationPath;
    }

    // lle-modules
    try {
        auto lle_modules_node = config_node["lle-modules"];

        for (const auto &lle_module_node : lle_modules_node) {
            cfg.lle_modules.push_back(lle_module_node.as<std::string>());
        }
    } catch (...) {
        std::cerr << "LLE modules node listed in config file has invalid syntax, please check again!\n";
        return InvalidApplicationPath;
    }

    return Success;
}

ExitCode serialize_config(Config &cfg, fs::path output_path) {
    output_path = check_path(output_path);
    if (output_path.empty())
        return InvalidApplicationPath;
    if (!fs::exists(output_path.parent_path()))
        fs::create_directories(output_path.parent_path());

    YAML::Emitter emitter;
    emitter << YAML::BeginMap;

    config_file_emit_single(emitter, "log-imports", cfg.log_imports);
    config_file_emit_single(emitter, "log-exports", cfg.log_exports);
    config_file_emit_single(emitter, "log-active-shaders", cfg.log_active_shaders);
    config_file_emit_single(emitter, "log-uniforms", cfg.log_uniforms);
    config_file_emit_single(emitter, "sync-rendering", cfg.sync_rendering);
    config_file_emit_single(emitter, "pstv-mode", cfg.pstv_mode);
    config_file_emit_single(emitter, "show-gui", cfg.show_gui);
    config_file_emit_single(emitter, "show-game-background", cfg.show_game_background);
    config_file_emit_single(emitter, "icon-size", cfg.icon_size);
    config_file_emit_single(emitter, "archive-log", cfg.archive_log);
    config_file_emit_single(emitter, "texture-cache", cfg.texture_cache);
    config_file_emit_single(emitter, "sys-button", cfg.sys_button);
    config_file_emit_single(emitter, "sys-lang", cfg.sys_lang);
    config_file_emit_single(emitter, "background-image", cfg.background_image);
    config_file_emit_single(emitter, "background-alpha", cfg.background_alpha);
    config_file_emit_single(emitter, "log-level", cfg.log_level);
    config_file_emit_single(emitter, "pref-path", cfg.pref_path);
    config_file_emit_vector(emitter, "lle-modules", cfg.lle_modules);
    config_file_emit_single(emitter, "discord-rich-presence", cfg.discord_rich_presence);
    config_file_emit_optional_single(emitter, "wait-for-debugger", cfg.wait_for_debugger);

    emitter << YAML::EndMap;

    fs::ofstream fo(output_path);
    if (!fo) {
        return InvalidApplicationPath;
    }

    fo << emitter.c_str();
    return Success;
}

ExitCode init_config(Config &cfg, int argc, char **argv, const Root &root_paths) {
    // Load base path configuration by default
    if (fs::exists(check_path(root_paths.get_base_path())))
        parse(cfg, root_paths.get_base_path(), root_paths.get_pref_path_string());

    try {
        // Declare all options
        // clang-format off
        Config command_line{};
        po::options_description generic_desc("Generic");
        generic_desc.add_options()
            ("help,h", "print this usage message")
            ("version,v", "print version string");

        po::options_description input_desc("Input");
        input_desc.add_options()
            ("input-vpk-path,i", po::value(&command_line.vpk_path), "path of app in .vpk format to install & run")
            ("input-installed-id,r", po::value(&command_line.run_title_id), "title ID of installed app to run")
            ("recompile-shader,s", po::value(&command_line.recompile_shader_path), "Recompile the given PS Vita shader (GXP format) to SPIR-V / GLSL and quit");

        po::options_description config_desc("Configuration");
        config_desc.add_options()
            ("archive-log,A", po::bool_switch(&command_line.archive_log), "Makes a duplicate of the log file with TITLE_ID and Game ID as title")
            ("config-location,c", po::value<fs::path>(&command_line.config_path), "Get a configuration file from a given location. If a filename is given, it must end with \".yml\", otherwise it will be assumed to be a directory. \nDefault: <Vita3K folder>/config.yml")
            ("keep-config,w", po::bool_switch(&command_line.overwrite_config)->default_value(true), "Do not modify the configuration file after loading.")
            ("load-config,f", po::bool_switch(&command_line.load_config), "Load a configuration file. Setting --keep-config with this option preserves the configuration file.")
            ("lle-modules,m", po::value<std::string>(), "Load given (decrypted) OS modules from disk. Separate by commas to specify multiple modules (no spaces). Full path and extension should not be included, the following are assumed: vs0:sys/external/<name>.suprx\nExample: --lle-modules libscemp4,libngs")
            ("log-level,l", po::value(&command_line.log_level), "logging level:\nTRACE = 0\nDEBUG = 1\nINFO = 2\nWARN = 3\nERROR = 4\nCRITICAL = 5\nOFF = 6")
            ("log-imports,I", po::bool_switch(&command_line.log_imports), "Log Imports")
            ("log-exports,E", po::bool_switch(&command_line.log_exports), "Log Exports")
            ("log-active-shaders,S", po::bool_switch(&command_line.log_active_shaders), "Log Active Shaders")
            ("log-uniforms,U", po::bool_switch(&command_line.log_uniforms), "Log Uniforms")
            ("sync-rendering,R", po::bool_switch(&command_line.sync_rendering), "Sync Rendering");
        // clang-format on

        // Positional args
        // Make .vpk (-i) argument the default
        po::positional_options_description positional;
        positional.add("input-vpk-path", 1);

        // Options description instance which includes all the options
        po::options_description all("All options");
        all.add(generic_desc).add(input_desc).add(config_desc);

        // Options description instance which will be shown to the user
        po::options_description visible("Allowed options");
        visible.add(generic_desc).add(input_desc).add(config_desc);

        // Options description instance which includes options exposed in config file
        po::options_description config_file("Config file options");
        config_file.add(config_desc);

        // Command-line argument parsing
        po::variables_map var_map;
        po::store(po::command_line_parser(argc, argv).options(all).positional(positional).run(), var_map);
        po::notify(var_map);

        if (var_map.count("help")) {
            std::cout << visible << std::endl;
            return QuitRequested;
        }
        if (var_map.count("version")) {
            std::cout << window_title << std::endl;
            return QuitRequested;
        }
        if (var_map.count("recompile-shader")) {
            return QuitRequested;
        }
        if (command_line.load_config || command_line.config_path != root_paths.get_base_path()) {
            if (command_line.config_path.empty()) {
                command_line.config_path = root_paths.get_base_path();
            } else {
                if (parse(command_line, command_line.config_path, root_paths.get_pref_path_string()) != Success)
                    return InitConfigFailed;
            }
        }

        // Get LLE modules from the command line, otherwise get the modules from the YML file
        if (var_map.count("lle-modules")) {
            const auto lle_modules = var_map["lle-modules"].as<std::string>();
            if (command_line.load_config) {
                const auto command_line_lle = string_utils::split_string(lle_modules, ',');
                command_line.lle_modules = vector_utils::merge_vectors(command_line.lle_modules, command_line_lle);
            } else {
                command_line.lle_modules = string_utils::split_string(lle_modules, ',');
            }
        }

        if (!command_line.load_config) {
            // Merge command line and base configs
            //cfg.merge_config(command_line, root_paths.get_base_path_string());
        } else {
            // Replace the base config with the parsed config
            cfg = std::move(command_line);
            LOG_INFO("Custom configuration file loaded successfully.");
        }

        logging::set_level(static_cast<spdlog::level::level_enum>(cfg.log_level));

        LOG_INFO_IF(cfg.vpk_path, "input-vpk-path: {}", *cfg.vpk_path);
        LOG_INFO_IF(cfg.run_title_id, "input-installed-id: {}", *cfg.run_title_id);
        if (!cfg.lle_modules.empty()) {
            std::string modules;
            for (const auto &mod : cfg.lle_modules) {
                modules += mod + ",";
            }
            modules.pop_back();
            LOG_INFO("lle-modules: {}", modules);
        }
        LOG_INFO("log-level: {}", cfg.log_level);
        LOG_INFO("log-imports: {}", cfg.log_imports);
        LOG_INFO("log-exports: {}", cfg.log_exports);
        LOG_INFO("log-active-shaders: {}", cfg.log_active_shaders);
        LOG_INFO("log-uniforms: {}", cfg.log_uniforms);
        LOG_INFO("sync-rendering: {}", cfg.sync_rendering);

    } catch (std::exception &e) {
        std::cerr << "error: " << e.what() << "\n";
        return InitConfigFailed;
    } catch (...) {
        std::cerr << "Exception of unknown type!\n";
        return InitConfigFailed;
    }

    // Save any changes made in command-line arguments
    if (cfg.overwrite_config || !fs::exists(check_path(cfg.config_path))) {
        if (serialize_config(cfg, cfg.config_path) != Success)
            return InitConfigFailed;
    }

    return Success;
}

} // namespace app
