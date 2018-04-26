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

#include <yaml-cpp/yaml.h>

#ifdef WIN32
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#else
#include <sys/stat.h>
#endif

#include <fstream>
#include <iostream>

using namespace YAML;

typedef std::map<std::string, std::string> Functions;
typedef std::map<std::string, Functions> Libraries;
typedef std::map<std::string, Libraries> Modules;
typedef Functions::value_type Function;
typedef Libraries::value_type Library;
typedef Modules::value_type Module;

static const std::string indent("    ");

static std::string dedupe(const std::string &name, const std::set<std::string> existing) {
    std::string candidate = name;
    while (existing.find(candidate) != existing.end()) {
        candidate += "_dupe";
    }

    return candidate;
}

static Modules parse_db(const Node &db) {
    Modules modules;
    std::set<std::string> functions;

    for (const auto &module : db["modules"]) {
        const auto module_name = module.first.as<std::string>();

        for (const auto &library : module.second["libraries"]) {
            const auto library_name = library.first.as<std::string>();
            const bool kernel = library.second["kernel"].as<bool>();
            if (kernel) {
                continue;
            }

            for (const auto &function : library.second["functions"]) {
                const auto original_name = function.first.as<std::string>();
                const auto deduped_name = dedupe(original_name, functions);
                const auto function_nid = function.second.as<std::string>();

                modules[module_name][library_name][deduped_name] = function_nid;
                functions.insert(deduped_name);
            }
        }
    }

    return modules;
}

static void gen_license_comment(std::ostream &dst) {
    dst << "// Vita3K emulator project" << std::endl;
    dst << "// Copyright (C) 2018 Vita3K team" << std::endl;
    dst << "//" << std::endl;
    dst << "// This program is free software; you can redistribute it and/or modify" << std::endl;
    dst << "// it under the terms of the GNU General Public License as published by" << std::endl;
    dst << "// the Free Software Foundation; either version 2 of the License, or" << std::endl;
    dst << "// (at your option) any later version." << std::endl;
    dst << "//" << std::endl;
    dst << "// This program is distributed in the hope that it will be useful," << std::endl;
    dst << "// but WITHOUT ANY WARRANTY; without even the implied warranty of" << std::endl;
    dst << "// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the" << std::endl;
    dst << "// GNU General Public License for more details." << std::endl;
    dst << "//" << std::endl;
    dst << "// You should have received a copy of the GNU General Public License along" << std::endl;
    dst << "// with this program; if not, write to the Free Software Foundation, Inc.," << std::endl;
    dst << "// 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA." << std::endl;
    dst << std::endl;
}

static void gen_nids_h(const Modules &modules) {
    std::ofstream out("src/emulator/nids/include/nids/nids.h");
    gen_license_comment(out);

    for (const Module &module : modules) {
        out << "// Module \"" << module.first << "\"" << std::endl;

        for (const Library &library : module.second) {
            out << "// Library \"" << library.first << "\"" << std::endl;

            for (const auto &function : library.second) {
                out << "NID(" << function.first << ", " << function.second << ")" << std::endl;
            }
        }
    }
}

static void gen_library_cpp(std::ostream &dst, const Library &library) {
    gen_license_comment(dst);
    dst << "#include \"" << library.first << ".h\"" << std::endl;

    for (const Function &function : library.second) {
        dst << std::endl;
        dst << "EXPORT(int, " << function.first << ") {" << std::endl;
        dst << indent << "return unimplemented(\"" << function.first << "\");" << std::endl;
        dst << "}" << std::endl;
    }

    dst << std::endl;
    for (const Function &function : library.second) {
        dst << "BRIDGE_IMPL(" << function.first << ")" << std::endl;
    }
}

static void gen_library_h(std::ostream &dst, const Library &library) {
    gen_license_comment(dst);
    dst << "#pragma once" << std::endl;
    dst << std::endl;
    dst << "#include <module/module.h>" << std::endl;
    dst << std::endl;

    for (const auto &function : library.second) {
        dst << "BRIDGE_DECL(" << function.first << ")" << std::endl;
    }
}

static void gen_module_stubs(const Modules &modules) {
    for (const Module &module : modules) {
        const std::string module_path = "src/emulator/modules/" + module.first;

#ifdef WIN32
        CreateDirectoryA(module_path.c_str(), nullptr);
#else
        const int mode = S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH;
        mkdir(module_path.c_str(), mode);
#endif

        for (const Library &library : module.second) {
            const std::string library_cpp_path = module_path + "/" + library.first + ".cpp";
            const std::string library_h_path = module_path + "/" + library.first + ".h";
            std::ofstream library_cpp(library_cpp_path.c_str());
            gen_library_cpp(library_cpp, library);
            std::ofstream library_h(library_h_path.c_str());
            gen_library_h(library_h, library);
        }
    }
}

static void gen_modules_cmakelists(const Modules &modules) {
    std::ofstream out("src/emulator/modules/CMakeLists.txt");
    out << "add_library(modules STATIC";

    for (const Module &module : modules) {
        for (const Library &library : module.second) {
            out << "\n\t" << module.first << "/" << library.first << ".cpp";
            out << " " << module.first << "/" << library.first << ".h";
        }
    }

    out << ")" << std::endl;
    out << "target_link_libraries(modules PRIVATE module)" << std::endl;
}

int main(int argc, const char *argv[]) {
    const Node db = LoadFile("src/external/vita-headers/db.yml");
    const Modules modules = parse_db(db);

    gen_nids_h(modules);
    gen_module_stubs(modules);
    gen_modules_cmakelists(modules);

    return 0;
}
