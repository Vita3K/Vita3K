// Vita3K emulator project
// Copyright (C) 2023 Vita3K team
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

#include <regex>
#include <regmgr/functions.h>

#include <util/log.h>

namespace regmgr {

const std::array<unsigned char, 16> xorKey = { 0x89, 0xFA, 0x95, 0x48, 0xCB, 0x6D, 0x77, 0x9D, 0xA2, 0x25, 0x34, 0xFD, 0xA9, 0x35, 0x59, 0x6E };

static std::string decryptRegistryFile(const fs::path reg_path) {
    std::ifstream file(reg_path.string(), std::ios::binary);
    if (!file.is_open()) {
        LOG_WARN("Error while opening file: {}, install firmware for solve this!", reg_path.string());
        return {};
    }

    // Read the data from the file encrypted
    std::vector<uint8_t> encryptedData((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

    // Check if file is empty
    if (encryptedData.empty()) {
        LOG_DEBUG("File is empty: {}", reg_path.string());
        return {};
    }

    // Remove the header of the file (138 bytes)
    encryptedData.erase(encryptedData.begin(), encryptedData.begin() + 138);

    // Decrypt the data with the xor key
    for (size_t i = 0; i < encryptedData.size(); i++) {
        encryptedData[i] ^= xorKey[i & 0xF];
    }

    // Convert the data to a string
    std::string res;
    for (const auto &byte : encryptedData) {
        res += byte;
    }

    return res;
}

static std::unordered_map<std::string, std::vector<RegValue>> reg_template;

void init_reg_template(RegMgrState &regmgr, const std::string &reg) {
    std::unordered_map<int, std::string> reg_map;

    std::istringstream iss(reg);
    std::string line;
    while (std::getline(iss, line)) {
        if (line.empty())
            continue;

        // Found the base register
        if (line.find("[BASE") != std::string::npos) {
            // Register the names with their numbers
            while (std::getline(iss, line) && (line != "[REG-BAS")) {
                if (line.empty())
                    continue;

                size_t pos = line.find('=');
                if (pos != std::string::npos) {
                    const auto num = std::stoi(line.substr(0, pos));
                    const auto name = line.substr(pos + 1);
                    reg_map[num] = name;
                }
            }
        }

        if (line == "[REG-BAS") {
            // Register the base register
            while (std::getline(iss, line) && (line != ("[REG-J1"))) {
                if (line.empty())
                    continue;

                size_t pos = line.find('=');
                if (pos != std::string::npos) {
                    std::string entry = line.substr(0, pos);
                    std::string value = line.substr(pos + 1);

                    std::string name = "/";
                    std::istringstream f(entry);
                    std::string s;
                    while (std::getline(f, s, '/')) {
                        if (!s.empty())
                            name += reg_map[std::stoi(s)];
                    }

                    const auto last_slash_pos = name.find_last_of('/') + 1;
                    const auto valueName = name.substr(last_slash_pos);
                    const auto category = name.substr(0, last_slash_pos);

                    std::istringstream v(value);
                    std::vector<std::string> values;
                    while (std::getline(v, s, ':')) {
                        if (!s.empty())
                            values.push_back(s);
                    }

                    const auto valueSize = static_cast<uint32_t>(std::stoi(values[1]));
                    const std::regex categoryRangePattern(R"((.*\/)([0-9]{2})-([0-9]{2})(\/.*))");
                    std::smatch matches;
                    if (std::regex_search(category, matches, categoryRangePattern)) {
                        const auto cat_begin = matches[1].str();
                        const auto firstNum = std::stoi(matches[2].str());
                        const auto secondNum = std::stoi(matches[3].str());
                        const auto cat_end = matches[4].str();

                        for (int i = firstNum; i <= secondNum; i++) {
                            const std::string categoryName = fmt::format("{}{:0>2d}{}", cat_begin, i, cat_end);
                            reg_template[categoryName].push_back({ valueName, valueSize, values.back() });
                        }
                    } else
                        reg_template[category].push_back({ valueName, valueSize, values.back() });
                }
            }
        }
    }
}

static constexpr uint32_t spaceSize = 32;
static uint32_t get_space_size(const uint32_t str_size) {
    const uint32_t line = 16;

    return spaceSize > str_size ? spaceSize - str_size : ((str_size / line) + 1) * line - str_size;
}

static bool load_system_dreg(RegMgrState &regmgr, const std::wstring &pref_path) {
    const auto system_dreg_path{ fs::path(pref_path) / "vd0/registry/system.dreg" };
    fs::ifstream file(system_dreg_path, std::ios::in | std::ios::binary);
    if (file.is_open()) {
        char buf = 0;

        // Skip the space
        for (uint32_t i = 0; i < (spaceSize + 1); i++) {
            file.read(&buf, 1);
        }

        for (const auto &reg : reg_template) {
            // Read the category
            const uint32_t cat_size = static_cast<uint32_t>(reg.first.size());
            std::vector<char> category(cat_size);
            file.read(category.data(), cat_size);
            const auto category_str = std::string(category.begin(), category.end());
            if (category_str != reg.first) {
                LOG_ERROR("Invalid category: {}, expected: {}", category_str, reg.first);
                return false;
            }

            // Skip the space
            const auto diff_cat = get_space_size(cat_size);
            for (uint32_t i = 0; i < diff_cat; i++) {
                file.read(&buf, 1);
            }

            for (const auto &entry : reg.second) {
                // Read the name
                const uint32_t name_size = static_cast<uint32_t>(entry.name.size());
                std::vector<char> name(name_size);
                file.read(name.data(), name_size);
                const auto name_str = std::string(name.begin(), name.end());
                if (name_str != entry.name) {
                    LOG_ERROR("Invalid entry name: {}, expected: {}", name_str, entry.name);
                    return false;
                }

                // Skip the space
                const auto diff_name = get_space_size(name_size + 1);
                for (uint32_t i = 0; i < diff_name; i++) {
                    file.read(&buf, 1);
                }

                // Read the value
                const uint32_t value_size = entry.size;
                auto &value = regmgr.system_dreg[reg.first][entry.name];
                value.resize(value_size);
                file.read(value.data(), value_size);

                // Skip the space
                const auto diff_value = get_space_size(value_size);
                for (uint32_t i = 0; i < (diff_value + 1); i++) {
                    file.read(&buf, 1);
                }
            }
        }

        file.close();
    }

    return !regmgr.system_dreg.empty();
}

static void save_system_dreg(RegMgrState &regmgr, const std::wstring pref_path) {
    const auto system_dreg_path{ fs::path(pref_path) / "vd0/registry/system.dreg" };
    fs::ofstream file(system_dreg_path, std::ios::out | std::ios::binary);
    if (file.is_open()) {
        const char buf = 0;

        // Write the space
        for (uint32_t i = 0; i < (spaceSize + 1); i++) {
            file.write(&buf, 1);
        }

        // Write each category
        for (const auto &reg : reg_template) {
            // Write the category
            const uint32_t cat_size = static_cast<uint32_t>(reg.first.size());
            std::vector<char> category(cat_size);
            strncpy(category.data(), reg.first.c_str(), cat_size);
            file.write(reg.first.c_str(), cat_size);

            // Write the space
            const auto diff_cat = get_space_size(cat_size);
            for (uint32_t i = 0; i < diff_cat; i++) {
                file.write(&buf, 1);
            }

            // Write each entry
            for (const auto &entry : reg.second) {
                // Write the name
                const uint32_t name_size = static_cast<uint32_t>(entry.name.size());
                std::vector<char> name(name_size);
                strncpy(name.data(), entry.name.c_str(), name_size);
                file.write(name.data(), name_size);

                // Write the space
                const auto diff_name = get_space_size(name_size + 1);
                for (uint32_t i = 0; i < diff_name; i++) {
                    file.write(&buf, 1);
                }

                // Write the value
                const uint32_t value_size = entry.size;
                file.write(regmgr.system_dreg[reg.first][entry.name].data(), value_size);

                // Write the space
                const auto diff_value = get_space_size(value_size);
                for (uint32_t i = 0; i < (diff_value + 1); i++) {
                    file.write(&buf, 1);
                }
            }
        }

        file.close();
    }
}

static void init_system_dreg(RegMgrState &regmgr, const std::wstring pref_path) {
    regmgr.system_dreg.clear();

    for (const auto &reg : reg_template) {
        for (const auto &entry : reg.second) {
            auto &value = regmgr.system_dreg[reg.first][entry.name];
            value.resize(entry.size);
            value.assign(entry.value.begin(), entry.value.end());
        }
    }

    save_system_dreg(regmgr, pref_path);
}

static std::string fix_category(const std::string &category) {
    return category.back() == '/' ? category : category + '/';
}

void get_bin_value(RegMgrState &regmgr, const std::string &category, const std::string &name, void *buf, uint32_t bufSize) {
    std::lock_guard<std::mutex> lock(regmgr.mutex);
    const auto &sys = regmgr.system_dreg[fix_category(category)][name];

    memcpy(buf, sys.data(), bufSize);
}

void set_bin_value(RegMgrState &regmgr, const std::wstring &pref_path, const std::string &category, const std::string &name, const void *buf, const uint32_t bufSize) {
    std::lock_guard<std::mutex> lock(regmgr.mutex);

    const char *data = reinterpret_cast<const char *>(buf);
    regmgr.system_dreg[fix_category(category)][name].assign(data, data + bufSize);

    save_system_dreg(regmgr, pref_path);
}

int32_t get_int_value(RegMgrState &regmgr, const std::string &category, const std::string &name) {
    std::lock_guard<std::mutex> lock(regmgr.mutex);

    const auto &sys = regmgr.system_dreg[fix_category(category)][name];
    const auto value_str = std::string(sys.begin(), sys.end());

    return std::stoi(value_str);
}

void set_int_value(RegMgrState &regmgr, const std::wstring &pref_path, const std::string &category, const std::string &name, int32_t value) {
    std::lock_guard<std::mutex> lock(regmgr.mutex);

    const auto str_value = std::to_string(value);
    regmgr.system_dreg[fix_category(category)][name].assign(str_value.begin(), str_value.end());

    save_system_dreg(regmgr, pref_path);
}

std::string get_str_value(RegMgrState &regmgr, const std::string &category, const std::string &name) {
    std::lock_guard<std::mutex> lock(regmgr.mutex);
    const auto &sys = regmgr.system_dreg[fix_category(category)][name];

    return std::string(sys.begin(), sys.end());
}

void set_str_value(RegMgrState &regmgr, const std::wstring &pref_path, const std::string &category, const std::string &name, const char *value, const uint32_t bufSize) {
    std::lock_guard<std::mutex> lock(regmgr.mutex);
    regmgr.system_dreg[fix_category(category)][name].assign(value, value + bufSize);

    save_system_dreg(regmgr, pref_path);
}

void init_regmgr(RegMgrState &regmgr, const std::wstring &pref_path) {
    const auto reg = decryptRegistryFile(fs::path(pref_path) / "os0/kd/registry.db0");
    if (reg.empty()) {
        return;
    }

    // Initialize the registry template
    init_reg_template(regmgr, reg);

    // Initialize the system.dreg
    if (!load_system_dreg(regmgr, pref_path)) {
        LOG_WARN("Failed to load system.dreg, attempting to create it");
        init_system_dreg(regmgr, pref_path);
    }
}

std::pair<std::string, std::string> get_category_and_name_by_id(const int id, const std::string &export_name) {
    switch (id) {
    case 0x00023fc2:
        return { "/CONFIG/ACCESSIBILITY/", "large_text" };
    case 0x00037502:
        return { "/CONFIG/SYSTEM/", "language" };
    case 0x00088776:
        return { "/CONFIG/DATE/", "date_format" };
    case 0x00186122:
        return { "/CONFIG/SECURITY/PARENTAL/", "passcode" };
    case 0x00229142:
        return { "/CONFIG/SYSTEM/", "button_assign" };
    case 0x00450F32:
        return { "/CONFIG/NP/", "account_id" };
    case 0x00598438:
        return { "/CONFIG/SYSTEM/", "username" };
    case 0x00611DC9:
        return { "/CONFIG/ACCESSIBILITY/", "bold_text" };
    case 0x00668503:
        return { "/CONFIG/DATE/", "time_format" };
    case 0x00683DCD:
        return { "/CONFIG/SYSTEM/", "key_pad" };
    case 0x008A2AD7:
        return { "/CONFIG/ACCESSIBILITY/", "contrast" };
    default:
        LOG_WARN("{}: unknown id: {}", export_name, log_hex(id));
        return { "", "" };
    }
}

} // namespace regmgr
