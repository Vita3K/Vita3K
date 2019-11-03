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

#include <config/functions.h>
#include <config/state.h>
#include <config/version.h>

#include <util/log.h>
#include <util/string_utils.h>
#include <util/vector_utils.h>

#include <boost/optional/optional_io.hpp>
#include <boost/program_options.hpp>

#include <exception>
#include <iostream>

namespace po = boost::program_options;

namespace config {

static fs::path check_path(const fs::path &output_path) {
    if (output_path.filename() != "config.yml") {
        if (!output_path.has_extension()) // assume it is a folder
            return output_path / "config.yml";
        if (output_path.extension() != ".yml")
            return "";
    }
    return output_path;
}

static ExitCode parse(Config &cfg, const fs::path &load_path, const std::string &root_pref_path) {
    const auto loaded_path = check_path(load_path);
    if (loaded_path.empty() || !fs::exists(loaded_path)) {
        LOG_ERROR("Config file input path invalid (did you make sure to name the extension \".yml\"?)");
        return FileNotFound;
    }

    try {
        cfg.load_new_config(loaded_path);
    } catch (YAML::Exception &exception) {
        std::cerr << "Config file can't be loaded: Error: " << exception.what() << "\n";
        return FileNotFound;
    }

    if (cfg.pref_path.empty())
        cfg.pref_path = root_pref_path;
    if (!fs::exists(cfg.pref_path) && cfg.pref_path != root_pref_path) {
        LOG_ERROR("Cannot find preference path: {}", cfg.pref_path);
        return InvalidApplicationPath;
    }

    try {
        // lle-modules
        auto lle_modules_node = cfg.yaml_node["lle_modules"];

        for (const auto &lle_module_node : lle_modules_node) {
            cfg.lle_modules.push_back(lle_module_node.as<std::string>());
        }
    } catch (...) {
        std::cerr << "LLE modules node listed in config file has invalid syntax, please check again!\n";
        return InvalidApplicationPath;
    }

    return Success;
}

ExitCode serialize_config(Config &cfg, const fs::path &output_path) {
    const auto output = check_path(output_path);
    if (output.empty())
        return InvalidApplicationPath;
    if (!fs::exists(output.parent_path()))
        fs::create_directories(output.parent_path());

    // Update the YAML node
    cfg.yaml_node = cfg.get();

    YAML::Emitter emitter;
    emitter << YAML::BeginDoc;
    emitter << cfg.yaml_node;
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
    serialize_config(command_line, root_paths.get_base_path() / "data/config/default.yml");

    // Load base path configuration by default; otherwise, use the default
    if (fs::exists(check_path(root_paths.get_base_path())))
        parse(cfg, root_paths.get_base_path(), root_paths.get_pref_path_string());
    else
        fs::copy(root_paths.get_base_path() / "data/config/default.yml", root_paths.get_base_path() / "config.yml");

    try {
        // Declare all options
        // clang-format off
        po::options_description generic_desc("Generic");
        generic_desc.add_options()
            ("help,h", "print this usage message")
            ("version,v", "print version string");

        po::options_description input_desc("Input");
        input_desc.add_options()
            ("input_vpk_path,i", po::value(&command_line.vpk_path), "path of app in .vpk format to install & run")
            ("input_installed_id,r", po::value(&command_line.run_title_id), "title ID of installed app to run")
            ("recompile_shader,s", po::value(&command_line.recompile_shader_path), "Recompile the given PS Vita shader (GXP format) to SPIR_V / GLSL and quit");

        po::options_description config_desc("Configuration");
        config_desc.add_options()
            ("archive_log,A", po::bool_switch(&command_line.archive_log), "Makes a duplicate of the log file with TITLE_ID and Game ID as title")
            ("backend_renderer,B", po::value(&command_line.backend_renderer), "Renderer backend to use, either \"OpenGL\" or \"Vulkan\"")
            ("color_surface_debug,C", po::bool_switch(&command_line.color_surface_debug), "Save color surfaces")
            ("config_location,c", po::value<fs::path>(&command_line.config_path), "Get a configuration file from a given location. If a filename is given, it must end with \".yml\", otherwise it will be assumed to be a directory. \nDefault: <Vita3K folder>/config.yml")
            ("keep_config,w", po::bool_switch(&command_line.overwrite_config)->default_value(true), "Do not modify the configuration file after loading.")
            ("load_config,f", po::bool_switch(&command_line.load_config), "Load a configuration file. Setting __keep_config with this option preserves the configuration file.")

            ("lle_modules,m", po::value<std::string>(), "Load given (decrypted) OS modules from disk. Separate by commas to specify multiple modules (no spaces). Full path and extension should not be included, the following are assumed: vs0:sys/external/<name>.suprx\nExample: __lle_modules libscemp4,libngs")
            ("log_level,l", po::value(&command_line.log_level), "logging level:\nTRACE = 0\nDEBUG = 1\nINFO = 2\nWARN = 3\nERROR = 4\nCRITICAL = 5\nOFF = 6")
            ("log_imports,I", po::bool_switch(&command_line.log_imports), "Log Imports")
            ("log_exports,E", po::bool_switch(&command_line.log_exports), "Log Exports")
            ("log_active_shaders,S", po::bool_switch(&command_line.log_active_shaders), "Log Active Shaders")
            ("log_uniforms,U", po::bool_switch(&command_line.log_uniforms), "Log Uniforms");
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
        if (var_map.count("recompile_shader")) {
            cfg.recompile_shader_path = std::move(command_line.recompile_shader_path);
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
        if (var_map.count("lle_modules")) {
            const auto lle_modules = var_map["lle_modules"].as<std::string>();
            if (command_line.load_config) {
                const auto command_line_lle = string_utils::split_string(lle_modules, ',');
                command_line.lle_modules = vector_utils::merge_vectors(command_line.lle_modules, command_line_lle);
            } else {
                command_line.lle_modules = string_utils::split_string(lle_modules, ',');
            }
        }

        // Merge configurations
        cfg = command_line;
        if (cfg.pref_path.empty())
            cfg.pref_path = root_paths.get_pref_path_string();

        LOG_INFO_IF(cfg.load_config, "Custom configuration file loaded successfully.");

        logging::set_level(static_cast<spdlog::level::level_enum>(cfg.log_level));

        LOG_INFO_IF(cfg.vpk_path, "input_vpk_path: {}", *cfg.vpk_path);
        LOG_INFO_IF(cfg.run_title_id, "input_installed_id: {}", *cfg.run_title_id);
        LOG_INFO("backend_renderer: {}", cfg.backend_renderer);
        LOG_INFO("hardware_flip: {}", cfg.hardware_flip);
        if (!cfg.lle_modules.empty()) {
            std::string modules;
            for (const auto &mod : cfg.lle_modules) {
                modules += mod + ",";
            }
            modules.pop_back();
            LOG_INFO("lle_modules: {}", modules);
        }
        LOG_INFO("log_level: {}", cfg.log_level);
        LOG_INFO("log_imports: {}", cfg.log_imports);
        LOG_INFO("log_exports: {}", cfg.log_exports);
        LOG_INFO("log_active_shaders: {}", cfg.log_active_shaders);
        LOG_INFO("log_uniforms: {}", cfg.log_uniforms);

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

} // namespace config
