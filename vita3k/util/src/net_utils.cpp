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

#include <util/log.h>
#include <util/net_utils.h>

#include <curl/curl.h>

#ifdef _WIN32
#include <iphlpapi.h>
#include <winsock2.h>
#else
#include <arpa/inet.h>
#include <fcntl.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <netinet/in.h>
#endif

#include <algorithm>
#include <cctype>
#include <charconv>
#include <condition_variable>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <mutex>
#include <string_view>
#include <system_error>

namespace net_utils {

namespace {

bool parse_decimal(std::string_view str, SceULong64 &out) {
    if (str.empty())
        return false;

    SceULong64 value = 0;
    const auto *begin = str.data();
    const auto *end = begin + str.size();
    const auto [ptr, ec] = std::from_chars(begin, end, value);
    if (ec != std::errc() || ptr != end)
        return false;

    out = value;
    return true;
}

bool is_digit(char ch) {
    return std::isdigit(static_cast<unsigned char>(ch)) != 0;
}

} // namespace

// 0 is ok, negative is bad
SceHttpErrorCode parse_url(const std::string &url, parsedUrl &out) {
    out = {};

    const auto scheme_end = url.find(':');
    if (scheme_end == std::string::npos) {
        out.scheme = url;
        if (out.scheme == "http" || out.scheme == "https") {
            out.invalid = true;
            return (SceHttpErrorCode)0;
        }
        return SCE_HTTP_ERROR_UNKNOWN_SCHEME;
    }

    out.scheme = url.substr(0, scheme_end);
    if (out.scheme != "http" && out.scheme != "https")
        return SCE_HTTP_ERROR_UNKNOWN_SCHEME;

    // Check if URL is opaque. HTTP URLs accepted by this parser must use //.
    if (url.size() < scheme_end + 3 || url[scheme_end + 1] != '/' || url[scheme_end + 2] != '/') {
        out.invalid = true;
        return (SceHttpErrorCode)0;
    }

    const auto authority_start = scheme_end + strlen("://");
    const auto path_start = url.find_first_of("/?#", authority_start);
    const std::string_view authority(url.data() + authority_start,
        (path_start == std::string::npos ? url.size() : path_start) - authority_start);
    if (authority.empty()) {
        out.invalid = true;
        return (SceHttpErrorCode)0;
    }

    std::string_view host_port = authority;
    const auto userinfo_end = authority.rfind('@');
    if (userinfo_end != std::string_view::npos) {
        const auto userinfo = authority.substr(0, userinfo_end);
        host_port = authority.substr(userinfo_end + 1);

        const auto password_start = userinfo.find(':');
        const auto username = userinfo.substr(0, password_start);
        const auto password = password_start == std::string_view::npos
            ? std::string_view()
            : userinfo.substr(password_start + 1);

        if (username.length() >= SCE_HTTP_USERNAME_MAX_SIZE)
            return SCE_HTTP_ERROR_OUT_OF_SIZE;
        if (password.length() >= SCE_HTTP_PASSWORD_MAX_SIZE)
            return SCE_HTTP_ERROR_OUT_OF_SIZE;

        out.username = std::string(username);
        out.password = std::string(password);
    }

    if (host_port.empty()) {
        out.invalid = true;
        return (SceHttpErrorCode)0;
    }

    const auto port_start = host_port.rfind(':');
    if (port_start != std::string_view::npos) {
        out.hostname = std::string(host_port.substr(0, port_start));
        out.port = std::string(host_port.substr(port_start + 1));
    } else {
        out.hostname = std::string(host_port);
    }

    if (out.hostname.empty()) {
        out.invalid = true;
        return (SceHttpErrorCode)0;
    }

    if (path_start != std::string::npos) {
        auto query_start = url.find('?', path_start);
        const auto fragment_start = url.find('#', path_start);
        if (query_start != std::string::npos && fragment_start != std::string::npos && fragment_start < query_start)
            query_start = std::string::npos;

        const auto path_end = std::min(query_start == std::string::npos ? url.size() : query_start,
            fragment_start == std::string::npos ? url.size() : fragment_start);

        if (url[path_start] == '/')
            out.path = url.substr(path_start, path_end - path_start);

        if (query_start != std::string::npos) {
            const auto query_end = fragment_start == std::string::npos ? url.size() : fragment_start;
            out.query = url.substr(query_start, query_end - query_start);
        }

        if (fragment_start != std::string::npos)
            out.fragment = url.substr(fragment_start);
    }

    return (SceHttpErrorCode)0;
}

int char_method_to_int(const char *method) {
    if (strcmp(method, "GET") == 0) {
        return SCE_HTTP_METHOD_GET;
    } else if (strcmp(method, "POST") == 0) {
        return SCE_HTTP_METHOD_POST;
    } else if (strcmp(method, "HEAD") == 0) {
        return SCE_HTTP_METHOD_HEAD;
    } else if (strcmp(method, "OPTIONS") == 0) {
        return SCE_HTTP_METHOD_OPTIONS;
    } else if (strcmp(method, "PUT") == 0) {
        return SCE_HTTP_METHOD_PUT;
    } else if (strcmp(method, "DELETE") == 0) {
        return SCE_HTTP_METHOD_DELETE;
    } else if (strcmp(method, "TRACE") == 0) {
        return SCE_HTTP_METHOD_TRACE;
    } else if (strcmp(method, "CONNECT") == 0) {
        return SCE_HTTP_METHOD_CONNECT;
    } else {
        return -1;
    }
}

const char *int_method_to_char(const int n) {
    switch (n) {
    case 0: return "GET";
    case 1: return "POST";
    case 2: return "HEAD";
    case 3: return "OPTIONS";
    case 4: return "PUT";
    case 5: return "DELETE";
    case 6: return "TRACE";
    case 7: return "CONNECT";

    default:
        return "INVALID";
        break;
    }
}

std::string constructHeaders(const HeadersMapType &headers) {
    std::string headersString;
    for (const auto &head : headers) {
        headersString.append(head.first);
        headersString.append(": ");
        headersString.append(head.second);
        headersString.append("\r\n");
    }

    return headersString;
}

bool parseStatusLine(const std::string &line, std::string &httpVer, int &statusCode, std::string &reason) {
    auto lineClean = line.substr(0, line.find("\r\n"));

    // Do a light sanity check before parsing the status line fields.
    if (!lineClean.starts_with("HTTP/"))
        return false;

    const auto firstSpace = lineClean.find(' ');
    if (firstSpace == std::string::npos)
        return false;

    const std::string fullHttpVerStr = lineClean.substr(0, firstSpace);
    const std::string httpVerStr = fullHttpVerStr.substr(strlen("HTTP/"));

    if (httpVerStr.empty() || !is_digit(httpVerStr[0]))
        return false;

    if (lineClean.length() < fullHttpVerStr.length() + strlen(" XXX"))
        return false; // the rest of the line is shorter than a 3-digit status code

    const auto codeAndReason = lineClean.substr(firstSpace + 1);
    const auto statusCodeStr = codeAndReason.substr(0, 3);
    if (!is_digit(statusCodeStr[0]) || !is_digit(statusCodeStr[1]) || !is_digit(statusCodeStr[2]))
        return false; // status code contains non digit characters, abort

    SceULong64 parsedStatusCode = 0;
    if (!parse_decimal(statusCodeStr, parsedStatusCode) || parsedStatusCode > std::numeric_limits<int>::max())
        return false;
    const int statusCodeInt = static_cast<int>(parsedStatusCode);

    std::string reasonStr = "";
    bool hasReason = codeAndReason.find(' ') != std::string::npos;
    if (hasReason) // standard says that reasons CAN be empty, we have to take this edge case into account
        reasonStr = codeAndReason.substr(4);

    httpVer = httpVerStr;
    statusCode = statusCodeInt;
    reason = reasonStr;

    return true;
}

/*
    CANNOT have ANYTHING after the last \r\n or \r\n\r\n else it will be treated as a header
*/
bool parseHeaders(std::string &headersRaw, HeadersMapType &headersOut) {
    char *ptr = strtok(headersRaw.data(), "\r\n");
    // use while loop to check ptr is not null
    while (ptr != NULL) {
        auto line = std::string_view(ptr);

        if (line.find(':') == std::string::npos)
            return false; // separator is missing, the header is invalid

        auto name = line.substr(0, line.find(':'));
        int valueStart = name.length() + 1;
        if (line.find(": ") != std::string_view::npos)
            // Theres a space between semicolon and value, trim it
            valueStart++;

        auto value = line.substr(valueStart);

        headersOut.emplace(std::string(name), std::string(value));
        ptr = strtok(nullptr, "\r\n");
    }
    return true;
}

bool parseResponse(const std::string &res, SceRequestResponse &reqres) {
    const auto status_line_end = res.find("\r\n");
    if (status_line_end == std::string::npos)
        return false;

    auto statusLine = res.substr(0, status_line_end);
    if (!parseStatusLine(statusLine, reqres.httpVer, reqres.statusCode, reqres.reasonPhrase))
        return false;

    const auto headers_start = status_line_end + strlen("\r\n");
    const auto headers_end = res.find("\r\n\r\n", headers_start);
    auto headersRaw = res.substr(headers_start,
        headers_end == std::string::npos ? std::string::npos : headers_end - headers_start);

    if (!parseHeaders(headersRaw, reqres.headers))
        return false;

    auto contLenIt = reqres.headers.find("Content-Length");
    if (contLenIt == reqres.headers.end()) {
        reqres.contentLength = 0;
    } else {
        SceULong64 contentLength = 0;
        if (!parse_decimal(contLenIt->second, contentLength))
            return false;
        reqres.contentLength = contentLength;
    }

    return true;
}

bool socketSetBlocking(int sockfd, bool blocking) {
#ifdef _WIN32
    u_long blocking_tmp = blocking;
    ioctlsocket(sockfd, FIONBIO, &blocking_tmp);
#else
    if (blocking) { // Blocking
        int flags = fcntl(sockfd, F_GETFL); // Get flags
        fcntl(sockfd, F_SETFL, flags & ~O_NONBLOCK); // Set NONBLOCK flag off
    } else { // Non blocking
        int flags = fcntl(sockfd, F_GETFL); // Get flags
        fcntl(sockfd, F_SETFL, flags | O_NONBLOCK); // Set NONBLOCK flag on
    }
#endif
    return true;
}

std::string get_web_response(const std::string &url) {
    auto curl = curl_easy_init();
    if (!curl)
        return {};

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "Vita3K Emulator");
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, true); // Follow redirects
    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 1L);

#ifdef __ANDROID__
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
#endif

    std::string response_string;
    const auto writeFunc = +[](void *ptr, size_t size, size_t nmemb, std::string *data) {
        data->append((char *)ptr, size * nmemb);
        return size * nmemb;
    };
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeFunc);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_string);
    long response_code;
    curl_easy_perform(curl);
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);

    curl_easy_cleanup(curl);

    if (response_code / 100 == 2)
        return response_string;

    return {};
}

std::string get_web_regex_result(const std::string &url, const std::regex &regex) {
    std::string result;

    // Get the response of the web
    const auto response = get_web_response(url);

    // Check if the response is not empty
    if (!response.empty()) {
        std::smatch match;
        // Check if the response matches the regex
        if (std::regex_search(response, match, regex)) {
            result = match[1];
        } else
            LOG_ERROR("No success found regex: {}", response);
    }

    return result;
}

std::vector<AssignedAddr> get_all_assigned_addrs() {
    std::vector<AssignedAddr> out_addrs;
    const auto ret_addrs = [&out_addrs]() {
        if (out_addrs.empty())
            out_addrs.push_back({ "localhost", "127.0.0.1", "255.255.255.255" });

        return out_addrs;
    };

#ifdef _WIN32
    PIP_ADAPTER_INFO pAdapterInfo;
    DWORD dwRetVal = 0;
    UINT i;
    ULONG ulOutBufLen = sizeof(IP_ADAPTER_INFO);
    pAdapterInfo = (IP_ADAPTER_INFO *)malloc(sizeof(IP_ADAPTER_INFO));
    if (pAdapterInfo == NULL) {
        LOG_CRITICAL("Error allocating memory needed to call GetAdaptersinfo");
        return ret_addrs();
    }
    // Make an initial call to GetAdaptersInfo to get the necessary size into the ulOutBufLen variable
    if (GetAdaptersInfo(pAdapterInfo, &ulOutBufLen) == ERROR_BUFFER_OVERFLOW) {
        free(pAdapterInfo);
        pAdapterInfo = (IP_ADAPTER_INFO *)malloc(ulOutBufLen);
        if (pAdapterInfo == NULL) {
            LOG_CRITICAL("Error allocating memory needed to call GetAdaptersinfo");
            return ret_addrs();
        }
    }
    if ((dwRetVal = GetAdaptersInfo(pAdapterInfo, &ulOutBufLen)) == NO_ERROR) {
        PIP_ADAPTER_INFO pAdapter = pAdapterInfo;
        const std::string noAddress = "0.0.0.0";
        while (pAdapter) {
            IP_ADDR_STRING *pIPAddr = &pAdapter->IpAddressList;
            while (pIPAddr) {
                if (noAddress.compare(pIPAddr->IpAddress.String) != 0)
                    out_addrs.push_back({ pAdapter->Description, pIPAddr->IpAddress.String, pIPAddr->IpMask.String });
                pIPAddr = pIPAddr->Next;
            }
            pAdapter = pAdapter->Next;
        }
    } else {
        LOG_CRITICAL("GetAdaptersInfo failed with error: {}", dwRetVal);
    }
#else
    struct ifaddrs *ifAddrStruct = NULL;
    struct ifaddrs *ifa = NULL;
    void *tmpAddrPtr = NULL;

    getifaddrs(&ifAddrStruct);

    for (ifa = ifAddrStruct; ifa != NULL; ifa = ifa->ifa_next) {
        if (!ifa->ifa_addr)
            continue;
        if ((ifa->ifa_flags & IFF_LOOPBACK) != 0)
            continue;
        if (ifa->ifa_flags)
            if (ifa->ifa_addr->sa_family == AF_INET) { // check it is IP4
                char netMaskAddrStr[INET_ADDRSTRLEN];
                auto netMaskAddr = ((sockaddr_in *)ifa->ifa_netmask)->sin_addr;
                inet_ntop(AF_INET, &netMaskAddr, netMaskAddrStr, INET_ADDRSTRLEN);
                // is a valid IP4 Address
                tmpAddrPtr = &((sockaddr_in *)ifa->ifa_addr)->sin_addr;
                char addressBuffer[INET_ADDRSTRLEN];
                inet_ntop(AF_INET, tmpAddrPtr, addressBuffer, INET_ADDRSTRLEN);
                out_addrs.push_back({ ifa->ifa_name, addressBuffer, netMaskAddrStr });
            }
    }
    if (ifAddrStruct != NULL)
        freeifaddrs(ifAddrStruct);
#endif
    return ret_addrs();
}

AssignedAddr get_selected_assigned_addr(int32_t &outIndex) {
    const auto addrs = get_all_assigned_addrs();
    if (outIndex >= addrs.size()) {
        LOG_ERROR("Invalid index {}, returning first address", outIndex);
        outIndex = 0;
    }
    return addrs[outIndex];
}

void init_address(int32_t &outIndex, uint32_t &netAddr, uint32_t &broadcastAddr) {
    // Initialize the net and broadcast address based on the assigned address and netmask
    const auto addr = get_selected_assigned_addr(outIndex);
    int netMask;
    inet_pton(AF_INET, addr.addr.c_str(), &netAddr);
    inet_pton(AF_INET, addr.netMask.c_str(), &netMask);
    broadcastAddr = netAddr | ~netMask;
}

} // namespace net_utils
