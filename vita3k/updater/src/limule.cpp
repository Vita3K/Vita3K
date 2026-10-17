#include <updater/functions.h>

#include <nlohmann/json.hpp>

#include <algorithm>
#include <charconv>
#include <regex>

namespace updater {

std::optional<LimuleRelease> parse_limule_release(std::string_view response) {
    const auto json = nlohmann::json::parse(response, nullptr, false);
    if (!json.is_object() || !json.contains("body") || !json["body"].is_string()
        || !json.contains("tag_name") || json["tag_name"] != "continuous")
        return std::nullopt;

    const auto &body = json["body"].get_ref<const std::string &>();
    static const std::regex metadata_regex(
        R"(Corresponding commit:\s*([a-f0-9]{7,40})\s+Vita3K Build:\s*([0-9]+)\s*<!--\s*App master count:\s*([0-9]+)\s*-->)");
    std::smatch match;
    if (!std::regex_search(body, match, metadata_regex))
        return std::nullopt;

    LimuleRelease release{ match[1].str(), 0, 0 };
    const auto parse_number = [](const std::string &text, int &value) {
        const auto result = std::from_chars(text.data(), text.data() + text.size(), value);
        return result.ec == std::errc{} && result.ptr == text.data() + text.size();
    };
    if (!parse_number(match[2].str(), release.build_number) || release.build_number <= 0
        || !parse_number(match[3].str(), release.master_number) || release.master_number > release.build_number)
        return std::nullopt;
    return release;
}

std::optional<std::vector<LimuleCommit>> parse_limule_commits(std::string_view response) {
    const auto json = nlohmann::json::parse(response, nullptr, false);
    if (!json.is_array())
        return std::nullopt;
    std::vector<LimuleCommit> commits;
    for (const auto &item : json) {
        if (!item.is_object() || !item.contains("sha") || !item["sha"].is_string()
            || !item.contains("commit") || !item["commit"].is_object())
            return std::nullopt;
        const auto &commit = item["commit"];
        if (!commit.contains("author") || !commit["author"].is_object()
            || !commit["author"].contains("name") || !commit["author"]["name"].is_string()
            || !commit.contains("message") || !commit["message"].is_string())
            return std::nullopt;
        commits.push_back({ commit["author"]["name"].get<std::string>(), item["sha"].get<std::string>(),
            commit["message"].get<std::string>() });
    }
    return commits;
}

unsigned int limule_update_count(int current_build, int release_build) {
    return static_cast<unsigned int>(std::max(release_build - std::max(current_build, 0), 0));
}

} // namespace updater