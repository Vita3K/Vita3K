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

#include <boost/algorithm/string/trim.hpp>
#include <compat/state.h>

#ifdef WIN32
#include <io.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <netdb.h>
#include <sys/socket.h>
#endif

#include <openssl/err.h>
#include <openssl/ssl.h>

#include <emuenv/state.h>
#include <gui/state.h>

#include <pugixml.hpp>

enum LabelIdState {
    Nothing = 1260231569, // 0x4b1d9b91
    Bootable = 1344750319, // 0x502742ef
    Intro = 1260231381, // 0x4B9F5E5D
    Menu = 1344751053, // 0x4F1B9135
    Ingame_Less = 1344752299, // 0x4F7B6B3B
    Ingame_More = 1260231985, // 0x4B2A9819
    Playable = 920344019, // 0x36db55d3
};

namespace compat {

static std::string db_updated_at;
static const uint32_t db_version = 1;

bool load_compat_app_db(GuiState &gui, EmuEnvState &emuenv) {
    const auto app_compat_db_path = fs::path(emuenv.base_path) / "cache/app_compat_db.xml";
    if (!fs::exists(app_compat_db_path)) {
        LOG_WARN("Compatibility database not found at {}.", app_compat_db_path.string());
        return false;
    }

    // Parse and load file of compatibility database
    pugi::xml_document doc;
    pugi::xml_parse_result result = doc.load_file(app_compat_db_path.c_str());
    if (!result) {
        LOG_ERROR("Compatibility database {} could not be loaded: {}", app_compat_db_path.string(), result.description());
        return false;
    }

    // Check compatibility database version
    const auto compatibility = doc.child("compatibility");
    const auto version = compatibility.attribute("version").as_uint();
    if (db_version != version) {
        LOG_WARN("Compatibility database version {} is outdated, download it again.", version);
        return false;
    }

    // Clear old compat database
    gui.compat.compat_db_loaded = false;
    gui.compat.app_compat_db.clear();

    db_updated_at = compatibility.attribute("db_updated_at").as_string();

    //  Load compatibility database
    for (const auto &app : doc.child("compatibility")) {
        const std::string title_id = app.attribute("title_id").as_string();
        const auto issue_id = app.child("issue_id").text().as_uint();
        auto state = CompatibilityState::Unknown;
        const auto labels = app.child("labels");
        if (!labels.empty()) {
            for (const auto &label : labels) {
                const auto label_id = static_cast<LabelIdState>(label.text().as_uint());
                switch (label_id) {
                case LabelIdState::Nothing: state = Nothing; break;
                case LabelIdState::Bootable: state = Bootable; break;
                case LabelIdState::Intro: state = Intro; break;
                case LabelIdState::Menu: state = Menu; break;
                case LabelIdState::Ingame_Less: state = Ingame_Less; break;
                case LabelIdState::Ingame_More: state = Ingame_More; break;
                case LabelIdState::Playable: state = Playable; break;
                default: break;
                }
            }
        }
        const auto updated_at = app.child("updated_at").text().as_llong();

        if (state == Unknown)
            LOG_WARN("App with title ID {} has an issue but no status label. Please check GitHub issue {} and request a status label to be added.", title_id, issue_id);

        gui.compat.app_compat_db[title_id] = { issue_id, state, updated_at };
    }

    // Update compatibility status of all user apps
    for (auto &app : gui.app_selector.user_apps)
        app.compat = gui.compat.app_compat_db.contains(app.title_id) ? gui.compat.app_compat_db[app.title_id].state : CompatibilityState::Unknown;

    return !gui.compat.app_compat_db.empty();
}

constexpr int READ_BUFFER_SIZE = 1048;
static std::string get_https_response(const std::string &url) {
#ifdef WIN32
    WORD versionWanted = MAKEWORD(2, 2);
    WSADATA wsaData;
    WSAStartup(versionWanted, &wsaData);
#endif

    // Initialize SSL
    const auto ctx = SSL_CTX_new(SSLv23_client_method());
    if (!ctx) {
        LOG_ERROR("Error creating SSL context");
        return {};
    }

    // Create socket
    const auto sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        LOG_ERROR("ERROR opening socket: {}", sockfd);
        return {};
    }

    // Parse URL to get host and uri
    std::string host, uri;
    size_t start = url.find("://");
    if (start != std::string::npos) {
        start += 3; // skip "://"
        size_t end = url.find("/", start);
        if (end != std::string::npos) {
            host = url.substr(start, end - start);
            uri = url.substr(end);
        } else {
            host = url.substr(start);
            uri = "/";
        }
    }

    // Set address info option
    const addrinfo hints = {
        AI_PASSIVE, /* For wildcard IP address */
        AF_UNSPEC, /* Allow IPv4 or IPv6 */
        SOCK_DGRAM, /* Datagram socket */
        0, /* Any protocol */
    };
    addrinfo *result = { 0 };

    // Get address info with using host and port
    auto ret = getaddrinfo(host.c_str(), "https", &hints, &result);
    if (ret < 0) {
        LOG_ERROR("getaddrinfo = {}", ret);
        return {};
    }

    // Connect to host
    ret = connect(sockfd, result->ai_addr, static_cast<uint32_t>(result->ai_addrlen));
    if (ret < 0) {
        LOG_ERROR("connect({},...) = {}, errno={}({})", sockfd, ret, errno, strerror(errno));
        return {};
    }

    // Create and connect SSL
    SSL *ssl;
    ssl = SSL_new(ctx);
    SSL_set_fd(ssl, static_cast<uint32_t>(sockfd));
    if (SSL_connect(ssl) <= 0) {
        char err_buf[256];
        ERR_error_string_n(ERR_get_error(), err_buf, sizeof(err_buf));
        LOG_ERROR("Error establishing SSL connection: {}", err_buf);
        return {};
    }

    // Send HTTP GET request to extracted URI
    std::string request = "GET " + uri + " HTTP/1.1\r\n";
    request += "Host: " + host + "\r\n";
    request += "User-Agent: OpenSSL/1.1.1\r\n";
    request += "Connection: close\r\n\r\n";
    if (SSL_write(ssl, request.c_str(), static_cast<uint32_t>(request.length())) <= 0) {
        char err_buf[256];
        ERR_error_string_n(ERR_get_error(), err_buf, sizeof(err_buf));
        LOG_ERROR("Error sending request: {}", err_buf);
        return {};
    }

    std::array<char, READ_BUFFER_SIZE> read_buffer{};
    std::string response;
    while (auto bytes_read = SSL_read(ssl, read_buffer.data(), static_cast<uint32_t>(read_buffer.size()))) {
        response += std::string(read_buffer.data(), bytes_read);
    }

    SSL_shutdown(ssl);
    SSL_free(ssl);
    SSL_CTX_free(ctx);

#ifdef WIN32
    closesocket(sockfd);
    WSACleanup();
#else
    close(sockfd);
#endif // WIN32

    boost::trim(response);

    return response;
}

static std::string get_updated_at() {
    const std::string latest_link = "https://api.github.com/repos/Vita3K/compatibility/releases/latest";
    std::string updated_at;

    // Get the response of last release from the GitHub API
    const auto response = get_https_response(latest_link);

    // Check if the response is not empty
    if (!response.empty()) {
        // Extract the content of the response
        const std::string content = response.substr(response.find("\r\n\r\n") + 4);

        // Get the Updated at of content
        updated_at = content.substr(content.find("Updated at:") + 12, content.find_last_of("\r\n}") - content.find("Updated at:") - 13);
    }

    return updated_at;
}

static bool download_app_compat_db(const std::string &output_file_path) {
    const std::string app_compat_db_link = "https://github.com/Vita3K/compatibility/releases/download/compat_db/app_compat_db.xml";

    // Get the response of the app compat db
    auto response = get_https_response(app_compat_db_link);

    // Check if the response is not empty
    if (!response.empty()) {
        // Check if the response is a redirection
        if (response.find("HTTP/1.1 302 Found") != std::string::npos) {
            // Extract the redirection URL from the response
            std::string location_header = "Location: ";
            size_t location_index = response.find(location_header);
            size_t end_of_location_index = response.find("\r\n", location_index + location_header.length());
            const auto redirected_url = response.substr(location_index + location_header.length(), end_of_location_index - location_index - location_header.length());

            // Get response of redirect url
            response = get_https_response(redirected_url);
            if (response.empty()) {
                LOG_ERROR("Failed to get response on redirected url: {}", redirected_url);
                return false;
            }
        }

        // Get the XML content from the response
        std::string xml = response.substr(response.find("\r\n\r\n") + 4);

        // Create the xml output file
        std::ofstream output_file(output_file_path, std::ios::binary);
        if (!output_file.is_open()) {
            LOG_ERROR("Failed to open output file: {}", output_file_path);
            return false;
        }

        // Write the XML content to the output file
        output_file.write(xml.c_str(), xml.length());
        output_file.close();

        return fs::exists(output_file_path);
    }

    return false;
}

bool update_compat_app_db(GuiState &gui, EmuEnvState &emuenv) {
    const auto compat_db_path = fs::path(emuenv.base_path) / "cache/app_compat_db.xml";
    gui.info_message.function = SPDLOG_FUNCTION;

    // Get current date of last issue updated
    const auto updated_at = get_updated_at();
    if (updated_at.empty()) {
        gui.info_message.level = spdlog::level::err;
        gui.info_message.msg = "Failed to get current compatibility database version, check firewall/internet access, try again later.";
        return false;
    }

    // Check if database is up to date
    if (db_updated_at == updated_at) {
        LOG_INFO("Applications compatibility database is up to date.");
        return false;
    }

    const auto compat_db_exist = fs::exists(compat_db_path);

    LOG_INFO("Applications compatibility database is {}, attempting to download latest updated at: {}", compat_db_exist ? "outdated" : "missing", updated_at);

    if (!download_app_compat_db(compat_db_path.string())) {
        gui.info_message.level = spdlog::level::err;
        gui.info_message.msg = fmt::format("Failed to download Applications compatibility database updated at: {}", updated_at);
        return false;
    }

    const auto old_db_updated_at = db_updated_at;
    const auto old_compat_count = !gui.compat.app_compat_db.empty() ? gui.compat.app_compat_db.size() : 0;

    gui.compat.compat_db_loaded = load_compat_app_db(gui, emuenv);
    if (!gui.compat.compat_db_loaded || (db_updated_at != updated_at)) {
        gui.info_message.level = spdlog::level::err;
        gui.info_message.msg = fmt::format("Failed to load Applications compatibility database downloaded updated at: {}", updated_at);
        return false;
    }

    gui.info_message.level = spdlog::level::info;

    if (compat_db_exist) {
        const auto dif = static_cast<int32_t>(gui.compat.app_compat_db.size() - old_compat_count);
        if (dif > 0)
            gui.info_message.msg = fmt::format("The compatibility database was successfully updated from {} to {}.\n\n{} new application(s) are listed!", old_db_updated_at, db_updated_at, dif);
        else
            gui.info_message.msg = fmt::format("The compatibility database was successfully updated from {} to {}.\n\n{} applications are listed!", old_db_updated_at, db_updated_at, gui.compat.app_compat_db.size());
    } else
        gui.info_message.msg = fmt::format("The compatibility database updated at {} has been successfully downloaded and loaded.\n\n{} applications are listed!", db_updated_at, gui.compat.app_compat_db.size());

    return true;
}

} // namespace compat
