#include <updater/functions.h>

#include <algorithm>
#define BOOST_CONTAINER_NO_LIB
#define BOOST_JSON_NO_LIB
#include <boost/json/src.hpp>
#include <cctype>
#include <cstdint>
#include <limits>
#include <regex>
#include <sstream>
#include <utility>

namespace updater {
namespace {

struct ParsedUrl {
    std::string scheme;
    std::string host;
    std::string path;
};

std::optional<ParsedUrl> parse_url(std::string_view value) {
    static const std::regex expression(R"(^([A-Za-z][A-Za-z0-9+.-]*)://([^/?#]+)([^?#]*).*$)");
    const std::string url(value);
    std::smatch match;
    if (!std::regex_match(url, match, expression))
        return std::nullopt;
    std::string authority = match[2].str();
    if (authority.find('@') != std::string::npos)
        return std::nullopt;
    const std::size_t port_separator = authority.find(':');
    std::string scheme = match[1].str();
    std::transform(scheme.begin(), scheme.end(), scheme.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    if (port_separator != std::string::npos) {
        if (authority.find(':', port_separator + 1) != std::string::npos || scheme != "https"
            || authority.substr(port_separator + 1) != "443")
            return std::nullopt;
        authority.resize(port_separator);
    }
    std::string host = std::move(authority);
    std::transform(host.begin(), host.end(), host.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return ParsedUrl{ std::move(scheme), std::move(host), match[3].str() };
}

std::string field(const boost::json::object &tree, std::string_view key) {
    const boost::json::value *value = tree.if_contains(boost::json::string_view(key.data(), key.size()));
    if (!value || !value->is_string())
        return {};
    const auto text = value->as_string();
    return { text.data(), text.size() };
}

std::string trim_ascii_whitespace(std::string value) {
    const auto not_whitespace = [](unsigned char character) { return !std::isspace(character); };
    const auto first = std::find_if(value.begin(), value.end(), not_whitespace);
    const auto last = std::find_if(value.rbegin(), value.rend(), not_whitespace).base();
    if (first >= last)
        return {};
    return std::string(first, last);
}

bool has_dot_path_segment(std::string_view path) {
    std::string decoded;
    decoded.reserve(path.size());
    const auto hex_value = [](const char c) -> int {
        if (c >= '0' && c <= '9')
            return c - '0';
        if (c >= 'a' && c <= 'f')
            return c - 'a' + 10;
        if (c >= 'A' && c <= 'F')
            return c - 'A' + 10;
        return -1;
    };
    for (std::size_t i = 0; i < path.size(); ++i) {
        if (path[i] != '%') {
            decoded.push_back(path[i]);
            continue;
        }
        if (i + 2 >= path.size())
            return true;
        const int high = hex_value(path[i + 1]);
        const int low = hex_value(path[i + 2]);
        if (high < 0 || low < 0)
            return true;
        decoded.push_back(static_cast<char>((high << 4) | low));
        i += 2;
    }
    std::string_view remaining(decoded);
    while (!remaining.empty()) {
        const std::size_t separator = remaining.find('/');
        const std::string_view segment = remaining.substr(0, separator);
        if (segment == "." || segment == "..")
            return true;
        if (separator == std::string_view::npos)
            break;
        remaining.remove_prefix(separator + 1);
    }
    return false;
}

std::string clean_notes(const std::string &body) {
    static const std::regex metadata(R"(^\s*(Corresponding commit:|Vita3K Build:))", std::regex::icase);
    std::istringstream lines(body);
    std::string line;
    std::string result;
    bool first_line = true;
    while (std::getline(lines, line)) {
        if (!line.empty() && line.back() == '\r')
            line.pop_back();
        if (!std::regex_search(line, metadata)) {
            if (!first_line)
                result.push_back('\n');
            result += line;
            first_line = false;
        }
    }
    const auto first = result.find_first_not_of(" \t\r\n");
    if (first == std::string::npos)
        return {};
    const auto last = result.find_last_not_of(" \t\r\n");
    return result.substr(first, last - first + 1);
}

bool is_trusted_release_page(std::string_view url) {
    const auto parsed = parse_url(url);
    return parsed && parsed->scheme == "https" && parsed->host == "github.com"
        && parsed->path == "/Vita3K/Vita3K/releases/tag/continuous";
}

bool is_trusted_asset_url(std::string_view url) {
    const auto parsed = parse_url(url);
    if (!parsed || parsed->scheme != "https" || parsed->host != "github.com")
        return false;

    constexpr std::string_view release_download_prefix = "/Vita3K/Vita3K/releases/download/";
    if (!parsed->path.starts_with(release_download_prefix))
        return false;

    const std::string_view remaining_path(parsed->path.data() + release_download_prefix.size(), parsed->path.size() - release_download_prefix.size());
    return !has_dot_path_segment(remaining_path);
}

} // namespace

bool is_release_api_url(std::string_view url) {
    const auto parsed = parse_url(url);
    return parsed && parsed->scheme == "https" && parsed->host == "api.github.com"
        && parsed->path == "/repos/Vita3K/Vita3K/releases/tags/continuous";
}

bool is_release_asset_url(std::string_view url) {
    return is_trusted_asset_url(url);
}

std::optional<UpdateAsset> find_asset(const UpdateInfo &info, std::string_view expected_name) {
    const auto found = std::find_if(info.assets.begin(), info.assets.end(), [expected_name](const UpdateAsset &asset) {
        return asset.name == expected_name;
    });
    return found == info.assets.end() ? std::nullopt : std::optional<UpdateAsset>(*found);
}

std::string_view update_status_name(const UpdateCheckStatus status) {
    switch (status) {
    case UpdateCheckStatus::Failed: return "Failed";
    case UpdateCheckStatus::UpToDate: return "UpToDate";
    case UpdateCheckStatus::UpdateAvailable: return "UpdateAvailable";
    case UpdateCheckStatus::CurrentBuildNewerThanLatest: return "CurrentBuildNewerThanLatest";
    case UpdateCheckStatus::CustomBuildCanUpdate: return "CustomBuildCanUpdate";
    }
    return "Failed";
}

std::string_view update_failure_reason_name(const UpdateCheckFailureReason reason) {
    switch (reason) {
    case UpdateCheckFailureReason::None: return "None";
    case UpdateCheckFailureReason::InvalidReleaseResponse: return "InvalidReleaseResponse";
    case UpdateCheckFailureReason::InvalidBuildMetadata: return "InvalidBuildMetadata";
    }
    return "None";
}

UpdateCheckResult check_release_response(const std::string_view response, const std::uint64_t current_build, const bool official_build, std::string current_display_version) {
    UpdateCheckResult result;
    result.current_display_version = std::move(current_display_version);
    boost::system::error_code parse_error;
    const boost::json::value parsed_root = boost::json::parse(boost::json::string_view(response.data(), response.size()), parse_error);
    if (parse_error || !parsed_root.is_object()) {
        result.failure_reason = UpdateCheckFailureReason::InvalidReleaseResponse;
        return result;
    }
    const boost::json::object &root = parsed_root.as_object();

    const std::string body = field(root, "body");
    static const std::regex build_regex(R"(^[ \t]*Vita3K Build:[ \t]*([0-9]+)[ \t]*$)");
    std::smatch build_match;
    std::istringstream body_lines(body);
    std::string build_line;
    bool found_build = false;
    while (std::getline(body_lines, build_line)) {
        if (!build_line.empty() && build_line.back() == '\r')
            build_line.pop_back();
        if (std::regex_match(build_line, build_match, build_regex)) {
            found_build = true;
            break;
        }
    }
    if (!found_build) {
        result.failure_reason = UpdateCheckFailureReason::InvalidBuildMetadata;
        return result;
    }
    try {
        result.info.build_number = std::stoull(build_match[1].str());
    } catch (...) {
        result.failure_reason = UpdateCheckFailureReason::InvalidBuildMetadata;
        return result;
    }
    if (result.info.build_number == 0) {
        result.failure_reason = UpdateCheckFailureReason::InvalidBuildMetadata;
        return result;
    }
#if defined(__ANDROID__)
    if (result.info.build_number > static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max())) {
        result.failure_reason = UpdateCheckFailureReason::InvalidBuildMetadata;
        return result;
    }
#endif

    result.info.version = trim_ascii_whitespace(field(root, "name"));
    if (result.info.version.empty())
        result.info.version = trim_ascii_whitespace(field(root, "tag_name"));
    result.info.release_url = field(root, "html_url");
    if (!is_trusted_release_page(result.info.release_url))
        result.info.release_url = release_page_url();
    result.info.published_at = field(root, "published_at");
    result.info.notes = clean_notes(body);

    if (const boost::json::value *changelog = root.if_contains("changelog"); changelog && changelog->is_array()) {
        for (const boost::json::value &item : changelog->as_array()) {
            if (!item.is_object())
                continue;
            const auto &item_object = item.as_object();
            ChangelogEntry entry{ field(item_object, "version"), field(item_object, "title") };
            if (!entry.version.empty() || !entry.title.empty())
                result.info.changelog.push_back(std::move(entry));
        }
    }
    if (const boost::json::value *assets = root.if_contains("assets"); assets && assets->is_array()) {
        for (const boost::json::value &item : assets->as_array()) {
            if (!item.is_object())
                continue;
            const auto &item_object = item.as_object();
            UpdateAsset asset;
            asset.name = field(item_object, "name");
            asset.download_url = field(item_object, "browser_download_url");
            if (const boost::json::value *size = item_object.if_contains("size"); size) {
                if (size->is_uint64())
                    asset.size = size->as_uint64();
                else if (size->is_int64() && size->as_int64() >= 0)
                    asset.size = static_cast<std::uint64_t>(size->as_int64());
            }
            if (!asset.name.empty() && is_trusted_asset_url(asset.download_url))
                result.info.assets.push_back(std::move(asset));
        }
    }

    if (!official_build) {
        result.status = UpdateCheckStatus::CustomBuildCanUpdate;
    } else if (result.info.build_number > current_build) {
        result.status = UpdateCheckStatus::UpdateAvailable;
    } else if (result.info.build_number < current_build) {
        result.status = UpdateCheckStatus::CurrentBuildNewerThanLatest;
        result.message = "This installation appears newer than the latest official continuous build.";
    } else {
        result.status = UpdateCheckStatus::UpToDate;
        result.message = "You already have the latest official continuous build installed.";
    }
    return result;
}

UpdateCheckResult check_release_response(const std::string_view response, const std::string_view current_version, const bool official_build) {
    static const std::regex version_pattern(R"(^([^-]+)-(\d+)(?:-|$))");
    const std::string version(current_version);
    std::smatch match;
    std::uint64_t build_number = 0;
    std::string display = version;
    if (std::regex_search(version, match, version_pattern)) {
        try {
            build_number = std::stoull(match[2].str());
            display = match[1].str() + " (" + match[2].str() + ")";
        } catch (...) {
            build_number = 0;
        }
    }
    return check_release_response(response, build_number, official_build, std::move(display));
}

} // namespace updater
