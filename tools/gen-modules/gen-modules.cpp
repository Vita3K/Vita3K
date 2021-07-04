// Vita3K emulator project
// Copyright (C) 2021 Vita3K team
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

#include <filesystem>
#include <fstream>
#include <iostream>

using namespace YAML;

using Functions = std::map<std::string, std::string>;
using Libraries = std::map<std::string, Functions>;
using Modules = std::map<std::string, Libraries>;
using Function = Functions::value_type;
using Library = Libraries::value_type;
using Module = Modules::value_type;

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
    std::map<std::string, std::vector<std::string>> kernel_exceptions = {
        { "SceFios2Kernel", { "SceFios2KernelForDriver" } },
        { "SceKernelThreadMgr", { "SceThreadmgrForDriver", "SceThreadmgrForKernel" } },
        { "SceProcessmgr", { "SceProcessmgrForDriver" } },
        { "SceSysmem", { "SceCpuForDriver", "SceDebugForDriver", "SceDipswForDriver", "SceProcEventForDriver", "SceSysclibForDriver", "SceSysmemForDriver" } }
    };

    for (const auto &module : db["modules"]) {
        const auto module_name = module.first.as<std::string>();

        for (const auto &library : module.second["libraries"]) {
            const auto library_name = library.first.as<std::string>();
            const bool kernel = library.second["kernel"].as<bool>();
            if (kernel) {
                bool is_exception = false;

                for (const auto &[kernel_module_name, kernel_library_names] : kernel_exceptions) {
                    if (kernel_module_name == module_name) {
                        for (const auto &kernel_library_name : kernel_library_names) {
                            if (kernel_library_name == library_name) {
                                is_exception = true;
                            }
                        }
                    }
                }

                if (!is_exception) {
                    continue;
                }
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
    dst << "// Vita3K emulator project" << '\n';
    dst << "// Copyright (C) 2021 Vita3K team" << '\n';
    dst << "//" << '\n';
    dst << "// This program is free software; you can redistribute it and/or modify" << '\n';
    dst << "// it under the terms of the GNU General Public License as published by" << '\n';
    dst << "// the Free Software Foundation; either version 2 of the License, or" << '\n';
    dst << "// (at your option) any later version." << '\n';
    dst << "//" << '\n';
    dst << "// This program is distributed in the hope that it will be useful," << '\n';
    dst << "// but WITHOUT ANY WARRANTY; without even the implied warranty of" << '\n';
    dst << "// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the" << '\n';
    dst << "// GNU General Public License for more details." << '\n';
    dst << "//" << '\n';
    dst << "// You should have received a copy of the GNU General Public License along" << '\n';
    dst << "// with this program; if not, write to the Free Software Foundation, Inc.," << '\n';
    dst << "// 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA." << '\n';
    dst << '\n';
}

static void gen_nids_h(std::ostream &out, const Modules &modules) {
    for (const Module &module : modules) {
        out << "// Module \"" << module.first << "\"" << '\n';

        for (const Library &library : module.second) {
            out << "// Library \"" << library.first << "\"" << '\n';

            for (const auto &function : library.second) {
                out << "NID(" << function.first << ", " << function.second << ")" << '\n';
            }
        }
    }
}

static void gen_library_cpp(std::ostream &dst, const Library &library) {
    gen_license_comment(dst);
    dst << "#include \"" << library.first << ".h\"" << '\n';

    for (const Function &function : library.second) {
        dst << '\n';
        dst << "EXPORT(int, " << function.first << ") {" << '\n';
        dst << indent << "return UNIMPLEMENTED();" << '\n';
        dst << "}" << '\n';
    }

    dst << '\n';
    for (const Function &function : library.second) {
        dst << "BRIDGE_IMPL(" << function.first << ")" << '\n';
    }
}

static void gen_library_h(std::ostream &dst, const Library &library) {
    gen_license_comment(dst);
    dst << "#pragma once" << '\n';
    dst << '\n';
    dst << "#include <module/module.h>" << '\n';
    dst << '\n';

    for (const auto &function : library.second) {
        dst << "BRIDGE_DECL(" << function.first << ")" << '\n';
    }
}

static void gen_module_stubs(const Modules &modules) {
    for (const Module &module : modules) {
        const std::string module_path = "vita3k/modules/" + module.first;

#ifdef WIN32
        CreateDirectoryA(module_path.c_str(), nullptr);
#else
        const int mode = S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH;
        mkdir(module_path.c_str(), mode);
#endif

        for (const Library &library : module.second) {
            const std::string library_cpp_path = module_path + "/" + library.first + ".cpp";
            const std::string library_h_path = module_path + "/" + library.first + ".h";
            std::ofstream library_cpp(library_cpp_path.c_str(), std::ios::binary);
            gen_library_cpp(library_cpp, library);
            std::ofstream library_h(library_h_path.c_str(), std::ios::binary);
            gen_library_h(library_h, library);
        }
    }
}

static void gen_modules_cmakelists(std::ostream &out, const Modules &modules) {
    for (const Module &module : modules) {
        for (const Library &library : module.second) {
            out << "\n\t" << module.first << "/" << library.first << ".cpp";
            out << " " << module.first << "/" << library.first << ".h";
        }
    }
}

int main(int argc, const char *argv[]) {
    if (argc != 2) {
        std::cout << "Usage: " << argv[0] << " <path_to_vita_headers_dir>" << std::endl;
        return 0;
    }

    std::filesystem::path db_dir = std::filesystem::path(argv[1]) / "db";
    if (!std::filesystem::exists(db_dir) || !std::filesystem::is_directory(db_dir)) {
        std::cout << db_dir << " is not a valid directory" << std::endl;
        return 0;
    }

    std::ofstream cmake("vita3k/modules/CMakeLists.txt", std::ios::binary);
    cmake << "set(SOURCE_LIST" << '\n';
    cmake << '\t' << "module_parent.cpp include/modules/module_parent.h include/modules/library_init_list.inc" << '\n';

    std::ofstream nids("vita3k/nids/include/nids/nids.inc", std::ios::binary);
    gen_license_comment(nids);

    for (const auto &entry : std::filesystem::recursive_directory_iterator(db_dir)) {
        // Process only yml files
        if (std::filesystem::is_regular_file(entry) && (entry.path().extension() == ".yml")) {
            const Node db = LoadFile(entry.path().string());
            const Modules modules = parse_db(db);

            gen_nids_h(nids, modules);
            gen_module_stubs(modules);
            gen_modules_cmakelists(cmake, modules);
        }
    }

    cmake << '\n'
          << ")" << '\n'
          << '\n';
    cmake << "add_library(modules STATIC ${SOURCE_LIST})" << '\n';
    cmake << "target_include_directories(modules PUBLIC include)" << '\n';
    cmake << "target_link_libraries(modules PRIVATE xxHash::xxhash)" << '\n';
    cmake << "target_link_libraries(modules PUBLIC module)" << '\n';
    cmake << "source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${SOURCE_LIST})" << '\n';

    return 0;
}
