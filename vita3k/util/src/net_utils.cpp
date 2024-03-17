// Vita3K emulator project
// Copyright (C) 2024 Vita3K team
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

#include <util/net_utils.h>

#include <curl/curl.h>

#ifdef _WIN32
#include <winsock2.h>
#else
#include <fcntl.h>
#endif

#include <condition_variable>
#include <mutex>

namespace net_utils {

// 0 is ok, negative is bad
SceHttpErrorCode parse_url(const std::string &url, parsedUrl &out) {
    out.scheme = url.substr(0, url.find(':'));

    if (out.scheme != "http" && out.scheme != "https")
        return SCE_HTTP_ERROR_UNKNOWN_SCHEME;

    // Check if URL is opaque, if it is opaque then its invalid
    // http://
    //+    012
    //      ^^ Check these 2
    char url1Slash = *(url.c_str() + (out.scheme.length() + 1)); // get uint8 value
    char url2Slash = *(url.c_str() + (out.scheme.length() + 2)); // get next uint8 value
    // Compare the 2 characters that are supposed to be slahses
    if (url1Slash != '/' || url2Slash != '/') {
        out.invalid = true;
        return (SceHttpErrorCode)0;
    }

    auto end_scheme_pos = url.find(':');
    auto path_pos = url.find('/', end_scheme_pos + 3);
    auto has_path = path_pos != std::string::npos;

    auto full_wo_scheme = url.substr(end_scheme_pos + 3);

    if (has_path) {
        // username:password@lttstore.com:727/wysi/cookie.php?pog=gers#extremeexploit

        {
            path_pos = std::string(full_wo_scheme).find('/');
            auto full_no_scheme_path = full_wo_scheme.substr(0, path_pos);
            // full_no_scheme_path = username:password@lttstore.com:727
            auto c = full_no_scheme_path.find('@');
            if (c != std::string::npos) { // we have credentials
                auto credentials = full_no_scheme_path.substr(0, c);
                // credentials = username:password
                auto semicolon_pos = credentials.find(':');

                if (semicolon_pos != std::string::npos) { // we have password?
                    auto username = credentials.substr(0, semicolon_pos);
                    auto password = credentials.substr(semicolon_pos + 1);

                    if (username.length() >= SCE_HTTP_USERNAME_MAX_SIZE)
                        return SCE_HTTP_ERROR_OUT_OF_SIZE;
                    if (password.length() >= SCE_HTTP_USERNAME_MAX_SIZE)
                        return SCE_HTTP_ERROR_OUT_OF_SIZE;

                    out.username = username;
                    out.password = password;
                } else { // we don't have password
                    out.username = credentials;
                }
            } else { // no credentials
                // lttstore.com:727
                auto semicolon_pos = full_no_scheme_path.find(':');

                if (semicolon_pos != std::string::npos) { // we have port
                    auto hostname = full_no_scheme_path.substr(0, semicolon_pos);
                    auto port = full_no_scheme_path.substr(semicolon_pos + 1);

                    out.hostname = hostname;
                    out.port = port;
                } else { // no port
                    out.hostname = full_no_scheme_path;
                }
            }
        }
        {
            auto path_pos = std::string(full_wo_scheme).find('/');
            auto full_no_scheme_hostname = full_wo_scheme.substr(path_pos);
            // /wysi/cookie.php?pog=gers#extremeexploit

            auto query_pos = full_no_scheme_hostname.find('?');
            if (query_pos != std::string::npos) { // we have query
                auto path = full_no_scheme_hostname.substr(0, query_pos);
                out.path = path;

                auto query_frag = full_no_scheme_hostname.substr(query_pos);

                auto frag_pos = query_frag.find('#');
                if (frag_pos != std::string::npos) {
                    // query and fragment

                    auto query = query_frag.substr(0, frag_pos);
                    auto fragment = query_frag.substr(frag_pos);

                    out.query = query;
                    out.fragment = fragment;
                } else { // no fragment
                    out.query = query_frag;
                }

            } else { // no query, dunno about fragment
                auto frag_pos = full_no_scheme_hostname.find('#');

                if (frag_pos != std::string::npos) { // we have fragment
                    auto path = full_no_scheme_hostname.substr(0, frag_pos);
                    auto fragment = full_no_scheme_hostname.substr(frag_pos);

                    out.path = path;
                    out.fragment = fragment;
                } else { // no fragment, only path
                    out.path = full_no_scheme_hostname;
                }
            }
        }
    } else {
        // username:password@lttstore.com:727
        auto c = full_wo_scheme.find('@');
        if (c != std::string::npos) { // we have credentials
            auto credentials = full_wo_scheme.substr(0, c);
            // credentials = username:password
            auto semicolon_pos = credentials.find(':');

            if (semicolon_pos != std::string::npos) { // we have password?
                auto username = credentials.substr(0, semicolon_pos);
                auto password = credentials.substr(semicolon_pos + 1);

                if (username.length() >= SCE_HTTP_USERNAME_MAX_SIZE)
                    return SCE_HTTP_ERROR_OUT_OF_SIZE;
                if (password.length() >= SCE_HTTP_USERNAME_MAX_SIZE)
                    return SCE_HTTP_ERROR_OUT_OF_SIZE;

                out.username = username;
                out.password = password;
            } else { // we don't have password
                out.username = credentials;
            }
        } else { // no credentials
            // lttstore.com:727
            auto semicolon_pos = full_wo_scheme.find(':');

            if (semicolon_pos != std::string::npos) { // we have port
                auto hostname = full_wo_scheme.substr(0, semicolon_pos);
                auto port = full_wo_scheme.substr(semicolon_pos + 1);

                out.hostname = hostname;
                out.port = port;
            } else { // no port
                out.hostname = full_wo_scheme;
            }
        }
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

    // do this check just in case the server is drunk or retarded, would be nice to do more checks with some regex
    if (!lineClean.starts_with("HTTP/"))
        return false; // what

    const auto firstSpace = lineClean.find(' ');
    if (firstSpace == std::string::npos)
        return false;

    const std::string fullHttpVerStr = lineClean.substr(0, firstSpace);
    const std::string httpVerStr = fullHttpVerStr.substr(strlen("HTTP/"));

    if (!std::isdigit(httpVerStr[0]))
        return false;

    if (lineClean.length() < fullHttpVerStr.length() + strlen(" XXX"))
        return false; // the rest of the line is less than 3 characters in length, what the fuck happened also abort

    const auto codeAndReason = lineClean.substr(firstSpace + 1);
    const auto statusCodeStr = codeAndReason.substr(0, 3);
    if (!std::isdigit(statusCodeStr[0]) || !std::isdigit(statusCodeStr[1]) || !std::isdigit(statusCodeStr[2]))
        return false; // status code contains non digit characters, abort

    const int statusCodeInt = std::stoi(statusCodeStr);

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
        if (line.find(": "))
            // Theres a space between semicolon and value, trim it
            valueStart++;

        auto value = line.substr(valueStart);

        headersOut.insert({ std::string(name), std::string(value) });
        ptr = strtok(NULL, "\r\n");
    }
    return true;
}

bool parseResponse(const std::string &res, SceRequestResponse &reqres) {
    auto statusLine = res.substr(0, res.find("\r\n"));
    if (!parseStatusLine(statusLine, reqres.httpVer, reqres.statusCode, reqres.reasonPhrase))
        return false;

    auto headersRaw = res.substr(res.find("\r\n") + strlen("\r\n"), res.find("\r\n\r\n"));

    if (!parseHeaders(headersRaw, reqres.headers))
        return false;

    auto contLenIt = reqres.headers.find("Content-Length");
    if (contLenIt == reqres.headers.end()) {
        reqres.contentLength = 0;
    } else {
        reqres.contentLength = std::stoi(contLenIt->second);
    }

    return true;
}

bool socketSetBlocking(int sockfd, bool blocking) {
#ifdef WIN32
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

uint64_t get_current_time_ms() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now().time_since_epoch()).count();
};

static size_t write_data(void *ptr, size_t size, size_t nmemb, void *stream) {
    size_t written = fwrite(ptr, size, nmemb, (FILE *)stream);
    return written;
}

typedef int (*CurlDownloadCallback)(void *c, long t, long d);

bool download_file(const std::string &url, const std::string &output_file_path, ProgressCallback progress_callback) {
    CURL *curl_download = curl_easy_init();
    if (!curl_download)
        return false;

    CurlDownloadCallback curl_callback = +[](void *user_data, long bytes_total, long downloaded_bytes) {
        const auto data = (CallbackData *)user_data;

        if (bytes_total == 0)
            return 0; // Ignore if we dont have the total yet

        if (!data->second)
            return 0;
        const auto progress_percent = static_cast<float>(downloaded_bytes) / static_cast<float>(bytes_total) * 100.0f;

        const auto elapsed_time_ms = std::difftime(get_current_time_ms(), data->first);

        // Calculate remaining time in seconds
        const auto remaining_bytes = static_cast<double>(bytes_total - downloaded_bytes);
        const auto remaining_time = static_cast<uint64_t>((remaining_bytes / downloaded_bytes) * elapsed_time_ms) / 1000;

        ProgressState *callback_result = data->second(progress_percent, remaining_time);

        std::unique_lock<std::mutex> lock(callback_result->mutex);

        callback_result->cv.wait(lock, [&]() {
            return !callback_result->pause;
        });

        if (!callback_result->download) {
            return 1; // Returning anything thats not 0 aborts the request
        }
        return 0;
    };

    auto start_time = get_current_time_ms();
    auto callbackData = CallbackData(start_time, progress_callback);

    curl_easy_setopt(curl_download, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl_download, CURLOPT_WRITEFUNCTION, write_data);
    curl_easy_setopt(curl_download, CURLOPT_NOPROGRESS, false); // Enable progress function
    curl_easy_setopt(curl_download, CURLOPT_XFERINFODATA, &callbackData);
    curl_easy_setopt(curl_download, CURLOPT_XFERINFOFUNCTION, curl_callback);
    curl_easy_setopt(curl_download, CURLOPT_FOLLOWLOCATION, true); // Follow redirects

    auto fp = fopen(output_file_path.c_str(), "wb");
    if (!fp) {
        LOG_CRITICAL("Could not fopen file {}", output_file_path);
        curl_easy_cleanup(curl_download);
        if (fs::exists(output_file_path))
            fs::remove(output_file_path);
        return false;
    }

    curl_easy_setopt(curl_download, CURLOPT_WRITEDATA, fp);
    int res = curl_easy_perform(curl_download);

    fclose(fp);
    curl_easy_cleanup(curl_download);
    if (progress_callback)
        progress_callback(0, 0);

    if (res != CURLE_OK) {
        if (fs::exists(output_file_path))
            fs::remove(output_file_path);
    }

    if (res == CURLE_ABORTED_BY_CALLBACK)
        LOG_CRITICAL("Aborted update by user");

    return res == CURLE_OK;
}

} // namespace net_utils
