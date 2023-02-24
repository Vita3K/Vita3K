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
#include <https/functions.h>

#ifdef WIN32
#include <io.h>
#include <winsock2.h>
#include <ws2tcpip.h>
typedef SOCKET abs_socket;
#else
#include <netdb.h>
#include <sys/socket.h>
typedef int abs_socket;
#endif

#include <openssl/err.h>
#include <openssl/ssl.h>

#include <util/log.h>

#include <array>

namespace https {

static void close_socket(const abs_socket sockfd) {
#ifdef WIN32
    closesocket(sockfd);
    WSACleanup();
#else
    close(sockfd);
#endif // WIN32
}

static void close_ssl(SSL *ssl) {
    SSL_shutdown(ssl);
    SSL_free(ssl);
}

constexpr int READ_BUFFER_SIZE = 1048;
std::string get_web_response(const std::string url) {
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
        SSL_CTX_free(ctx);
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
        SSL_CTX_free(ctx);
        close_socket(sockfd);
        return {};
    }

    // Connect to host
    ret = connect(sockfd, result->ai_addr, static_cast<uint32_t>(result->ai_addrlen));
    if (ret < 0) {
        LOG_ERROR("connect({},...) = {}, errno={}({})", sockfd, ret, errno, strerror(errno));
        SSL_CTX_free(ctx);
        close_socket(sockfd);
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
        SSL_CTX_free(ctx);
        close_ssl(ssl);
        close_socket(sockfd);
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
        SSL_CTX_free(ctx);
        close_ssl(ssl);
        close_socket(sockfd);
        LOG_ERROR("Error sending request: {}", err_buf);
        return {};
    }

    std::array<char, READ_BUFFER_SIZE> read_buffer{};
    std::string response;
    while (auto bytes_read = SSL_read(ssl, read_buffer.data(), static_cast<uint32_t>(read_buffer.size()))) {
        response += std::string(read_buffer.data(), bytes_read);
    }

    SSL_CTX_free(ctx);
    close_ssl(ssl);
    close_socket(sockfd);

    boost::trim(response);

    // Check if the response is resource not found
    if (response.find("HTTP/1.1 404 Not Found") != std::string::npos) {
        LOG_ERROR("404 Not Found");
        return {};
    }

    return response;
}

std::string get_web_regex_result(const std::string url, const std::regex regex) {
    std::string result;

    // Get the response of the web
    const auto response = https::get_web_response(url);

    // Check if the response is not empty
    if (!response.empty()) {
        // Get the content of the response (without headers)
        const std::string content = response.substr(response.find("\r\n\r\n") + 4);

        std::smatch match;
        // Check if the content matches the regex
        if (std::regex_search(response, match, regex)) {
            result = match[1];
        } else
            LOG_ERROR("No success found regex: {}", content);
    }

    return result;
}

bool download_file(const std::string url, const std::string output_file_path) {
    // Get the response of the app compat db
    auto response = get_web_response(url);

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
            response = get_web_response(redirected_url);
            if (response.empty()) {
                LOG_ERROR("Failed to get response on redirected url: {}", redirected_url);
                return false;
            }
        }

        // Get the content of the response
        std::string content = response.substr(response.find("\r\n\r\n") + 4);

        // Create the output file
        std::ofstream output_file(output_file_path, std::ios::binary);
        if (!output_file.is_open()) {
            LOG_ERROR("Failed to open output file: {}", output_file_path);
            return false;
        }

        // Write the content to the output file
        output_file.write(content.c_str(), content.length());
        output_file.close();

        return fs::exists(output_file_path);
    }

    return false;
}

} // namespace https
