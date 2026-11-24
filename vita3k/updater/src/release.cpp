#include <updater/functions.h>

#include <algorithm>
#include <cctype>
#include <charconv>
#include <cstdint>
#include <regex>
#include <sstream>
#include <utility>
#include <vector>

namespace updater {
namespace {

struct ParsedUrl {
    std::string scheme;
    std::string host;
    std::string path;
};

struct JsonValue {
    enum class Type {
        Null,
        String,
        Number,
        Object,
        Array,
        Other,
    };

    Type type = Type::Null;
    std::string text;
    std::vector<std::pair<std::string, JsonValue>> members;
    std::vector<JsonValue> elements;

    const JsonValue *member(std::string_view name) const {
        const auto found = std::find_if(members.rbegin(), members.rend(), [name](const auto &entry) {
            return entry.first == name;
        });
        return found == members.rend() ? nullptr : &found->second;
    }
};

class JsonParser {
public:
    explicit JsonParser(std::string_view input)
        : input_(input) {
    }

    std::optional<JsonValue> parse() {
        JsonValue value;
        skip_whitespace();
        if (!parse_value(value, 0))
            return std::nullopt;
        skip_whitespace();
        if (position_ != input_.size())
            return std::nullopt;
        return value;
    }

private:
    void skip_whitespace() {
        while (position_ < input_.size() && (input_[position_] == ' ' || input_[position_] == '\t' || input_[position_] == '\r' || input_[position_] == '\n'))
            ++position_;
    }

    bool parse_value(JsonValue &value, unsigned depth) {
        if (depth > 64 || position_ >= input_.size())
            return false;
        skip_whitespace();
        if (position_ >= input_.size())
            return false;
        switch (input_[position_]) {
        case '"':
            value.type = JsonValue::Type::String;
            return parse_string(value.text);
        case '{':
            return parse_object(value, depth + 1);
        case '[':
            return parse_array(value, depth + 1);
        case 't':
            return parse_literal("true", JsonValue::Type::Other, value);
        case 'f':
            return parse_literal("false", JsonValue::Type::Other, value);
        case 'n':
            return parse_literal("null", JsonValue::Type::Null, value);
        default:
            return parse_number(value);
        }
    }

    bool parse_object(JsonValue &value, unsigned depth) {
        ++position_;
        value.type = JsonValue::Type::Object;
        skip_whitespace();
        if (consume('}'))
            return true;
        while (position_ < input_.size()) {
            std::string key;
            if (!parse_string(key))
                return false;
            skip_whitespace();
            if (!consume(':'))
                return false;
            JsonValue member_value;
            if (!parse_value(member_value, depth))
                return false;
            value.members.emplace_back(std::move(key), std::move(member_value));
            skip_whitespace();
            if (consume('}'))
                return true;
            if (!consume(','))
                return false;
            skip_whitespace();
        }
        return false;
    }

    bool parse_array(JsonValue &value, unsigned depth) {
        ++position_;
        value.type = JsonValue::Type::Array;
        skip_whitespace();
        if (consume(']'))
            return true;
        while (position_ < input_.size()) {
            JsonValue element;
            if (!parse_value(element, depth))
                return false;
            value.elements.push_back(std::move(element));
            skip_whitespace();
            if (consume(']'))
                return true;
            if (!consume(','))
                return false;
        }
        return false;
    }

    bool parse_string(std::string &value) {
        if (!consume('"'))
            return false;
        while (position_ < input_.size()) {
            const auto character = static_cast<unsigned char>(input_[position_++]);
            if (character == '"')
                return true;
            if (character < 0x20)
                return false;
            if (character != '\\') {
                value.push_back(static_cast<char>(character));
                continue;
            }
            if (position_ >= input_.size())
                return false;
            switch (input_[position_++]) {
            case '"': value.push_back('"'); break;
            case '\\': value.push_back('\\'); break;
            case '/': value.push_back('/'); break;
            case 'b': value.push_back('\b'); break;
            case 'f': value.push_back('\f'); break;
            case 'n': value.push_back('\n'); break;
            case 'r': value.push_back('\r'); break;
            case 't': value.push_back('\t'); break;
            case 'u': {
                std::uint32_t codepoint = 0;
                if (!parse_hex_quad(codepoint))
                    return false;
                if (codepoint >= 0xD800 && codepoint <= 0xDBFF) {
                    if (input_.size() - position_ < 6 || input_[position_] != '\\' || input_[position_ + 1] != 'u')
                        return false;
                    position_ += 2;
                    std::uint32_t low_surrogate = 0;
                    if (!parse_hex_quad(low_surrogate) || low_surrogate < 0xDC00 || low_surrogate > 0xDFFF)
                        return false;
                    codepoint = 0x10000 + ((codepoint - 0xD800) << 10) + (low_surrogate - 0xDC00);
                } else if (codepoint >= 0xDC00 && codepoint <= 0xDFFF) {
                    return false;
                }
                append_utf8(value, codepoint);
                break;
            }
            default: return false;
            }
        }
        return false;
    }

    bool parse_hex_quad(std::uint32_t &value) {
        if (input_.size() - position_ < 4)
            return false;
        value = 0;
        for (int index = 0; index < 4; ++index) {
            const char digit = input_[position_++];
            value <<= 4;
            if (digit >= '0' && digit <= '9')
                value |= static_cast<std::uint32_t>(digit - '0');
            else if (digit >= 'a' && digit <= 'f')
                value |= static_cast<std::uint32_t>(digit - 'a' + 10);
            else if (digit >= 'A' && digit <= 'F')
                value |= static_cast<std::uint32_t>(digit - 'A' + 10);
            else
                return false;
        }
        return true;
    }

    static void append_utf8(std::string &value, std::uint32_t codepoint) {
        if (codepoint <= 0x7F) {
            value.push_back(static_cast<char>(codepoint));
        } else if (codepoint <= 0x7FF) {
            value.push_back(static_cast<char>(0xC0 | (codepoint >> 6)));
            value.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
        } else if (codepoint <= 0xFFFF) {
            value.push_back(static_cast<char>(0xE0 | (codepoint >> 12)));
            value.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F)));
            value.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
        } else {
            value.push_back(static_cast<char>(0xF0 | (codepoint >> 18)));
            value.push_back(static_cast<char>(0x80 | ((codepoint >> 12) & 0x3F)));
            value.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F)));
            value.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
        }
    }

    bool parse_number(JsonValue &value) {
        const std::size_t start = position_;
        if (consume('-') && position_ >= input_.size())
            return false;
        if (consume('0')) {
            if (position_ < input_.size() && input_[position_] >= '0' && input_[position_] <= '9')
                return false;
        } else {
            if (!consume_digits())
                return false;
        }
        if (consume('.')) {
            if (!consume_digits())
                return false;
        }
        if (consume('e') || consume('E')) {
            if (!consume('+'))
                consume('-');
            if (!consume_digits())
                return false;
        }
        value.type = JsonValue::Type::Number;
        value.text = std::string(input_.substr(start, position_ - start));
        return true;
    }

    bool consume_digits() {
        const std::size_t start = position_;
        while (position_ < input_.size() && input_[position_] >= '0' && input_[position_] <= '9')
            ++position_;
        return position_ != start;
    }

    bool parse_literal(std::string_view literal, JsonValue::Type type, JsonValue &value) {
        if (input_.substr(position_, literal.size()) != literal)
            return false;
        position_ += literal.size();
        value.type = type;
        return true;
    }

    bool consume(char expected) {
        if (position_ >= input_.size() || input_[position_] != expected)
            return false;
        ++position_;
        return true;
    }

    std::string_view input_;
    std::size_t position_ = 0;
};

std::optional<ParsedUrl> parse_url(std::string_view value) {
    static const std::regex expression(R"(^([A-Za-z][A-Za-z0-9+.-]*)://([^/?#]+)([^?#]*).*$)");
    const std::string url(value);
    std::smatch match;
    if (!std::regex_match(url, match, expression))
        return std::nullopt;
    std::string authority = match[2].str();
    if (authority.find('@') != std::string::npos || authority.find(':') != std::string::npos)
        return std::nullopt;
    std::string host = std::move(authority);
    std::transform(host.begin(), host.end(), host.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return ParsedUrl{ match[1].str(), std::move(host), match[3].str() };
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

std::string field(const JsonValue &tree, std::string_view key) {
    const JsonValue *value = tree.member(key);
    return value && value->type == JsonValue::Type::String ? value->text : std::string();
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

    std::string_view remaining_path(parsed->path);
    remaining_path.remove_prefix(release_download_prefix.size());
    while (!remaining_path.empty()) {
        const std::size_t separator = remaining_path.find('/');
        const std::string_view segment = remaining_path.substr(0, separator);
        if (segment == "." || segment == "..")
            return false;
        if (separator == std::string_view::npos)
            break;
        remaining_path.remove_prefix(separator + 1);
    }
    return true;
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
    const auto parsed_root = JsonParser(response).parse();
    if (!parsed_root || parsed_root->type != JsonValue::Type::Object) {
        result.failure_reason = UpdateCheckFailureReason::InvalidReleaseResponse;
        return result;
    }
    const JsonValue &root = *parsed_root;

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

    result.info.version = field(root, "name");
    if (result.info.version.empty())
        result.info.version = field(root, "tag_name");
    result.info.release_url = field(root, "html_url");
    if (!is_trusted_release_page(result.info.release_url))
        result.info.release_url = release_page_url();
    result.info.published_at = field(root, "published_at");
    result.info.notes = clean_notes(body);

    if (const JsonValue *changelog = root.member("changelog"); changelog && changelog->type == JsonValue::Type::Array) {
        for (const JsonValue &item : changelog->elements) {
            if (item.type != JsonValue::Type::Object)
                continue;
            ChangelogEntry entry{ field(item, "version"), field(item, "title") };
            if (!entry.version.empty() || !entry.title.empty())
                result.info.changelog.push_back(std::move(entry));
        }
    }
    if (const JsonValue *assets = root.member("assets"); assets && assets->type == JsonValue::Type::Array) {
        for (const JsonValue &item : assets->elements) {
            if (item.type != JsonValue::Type::Object)
                continue;
            UpdateAsset asset;
            asset.name = field(item, "name");
            asset.download_url = field(item, "browser_download_url");
            if (const JsonValue *size = item.member("size"); size && size->type == JsonValue::Type::Number) {
                const auto [end, error] = std::from_chars(size->text.data(), size->text.data() + size->text.size(), asset.size);
                if (error != std::errc() || end != size->text.data() + size->text.size())
                    asset.size = 0;
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
