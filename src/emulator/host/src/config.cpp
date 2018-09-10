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

#include <host/config.h>

#include <host/app.h>
#include <host/version.h>
#include <util/log.h>
#include <util/string_utils.h>

#include <boost/optional.hpp>
#include <boost/optional/optional_io.hpp>
#include <boost/program_options.hpp>
#include <spdlog/spdlog.h>

#include <exception>
#include <iostream>

namespace po = boost::program_options;

namespace config {

bool init(Config &cfg, int argc, char **argv) {
    try {
        // Declare all options
        // clang-format off
        po::options_description generic_desc("Generic");
        generic_desc.add_options()
            ("help,h", "print this usage message")
            ("version,v", "print version string");

        po::options_description input_desc("Input");
        input_desc.add_options()
            ("input-vpk-path,i", po::value(&cfg.vpk_path), "path of app in .vpk format to install & run")
            ("input-installed-id,r", po::value(&cfg.run_title_id), "title ID of installed app to run");

        po::options_description config_desc("Configuration");
        config_desc.add_options()
            ("log-level,l", po::value(&cfg.log_level)->default_value(spdlog::level::trace), "logging level:\nTRACE = 0\nDEBUG = 1\nINFO = 2\nWARN = 3\nERROR = 4\nCRITICAL = 5\nOFF = 6");
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

        po::variables_map var_map;

        // Command-line argument parsing
        po::store(po::command_line_parser(argc, argv).options(all).positional(positional).run(), var_map);
        // TODO: Config file creation/parsing
        po::notify(var_map);

        // Handle some basic args
        if (var_map.count("help")) {
            std::cout << visible << std::endl;
            return true;
        }
        if (var_map.count("version")) {
            std::cout << window_title << std::endl;
            return true;
        }

        logging::set_level(static_cast<spdlog::level::level_enum>(*cfg.log_level));

    } catch (std::exception &e) {
        std::cerr << "error: " << e.what() << "\n";
        return false;
    } catch (...) {
        std::cerr << "Exception of unknown type!\n";
        return false;
    }
    return true;
}

} // namespace config
