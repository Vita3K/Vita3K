#pragma once

#include <http/state.h>
#include <util/log.h>

#include <condition_variable>
#include <cstdint>
#include <regex>

namespace net_utils {

enum SceHttpMethods {
    SCE_HTTP_METHOD_GET,
    SCE_HTTP_METHOD_POST,
    SCE_HTTP_METHOD_HEAD,
    SCE_HTTP_METHOD_OPTIONS,
    SCE_HTTP_METHOD_PUT,
    SCE_HTTP_METHOD_DELETE,
    SCE_HTTP_METHOD_TRACE,
    SCE_HTTP_METHOD_CONNECT,
};

// https://username:password@lttstore.com:727/wysi/cookie.php?pog=gers#extremeexploit
struct parsedUrl {
    std::string scheme; // https
    std::string username; // username
    std::string password; // password
    std::string hostname; // lttstore.com
    std::string port; // 727
    std::string path; // /wysi/cookie.php
    std::string query; // ?pog=gers
    std::string fragment; // #extremeexploit
    bool invalid = false;
};

struct ProgressState {
    std::condition_variable cv;
    std::mutex mutex;
    bool download = true;
    bool pause = false;
};

struct ProgressData {
    uint64_t time;
    uint64_t bytes_already_downloaded;
};

struct AssignedAddr {
    std::string name; // Name of the interface
    std::string addr; // Assigned address
    std::string netMask; // Network mask
};

typedef const std::function<ProgressState *(float, uint64_t, double)> &ProgressCallback;
typedef std::pair<ProgressData, ProgressCallback> CallbackData;

bool download_file(const std::string &url, const std::string &output_file_path, ProgressCallback progress_callback = nullptr);
std::string get_web_response(const std::string &url);
std::string get_web_regex_result(const std::string &url, const std::regex &regex);

SceHttpErrorCode parse_url(const std::string &url, parsedUrl &out);
const char *int_method_to_char(const int n);
int char_method_to_int(const char *srcUrl);
std::string constructHeaders(const HeadersMapType &headers);
bool parseStatusLine(const std::string &line, std::string &httpVer, int &statusCode, std::string &reason);
bool parseHeaders(std::string &headersRaw, HeadersMapType &headersOut);
bool parseResponse(const std::string &response, SceRequestResponse &reqres);

bool socketSetBlocking(int sockfd, bool blocking);

std::vector<AssignedAddr> get_all_assigned_addrs();
AssignedAddr get_selected_assigned_addr(int32_t &outIndex);
void init_address(int32_t &outIndex, uint32_t &netAddr, uint32_t &broadcastAddr);

} // namespace net_utils
