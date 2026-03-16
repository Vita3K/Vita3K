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
    auto has_path = url.find('/', end_scheme_pos + 3) != std::string::npos;

    auto full_wo_scheme = url.substr(end_scheme_pos + 3);

    if (has_path) {
        // username:password@lttstore.com:727/wysi/cookie.php?pog=gers#extremeexploit

        {
            auto path_pos = std::string(full_wo_scheme).find('/');
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

WebResponse get_web_response_ex(const std::string &url, const std::string &token, const std::string &post_data) {
    WebResponse web_response{};
    auto curl = curl_easy_init();
    if (!curl)
        return web_response;

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "Vita3K Emulator");
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, true); // Follow redirects
    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 1L);

#ifdef __ANDROID__
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
#endif

    struct curl_slist *headers = nullptr;
    if (!token.empty()) {
        const std::string auth_header = "Authorization: Bearer " + token;
        headers = curl_slist_append(headers, auth_header.c_str());
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    }

    if (!post_data.empty()) {
        curl_easy_setopt(curl, CURLOPT_POST, 1L);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post_data.c_str());
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, post_data.length());
    }

    std::string response_string;
    const auto writeFunc = +[](void *ptr, size_t size, size_t nmemb, std::string *data) {
        data->append((char *)ptr, size * nmemb);
        return size * nmemb;
    };
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeFunc);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &web_response.body);
    long response_code;
    web_response.curl_res = curl_easy_perform(curl);
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);

    if (!token.empty())
        curl_slist_free_all(headers);

    curl_easy_cleanup(curl);

    if (response_code / 100 == 2)
        return web_response;

    web_response.body.clear();
    return web_response;
}

std::string get_web_response(const std::string &url) {
    const auto response = get_web_response_ex(url, "");
    return response.body;
}

std::string get_web_regex_result(const std::string &url, const std::regex &regex) {
    const auto result = get_web_regex_results(url, regex);
    if (result.empty())
        return {};

    return result.back();
}

std::vector<std::string> get_web_regex_results(const std::string &url, const std::regex &regex) {
    std::vector<std::string> results;

    // Get the response of the web
    const auto response = get_web_response(url);

    // Check if the response is not empty
    if (!response.empty()) {
        std::smatch match;
        // Check if the response matches the regex
        if (std::regex_search(response, match, regex)) {
            for (size_t i = 1; i < match.size(); ++i)
                results.push_back(match[i].str());
        } else
            LOG_ERROR("No success found regex: {}", response);
    }

    return results;
}

static uint64_t get_current_time_ms() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now().time_since_epoch()).count();
}

static size_t read_data(void *ptr, size_t size, size_t nmemb, void *stream) {
    size_t readed = fread(ptr, size, nmemb, (FILE *)stream);
    return readed;
}

static size_t write_data(void *ptr, size_t size, size_t nmemb, void *stream) {
    size_t written = fwrite(ptr, size, nmemb, (FILE *)stream);
    return written;
}

typedef int (*CurlCallback)(void *c, curl_off_t dt, curl_off_t dn, curl_off_t ut, curl_off_t un);
static CurlCallback curl_callback(void *user_data, curl_off_t dltotal, curl_off_t dlnow, curl_off_t ultotal, curl_off_t ulnow) {
    auto *data = (CallbackData *)user_data;

    // Detect mode (download/upload)
    const bool is_download = (dltotal > 0);
    const bool is_upload = (ultotal > 0);

    // Skip if curl doesn't have info yet
    if (!is_download && !is_upload)
        return 0;

    if (!data->second)
        return 0;

    float progress = 0.f;
    double bytes_done = 0;
    double bytes_total = 0;

    if (is_download) {
        bytes_done = static_cast<double>(dlnow);
        bytes_total = static_cast<double>(dltotal);
        const auto total_bytes_done = static_cast<double>(dlnow + data->first.bytes_already_downloaded);
        const auto file_size = static_cast<double>(dltotal + data->first.bytes_already_downloaded);
        progress = static_cast<float>(total_bytes_done) / (static_cast<float>(file_size));
    } else if (is_upload) {
        bytes_done = static_cast<double>(ulnow);
        bytes_total = static_cast<double>(ultotal);
        progress = static_cast<float>(bytes_done) / static_cast<float>(bytes_total);
    }

    if (bytes_done == 0)
        return 0;

    // Calculate elapsed time
    const auto current_time = get_current_time_ms();
    const auto elapsed_time_ms = std::difftime(current_time, data->first.time);

    // Calculate remaining time in seconds
    const auto remaining_bytes = static_cast<double>(bytes_total - bytes_done);
    const uint64_t remaining_time = static_cast<uint64_t>((remaining_bytes / (double)bytes_done) * elapsed_time_ms) / 1000;

    // Callback UI
    ProgressState *callback_result = data->second(progress, remaining_time, bytes_done);

    std::unique_lock<std::mutex> lock(callback_result->mutex);

    const bool was_paused = callback_result->pause;

    callback_result->cv.wait(lock, [&]() {
        return !callback_result->pause;
    });

    if (was_paused)
        data->first.time += std::difftime(get_current_time_ms(), current_time);

    if (callback_result->canceled)
        return reinterpret_cast<CurlCallback>(1);

    return 0;
}

WebResponse download_file_ex(const std::string &url, const std::string &output_file_path, ProgressCallback progress_callback, const std::string &token, CurlSession *session) {
    WebResponse web_response{};

    CurlSession local_session;

    const bool has_session_given = session;

    if (!has_session_given) {
        local_session = init_curl_download_session(token);
        if (!local_session.handle) {
            web_response.curl_res = CURLE_FAILED_INIT;
            return web_response;
        }
        session = &local_session;
    }

    CURL *curl_download = static_cast<CURL *>(session->handle);
    curl_slist *headers = static_cast<curl_slist *>(session->headers);

    const auto start_time = get_current_time_ms();
    const uint64_t bytes_already_downloaded = fs::exists(output_file_path) ? fs::file_size(output_file_path) : 0;
    const auto callbackData = CallbackData({ start_time, bytes_already_downloaded }, progress_callback);

    curl_easy_setopt(curl_download, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl_download, CURLOPT_WRITEFUNCTION, write_data);
    curl_easy_setopt(curl_download, CURLOPT_NOPROGRESS, false); // Enable progress function
    curl_easy_setopt(curl_download, CURLOPT_RESUME_FROM_LARGE, bytes_already_downloaded);
    curl_easy_setopt(curl_download, CURLOPT_XFERINFODATA, &callbackData);
    curl_easy_setopt(curl_download, CURLOPT_XFERINFOFUNCTION, curl_callback);

    if (headers)
        curl_easy_setopt(curl_download, CURLOPT_HTTPHEADER, headers);

    auto fp = fopen(output_file_path.c_str(), "ab");
    if (!fp) {
        LOG_CRITICAL("Could not fopen file {}", output_file_path);
        if (!session)
            cleanup_curl_session(local_session);
        if (fs::exists(output_file_path))
            fs::remove(output_file_path);
        web_response.curl_res = CURLE_FAILED_INIT;
        return web_response;
    }

    curl_easy_setopt(curl_download, CURLOPT_WRITEDATA, fp);
    web_response.curl_res = curl_easy_perform(curl_download);

    long http_code = 0;
    curl_easy_getinfo(curl_download, CURLINFO_RESPONSE_CODE, &http_code);
    if ((http_code != 200) && (http_code != 206)) {
        LOG_WARN("Download failed with HTTP code: {}", http_code);
        fclose(fp);
        if (!session)
            cleanup_curl_session(local_session);
        if (fs::exists(output_file_path))
            fs::remove(output_file_path);
        web_response.curl_res = CURLE_HTTP_RETURNED_ERROR;
        return web_response;
    }

    fclose(fp);

    if (!has_session_given)
        cleanup_curl_session(local_session);

    if (web_response.curl_res == CURLE_ABORTED_BY_CALLBACK)
        LOG_WARN("Aborted by user");

    return web_response;
}

bool download_file(const std::string &url, const std::string &output_file_path, ProgressCallback progress_callback) {
    return download_file_ex(url, output_file_path, progress_callback).curl_res == CURLE_OK;
}

static size_t write_string(void *ptr, size_t size, size_t nmemb, void *userdata) {
    std::stringstream *ss = static_cast<std::stringstream *>(userdata);
    ss->write(static_cast<char *>(ptr), size * nmemb);
    return size * nmemb;
}

WebResponse upload_file(const std::string &url, const std::string &input_file_path, const std::string &token, const std::string &metadata, ProgressCallback progress_callback, CurlSession *session) {
    WebResponse response{};

    std::ifstream file(input_file_path, std::ios::binary);
    if (!file) {
        LOG_CRITICAL("Could not open file {}", input_file_path);
        response.curl_res = CURLE_READ_ERROR;
        return response;
    }

    std::vector<char> file_buffer((std::istreambuf_iterator<char>(file)),
        std::istreambuf_iterator<char>());

    CurlSession local_session;
    const bool has_session_given = session;

    if (!has_session_given) {
        local_session = init_curl_upload_session(token);
        if (!local_session.handle) {
            response.curl_res = CURLE_FAILED_INIT;
            return response;
        }
        session = &local_session;
    }

    CURL *curl_upload = static_cast<CURL *>(session->handle);
    curl_slist *headers = static_cast<curl_slist *>(session->headers);

    curl_easy_setopt(curl_upload, CURLOPT_URL, url.c_str());

    const auto start_time = get_current_time_ms();
    const auto callbackData = CallbackData({ start_time }, progress_callback);
    curl_easy_setopt(curl_upload, CURLOPT_NOPROGRESS, progress_callback ? false : true);
    if (progress_callback) {
        curl_easy_setopt(curl_upload, CURLOPT_XFERINFODATA, &callbackData);
        curl_easy_setopt(curl_upload, CURLOPT_XFERINFOFUNCTION, curl_callback);
    }

    std::stringstream response_stream;
    curl_easy_setopt(curl_upload, CURLOPT_WRITEFUNCTION, write_string);
    curl_easy_setopt(curl_upload, CURLOPT_WRITEDATA, &response_stream);

    if (headers)
        curl_easy_setopt(curl_upload, CURLOPT_HTTPHEADER, headers);

    curl_mime *mime = curl_mime_init(curl_upload);
    curl_mimepart *part = nullptr;

    part = curl_mime_addpart(mime);
    curl_mime_name(part, "file");
    curl_mime_filename(part, input_file_path.c_str());
    curl_mime_data(part, file_buffer.data(), file_buffer.size());

    part = curl_mime_addpart(mime);
    curl_mime_name(part, "xml");
    curl_mime_data(part, metadata.c_str(), CURL_ZERO_TERMINATED);

    curl_easy_setopt(curl_upload, CURLOPT_MIMEPOST, mime);

    response.curl_res = curl_easy_perform(curl_upload);
    response.body = response_stream.str();

    curl_mime_free(mime);

    if (!has_session_given)
        cleanup_curl_session(local_session);

    return response;
}

CurlSession init_curl_download_session(const std::string &token) {
    CurlSession session;
    session.handle = curl_easy_init();

    if (!session.handle)
        return session;

    CURL *curl = static_cast<CURL *>(session.handle);

    curl_easy_setopt(curl, CURLOPT_USERAGENT, "Vita3K Emulator");
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, true);
    curl_easy_setopt(curl, CURLOPT_TCP_KEEPALIVE, 1L);
    curl_easy_setopt(curl, CURLOPT_TCP_KEEPIDLE, 120L);
    curl_easy_setopt(curl, CURLOPT_TCP_KEEPINTVL, 60L);

#ifdef __ANDROID__
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
#endif

    if (!token.empty()) {
        curl_slist *headers = nullptr;
        std::string auth_header = "Authorization: Bearer " + token;
        headers = curl_slist_append(headers, auth_header.c_str());
        session.headers = headers;
    }

    return session;
}

CurlSession init_curl_upload_session(const std::string &token) {
    CurlSession session;
    session.handle = curl_easy_init();

    if (!session.handle)
        return session;

    CURL *curl = static_cast<CURL *>(session.handle);

    curl_easy_setopt(curl, CURLOPT_USERAGENT, "Vita3K Emulator");
    curl_easy_setopt(curl, CURLOPT_TCP_KEEPALIVE, 1L);
    curl_easy_setopt(curl, CURLOPT_TCP_KEEPIDLE, 120L);
    curl_easy_setopt(curl, CURLOPT_TCP_KEEPINTVL, 60L);

#ifdef __ANDROID__
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
#endif

    if (!token.empty()) {
        curl_slist *headers = nullptr;
        std::string auth_header = "Authorization: Bearer " + token;
        headers = curl_slist_append(headers, auth_header.c_str());
        session.headers = headers;
    }

    return session;
}

void cleanup_curl_session(CurlSession &session) {
    if (session.headers) {
        curl_slist_free_all(static_cast<curl_slist *>(session.headers));
        session.headers = nullptr;
    }
    if (session.handle) {
        curl_easy_cleanup(static_cast<CURL *>(session.handle));
        session.handle = nullptr;
    }
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
