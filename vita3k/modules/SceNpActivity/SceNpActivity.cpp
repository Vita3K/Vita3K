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

#include <module/module.h>

#include <io/state.h>
#include <np/state.h>
#include <v3kn/account.h>
#include <v3kn/activity.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string_view>

enum class MessageValidation { valid,
    empty,
    invalid };

static MessageValidation validate_message(std::string_view text) {
    if (text.empty())
        return MessageValidation::empty;

    size_t count = 0;
    size_t linefeeds = 0;
    for (size_t i = 0; i < text.size();) {
        const auto first = static_cast<unsigned char>(text[i]);
        size_t length = 0;
        if (first < 0x80) {
            length = 1;
            if (first == '\n' && ++linefeeds > 8)
                return MessageValidation::invalid;
        } else if (first >= 0xc2 && first <= 0xdf) {
            length = 2;
        } else if (first >= 0xe0 && first <= 0xef) {
            length = 3;
        } else if (first >= 0xf0 && first <= 0xf4) {
            length = 4;
        } else {
            return MessageValidation::invalid;
        }

        if (text.size() - i < length)
            return MessageValidation::invalid;
        for (size_t j = 1; j < length; ++j) {
            const auto next = static_cast<unsigned char>(text[i + j]);
            if (next < 0x80 || next > 0xbf)
                return MessageValidation::invalid;
            if (j == 1 && ((first == 0xe0 && next < 0xa0) || (first == 0xed && next > 0x9f) || (first == 0xf0 && next < 0x90) || (first == 0xf4 && next > 0x8f)))
                return MessageValidation::invalid;
        }
        if (++count > 256)
            return MessageValidation::invalid;
        i += length;
    }
    return MessageValidation::valid;
}

static bool validate_uri(std::string_view uri) {
    return std::all_of(uri.begin(), uri.end(), [](unsigned char c) { return c <= 0x7f; })
        && (uri.starts_with("http://") || uri.starts_with("https://") || uri.starts_with("psts:") || uri.starts_with("psgm:"));
}

static bool validate_app_version(uint32_t version) {
    return version == 0 || (version >= 100 && version <= 9999);
}

static bool validate_escaped_param(std::string_view param) {
    const auto is_hex = [](unsigned char c) {
        return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
    };
    for (size_t i = 0; i < param.size(); ++i) {
        const auto c = static_cast<unsigned char>(param[i]);
        if (c <= 0x20 || c >= 0x7f)
            return false;
        if (c == '%') {
            if (param.size() - i < 3 || !is_hex(param[i + 1]) || !is_hex(param[i + 2]))
                return false;
            i += 2;
        }
    }
    return true;
}

enum SceNpActivityError : uint32_t {
    SCE_NP_ACTIVITY_ERROR_ALREADY_INITIALIZED = 0x80552301,
    SCE_NP_ACTIVITY_ERROR_NOT_INITIALIZED = 0x80552302,
    SCE_NP_ACTIVITY_ERROR_INVALID_ARGUMENT = 0x80552303,
    SCE_NP_ACTIVITY_ERROR_INVALID_URI_SCHEME = 0x80552304,
    SCE_NP_ACTIVITY_ERROR_INVALID_STATUS_MESSAGE = 0x80552305,
    SCE_NP_ACTIVITY_ERROR_SIGNOUT = 0x80552306,
    SCE_NP_ACTIVITY_ERROR_INVALID_APP_VER = 0x80552308,
    SCE_NP_ACTIVITY_ERROR_INVALID_APP_PARAM = 0x80552309,
};

struct SceNpActivityStatusMessage {
    char data[1025];
};

struct SceNpActivityStatusActionUri {
    char data[1025];
};

struct SceNpActivityStatusAppStartupParam {
    uint32_t app_version;
    char escaped_param[897];
    uint8_t reserved[3];
};

static MessageValidation get_message(const SceNpActivityStatusMessage &message, std::string_view &text) {
    const auto end = std::find(std::begin(message.data), std::end(message.data), '\0');
    if (end == std::end(message.data))
        return MessageValidation::invalid;
    text = std::string_view(message.data, static_cast<size_t>(end - message.data));
    return validate_message(text);
}

EXPORT(int, sceNpActivityInit, void *option) {
    if (option)
        return RET_ERROR(SCE_NP_ACTIVITY_ERROR_INVALID_ARGUMENT);
    if (emuenv.np.activity_inited.exchange(true))
        return RET_ERROR(SCE_NP_ACTIVITY_ERROR_ALREADY_INITIALIZED);
    return 0;
}

EXPORT(int, sceNpActivityPostAppStartupStatus, const SceNpActivityStatusMessage *message, const SceNpActivityStatusAppStartupParam *param, void *option) {
    LOG_DEBUG("sceNpActivityPostAppStartupStatus: message_present={}, param_present={}, option_present={}", message != nullptr, param != nullptr, option != nullptr);
    if (!emuenv.np.activity_inited.load())
        return RET_ERROR(SCE_NP_ACTIVITY_ERROR_NOT_INITIALIZED);
    if (!message || !param || option)
        return RET_ERROR(SCE_NP_ACTIVITY_ERROR_INVALID_ARGUMENT);

    std::string_view text;
    const auto validation = get_message(*message, text);
    if (validation == MessageValidation::empty)
        return RET_ERROR(SCE_NP_ACTIVITY_ERROR_INVALID_ARGUMENT);
    if (validation != MessageValidation::valid)
        return RET_ERROR(SCE_NP_ACTIVITY_ERROR_INVALID_STATUS_MESSAGE);
    LOG_DEBUG("sceNpActivityPostAppStartupStatus: message='{}', app_version={}", text, param->app_version);
    if (!validate_app_version(param->app_version))
        return RET_ERROR(SCE_NP_ACTIVITY_ERROR_INVALID_APP_VER);

    const auto end = std::find(std::begin(param->escaped_param), std::end(param->escaped_param), '\0');
    const std::string_view escaped_param(param->escaped_param, static_cast<size_t>(end - param->escaped_param));
    LOG_DEBUG("sceNpActivityPostAppStartupStatus: escaped_param='{}', terminated={}", escaped_param, end != std::end(param->escaped_param));
    if (end == std::end(param->escaped_param) || !validate_escaped_param(escaped_param))
        return RET_ERROR(SCE_NP_ACTIVITY_ERROR_INVALID_APP_PARAM);
    if (v3kn::get_v3kn_user_info(emuenv).online_id.empty())
        return RET_ERROR(SCE_NP_ACTIVITY_ERROR_SIGNOUT);

    return UNIMPLEMENTED();
}

EXPORT(int, sceNpActivityPostStatus, const SceNpActivityStatusMessage *message, const SceNpActivityStatusActionUri *action_uri, void *option) {
    if (!emuenv.np.activity_inited.load())
        return RET_ERROR(SCE_NP_ACTIVITY_ERROR_NOT_INITIALIZED);
    if (!message || option)
        return RET_ERROR(SCE_NP_ACTIVITY_ERROR_INVALID_ARGUMENT);

    std::string_view text;
    const auto validation = get_message(*message, text);
    if (validation == MessageValidation::empty)
        return RET_ERROR(SCE_NP_ACTIVITY_ERROR_INVALID_ARGUMENT);
    if (validation != MessageValidation::valid)
        return RET_ERROR(SCE_NP_ACTIVITY_ERROR_INVALID_STATUS_MESSAGE);

    std::string_view uri;
    if (action_uri) {
        const auto uri_end = std::find(std::begin(action_uri->data), std::end(action_uri->data), '\0');
        if (uri_end == std::end(action_uri->data))
            return RET_ERROR(SCE_NP_ACTIVITY_ERROR_INVALID_URI_SCHEME);
        uri = std::string_view(action_uri->data, static_cast<size_t>(uri_end - action_uri->data));
        if (!validate_uri(uri))
            return RET_ERROR(SCE_NP_ACTIVITY_ERROR_INVALID_URI_SCHEME);
    }

    if (v3kn::get_v3kn_user_info(emuenv).online_id.empty())
        return RET_ERROR(SCE_NP_ACTIVITY_ERROR_SIGNOUT);

    ActivityPostStatus status{
        .type = "game",
        .title_id = emuenv.io.title_id,
        .message = std::string(text),
        .game_activity = { .content_id = emuenv.io.content_id, .action_uri = std::string(uri) },
    };
    v3kn::create_and_post_activity_status(emuenv, status);
    return 0;
}

EXPORT(int, sceNpActivityTerm) {
    if (!emuenv.np.activity_inited.exchange(false))
        return RET_ERROR(SCE_NP_ACTIVITY_ERROR_NOT_INITIALIZED);
    return 0;
}
