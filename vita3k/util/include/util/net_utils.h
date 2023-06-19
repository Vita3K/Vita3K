#pragma once

#include <http/state.h>
#include <map>
#include <stdint.h>

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

SceHttpErrorCode parse_url(std::string url, parsedUrl &out);
const char *int_method_to_char(const int n);
int char_method_to_int(const char *srcUrl);
std::string constructHeaders(std::map<std::string, std::string, CaseInsensitiveComparator> &headers);
bool parseHeaders(std::string &headersRaw, std::map<std::string, std::string, CaseInsensitiveComparator> &headersOut);
bool parseResponse(std::string response, SceRequestResponse &reqres);

bool socketSetBlocking(int sockfd, bool blocking);

} // namespace net_utils
