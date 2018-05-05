#include <util/log.h>
#include <util/string_convert.h>

#ifdef WIN32
#include <direct.h>
#include <processenv.h>
#include <shellapi.h>
#include <windows.h>
#define getcwd _getcwd // stupid MSFT "deprecation" warning
#else
#include <unistd.h>
#endif

#ifdef _MSC_VER
#include <spdlog/sinks/msvc_sink.h>
#endif

#include <codecvt> // std::codecvt_utf8
#include <iostream>
#include <locale> // std::wstring_convert
#include <memory>
#include <string>

std::shared_ptr<spdlog::logger> g_logger;

static const
#ifdef WIN32
    std::wstring &LOG_FILE_NAME
    = L"\\vita3k.log";
#else
    std::string &LOG_FILE_NAME
    = "/vita3k.log";
#endif

void init_logging() {
#ifdef _MSC_VER
    static constexpr bool LOG_MSVC_OUTPUT = true;
#endif

    std::vector<spdlog::sink_ptr> sinks;

#ifdef WIN32
    sinks.push_back(std::make_shared<spdlog::sinks::wincolor_stdout_sink_mt>());
#else
    sinks.push_back(std::make_shared<spdlog::sinks::ansicolor_stdout_sink_mt>());
#endif
    try {
        spdlog::filename_t s_cwd;

#ifdef WIN32
        wchar_t buffer[MAX_PATH];
        GetModuleFileNameW(NULL, buffer, MAX_PATH);
        std::string::size_type pos = std::wstring(buffer).find_last_of(L"\\\\");
        std::wstring path = std::wstring(buffer).substr(0, pos);
        if (!path.empty())
            s_cwd = path;
#else
        char buffer[512];
        char *path = getcwd(buffer, sizeof(buffer));
        if (path)
            s_cwd = path;
#endif
        else {
            std::cerr << "failed to get working directory" << std::endl;
        }
        const auto full_log_path = s_cwd + LOG_FILE_NAME;

#ifdef WIN32
        DeleteFileW(full_log_path.c_str());
#else
        remove(full_log_path.c_str());
#endif

        sinks.push_back(std::make_shared<spdlog::sinks::simple_file_sink_mt>(full_log_path));
    } catch (const spdlog::spdlog_ex &ex) {
        std::cerr << "File log initialization failed: " << ex.what() << std::endl;
    }

#ifdef _MSC_VER
    if (LOG_MSVC_OUTPUT)
        sinks.push_back(std::make_shared<spdlog::sinks::msvc_sink_st>());
#endif

    g_logger = std::make_shared<spdlog::logger>("vita3k logger", begin(sinks), end(sinks));
    spdlog::register_logger(g_logger);

    spdlog::set_error_handler([](const std::string &msg) {
        std::cerr << "spdlog error: " << msg << std::endl;
    });

    spdlog::set_pattern("[%H:%M:%S.%e] %v");

    spdlog::set_level(spdlog::level::trace);

    g_logger->flush_on(spdlog::level::debug);
}

std::wstring utf_to_wide(const std::string &str) {
    std::wstring_convert<std::codecvt_utf8<wchar_t>> myconv;
    return myconv.from_bytes(str);
}

std::string wide_to_utf(const std::wstring &str) {
    std::wstring_convert<std::codecvt_utf8<wchar_t>> myconv;
    return myconv.to_bytes(str);
}

#ifdef WIN32
std::string utf16_to_utf8(const std::u16string &str) {
    std::wstring_convert<std::codecvt_utf8_utf16<int16_t>, int16_t> myconv;
    auto p = reinterpret_cast<const int16_t *>(str.data());
    return myconv.to_bytes(p, p + str.size());
}

std::u16string utf8_to_utf16(const std::string &str) {
    static_assert(sizeof(std::wstring::value_type) == sizeof(std::u16string::value_type),
        "std::wstring and std::u16string are expected to have the same character size");

    std::wstring_convert<std::codecvt_utf8_utf16<int16_t>, int16_t> myconv;
    auto p = reinterpret_cast<const char *>(str.data());
    auto a = myconv.from_bytes(p, p + std::strlen(p));
    return std::u16string(a.begin(), a.end());
}
#else
std::string utf16_to_utf8(const std::u16string &str) {
    std::wstring_convert<std::codecvt_utf8_utf16<char16_t>, char16_t> myconv;
    return myconv.to_bytes(str);
}

std::u16string utf8_to_utf16(const std::string &str) {
    std::wstring_convert<std::codecvt_utf8_utf16<char16_t>, char16_t> myconv;
    return myconv.from_bytes(str);
}
#endif

ProgramArgsWide process_args(int argc, char *argv[]) {
    ProgramArgsWide args;

    int n_args = argc;
    args.reserve(n_args);

#ifdef _WIN32
    LPWSTR *wide_arg_list = CommandLineToArgvW(GetCommandLineW(), &n_args);

    if (nullptr == wide_arg_list) {
        LOG_ERROR("CommandLineToArgvW failed\n");
        return {};
    }

    for (auto i = 0; i < n_args; i++) {
        args.emplace_back(wide_arg_list[i]);
    }

    LocalFree(wide_arg_list);
#else
    for (auto i = 0; i < n_args; i++) {
        args.emplace_back(utf_to_wide(argv[i]));
    }
#endif

    return args;
}
