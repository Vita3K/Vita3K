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

#include <chrono>
#include <cmath>
#include <condition_variable>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <mutex>
#include <sstream>

namespace net_utils {

static uint64_t get_current_time_ms() {
    return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count());
}

struct DownloadCallbackData {
    ProgressData progress;
    const ProgressCallback *callback;
};

static size_t write_download_data(void *ptr, size_t size, size_t count, void *stream) {
    return std::fwrite(ptr, size, count, static_cast<FILE *>(stream));
}

static int download_progress_callback(void *user_data, curl_off_t total, curl_off_t current, curl_off_t, curl_off_t) {
    if (total <= 0)
        return 0;

    auto &data = *static_cast<DownloadCallbackData *>(user_data);
    if (!*data.callback)
        return 0;

    const auto now = get_current_time_ms();
    const auto elapsed = std::max<uint64_t>(now - data.progress.time, 1);
    const auto done = static_cast<uint64_t>(std::max<curl_off_t>(current, 0));
    const auto total_bytes = static_cast<uint64_t>(total);
    const auto all_done = done + data.progress.bytes_already_downloaded;
    const auto all_total = total_bytes + data.progress.bytes_already_downloaded;
    const auto remaining_bytes = total_bytes - std::min(done, total_bytes);
    const auto seconds_left = done > 0 ? static_cast<uint64_t>((static_cast<double>(remaining_bytes) / static_cast<double>(done)) * static_cast<double>(elapsed) / 1000.0) : 0;
    auto *state = (*data.callback)(all_total ? (100.f * static_cast<float>(all_done) / static_cast<float>(all_total)) : 0.f, seconds_left, all_done);
    if (!state)
        return 0;

    std::unique_lock lock(state->mutex);
    const auto pause_started = state->pause ? get_current_time_ms() : 0;
    state->cv.wait(lock, [&]() { return !state->pause; });
    if (pause_started)
        data.progress.time += get_current_time_ms() - pause_started;
    return state->canceled ? 1 : 0;
}

bool download_file(const std::string &url, const std::string &output_file_path, const ProgressCallback &progress_callback) {
    auto *curl = curl_easy_init();
    if (!curl)
        return false;

    const auto bytes_already_downloaded = std::filesystem::exists(output_file_path) ? std::filesystem::file_size(output_file_path) : 0;
    DownloadCallbackData callback_data{ { get_current_time_ms(), bytes_already_downloaded }, &progress_callback };
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "Vita3K Emulator");
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, true);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_download_data);
    curl_easy_setopt(curl, CURLOPT_RESUME_FROM_LARGE, static_cast<curl_off_t>(bytes_already_downloaded));
    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
    curl_easy_setopt(curl, CURLOPT_XFERINFODATA, &callback_data);
    curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, download_progress_callback);

#ifdef __ANDROID__
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
#endif

    auto *file = std::fopen(output_file_path.c_str(), "ab");
    if (!file) {
        curl_easy_cleanup(curl);
        return false;
    }
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, file);
    const auto result = curl_easy_perform(curl);
    long response_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);
    std::fclose(file);
    curl_easy_cleanup(curl);
    return result == CURLE_OK && (response_code == 200 || response_code == 206);
}

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

static int cancel_request_callback(void *user_data, curl_off_t, curl_off_t, curl_off_t, curl_off_t) {
    const auto *cancel_flag = static_cast<const std::atomic<bool> *>(user_data);
    return cancel_flag && cancel_flag->load() ? 1 : 0;
}

WebResponse get_web_response_ex(const std::string &url, const std::string &token, const std::string &post_data, CurlSession *session, const std::atomic<bool> *cancel_flag) {
    WebResponse response{};
    CurlSession local_session;
    const bool owns_session = session == nullptr;
    if (owns_session) {
        local_session = init_curl_download_session(token, false);
        session = &local_session;
    }

    if (!session->handle) {
        response.curl_res = CURLE_FAILED_INIT;
        return response;
    }

    auto *curl = static_cast<CURL *>(session->handle);
    curl_easy_reset(curl);
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "Vita3K Emulator");
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, cancel_flag ? 0L : 1L);
    if (cancel_flag) {
        curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, cancel_request_callback);
        curl_easy_setopt(curl, CURLOPT_XFERINFODATA, cancel_flag);
    }
#ifdef __ANDROID__
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
#endif
    if (!post_data.empty()) {
        curl_easy_setopt(curl, CURLOPT_POST, 1L);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post_data.data());
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, static_cast<long>(post_data.size()));
    }
    const auto write_response = +[](void *data, size_t size, size_t count, std::string *body) {
        body->append(static_cast<char *>(data), size * count);
        return size * count;
    };
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_response);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response.body);
    if (session->headers)
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, static_cast<curl_slist *>(session->headers));
    response.curl_res = curl_easy_perform(curl);
    long status_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status_code);

    if (owns_session)
        cleanup_curl_session(local_session);
    if (status_code / 100 != 2)
        response.body.clear();
    return response;
}

struct TransferProgressData {
    const ProgressCallback *callback;
    uint64_t start_time;
    uint64_t bytes_already_downloaded;
};

static int transfer_progress_callback(void *user_data, curl_off_t download_total, curl_off_t download_now, curl_off_t upload_total, curl_off_t upload_now) {
    auto &data = *static_cast<TransferProgressData *>(user_data);
    if (!*data.callback)
        return 0;

    const bool downloading = download_total > 0;
    const auto done = static_cast<uint64_t>(std::max<curl_off_t>(downloading ? download_now : upload_now, 0));
    const auto total = static_cast<uint64_t>(std::max<curl_off_t>(downloading ? download_total : upload_total, 0));
    if (!total)
        return 0;

    const auto elapsed = std::max<uint64_t>(get_current_time_ms() - data.start_time, 1);
    const auto completed = done + (downloading ? data.bytes_already_downloaded : 0);
    const auto all_total = total + (downloading ? data.bytes_already_downloaded : 0);
    const auto remaining = total - std::min(done, total);
    const auto seconds_left = done ? static_cast<uint64_t>((static_cast<double>(remaining) / done) * elapsed / 1000.0) : 0;
    auto *state = (*data.callback)(all_total ? 100.f * static_cast<float>(completed) / static_cast<float>(all_total) : 0.f, seconds_left, completed);
    if (!state)
        return 0;

    std::unique_lock lock(state->mutex);
    state->cv.wait(lock, [&]() { return !state->pause; });
    return state->canceled ? 1 : 0;
}

WebResponse download_file_ex(const std::string &url, const std::string &output_file_path, const ProgressCallback &progress_callback, const std::string &token, CurlSession *session) {
    WebResponse response{};
    CurlSession local_session;
    const bool owns_session = session == nullptr;
    if (owns_session) {
        local_session = init_curl_download_session(token, false);
        session = &local_session;
    }
    if (!session->handle) {
        response.curl_res = CURLE_FAILED_INIT;
        return response;
    }

    const auto downloaded = std::filesystem::exists(output_file_path) ? std::filesystem::file_size(output_file_path) : 0;
    auto *file = std::fopen(output_file_path.c_str(), downloaded ? "ab" : "wb");
    if (!file) {
        response.curl_res = CURLE_WRITE_ERROR;
        if (owns_session)
            cleanup_curl_session(local_session);
        return response;
    }

    auto *curl = static_cast<CURL *>(session->handle);
    curl_easy_reset(curl);
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "Vita3K Emulator");
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_download_data);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, file);
    curl_easy_setopt(curl, CURLOPT_RESUME_FROM_LARGE, static_cast<curl_off_t>(downloaded));
    TransferProgressData progress_data{ &progress_callback, get_current_time_ms(), downloaded };
    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, progress_callback ? 0L : 1L);
    if (progress_callback) {
        curl_easy_setopt(curl, CURLOPT_XFERINFODATA, &progress_data);
        curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, transfer_progress_callback);
    }
    if (session->headers)
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, static_cast<curl_slist *>(session->headers));
#ifdef __ANDROID__
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
#endif
    response.curl_res = curl_easy_perform(curl);
    long status_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status_code);
    std::fclose(file);
    if (owns_session)
        cleanup_curl_session(local_session);
    if (response.curl_res == CURLE_OK && status_code / 100 != 2)
        response.curl_res = CURLE_HTTP_RETURNED_ERROR;
    return response;
}

static WebResponse upload_data_impl(const std::string &url, const char *bytes, size_t byte_count, const std::string &filename, const std::string &token, const std::string &metadata, const ProgressCallback &progress_callback, CurlSession *session) {
    WebResponse response{};
    CurlSession local_session;
    const bool owns_session = session == nullptr;
    if (owns_session) {
        local_session = init_curl_upload_session(token, false);
        session = &local_session;
    }
    if (!session->handle) {
        response.curl_res = CURLE_FAILED_INIT;
        return response;
    }

    auto *curl = static_cast<CURL *>(session->handle);
    curl_easy_reset(curl);
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "Vita3K Emulator");
    if (session->headers)
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, static_cast<curl_slist *>(session->headers));
#ifdef __ANDROID__
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
#endif

    std::ostringstream body;
    const auto write_response = +[](void *data, size_t size, size_t count, std::ostringstream *output) {
        output->write(static_cast<const char *>(data), static_cast<std::streamsize>(size * count));
        return size * count;
    };
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_response);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &body);
    TransferProgressData progress_data{ &progress_callback, get_current_time_ms(), 0 };
    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, progress_callback ? 0L : 1L);
    if (progress_callback) {
        curl_easy_setopt(curl, CURLOPT_XFERINFODATA, &progress_data);
        curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, transfer_progress_callback);
    }

    auto *mime = curl_mime_init(curl);
    auto *file_part = curl_mime_addpart(mime);
    curl_mime_name(file_part, "file");
    curl_mime_filename(file_part, filename.c_str());
    curl_mime_data(file_part, bytes, byte_count);
    if (!metadata.empty()) {
        auto *metadata_part = curl_mime_addpart(mime);
        curl_mime_name(metadata_part, "xml");
        curl_mime_data(metadata_part, metadata.c_str(), CURL_ZERO_TERMINATED);
    }
    curl_easy_setopt(curl, CURLOPT_MIMEPOST, mime);
    response.curl_res = curl_easy_perform(curl);
    long status_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status_code);
    curl_mime_free(mime);
    response.body = body.str();
    if (owns_session)
        cleanup_curl_session(local_session);
    if (status_code / 100 != 2)
        response.body.clear();
    return response;
}

WebResponse upload_file(const std::string &url, const std::string &input_file_path, const std::string &token, const std::string &metadata, const ProgressCallback &progress_callback, CurlSession *session) {
    std::ifstream file(input_file_path, std::ios::binary);
    if (!file)
        return { CURLE_READ_ERROR, {} };
    const std::vector<char> bytes{ std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>() };
    return upload_data_impl(url, bytes.data(), bytes.size(), std::filesystem::path(input_file_path).filename().string(), token, metadata, progress_callback, session);
}

WebResponse upload_data(const std::string &url, const std::vector<unsigned char> &data, const std::string &filename, const std::string &token, CurlSession *session) {
    if (data.empty())
        return { CURLE_READ_ERROR, {} };
    return upload_data_impl(url, reinterpret_cast<const char *>(data.data()), data.size(), filename, token, {}, {}, session);
}

CurlSession init_curl_download_session(const std::string &token, bool is_keep_alive) {
    CurlSession session;
    session.handle = curl_easy_init();
    if (!session.handle)
        return session;
    auto *curl = static_cast<CURL *>(session.handle);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "Vita3K Emulator");
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    if (is_keep_alive) {
        curl_easy_setopt(curl, CURLOPT_TCP_KEEPALIVE, 1L);
        curl_easy_setopt(curl, CURLOPT_TCP_KEEPIDLE, 120L);
        curl_easy_setopt(curl, CURLOPT_TCP_KEEPINTVL, 60L);
    }
    if (!token.empty()) {
        auto *headers = curl_slist_append(nullptr, ("Authorization: Bearer " + token).c_str());
        session.headers = headers;
    }
    return session;
}

CurlSession init_curl_upload_session(const std::string &token, bool is_keep_alive) {
    return init_curl_download_session(token, is_keep_alive);
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

std::string get_web_response(const std::string &url) {
    return get_web_response_ex(url).body;
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
