#include <util/string_convert.h>

#ifdef _WIN32
#include <windows.h>
#include <processenv.h>
#include <shellapi.h>
#endif

#include <codecvt>   // std::codecvt_utf8
#include <locale>    // std::wstring_convert
#include <iostream>

std::wstring utf_to_wide(const std::string& str)
{
    std::wstring_convert<std::codecvt_utf8<wchar_t>> myconv;
    return myconv.from_bytes(str);
}

std::string wide_to_utf(const std::wstring& str)
{
    std::wstring_convert<std::codecvt_utf8<wchar_t>> myconv;
    return myconv.to_bytes(str);
}

ProgramArgsWide process_args(int argc, char* argv[])
{
    ProgramArgsWide args;

    int n_args = argc;
    args.reserve(n_args);

#ifdef _WIN32
    LPWSTR *wide_arg_list = CommandLineToArgvW(GetCommandLineW(), &n_args);

    if (nullptr == wide_arg_list)
    {
        std::cerr << "CommandLineToArgvW failed\n";
        return {};
    }

    for (auto i = 0; i < n_args; i++)
    {
        args.emplace_back(wide_arg_list[i]);
    }

    LocalFree(wide_arg_list);
#else
    for (auto i = 0; i < n_args; i++)
    {
        args.emplace_back(utf_to_wide(argv[i]));
    }
#endif

    return args;
}
