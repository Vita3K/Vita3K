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

#include <algorithm>
#include <array>
#include <atomic>
#include <cerrno>
#include <limits>
#include <regex>
#include <regmgr/functions.h>
#include <set>
#include <sstream>
#include <system_error>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <fcntl.h>
#include <unistd.h>
#endif

#include <util/bytes.h>
#include <util/log.h>
#include <util/string_utils.h>
#include <util/vector_utils.h>

namespace regmgr {

constexpr std::array<unsigned char, 16> xorKey = { 0x89, 0xFA, 0x95, 0x48, 0xCB, 0x6D, 0x77, 0x9D, 0xA2, 0x25, 0x34, 0xFD, 0xA9, 0x35, 0x59, 0x6E };
static constexpr uint32_t spaceSize = 32;

static std::string decryptRegistryFile(const fs::path &reg_path) {
    std::vector<char> encryptedData(0);
    auto res = fs_utils::read_data(reg_path, encryptedData);
    if (!res) {
        LOG_WARN("Error while opening file: {}, install firmware to solve this!", reg_path);
        return {};
    }

    constexpr auto header_size = 138;
    // Check if file is empty or too small
    if (encryptedData.size() <= header_size) {
        LOG_DEBUG("File is empty or too small: {}", reg_path);
        return {};
    }

    // Decrypt the data with the xor key
    auto decryptedSize = encryptedData.size() - header_size;
    std::string decryptedData;
    decryptedData.resize(decryptedSize);
    for (size_t i = 0; i < decryptedSize; ++i) {
        decryptedData[i] = encryptedData[i + header_size] ^ xorKey[i & 0xF];
    }
    return decryptedData;
}

static bool init_reg_template(RegMgrState &regmgr, const std::string &reg) {
    regmgr.reg_category_template.clear();
    regmgr.reg_template.clear();

    std::map<int, std::string> reg_map;

    std::istringstream iss(reg);
    std::string line;
    while (std::getline(iss, line)) {
        if (line.empty())
            continue;

        // Found the base register
        if (line.contains("[BASE")) {
            // Register the names with their numbers
            while (std::getline(iss, line) && (line != "[REG-BAS")) {
                if (line.empty())
                    continue;

                size_t pos = line.find('=');
                if (pos != std::string::npos) {
                    const auto num = string_utils::stoi_def(line.substr(0, pos));
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
                            name += reg_map[string_utils::stoi_def(s)];
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

                    if (values.size() < 2)
                        return false;
                    const auto valueType = static_cast<RegType>(string_utils::stoi_def(values[0], -1));
                    const auto parsedSize = string_utils::stoi_def(values[1], -1);
                    if (parsedSize < 0 || (valueType != REG_TYPE_INT && valueType != REG_TYPE_STR && valueType != REG_TYPE_BIN)
                        || (valueType == REG_TYPE_INT && (parsedSize < 4 || valueName.size() >= spaceSize || static_cast<size_t>(parsedSize) > spaceSize - valueName.size() - 1)))
                        return false;
                    const auto valueSize = static_cast<uint32_t>(parsedSize);
                    const auto valueDefault = values.back();

                    // Init the value depending on the type
                    std::vector<char> initValueData(valueSize);
                    switch (valueType) {
                    case REG_TYPE_INT:
                        *reinterpret_cast<int32_t *>(initValueData.data()) = byte_swap(string_utils::stoi_def(valueDefault, 0));
                        break;
                    case REG_TYPE_STR:
                    case REG_TYPE_BIN:
                        if (valueDefault != "0")
                            initValueData.assign(valueDefault.begin(), valueDefault.end());
                        break;
                    default: break;
                    }

                    const std::regex categoryRangePattern(R"((.*\/)([0-9]{2})-([0-9]{2})(\/.*))");
                    std::smatch matches;
                    if (std::regex_search(category, matches, categoryRangePattern)) {
                        const auto cat_begin = matches[1].str();
                        const auto firstNum = string_utils::stoi_def(matches[2].str());
                        const auto secondNum = string_utils::stoi_def(matches[3].str());
                        const auto cat_end = matches[4].str();

                        for (int i = firstNum; i <= secondNum; i++) {
                            const std::string categoryName = fmt::format("{}{:0>2d}{}", cat_begin, i, cat_end);
                            regmgr.reg_template[categoryName].push_back({ valueName, valueType, valueSize, initValueData });
                            vector_utils::push_if_not_exists(regmgr.reg_category_template, categoryName);
                        }
                    } else {
                        regmgr.reg_template[category].push_back({ valueName, valueType, valueSize, initValueData });
                        vector_utils::push_if_not_exists(regmgr.reg_category_template, category);
                    }
                }
            }
        }
    }
    return !regmgr.reg_template.empty();
}

static uint32_t get_space_size(const uint32_t str_size) {
    const uint32_t line = 16;

    return spaceSize > str_size ? spaceSize - str_size : line - str_size % line;
}

constexpr const char *account_category = "/CONFIG/NP/";
constexpr const char *account_name = "account_id";

struct CheckedRegistry {
    std::vector<char> bytes;
    size_t account_offset;
    std::vector<char> *account_value;
    std::map<std::pair<std::string, std::string>, size_t> value_offsets;
};

// Validate the complete loaded image against the template and disk before using it.
static std::optional<CheckedRegistry> check_registry(RegMgrState &regmgr) {
    if (!regmgr.system_dreg_valid || regmgr.system_dreg_path.empty() || regmgr.reg_category_template.empty() || regmgr.reg_template.empty()) {
        LOG_ERROR("Registry is not initialized");
        return std::nullopt;
    }

    std::ifstream file(regmgr.system_dreg_path, std::ios::binary | std::ios::ate);
    if (!file) {
        LOG_ERROR("Cannot open registry");
        return std::nullopt;
    }
    const auto length = file.tellg();
    if (length < 0 || static_cast<uintmax_t>(length) > std::numeric_limits<size_t>::max()) {
        LOG_ERROR("Invalid registry size");
        return std::nullopt;
    }
    CheckedRegistry result{ std::vector<char>(static_cast<size_t>(length)), 0, nullptr, {} };
    file.seekg(0);
    if (!file || (!result.bytes.empty() && !file.read(result.bytes.data(), length))) {
        LOG_ERROR("Cannot read complete registry");
        return std::nullopt;
    }

    size_t offset = 0;
    const auto consume = [&](size_t count, size_t &start) {
        if (count > result.bytes.size() - offset) {
            LOG_ERROR("Truncated registry");
            return false;
        }
        start = offset;
        offset += count;
        return true;
    };
    const auto padding = [](size_t size) -> size_t {
        return size < spaceSize ? spaceSize - size : 16 - size % 16;
    };

    size_t ignored = 0;
    if (!consume(spaceSize + 1, ignored))
        return std::nullopt;
    std::set<std::string> categories;
    for (const auto &category : regmgr.reg_category_template) {
        if (!categories.insert(category).second) {
            LOG_ERROR("Duplicate registry category");
            return std::nullopt;
        }
        const auto tmpl = regmgr.reg_template.find(category);
        const auto values = regmgr.system_dreg.find(category);
        if (tmpl == regmgr.reg_template.end() || values == regmgr.system_dreg.end()) {
            LOG_ERROR("Missing registry category");
            return std::nullopt;
        }
        size_t cat_offset;
        if (!consume(category.size(), cat_offset))
            return std::nullopt;
        if (std::memcmp(result.bytes.data() + cat_offset, category.data(), category.size()) != 0) {
            LOG_ERROR("Invalid registry category");
            return std::nullopt;
        }
        if (!consume(padding(category.size()), ignored))
            return std::nullopt;
        std::set<std::string> names;
        for (const auto &entry : tmpl->second) {
            if (!names.insert(entry.name).second || (entry.type != REG_TYPE_INT && entry.type != REG_TYPE_STR && entry.type != REG_TYPE_BIN)) {
                LOG_ERROR("Invalid registry template");
                return std::nullopt;
            }
            if (entry.type == REG_TYPE_INT && (entry.name.size() >= spaceSize || entry.size > spaceSize - entry.name.size() - 1)) {
                LOG_ERROR("Invalid integer registry entry");
                return std::nullopt;
            }
            const auto value = values->second.find(entry.name);
            if (value == values->second.end() || value->second.size() != entry.size) {
                LOG_ERROR("Invalid loaded registry value {}{} (expected {} bytes, loaded {})", category, entry.name, entry.size,
                    value == values->second.end() ? 0 : value->second.size());
                return std::nullopt;
            }
            size_t name_offset;
            if (!consume(entry.name.size(), name_offset))
                return std::nullopt;
            if (std::memcmp(result.bytes.data() + name_offset, entry.name.data(), entry.name.size()) != 0) {
                LOG_ERROR("Invalid registry entry name");
                return std::nullopt;
            }
            size_t value_offset;
            if (!consume(entry.type == REG_TYPE_INT ? spaceSize - entry.name.size() - entry.size - 1 : padding(entry.name.size()) - 1, ignored)
                || !consume(entry.size, value_offset))
                return std::nullopt;
            result.value_offsets.emplace(std::make_pair(category, entry.name), value_offset);
            if (entry.size && std::memcmp(result.bytes.data() + value_offset, value->second.data(), entry.size) != 0) {
                LOG_ERROR("Loaded registry differs from disk");
                return std::nullopt;
            }
            if (!consume(entry.type == REG_TYPE_INT ? 1 : padding(entry.size) + 1, ignored))
                return std::nullopt;
            if (category == account_category && entry.name == account_name) {
                if (entry.type != REG_TYPE_BIN || entry.size != 8) {
                    LOG_ERROR("Invalid account ID template");
                    return std::nullopt;
                }
                result.account_offset = value_offset;
                result.account_value = &value->second;
            }
        }
    }
    if (categories.size() != regmgr.reg_template.size() || !result.account_value) {
        LOG_ERROR("Missing account ID template");
        return std::nullopt;
    }
    return result;
}

class ExclusiveRegistryFile {
public:
    explicit ExclusiveRegistryFile(const fs::path &path)
        : path_(path) {}

    std::error_code create(const fs::path &original) {
#ifdef _WIN32
        DWORD needed = 0;
        GetFileSecurityW(original.c_str(), DACL_SECURITY_INFORMATION, nullptr, 0, &needed);
        const auto security_error = GetLastError();
        if (!needed || security_error != ERROR_INSUFFICIENT_BUFFER)
            return { static_cast<int>(security_error == ERROR_SUCCESS ? ERROR_INVALID_SECURITY_DESCR : security_error), std::system_category() };
        std::vector<char> descriptor(needed);
        if (!GetFileSecurityW(original.c_str(), DACL_SECURITY_INFORMATION, descriptor.data(), needed, &needed))
            return { static_cast<int>(GetLastError()), std::system_category() };
        SECURITY_ATTRIBUTES attributes{ sizeof(attributes), descriptor.data(), FALSE };
        handle_ = CreateFileW(path_.c_str(), GENERIC_WRITE, 0, &attributes, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (handle_ == INVALID_HANDLE_VALUE)
            return { static_cast<int>(GetLastError()), std::system_category() };
#else
        (void)original;
        handle_ = open(path_.c_str(), O_WRONLY | O_CREAT | O_EXCL, 0600);
        if (handle_ < 0)
            return { errno, std::generic_category() };
#endif
        remove_ = true;
        return {};
    }

    ExclusiveRegistryFile(const ExclusiveRegistryFile &) = delete;
    ExclusiveRegistryFile &operator=(const ExclusiveRegistryFile &) = delete;

    ~ExclusiveRegistryFile() {
#ifdef _WIN32
        if (handle_ != INVALID_HANDLE_VALUE)
            CloseHandle(handle_);
#else
        if (handle_ >= 0)
            close(handle_);
#endif
        if (remove_) {
            std::error_code ignored;
            fs::remove(path_, ignored);
        }
    }

    bool write_and_close(const std::vector<char> &bytes) {
        size_t offset = 0;
        while (offset < bytes.size()) {
#ifdef _WIN32
            DWORD written = 0;
            const DWORD count = static_cast<DWORD>(std::min(bytes.size() - offset, static_cast<size_t>(std::numeric_limits<DWORD>::max())));
            if (!WriteFile(handle_, bytes.data() + offset, count, &written, nullptr)) {
                LOG_ERROR("Cannot write registry file: {}", std::error_code(static_cast<int>(GetLastError()), std::system_category()).message());
                return false;
            }
#else
            const auto count = std::min(bytes.size() - offset, static_cast<size_t>(std::numeric_limits<ssize_t>::max()));
            const auto written = write(handle_, bytes.data() + offset, count);
            if (written < 0 && errno == EINTR)
                continue;
            if (written < 0) {
                LOG_ERROR("Cannot write registry file: {}", std::error_code(errno, std::generic_category()).message());
                return false;
            }
#endif
            if (!written) {
                LOG_ERROR("Short registry write");
                return false;
            }
            offset += static_cast<size_t>(written);
        }
#ifdef _WIN32
        if (!FlushFileBuffers(handle_)) {
            LOG_ERROR("Cannot flush registry file: {}", std::error_code(static_cast<int>(GetLastError()), std::system_category()).message());
            return false;
        }
        if (!CloseHandle(handle_)) {
            const auto error = GetLastError();
            handle_ = INVALID_HANDLE_VALUE;
            LOG_ERROR("Cannot close registry file: {}", std::error_code(static_cast<int>(error), std::system_category()).message());
            return false;
        }
        handle_ = INVALID_HANDLE_VALUE;
#else
        if (fsync(handle_) != 0) {
            LOG_ERROR("Cannot flush registry file: {}", std::error_code(errno, std::generic_category()).message());
            return false;
        }
        if (close(handle_) != 0) {
            const auto error = errno;
            handle_ = -1;
            LOG_ERROR("Cannot close registry file: {}", std::error_code(error, std::generic_category()).message());
            return false;
        }
        handle_ = -1;
#endif
        return true;
    }

    void keep() { remove_ = false; }

private:
    fs::path path_;
    bool remove_ = false;
#ifdef _WIN32
    HANDLE handle_ = INVALID_HANDLE_VALUE;
#else
    int handle_ = -1;
#endif
};

enum class RegistryReplaceResult { failed,
    replaced,
    replaced_not_synced };

static RegistryReplaceResult replace_registry_image(const fs::path &path, const std::vector<char> &bytes) {
    static std::atomic<uint64_t> next_temp{ 0 };
    for (unsigned attempt = 0; attempt < 100; ++attempt) {
        auto temp_path = path;
        temp_path += ".registry.tmp." + std::to_string(next_temp.fetch_add(1, std::memory_order_relaxed));
        ExclusiveRegistryFile temp(temp_path);
        const auto error = temp.create(path);
        if (error) {
#ifdef _WIN32
            if (error.value() == ERROR_FILE_EXISTS || error.value() == ERROR_ALREADY_EXISTS)
#else
            if (error.value() == EEXIST)
#endif
                continue;
            LOG_ERROR("Cannot create registry staging file: {}", error.message());
            return RegistryReplaceResult::failed;
        }
        if (!temp.write_and_close(bytes))
            return RegistryReplaceResult::failed;
#ifdef _WIN32
        if (!MoveFileExW(temp_path.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
            LOG_ERROR("Cannot replace registry: {}", std::error_code(static_cast<int>(GetLastError()), std::system_category()).message());
            return RegistryReplaceResult::failed;
        }
#else
        if (::rename(temp_path.c_str(), path.c_str()) != 0) {
            LOG_ERROR("Cannot replace registry: {}", std::error_code(errno, std::generic_category()).message());
            return RegistryReplaceResult::failed;
        }
#endif
        temp.keep();
#ifndef _WIN32
        const int directory = open(path.parent_path().c_str(), O_RDONLY | O_DIRECTORY);
        if (directory < 0) {
            LOG_ERROR("Cannot open registry directory for synchronization: {}", std::error_code(errno, std::generic_category()).message());
            return RegistryReplaceResult::replaced_not_synced;
        }
        const int result = fsync(directory);
        const int sync_error = errno;
        close(directory);
        if (result != 0) {
            LOG_ERROR("Cannot synchronize registry directory: {}", std::error_code(sync_error, std::generic_category()).message());
            return RegistryReplaceResult::replaced_not_synced;
        }
#endif
        return RegistryReplaceResult::replaced;
    }
    LOG_ERROR("Cannot allocate a registry staging file");
    return RegistryReplaceResult::failed;
}

std::optional<std::array<uint8_t, 8>> get_account_id(RegMgrState &regmgr) {
    std::lock_guard<std::mutex> lock(regmgr.mutex);
    const auto registry = check_registry(regmgr);
    if (!registry)
        return std::nullopt;
    std::array<uint8_t, 8> account_id;
    std::memcpy(account_id.data(), registry->account_value->data(), account_id.size());
    return account_id;
}

bool set_account_id(RegMgrState &regmgr, const std::array<uint8_t, 8> &account_id) {
    return set_account_id(regmgr, account_id, false);
}

bool set_account_id(RegMgrState &regmgr, const std::array<uint8_t, 8> &account_id, bool backup_original) {
    std::lock_guard<std::mutex> lock(regmgr.mutex);
    auto registry = check_registry(regmgr);
    if (!registry)
        return false;
    if (backup_original) {
        auto backup_path = regmgr.system_dreg_path;
        backup_path += ".bak";
        ExclusiveRegistryFile backup(backup_path);
        const auto error = backup.create(regmgr.system_dreg_path);
        if (error) {
            LOG_ERROR("Cannot create registry backup: {}", error.message());
            return false;
        }
        if (!backup.write_and_close(registry->bytes))
            return false;
        backup.keep();
    }
    std::memcpy(registry->bytes.data() + registry->account_offset, account_id.data(), account_id.size());

    const auto result = replace_registry_image(regmgr.system_dreg_path, registry->bytes);
    if (result != RegistryReplaceResult::failed)
        std::memcpy(registry->account_value->data(), account_id.data(), account_id.size());
    return result == RegistryReplaceResult::replaced;
}

static bool load_system_dreg(RegMgrState &regmgr) {
    std::ifstream file(regmgr.system_dreg_path, std::ios::in | std::ios::binary);
    if (file.is_open()) {
        // Initialize the space buffer
        std::vector<char> space(spaceSize + 1);

        // Skip the space
        file.read(space.data(), spaceSize + 1);
        if (!file)
            return false;

        for (const auto &cat : regmgr.reg_category_template) {
            // Read the category
            const uint32_t cat_size = static_cast<uint32_t>(cat.size());
            std::vector<char> category(cat_size);
            file.read(category.data(), cat_size);
            if (!file)
                return false;
            const auto category_str = std::string(category.begin(), category.end());
            if (category_str != cat) {
                LOG_ERROR("Invalid registry category, expected: {}", cat);
                return false;
            }

            // Skip the space
            const auto diff_cat = get_space_size(cat_size);
            space.resize(diff_cat);
            file.read(space.data(), diff_cat);
            if (!file)
                return false;

            for (const auto &entry : regmgr.reg_template[cat]) {
                // Read the name
                const uint32_t name_size = static_cast<uint32_t>(entry.name.size());
                std::vector<char> name(name_size);
                file.read(name.data(), name_size);
                if (!file)
                    return false;
                const auto name_str = std::string(name.begin(), name.end());
                if (name_str != entry.name) {
                    LOG_ERROR("Invalid registry entry name, expected: {} in category: {}", entry.name, cat);
                    return false;
                }

                // Skip the space
                const auto diff_name = (entry.type == REG_TYPE_INT ? spaceSize - name_size - entry.size : get_space_size(name_size)) - 1;
                space.resize(diff_name);
                file.read(space.data(), diff_name);
                if (!file)
                    return false;

                // Read the value
                const uint32_t value_size = entry.size;
                auto &value = regmgr.system_dreg[cat][entry.name];
                value.resize(value_size);
                file.read(value.data(), value_size);
                if (!file)
                    return false;

                // Skip the space
                const auto diff_value = (entry.type == REG_TYPE_INT) ? 1 : get_space_size(value_size) + 1;
                space.resize(diff_value);
                file.read(space.data(), diff_value);
                if (!file)
                    return false;
            }
        }

        file.close();
    }

    return !regmgr.system_dreg.empty();
}

static void save_system_dreg(RegMgrState &regmgr) {
    for (const auto &category : regmgr.reg_category_template) {
        const auto entries = regmgr.reg_template.find(category);
        const auto values = regmgr.system_dreg.find(category);
        if (entries == regmgr.reg_template.end() || values == regmgr.system_dreg.end()) {
            LOG_ERROR("Cannot save registry: missing category {}", category);
            return;
        }
        for (const auto &entry : entries->second) {
            const auto value = values->second.find(entry.name);
            if (value == values->second.end() || value->second.size() != entry.size) {
                LOG_ERROR("Cannot save registry: invalid value {}{}", category, entry.name);
                return;
            }
        }
    }
    std::ofstream file(regmgr.system_dreg_path, std::ios::out | std::ios::binary);
    if (file.is_open()) {
        // Initialize the space buffer
        std::vector<char> space(spaceSize + 1);

        // Write the space
        file.write(space.data(), spaceSize + 1);

        // Write each category
        for (const auto &cat : regmgr.reg_category_template) {
            // Write the category
            const uint32_t cat_size = static_cast<uint32_t>(cat.size());
            std::vector<char> category(cat_size);
            strncpy(category.data(), cat.c_str(), cat_size);
            file.write(cat.c_str(), cat_size);

            // Write the space
            const auto diff_cat = get_space_size(cat_size);
            space.resize(diff_cat);
            file.write(space.data(), diff_cat);

            // Write each entry
            for (const auto &entry : regmgr.reg_template[cat]) {
                // Write the name
                const uint32_t name_size = static_cast<uint32_t>(entry.name.size());
                std::vector<char> name(name_size);
                strncpy(name.data(), entry.name.c_str(), name_size);
                file.write(name.data(), name_size);

                // Write the space
                const auto diff_name = (entry.type == REG_TYPE_INT ? spaceSize - name_size - entry.size : get_space_size(name_size)) - 1;
                space.resize(diff_name);
                file.write(space.data(), diff_name);

                // Write the value
                const uint32_t value_size = entry.size;
                file.write(regmgr.system_dreg[cat][entry.name].data(), value_size);

                // Write the space
                const auto diff_value = (entry.type == REG_TYPE_INT) ? 1 : get_space_size(value_size) + 1;
                space.resize(diff_value);
                file.write(space.data(), diff_value);
            }
        }

        file.close();
    }
}

static bool init_system_dreg(RegMgrState &regmgr) {
    std::vector<char> bytes(spaceSize + 1, 0);
    const auto append_padding = [&](size_t count) { bytes.insert(bytes.end(), count, 0); };
    for (const auto &category : regmgr.reg_category_template) {
        const auto cat = regmgr.reg_template.find(category);
        if (cat == regmgr.reg_template.end() || category.size() > UINT32_MAX) {
            LOG_ERROR("Invalid registry template");
            return false;
        }
        bytes.insert(bytes.end(), category.begin(), category.end());
        append_padding(get_space_size(static_cast<uint32_t>(category.size())));
        for (const auto &entry : cat->second) {
            if (entry.name.size() > UINT32_MAX || (entry.type == REG_TYPE_INT && (entry.name.size() >= spaceSize || entry.size > spaceSize - entry.name.size() - 1))) {
                LOG_ERROR("Invalid registry template");
                return false;
            }
            bytes.insert(bytes.end(), entry.name.begin(), entry.name.end());
            append_padding(entry.type == REG_TYPE_INT ? spaceSize - entry.name.size() - entry.size - 1 : get_space_size(static_cast<uint32_t>(entry.name.size())) - 1);
            auto value = entry.init_value;
            value.resize(entry.size, 0);
            bytes.insert(bytes.end(), value.begin(), value.end());
            append_padding(entry.type == REG_TYPE_INT ? 1 : get_space_size(entry.size) + 1);
        }
    }
    // No original exists yet. Exclusive creation also protects against races.
    ExclusiveRegistryFile file(regmgr.system_dreg_path);
#ifdef _WIN32
    // A newly created registry has no source ACL to copy; use its parent directory.
    const auto error = file.create(regmgr.system_dreg_path.parent_path());
#else
    const auto error = file.create(regmgr.system_dreg_path);
#endif
    if (error) {
        LOG_ERROR("Cannot create registry file: {}", error.message());
        return false;
    }
    if (!file.write_and_close(bytes))
        return false;
    file.keep();
    return true;
}

static bool reg_category_or_name_is_empty(const RegMgrState &regmgr, const std::string &category, const std::string &name) {
    return !regmgr.system_dreg_valid || regmgr.reg_template.empty() || category.empty() || name.empty();
}

static std::string fix_category(const std::string &category) {
    return category.back() == '/' ? category : category + '/';
}

static std::string set_default_value(RegMgrState &regmgr, const std::string &category, const std::string &name) {
    if (reg_category_or_name_is_empty(regmgr, category, name))
        return {};

    const auto &reg = std::find_if(regmgr.reg_template[category].begin(), regmgr.reg_template[category].end(), [&](const auto &n) {
        return n.name == name;
    });

    if (reg == regmgr.reg_template[category].end()) {
        LOG_ERROR("No default value found for {}{}", category, name);
        return {};
    }

    auto &sys = regmgr.system_dreg[category][name];
    sys = reg->init_value;
    sys.resize(reg->size, 0);

    LOG_INFO("Successfully set default value for {}{}", category, name);

    save_system_dreg(regmgr);

    return std::string(reg->init_value.begin(), reg->init_value.end());
}

void get_bin_value(RegMgrState &regmgr, const std::string &category, const std::string &name, void *buf, uint32_t bufSize) {
    if (reg_category_or_name_is_empty(regmgr, category, name))
        return;

    std::lock_guard<std::mutex> lock(regmgr.mutex);
    const auto &sys = regmgr.system_dreg[fix_category(category)][name];

    memcpy(buf, sys.data(), bufSize);
}

static bool set_variable_value(RegMgrState &regmgr, const std::string &category, const std::string &name,
    const void *data, uint32_t size, RegType type) {
    const std::lock_guard<std::mutex> lock(regmgr.mutex);
    if (reg_category_or_name_is_empty(regmgr, category, name) || (!data && size)) {
        LOG_ERROR("Cannot update registry: invalid key or buffer");
        return false;
    }
    const auto fixed_category = fix_category(category);
    const auto category_template = regmgr.reg_template.find(fixed_category);
    if (category_template == regmgr.reg_template.end()) {
        LOG_ERROR("Cannot update unknown registry category {}", fixed_category);
        return false;
    }
    const auto entry = std::find_if(category_template->second.begin(), category_template->second.end(), [&](const RegValue &item) {
        return item.name == name;
    });
    if (entry == category_template->second.end() || entry->type != type || size > entry->size) {
        LOG_ERROR("Cannot update registry value {}{}: unknown key, wrong type, or oversized input", fixed_category, name);
        return false;
    }
    auto checked = check_registry(regmgr);
    if (!checked)
        return false;
    const auto offset = checked->value_offsets.find({ fixed_category, name });
    if (offset == checked->value_offsets.end()) {
        LOG_ERROR("Cannot locate registry value {}{} in the checked image", fixed_category, name);
        return false;
    }
    std::vector<char> new_value(entry->size, 0);
    if (size)
        std::memcpy(new_value.data(), data, size);
    std::copy(new_value.begin(), new_value.end(), checked->bytes.begin() + offset->second);
    const auto result = replace_registry_image(regmgr.system_dreg_path, checked->bytes);
    if (result != RegistryReplaceResult::failed)
        regmgr.system_dreg.at(fixed_category).at(name) = std::move(new_value);
    return result == RegistryReplaceResult::replaced;
}

bool set_bin_value(RegMgrState &regmgr, const std::string &category, const std::string &name, const void *buf, const uint32_t bufSize) {
    return set_variable_value(regmgr, category, name, buf, bufSize, REG_TYPE_BIN);
}

int32_t get_int_value(RegMgrState &regmgr, const std::string &category, const std::string &name) {
    if (reg_category_or_name_is_empty(regmgr, category, name))
        return 0;

    std::lock_guard<std::mutex> lock(regmgr.mutex);

    const auto cat = fix_category(category);

    return byte_swap(*reinterpret_cast<const uint32_t *>(regmgr.system_dreg[cat][name].data()));
}

void set_int_value(RegMgrState &regmgr, const std::string &category, const std::string &name, const int32_t value) {
    if (reg_category_or_name_is_empty(regmgr, category, name))
        return;

    std::lock_guard<std::mutex> lock(regmgr.mutex);

    *reinterpret_cast<int32_t *>(regmgr.system_dreg[fix_category(category)][name].data()) = byte_swap(value);

    save_system_dreg(regmgr);
}

std::string get_str_value(RegMgrState &regmgr, const std::string &category, const std::string &name) {
    if (reg_category_or_name_is_empty(regmgr, category, name))
        return {};

    std::lock_guard<std::mutex> lock(regmgr.mutex);
    const auto &sys = regmgr.system_dreg[fix_category(category)][name];

    return std::string(sys.begin(), sys.end());
}

bool set_str_value(RegMgrState &regmgr, const std::string &category, const std::string &name, const char *value, const uint32_t bufSize) {
    return set_variable_value(regmgr, category, name, value, bufSize, REG_TYPE_STR);
}

static void reset_regmgr_unlocked(RegMgrState &regmgr) {
    regmgr.system_dreg.clear();
    regmgr.system_dreg_path.clear();
    regmgr.system_dreg_valid = false;
    regmgr.reg_category_template.clear();
    regmgr.reg_template.clear();
}

void reset_regmgr(RegMgrState &regmgr) {
    std::lock_guard<std::mutex> lock(regmgr.mutex);
    reset_regmgr_unlocked(regmgr);
}

void init_regmgr(RegMgrState &regmgr, const fs::path &vita_fs_path) {
    std::lock_guard<std::mutex> lock(regmgr.mutex);
    // Load the registry template
    const auto reg = decryptRegistryFile(vita_fs_path / "os0/kd/registry.db0");
    if (reg.empty()) {
        regmgr.system_dreg_valid = false;
        if (regmgr.system_dreg_path != vita_fs_path / "vd0/registry/system.dreg")
            reset_regmgr_unlocked(regmgr);
        return;
    }

    // Initialize the registry template
    regmgr.system_dreg_valid = false;
    if (!init_reg_template(regmgr, reg)) {
        regmgr.system_dreg.clear();
        regmgr.reg_category_template.clear();
        regmgr.reg_template.clear();
        LOG_WARN("Invalid registry template");
        return;
    }

    // Initialize the system.dreg
    regmgr.system_dreg_path = vita_fs_path / "vd0/registry/system.dreg";
    regmgr.system_dreg.clear();
    regmgr.system_dreg_valid = load_system_dreg(regmgr);
    if (!regmgr.system_dreg_valid) {
        regmgr.system_dreg.clear();
        std::error_code error;
        const auto status = fs::symlink_status(regmgr.system_dreg_path, error);
        if (error == std::errc::no_such_file_or_directory || (!error && status.type() == fs::file_type::not_found)) {
            LOG_WARN("system.dreg not found, attempting to create it");
            if (!init_system_dreg(regmgr)) {
                LOG_WARN("Failed to create system.dreg");
            }
            regmgr.system_dreg.clear();
            regmgr.system_dreg_valid = load_system_dreg(regmgr);
            if (!regmgr.system_dreg_valid)
                regmgr.system_dreg.clear();
        } else {
            LOG_WARN("Failed to load existing system.dreg; leaving it unchanged");
        }
    }
}

std::pair<std::string, std::string> get_category_and_name_by_id(const int id, const std::string &export_name) {
    switch (id) {
    case 0x00023FC2:
        return { "/CONFIG/ACCESSIBILITY/", "large_text" };
    case 0x00033818:
        return { "/CONFIG/NP/", "env" };
    case 0x00037502:
        return { "/CONFIG/SYSTEM/", "language" };
    case 0x000504E4:
        return { "/CONFIG/NP2/TELEPORT/", "passcode_client" };
    case 0x00068303:
        return { "/CONFIG/BROWSER/ADDIN/TRENDMICRO/", "tm_ec_ttl" };
    case 0x00088776:
        return { "/CONFIG/DATE/", "date_format" };
    case 0x000A0495:
        return { "/CONFIG/NP/", "nav_only" };
    case 0x000B6ECD:
        return { "/CONFIG/NP/", "np_ad_clock_diff" };
    case 0x000B73CD:
        return { "/CONFIG/NP/", "debug" };
    case 0x000D18E5:
        return { "/CONFIG/NP/", "np_geo_filtering" };
    case 0x00100591:
        return { "/CONFIG/DATE/", "time_zone" };
    case 0x00134C03:
        return { "/CONFIG/NET/", "pspnet_adhoc_ssid_prefix" };
    case 0x00146E23:
        return { "/CONFIG/GAME/", "show_debug_info" };
    case 0x00154A2C:
        return { "/CONFIG/GAME/", "fake_free_space" };
    case 0x00156489:
        return { "/CONFIG/NP/", "debug_ingame_commerce2" };
    case 0x00168B9B:
        return { "/CONFIG/NP2/", "tpps_proxy_password" };
    case 0x00186122:
        return { "/CONFIG/SECURITY/PARENTAL/", "passcode" };
    case 0x001B2292:
        return { "/CONFIG/BROWSER/ADDIN/TRENDMICRO/", "tm_ec_ttl_update_time" };
    case 0x00229142:
        return { "/CONFIG/SYSTEM/", "button_assign" };
    case 0x0022B191:
        return { "/CONFIG/NP2/", "tpps_proxy_port" };
    case 0x0025CE9A:
        return { "/CONFIG/GAME/", "fake_free_space_quota" };
    case 0x002FDFB4:
        return { "/CONFIG/DISPLAY/", "hdmi_out_scaling_ratio" };
    case 0x00313905:
        return { "/CONFIG/NP2/", "test_patch" };
    case 0x003317A1:
        return { "/CONFIG/NP2/", "trophy_setup_dialog_debug" };
    case 0x0036F14E:
        return { "/CONFIG/NP2/TELEPORT/", "enable_media_transfer" };
    case 0x003CB6A4:
        return { "/DEVENV/TOOL/", "gpi_switch" };
    case 0x00424500:
        return { "/CONFIG/GAME/", "fake_sdslot_broken" };
    case 0x00450F32:
        return { "/CONFIG/NP/", "account_id" };
    case 0x004E7A16:
        return { "/CONFIG/NP2/TELEPORT/", "target_name" };
    case 0x004F7E60:
        return { "/CONFIG/PS4LINK/", "counter" };
    case 0x00505BCE:
        return { "/CONFIG/NP2/", "fake_ratelimit" };
    case 0x0051F6AE:
        return { "/CONFIG/SPECIFIC/", "idu_mode" };
    case 0x00528C0D:
        return { "/CONFIG/NP2/", "ignore_titleid" };
    case 0x00563BFE:
        return { "/CONFIG/NET/", "ssl_cert_ignorable" };
    case 0x00598438:
        return { "/CONFIG/SYSTEM/", "username" };
    case 0x005F6737:
        return { "/CONFIG/NP2/TWITTER/", "access_token" };
    case 0x00611DC9:
        return { "/CONFIG/ACCESSIBILITY/", "bold_text" };
    case 0x00612B3E:
        return { "/CONFIG/BROWSER/", "web_security_status" };
    case 0x00646A8E:
        return { "/CONFIG/NP2/", "tpps_proxy_server" };
    case 0x00668503:
        return { "/CONFIG/DATE/", "time_format" };
    case 0x00683DCD:
        return { "/CONFIG/SYSTEM/", "key_pad" };
    case 0x006FF829:
        return { "/CONFIG/NP2/", "tpps_proxy_flag" };
    case 0x00711659:
        return { "/CONFIG/SECURITY/PARENTAL/", "content_start_control" };
    case 0x00760538:
        return { "/CONFIG/DATE/", "summer_time" };
    case 0x007C9764:
        return { "/CONFIG/NP2/", "fake_plus" };
    case 0x007D12C4:
        return { "/CONFIG/GAME/", "fake_contents_max" };
    case 0x007F9315:
        return { "/CONFIG/DATE/", "is_summer_time" };
    case 0x0081649F:
        return { "/CONFIG/BROWSER/ADDIN/TRENDMICRO/", "tm_service" };
    case 0x00872621:
        return { "/CONFIG/BROWSER/ADDIN/TRENDMICRO/", "tm_service_sub_status" };
    case 0x0089C9CF:
        return { "/CONFIG/SECURITY/PARENTAL/", "store_start_control" };
    case 0x008A2AD7:
        return { "/CONFIG/ACCESSIBILITY/", "contrast" };
    case 0x008C3860:
        return { "/CONFIG/BROWSER/DEBUG/", "net_dbg_config" };
    case 0x008D89EB:
        return { "/CONFIG/NP2/TELEPORT/", "wol_target_mac_address" };
    case 0x008E3939:
        return { "/CONFIG/MUSIC/MUSIC_APP/", "impose_audio_balance" };
    case 0x008EB468:
        return { "/CONFIG/NP2/", "tpps_proxy_user_name" };
    case 0x008F94F9:
        return { "/CONFIG/NP/", "country" };
    case 0x0091F34F:
        return { "/CONFIG/NP2/TWITTER/", "access_token_secret" };
    case 0x0093C981:
        return { "/CONFIG/PSM/", "revocation_check_req" };
    case 0x0094E320:
        return { "/CONFIG/PS4LINK/", "keys" };
    case 0x009623D0:
        return { "/CONFIG/GAME/", "fake_no_memory_card" };
    case 0x00971FA1:
        return { "/CONFIG/SHELL/", "voice_priority" };
    case 0x00987180:
        return { "/CONFIG/NP2/TELEPORT/", "initial_target" };
    case 0x00988B81:
        return { "/CONFIG/PSNOW/", "app_cached_url" };
    default:
        LOG_WARN("{}: unknown id: {}", export_name, log_hex(id));
        return { "", "" };
    }
}

} // namespace regmgr
