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

static uint64_t file_size = 0, header_size = 0;
constexpr int READ_BUFFER_SIZE = 1048;
std::string get_web_response(const std::string &url, const std::string &method, ProgressCallback progress_callback) {
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
#ifdef WIN32
    if (sockfd == INVALID_SOCKET) {
        LOG_ERROR("ERROR opening socket: {}", log_hex(WSAGetLastError()));
#else
    if (sockfd < 0) {
        LOG_ERROR("ERROR opening socket: {}", log_hex(sockfd));
#endif
        SSL_CTX_free(ctx);
        return {};
    }

    // Parse URL to get host and uri
    std::string host, uri;
    size_t start = url.find("://");
    if (start != std::string::npos) {
        start += 3; // skip "://"
        size_t end = url.find('/', start);
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

    // Check if getaddrinfo failed or unable to resolve address for host
    if ((ret < 0) || !result) {
        if (!result)
            LOG_ERROR("Unable to resolve address for host: {}", host);
        else {
            LOG_ERROR("getaddrinfo error: {}", gai_strerror(ret));
            freeaddrinfo(result);
        }
        SSL_CTX_free(ctx);
        close_socket(sockfd);
        return {};
    }

    // Connect to host
    ret = connect(sockfd, result->ai_addr, static_cast<uint32_t>(result->ai_addrlen));
    if (ret < 0) {
        LOG_ERROR("connect({},...) = {}, errno={}({})", sockfd, ret, errno, strerror(errno));
        freeaddrinfo(result);
        SSL_CTX_free(ctx);
        close_socket(sockfd);
        return {};
    }

    // Check if socket is connected
    int error = 0;
    socklen_t errlen = sizeof(error);
    ret = getsockopt(sockfd, SOL_SOCKET, SO_ERROR, reinterpret_cast<char *>(&error), &errlen);
    if (ret < 0) {
        LOG_ERROR("getsockopt({}, SOL_SOCKET, SO_ERROR, ...) failed: {}", sockfd, strerror(errno));
        freeaddrinfo(result);
        SSL_CTX_free(ctx);
        close_socket(sockfd);
        return {};
    }
    if (error != 0) {
        LOG_ERROR("connect({}, ...) failed: {}", sockfd, error);
        freeaddrinfo(result);
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
        freeaddrinfo(result);
        SSL_CTX_free(ctx);
        close_ssl(ssl);
        close_socket(sockfd);
        return {};
    }

    // Send HTTP GET request to extracted URI
    std::string request = method + " " + uri + " HTTP/1.1\r\n";
    request += "Host: " + host + "\r\n";
    request += "User-Agent: OpenSSL/1.1.1\r\n";
    request += "Connection: close\r\n\r\n";

    if (SSL_write(ssl, request.c_str(), static_cast<uint32_t>(request.length())) <= 0) {
        char err_buf[256];
        ERR_error_string_n(ERR_get_error(), err_buf, sizeof(err_buf));
        freeaddrinfo(result);
        SSL_CTX_free(ctx);
        close_ssl(ssl);
        close_socket(sockfd);
        LOG_ERROR("Error sending request: {}", err_buf);
        return {};
    }

    std::array<char, READ_BUFFER_SIZE> read_buffer{};
    std::string response;
    int64_t downloaded_size = 0;

    // Remove header size from downloaded size if header size is not 0
    if (header_size > 0)
        downloaded_size -= header_size;

    while (const auto bytes_read = SSL_read(ssl, read_buffer.data(), static_cast<uint32_t>(read_buffer.size()))) {
        response += std::string(read_buffer.data(), bytes_read);
        downloaded_size += bytes_read;
        if (progress_callback && (file_size > 0)) {
            float progress_percent = static_cast<float>(downloaded_size) / static_cast<float>(file_size) * 100.0f;
            progress_callback(progress_percent);
        }
    }

    freeaddrinfo(result);
    SSL_CTX_free(ctx);
    close_ssl(ssl);
    close_socket(sockfd);

    boost::trim(response);

    // Set header size if method is HEAD
    if (method == "HEAD") {
        header_size = downloaded_size;
    } else if ((header_size > 0) && (downloaded_size < (file_size * 0.01))) {
        LOG_ERROR("Downloaded size is not equal to file size, downloaded size: {}/{}", downloaded_size, file_size);
        return {};
    }

    // Check if the response is resource not found
    if (response.find("HTTP/1.1 404 Not Found") != std::string::npos) {
        LOG_ERROR("404 Not Found");
        return {};
    }

    return response;
}

std::string get_web_regex_result(const std::string &url, const std::regex &regex, const std::string &method) {
    std::string result;

    // Get the response of the web
    const auto response = https::get_web_response(url, method);

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

static uint64_t get_file_size(const std::string &url) {
    uint64_t content_length = 0;

    // Get the file size from the header
    const auto content_length_str = get_web_regex_result(url, std::regex("Content-Length: (\\d+)"), "HEAD");

    // Check if the content length is not empty
    if (!content_length_str.empty())
        content_length = std::stoll(content_length_str);

    return content_length;
}

bool download_file(const std::string &url, const std::string &output_file_path, ProgressCallback progress_callback) {
    header_size = 0;
    file_size = 0;

    // Get the response
    auto response = get_web_response(url);

    // Check if the response is not empty
    if (response.empty()) {
        LOG_ERROR("Failed to download file on url: {}", url);
        return false;
    }
    // Check if the response is a redirection
    if (response.find("HTTP/1.1 302 Found") != std::string::npos) {
        // Get the redirection URL from the response header (Location)
        std::smatch match;
        if (std::regex_search(response, match, std::regex("Location: (https?://[^\\s]+)"))) {
            const std::string redirected_url(match[1]);

            // Get file size from the redirection URL
            file_size = get_file_size(redirected_url);
            if (file_size == 0) {
                LOG_ERROR("Failed to get file size on url: {}", redirected_url);
                return false;
            }

            // Download the file from the redirection URL with using progress callback
            response = get_web_response(redirected_url, "GET", progress_callback);

            // Check if the response is empty
            if (response.empty()) {
                LOG_ERROR("Failed to download file on url: {}", redirected_url);
                return false;
            }
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
} // namespace https
