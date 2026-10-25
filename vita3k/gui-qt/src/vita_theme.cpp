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

#include <gui-qt/qt_utils.h>
#include <gui-qt/vita_theme.h>

#include <util/log.h>
#include <util/vita_theme_utils.h>
#include <vita_shell/functions.h>

#include <QFile>

#include <algorithm>
#include <cmath>
#include <optional>
#include <unordered_map>

namespace gui {
namespace {

std::string path_leaf_string(const fs::path &path) {
    return fs_utils::path_to_utf8(path.filename());
}

double luminance(const VitaThemeColor &color) {
    return (0.2126 * color.red + 0.7152 * color.green + 0.0722 * color.blue) / 255.0;
}

double color_distance(const VitaThemeColor &lhs, const VitaThemeColor &rhs) {
    const auto component_delta = [](const std::uint8_t first, const std::uint8_t second) {
        return static_cast<double>(static_cast<int>(first) - static_cast<int>(second)) / 255.0;
    };

    const double red = component_delta(lhs.red, rhs.red);
    const double green = component_delta(lhs.green, rhs.green);
    const double blue = component_delta(lhs.blue, rhs.blue);
    return std::sqrt((red * red + green * green + blue * blue) / 3.0);
}

VitaThemeColor mix(const VitaThemeColor &lhs, const VitaThemeColor &rhs, const double ratio) {
    const double clamped = std::clamp(ratio, 0.0, 1.0);
    const auto blend = [clamped](const std::uint8_t first, const std::uint8_t second) {
        return static_cast<std::uint8_t>(std::lround((first * (1.0 - clamped)) + (second * clamped)));
    };

    return VitaThemeColor{
        blend(lhs.red, rhs.red),
        blend(lhs.green, rhs.green),
        blend(lhs.blue, rhs.blue),
        blend(lhs.alpha, rhs.alpha),
    };
}

VitaThemeColor darken(const VitaThemeColor &color, const double ratio) {
    return mix(color, VitaThemeColor{}, ratio);
}

VitaThemeColor lighten(const VitaThemeColor &color, const double ratio) {
    return mix(color, VitaThemeColor{ 255, 255, 255, 255 }, ratio);
}

VitaThemeColor contrasting_text(const VitaThemeColor &background) {
    return luminance(background) > 0.55
        ? VitaThemeColor{ 10, 18, 24, 255 }
        : VitaThemeColor{ 245, 247, 250, 255 };
}

VitaThemeColor ensure_readable(const VitaThemeColor &foreground, const VitaThemeColor &background) {
    return std::abs(luminance(foreground) - luminance(background)) < 0.28
        ? contrasting_text(background)
        : foreground;
}

std::string css_hex(const VitaThemeColor &color) {
    return fmt::format("#{:02X}{:02X}{:02X}", color.red, color.green, color.blue);
}

std::string css_argb(const VitaThemeColor &color, const std::uint8_t alpha) {
    return fmt::format("#{:02X}{:02X}{:02X}{:02X}", alpha, color.red, color.green, color.blue);
}

std::string css_rgba(const VitaThemeColor &color, const std::uint8_t alpha) {
    return fmt::format("rgba({}, {}, {}, {:.3f})",
        color.red,
        color.green,
        color.blue,
        static_cast<double>(alpha) / 255.0);
}

std::uint8_t readability_alpha(const int readability_percent, const std::uint8_t minimum, const std::uint8_t maximum) {
    const double amount = std::clamp(static_cast<double>(readability_percent) / 100.0, 0.0, 1.0);
    return static_cast<std::uint8_t>(std::lround((minimum * (1.0 - amount)) + (maximum * amount)));
}

std::string stylesheet_path_url(const fs::path &path, const fs::path &vita_fs_path) {
    fs::path normalized_path = path.lexically_normal();
    if (!vita_fs_path.empty()) {
        const fs::path relative = normalized_path.lexically_relative(vita_fs_path);
        if (!relative.empty() && relative != normalized_path)
            normalized_path = relative;
    }

    std::string normalized = fs_utils::path_to_utf8(normalized_path);
    std::ranges::replace(normalized, '\\', '/');
    return normalized;
}

std::string join_stylesheet_path_urls(const std::vector<fs::path> &paths, const fs::path &vita_fs_path) {
    std::string joined;
    bool first = true;
    for (const fs::path &path : paths) {
        if (path.empty())
            continue;

        if (!first)
            joined += ", ";
        joined += stylesheet_path_url(path, vita_fs_path);
        first = false;
    }
    return joined;
}

std::optional<std::string> load_vita_theme_stylesheet_template() {
    static const std::optional<std::string> stylesheet_template = []() -> std::optional<std::string> {
        QFile file(QStringLiteral(":/themes/vita/generated.qss.in"));
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            LOG_ERROR("Failed to load Vita theme stylesheet template from resources");
            return std::nullopt;
        }

        return std::string(file.readAll().constData(), static_cast<std::size_t>(file.size()));
    }();

    return stylesheet_template;
}

struct ThemeTemplateContext {
    std::unordered_map<std::string, std::string> strings;
};

std::optional<std::string> render_template_token(
    const ThemeTemplateContext &context,
    const std::string &token) {
    const auto it = context.strings.find(token);
    return (it != context.strings.end()) ? std::optional<std::string>(it->second) : std::nullopt;
}

std::optional<std::string> render_stylesheet_template(
    const ThemeTemplateContext &context) {
    auto stylesheet_template = load_vita_theme_stylesheet_template();
    if (!stylesheet_template)
        return std::nullopt;

    std::string rendered;
    rendered.reserve(stylesheet_template->size());

    std::size_t cursor = 0;
    while (cursor < stylesheet_template->size()) {
        const std::size_t start = stylesheet_template->find("{{", cursor);
        if (start == std::string::npos) {
            rendered.append(*stylesheet_template, cursor, std::string::npos);
            break;
        }

        rendered.append(*stylesheet_template, cursor, start - cursor);
        const std::size_t finish = stylesheet_template->find("}}", start + 2);
        if (finish == std::string::npos) {
            rendered.append(*stylesheet_template, start, std::string::npos);
            break;
        }

        const std::string token = stylesheet_template->substr(start + 2, finish - (start + 2));
        const auto value = render_template_token(context, token);
        if (!value) {
            LOG_ERROR("Unknown Vita theme stylesheet template token '{}'", token);
            return std::nullopt;
        }

        rendered.append(*value);
        cursor = finish + 2;
    }

    return rendered;
}

std::optional<VitaThemeColor> to_vita_theme_color(const std::optional<vita_shell::Color> &color) {
    if (!color)
        return std::nullopt;
    return VitaThemeColor{ color->r, color->g, color->b, color->a };
}

std::optional<VitaThemeInfo> to_vita_theme_info(vita_shell::ThemeState state) {
    if (!state.loaded)
        return std::nullopt;

    VitaThemeInfo theme;
    theme.content_id = std::move(state.content_id);
    theme.title = std::move(state.name);
    theme.provider = std::move(state.provider);
    theme.content_version = std::move(state.content_version);
    theme.theme_id = vita_theme_utils::resolve_theme_id(theme.content_id, path_leaf_string(state.installed_path), theme.title);
    if (theme.title.empty())
        theme.title = theme.theme_id;
    theme.installed_path = std::move(state.installed_path);
    if (!state.bgm.path.empty())
        theme.bgm_path = std::move(state.bgm.path);
    theme.package_thumbnail_path = std::move(state.package_thumbnail_path);
    theme.installed_size = state.installed_size;
    theme.updated_time = state.updated_time;
    theme.palette.information_bar_color = to_vita_theme_color(state.information_bar.bar_color);
    theme.palette.information_indicator_color = to_vita_theme_color(state.information_bar.indicator_color);
    theme.palette.notify_background_color = to_vita_theme_color(state.start_notice_background_color);
    theme.palette.notify_border_color = to_vita_theme_color(state.start_notice_border_color);
    theme.palette.notify_font_color = to_vita_theme_color(state.start_notice_font_color);
    theme.palette.date_color = to_vita_theme_color(state.start_date_color);

    int home_index = 0;
    for (auto &background : state.backgrounds) {
        theme.background_options.push_back({
            fmt::format("home-{}", home_index),
            fmt::format("Home Background {}", home_index + 1),
            std::move(background.path),
            VitaThemeBackgroundSource::Home,
            to_vita_theme_color(background.text_color),
        });
        ++home_index;
    }
    if (!state.start_screen_background.empty() && fs::exists(state.start_screen_background)) {
        theme.background_options.push_back({
            "lockscreen",
            "Lockscreen",
            std::move(state.start_screen_background),
            VitaThemeBackgroundSource::Lockscreen,
            theme.palette.notify_font_color,
        });
    }

    if (theme.background_options.empty()) {
        LOG_WARN("Theme '{}' has no usable background assets, skipping", theme.theme_id);
        return std::nullopt;
    }
    return theme;
}

const VitaThemeBackgroundOption *find_background_option(
    const VitaThemeInfo &theme,
    const std::string &background_option_id) {
    if (theme.background_options.empty())
        return nullptr;

    if (background_option_id.empty())
        return &theme.background_options.front();

    const auto it = std::ranges::find_if(theme.background_options,
        [&background_option_id](const VitaThemeBackgroundOption &option) {
            return option.id == background_option_id;
        });
    return (it != theme.background_options.end()) ? &(*it) : nullptr;
}

std::string build_stylesheet(
    const VitaThemeInfo &theme,
    const VitaThemeBackgroundOption &background_option,
    const int readability_percent,
    const bool normalize_font_colors,
    const std::vector<fs::path> &background_image_paths,
    const int cycle_interval_seconds,
    const bool cycle_enabled,
    const fs::path &vita_fs_path) {
    const VitaThemeColor window = theme.palette.information_bar_color.value_or(VitaThemeColor{ 18, 26, 34, 255 });
    const bool dark_theme = luminance(window) < 0.5;
    const VitaThemeColor base = darken(window, 0.18);
    const VitaThemeColor alternate_base = lighten(base, 0.08);
    const VitaThemeColor accent_hint = dark_theme
        ? VitaThemeColor{ 140, 212, 255, 255 }
        : VitaThemeColor{ 20, 138, 255, 255 };
    VitaThemeColor accent = theme.palette.information_indicator_color.value_or(accent_hint);
    const VitaThemeColor normalized_text = contrasting_text(window);
    const VitaThemeColor neutral_border = mix(normalized_text, window, dark_theme ? 0.34 : 0.56);
    VitaThemeColor border = theme.palette.notify_border_color
        ? mix(*theme.palette.notify_border_color, neutral_border, dark_theme ? 0.42 : 0.58)
        : neutral_border;
    const VitaThemeColor theme_text = normalize_font_colors
        ? normalized_text
        : ensure_readable(
              background_option.primary_text_color.value_or(
                  theme.palette.notify_font_color.value_or(normalized_text)),
              window);
    const VitaThemeColor dialog_text = dark_theme ? VitaThemeColor{ 224, 224, 224, 255 } : VitaThemeColor{ 32, 32, 32, 255 };
    const VitaThemeColor dialog_disabled_text = dark_theme ? VitaThemeColor{ 119, 119, 119, 255 } : VitaThemeColor{ 138, 138, 138, 255 };
    const VitaThemeColor dialog_muted_text = dark_theme ? VitaThemeColor{ 168, 168, 168, 255 } : VitaThemeColor{ 91, 91, 91, 255 };
    const VitaThemeColor overlay_text = (normalize_font_colors && !dark_theme)
        ? VitaThemeColor{ 245, 247, 250, 255 }
        : theme_text;
    const VitaThemeColor text = overlay_text;
    VitaThemeColor muted_text = ensure_readable(
        normalize_font_colors
            ? mix(dialog_text, window, dark_theme ? 0.32 : 0.48)
            : theme.palette.date_color.value_or(mix(theme_text, window, dark_theme ? 0.42 : 0.52)),
        window);
    const VitaThemeColor overlay_muted_text = normalize_font_colors
        ? (dark_theme ? VitaThemeColor{ 168, 168, 168, 255 } : VitaThemeColor{ 208, 220, 230, 255 })
        : muted_text;
    if ((std::abs(luminance(accent) - luminance(window)) < 0.16)
        && (std::abs(luminance(accent) - luminance(theme_text)) < 0.10)) {
        accent = accent_hint;
    }
    accent = ensure_readable(accent, window);

    const VitaThemeColor button = theme.palette.notify_background_color.value_or(mix(window, accent, 0.10));
    const VitaThemeColor button_text = normalize_font_colors
        ? ensure_readable(dialog_text, button)
        : ensure_readable(
              theme.palette.notify_font_color.value_or(text),
              button);
    const VitaThemeColor highlight_text = contrasting_text(accent);
    const VitaThemeColor checkbox_fill = mix(base, window, dark_theme ? 0.22 : 0.14);
    const VitaThemeColor disabled_text = mix(muted_text, window, 0.32);
    const VitaThemeColor overlay_disabled_text = normalize_font_colors
        ? (dark_theme ? VitaThemeColor{ 119, 119, 119, 255 } : VitaThemeColor{ 166, 180, 194, 255 })
        : disabled_text;
    const VitaThemeColor line_edit_fill = mix(base, window, 0.35);
    const VitaThemeColor subtle_surface = mix(base, window, 0.40);
    const VitaThemeColor overlay_selection_fill = dark_theme
        ? VitaThemeColor{ 255, 255, 255, 255 }
        : VitaThemeColor{ 10, 18, 24, 255 };
    const VitaThemeColor overlay_selection_text = VitaThemeColor{ 255, 255, 255, 255 };
    const VitaThemeColor hover_surface = overlay_selection_fill;
    const VitaThemeColor selected_text = overlay_selection_text;
    const VitaThemeColor toolbar_icon_tint = normalize_font_colors
        ? overlay_text
        : (dark_theme ? text : mix(text, window, 0.48));
    const VitaThemeColor dialog_button_fill = dark_theme ? VitaThemeColor{ 48, 48, 48, 255 } : VitaThemeColor{ 236, 236, 236, 255 };
    const VitaThemeColor dialog_bind_fill = dark_theme ? VitaThemeColor{ 47, 47, 47, 255 } : VitaThemeColor{ 230, 230, 230, 255 };
    const VitaThemeColor dialog_sidebar_fill = dark_theme ? VitaThemeColor{ 42, 42, 42, 255 } : VitaThemeColor{ 255, 255, 255, 255 };
    const VitaThemeColor dialog_help_fill = dark_theme ? VitaThemeColor{ 42, 42, 42, 255 } : VitaThemeColor{ 255, 255, 255, 255 };
    const VitaThemeColor dialog_hover_fill = dark_theme ? VitaThemeColor{ 58, 58, 58, 255 } : VitaThemeColor{ 242, 242, 242, 255 };
    const VitaThemeColor dialog_selected_fill = dark_theme ? VitaThemeColor{ 255, 255, 255, 255 } : VitaThemeColor{ 20, 138, 255, 255 };
    const VitaThemeColor dialog_selected_text = VitaThemeColor{ 255, 255, 255, 255 };
    const VitaThemeColor dialog_mode_checked_text = dark_theme ? dialog_text : dialog_selected_text;
    const VitaThemeColor renderer_vulkan = VitaThemeColor{ 255, 68, 68, 255 };
    const VitaThemeColor renderer_opengl = VitaThemeColor{ 68, 136, 255, 255 };
    const VitaThemeColor status_high = VitaThemeColor{ 255, 136, 0, 255 };
    const VitaThemeColor status_standard = text;
    const VitaThemeColor status_neutral = text;
    const VitaThemeColor status_audio = dark_theme ? VitaThemeColor{ 136, 221, 68, 255 } : VitaThemeColor{ 75, 155, 47, 255 };
    const VitaThemeColor status_muted = text;
    const VitaThemeColor status_update = dark_theme ? VitaThemeColor{ 109, 224, 137, 255 } : VitaThemeColor{ 31, 143, 78, 255 };
    const VitaThemeColor log_fatal = ensure_readable(VitaThemeColor{ 255, 159, 243, 255 }, window);
    const VitaThemeColor log_error = ensure_readable(VitaThemeColor{ 255, 154, 127, 255 }, window);
    const VitaThemeColor log_warning = ensure_readable(VitaThemeColor{ 255, 211, 127, 255 }, window);
    const VitaThemeColor log_debug = ensure_readable(VitaThemeColor{ 140, 212, 255, 255 }, window);

    if (std::abs(luminance(muted_text) - luminance(text)) < 0.08)
        muted_text = ensure_readable(mix(text, window, dark_theme ? 0.32 : 0.48), window);

    if ((std::abs(luminance(border) - luminance(window)) < 0.11)
        || (color_distance(border, window) < 0.22)) {
        border = neutral_border;
    }

    const std::uint8_t dialog_alpha = readability_alpha(readability_percent, 188, 252);
    const std::uint8_t chrome_alpha = readability_alpha(readability_percent, 58, 196);
    const std::uint8_t chrome_border_alpha = readability_alpha(readability_percent, 18, 64);
    const std::uint8_t control_alpha = readability_alpha(readability_percent, 26, 176);
    const std::uint8_t input_alpha = readability_alpha(readability_percent, 32, 188);
    const std::uint8_t sidebar_alpha = readability_alpha(readability_percent, 28, 210);
    const std::uint8_t surface_alpha = readability_alpha(readability_percent, 10, 156);
    const std::uint8_t pane_alpha = readability_alpha(readability_percent, 12, 190);
    const std::uint8_t search_alpha = readability_alpha(readability_percent, 20, 210);
    const std::uint8_t progress_alpha = readability_alpha(readability_percent, 28, 140);
    const std::uint8_t scroll_alpha = readability_alpha(readability_percent, 18, 110);
    const std::uint8_t hover_alpha = readability_alpha(readability_percent, 44, 96);
    const std::uint8_t overlay_selected_alpha = dark_theme
        ? readability_alpha(readability_percent, 28, 82)
        : readability_alpha(readability_percent, 42, 132);
    const std::uint8_t overlay_selected_soft_alpha = dark_theme
        ? readability_alpha(readability_percent, 18, 54)
        : readability_alpha(readability_percent, 28, 94);
    const std::uint8_t overlay_hover_alpha = dark_theme
        ? readability_alpha(readability_percent, 16, 42)
        : readability_alpha(readability_percent, 12, 34);
    const std::uint8_t dialog_selected_alpha = dark_theme
        ? readability_alpha(readability_percent, 28, 78)
        : readability_alpha(readability_percent, 96, 220);

    const std::string icon_suffix = dark_theme ? "light" : "dark";
    const std::string check_icon_suffix = luminance(highlight_text) > 0.5 ? "light" : "dark";
    const std::string background_url = stylesheet_path_url(background_option.asset_path, vita_fs_path);
    ThemeTemplateContext context;
    const auto put = [&context](const std::string &key, std::string value) {
        context.strings[key] = std::move(value);
    };
    const auto put_hex = [&put](const std::string &key, const VitaThemeColor &color) {
        put(key, css_hex(color));
    };
    const auto put_rgba = [&put](const std::string &key, const VitaThemeColor &color, const int alpha) {
        put(key, css_rgba(color, static_cast<std::uint8_t>(std::clamp(alpha, 0, 255))));
    };
    const auto put_argb = [&put](const std::string &key, const VitaThemeColor &color, const int alpha) {
        put(key, css_argb(color, static_cast<std::uint8_t>(std::clamp(alpha, 0, 255))));
    };

    put("generated_background_images", join_stylesheet_path_urls(background_image_paths, vita_fs_path));
    put("generated_background_interval_seconds", std::to_string(std::clamp(cycle_interval_seconds, 5, 120)));
    put("generated_background_animated", cycle_enabled ? "true" : "false");

    put("palette_dark", dark_theme ? "dark" : "light");
    put_hex("window_hex", window);
    put_hex("text_hex", text);
    put_hex("base_hex", base);
    put_hex("alternate_base_hex", alternate_base);
    put_hex("button_hex", button);
    put_hex("button_text_hex", button_text);
    put_hex("button_bright_text_hex", contrasting_text(button));
    put_hex("accent_hex", accent);
    put_argb("highlight_argb", overlay_selection_fill, overlay_selected_alpha);
    put_argb("inactive_highlight_argb", overlay_selection_fill, overlay_selected_soft_alpha);
    put_argb("disabled_highlight_argb", overlay_selection_fill, overlay_hover_alpha);
    put_hex("highlight_text_hex", overlay_selection_text);
    put_hex("disabled_text_hex", disabled_text);
    put_hex("muted_text_hex", muted_text);
    put_hex("overlay_disabled_text_hex", overlay_disabled_text);
    put_hex("dialog_text_hex", dialog_text);
    put_hex("dialog_muted_text_hex", dialog_muted_text);
    put_hex("dialog_disabled_text_hex", dialog_disabled_text);
    put_hex("selected_text_hex", selected_text);
    put_hex("push_button_pressed_text_hex", highlight_text);
    put_hex("dialog_selected_text_hex", dialog_selected_text);
    put_hex("dialog_mode_checked_text_hex", dialog_mode_checked_text);
    put_hex("toolbar_icon_tint_hex", toolbar_icon_tint);
    put_hex("renderer_vulkan_hex", renderer_vulkan);
    put_hex("renderer_opengl_hex", renderer_opengl);
    put_hex("status_high_hex", status_high);
    put_hex("status_standard_hex", status_standard);
    put_hex("status_neutral_hex", status_neutral);
    put_hex("status_audio_hex", status_audio);
    put_hex("status_muted_hex", status_muted);
    put_hex("status_update_hex", status_update);
    put_hex("log_fatal_hex", log_fatal);
    put_hex("log_error_hex", log_error);
    put_hex("log_warning_hex", log_warning);
    put_hex("log_debug_hex", log_debug);
    put_hex("severity_success_text_hex", contrasting_text(status_audio));
    put_hex("severity_error_text_hex", contrasting_text(log_error));

    put("background_url", background_url);
    put("icon_suffix", icon_suffix);
    put("check_icon_suffix", check_icon_suffix);

    put_rgba("dialog_bg", base, dialog_alpha);
    put_rgba("menu_bar_bg", base, chrome_alpha);
    put_rgba("menu_bar_border", border, chrome_border_alpha);
    put_rgba("menu_bar_item_hover_bg", hover_surface, hover_alpha);
    put_rgba("menu_bg", base, 245);
    put_rgba("menu_border", border, 40);
    put_rgba("menu_selected_bg", overlay_selection_fill, overlay_selected_soft_alpha);
    put_rgba("menu_separator_bg", border, 30);
    put_rgba("toolbar_bg", base, chrome_alpha);
    put_rgba("toolbar_border_top", border, chrome_border_alpha + 8);
    put_rgba("toolbar_border_bottom", border, chrome_border_alpha);
    put_rgba("toolbar_separator_bg", border, 40);
    put_rgba("tool_button_hover_bg", hover_surface, hover_alpha);
    put_rgba("tool_button_hover_border", border, chrome_border_alpha + 6);
    put_rgba("tool_button_pressed_bg", accent, 54);
    put_rgba("welcome_button_bg", button, 88);
    put_rgba("welcome_button_border", border, 36);
    put_rgba("welcome_button_hover_bg", hover_surface, 88);
    put_rgba("welcome_button_hover_border", border, 48);
    put_rgba("welcome_button_pressed_bg", accent, 56);
    put_rgba("welcome_menu_button_border", border, 36);
    put_rgba("welcome_menu_button_bg", base, 28);
    put_rgba("welcome_menu_button_hover_bg", hover_surface, 74);
    put_rgba("welcome_menu_button_hover_border", border, 48);
    put_rgba("welcome_menu_button_pressed_bg", accent, 48);
    put_rgba("push_button_bg", button, control_alpha);
    put_rgba("push_button_border", border, 36);
    put_rgba("push_button_hover_bg", hover_surface, hover_alpha + 12);
    put_rgba("push_button_hover_border", border, chrome_border_alpha + 18);
    put_rgba("push_button_pressed_bg", accent, 62);
    put_rgba("push_button_disabled_bg", base, 72);
    put_rgba("push_button_disabled_border", border, 20);
    put_rgba("push_button_default_border", accent, 110);
    put_rgba("vita_list_bg", dialog_sidebar_fill, sidebar_alpha);
    put_rgba("vita_list_selected_bg", dialog_selected_fill, dialog_selected_alpha);
    put_rgba("vita_list_hover_bg", dialog_hover_fill, dark_theme ? hover_alpha + 8 : hover_alpha);
    put_rgba("background_list_bg", line_edit_fill, pane_alpha);
    put_rgba("background_list_border", border, 26);
    put_rgba("background_list_selected_bg", dialog_selected_fill, dialog_selected_alpha);
    put_rgba("background_list_hover_bg", dialog_hover_fill, dark_theme ? hover_alpha + 6 : hover_alpha + 4);
    put_rgba("vita_help_bg", dialog_help_fill, pane_alpha);
    put_rgba("controls_mode_hover_bg", dialog_hover_fill, dark_theme ? hover_alpha : hover_alpha + 10);
    put_rgba("controls_mode_checked_bg", dialog_selected_fill, dialog_selected_alpha);
    put_rgba("controls_mode_pressed_bg", dialog_selected_fill, dark_theme ? dialog_selected_alpha + 16 : std::min(255, dialog_selected_alpha + 24));
    put_rgba("controls_button_bg", dialog_button_fill, control_alpha);
    put_rgba("controls_button_hover_bg", dialog_hover_fill, dark_theme ? hover_alpha + 10 : hover_alpha + 6);
    put_rgba("controls_button_pressed_bg", dialog_hover_fill, dark_theme ? hover_alpha + 20 : hover_alpha + 18);
    put_rgba("controls_button_disabled_bg", dialog_button_fill, std::max(40, control_alpha - 28));
    put_rgba("controls_bind_bg", dialog_bind_fill, input_alpha);
    put_rgba("controls_bind_hover_bg", dialog_hover_fill, dark_theme ? hover_alpha + 6 : hover_alpha + 4);
    put_rgba("controls_bind_pressed_bg", dialog_hover_fill, dark_theme ? hover_alpha + 16 : hover_alpha + 14);
    put_rgba("controls_capture_bg", dialog_selected_fill, dark_theme ? dialog_selected_alpha + 18 : dialog_selected_alpha);
    put_rgba("controls_capture_hover_bg", dialog_selected_fill, dark_theme ? dialog_selected_alpha + 28 : std::min(255, dialog_selected_alpha + 18));
    put_rgba("controls_capture_pressed_bg", dialog_selected_fill, dark_theme ? dialog_selected_alpha + 38 : std::min(255, dialog_selected_alpha + 28));
    put_rgba("status_bar_bg", base, chrome_alpha + 18);
    put_rgba("status_bar_border", border, 26);
    put_rgba("status_button_hover_border", border, 32);
    put_rgba("status_button_hover_bg", hover_surface, overlay_hover_alpha);
    put_rgba("status_button_pressed_border", border, 40);
    put_rgba("status_button_pressed_bg", overlay_selection_fill, overlay_selected_soft_alpha);
    put_rgba("line_edit_bg", line_edit_fill, input_alpha);
    put_rgba("line_edit_border", border, 36);
    put_rgba("line_edit_selection_bg", overlay_selection_fill, overlay_selected_alpha);
    put_rgba("line_edit_focus_border", accent, 96);
    put_rgba("line_edit_disabled_bg", base, 76);
    put_rgba("line_edit_disabled_border", border, 20);
    put_rgba("main_search_border", border, 36);
    put_rgba("main_search_selection_bg", overlay_selection_fill, overlay_selected_soft_alpha);
    put_rgba("main_search_focus_bg", base, 18);
    put_rgba("main_search_focus_border", accent, 72);
    put_rgba("combo_bg", line_edit_fill, input_alpha);
    put_rgba("combo_border", border, 36);
    put_rgba("combo_hover_border", border, 54);
    put_rgba("combo_disabled_bg", base, 76);
    put_rgba("combo_disabled_border", border, 20);
    put_rgba("combo_popup_bg", base, 244);
    put_rgba("combo_popup_border", border, 40);
    put_rgba("combo_popup_selection_bg", overlay_selection_fill, overlay_selected_soft_alpha);
    put_rgba("spin_bg", line_edit_fill, input_alpha);
    put_rgba("spin_border", border, 36);
    put_rgba("spin_selection_bg", overlay_selection_fill, overlay_selected_alpha);
    put_rgba("spin_disabled_bg", base, 76);
    put_rgba("spin_disabled_border", border, 20);
    put_rgba("indicator_border", border, 92);
    put_rgba("indicator_bg", checkbox_fill, std::max(62, static_cast<int>(input_alpha) - 12));
    put_rgba("indicator_hover_border", accent, 138);
    put_rgba("indicator_hover_bg", checkbox_fill, std::max(82, static_cast<int>(input_alpha) + 6));
    put_rgba("indicator_checked_bg", accent, 196);
    put_rgba("indicator_checked_border", accent, 204);
    put_rgba("indicator_disabled_border", border, 54);
    put_rgba("indicator_disabled_bg", base, 88);
    put_rgba("indicator_checked_disabled_bg", accent, 96);
    put_rgba("indicator_checked_disabled_border", accent, 108);
    put_rgba("radio_indicator_border", border, 92);
    put_rgba("radio_indicator_bg", checkbox_fill, std::max(62, static_cast<int>(input_alpha) - 12));
    put_rgba("radio_indicator_hover_border", accent, 138);
    put_rgba("radio_indicator_hover_bg", checkbox_fill, std::max(82, static_cast<int>(input_alpha) + 6));
    put_rgba("radio_checked_bg", accent, 196);
    put_rgba("radio_checked_border", accent, 204);
    put_rgba("radio_checked_disabled_bg", accent, 96);
    put_rgba("radio_checked_disabled_border", accent, 108);
    put_rgba("group_border", border, 32);
    put_rgba("group_disabled_border", border, 18);
    put_rgba("dock_title_bg", base, chrome_alpha);
    put_rgba("dock_button_hover_bg", hover_surface, hover_alpha + 8);
    put_rgba("apps_surface_bg", base, surface_alpha);
    put_rgba("alt_row_bg", text, std::max<std::uint8_t>(8, surface_alpha / 2));
    put_rgba("table_gridline", border, 18);
    put_rgba("table_disabled_bg", base, 54);
    put_rgba("table_corner_bg", darken(base, 0.08), 80);
    put_rgba("table_corner_border", border, 24);
    put_rgba("table_selected_bg", overlay_selection_fill, overlay_selected_alpha);
    put_rgba("header_bg", darken(base, 0.08), surface_alpha);
    put_rgba("header_border", border, 18);
    put_rgba("header_hover_bg", hover_surface, 84);
    put_rgba("log_search_bg", subtle_surface, search_alpha);
    put_rgba("log_search_border", border, 30);
    put_rgba("toolbar_slider_groove_bg", text, 24);
    put_rgba("toolbar_slider_subpage_bg", text, 228);
    put_rgba("toolbar_slider_addpage_bg", text, 20);
    put_hex("toolbar_slider_handle_bg", lighten(text, dark_theme ? 0.0 : 0.12));
    put_hex("toolbar_slider_handle_hover_bg", lighten(text, 0.12));
    put_rgba("severity_success_bg", status_audio, 124);
    put_rgba("severity_error_bg", log_error, 124);
    put_rgba("tab_pane_border", border, 24);
    put_rgba("tab_pane_bg", base, pane_alpha);
    put_rgba("tab_bg", subtle_surface, sidebar_alpha);
    put_rgba("tab_border", border, 24);
    put_rgba("tab_selected_bg", base, pane_alpha);
    put_rgba("tab_hover_bg", hover_surface, hover_alpha);
    put_rgba("list_bg", base, pane_alpha);
    put_rgba("list_border", border, 22);
    put_rgba("list_disabled_bg", base, 54);
    put_rgba("list_disabled_border", border, 16);
    put_rgba("list_selected_bg", overlay_selection_fill, overlay_selected_alpha);
    put_rgba("list_hover_bg", hover_surface, hover_alpha);
    put_rgba("settings_category_bg", dialog_sidebar_fill, sidebar_alpha);
    put_rgba("settings_category_selected_bg", dialog_selected_fill, dialog_selected_alpha);
    put_rgba("settings_category_hover_bg", dialog_hover_fill, dark_theme ? hover_alpha + 8 : hover_alpha);
    put_rgba("modules_list_bg", line_edit_fill, pane_alpha);
    put_rgba("modules_list_border", border, 30);
    put_rgba("modules_list_hover_bg", dialog_hover_fill, dark_theme ? hover_alpha + 4 : hover_alpha + 8);
    put_rgba("settings_help_bg", dialog_help_fill, pane_alpha);
    put_rgba("scrollbar_bg", base, scroll_alpha);
    put_rgba("scrollbar_handle_bg", text, scroll_alpha + 22);
    put_rgba("scrollbar_handle_hover_bg", text, scroll_alpha + 40);
    put_rgba("slider_subpage_bg", text, 224);
    put_rgba("slider_addpage_bg", text, 28);
    put_hex("slider_handle_bg", lighten(text, dark_theme ? 0.0 : 0.12));
    put_hex("slider_handle_hover_bg", lighten(text, 0.12));
    put_rgba("progress_bg", base, progress_alpha);
    put_rgba("progress_border", border, 22);
    put_hex("progress_chunk_bg", accent);
    put_rgba("splitter_handle_bg", border, 34);
    put_rgba("tooltip_bg", base, 245);
    put_rgba("tooltip_border", border, 40);
    put_rgba("text_edit_bg", base, pane_alpha);
    put_rgba("text_edit_border", border, 22);
    put_rgba("text_edit_disabled_bg", base, 54);
    put_rgba("text_edit_disabled_border", border, 16);

    const auto rendered = render_stylesheet_template(context);
    return rendered.value_or(std::string());
}

} // namespace

std::optional<VitaThemeInfo> load_vita_theme(
    const fs::path &theme_dir,
    const std::string &preferred_locale) {
    return to_vita_theme_info(vita_shell::load_theme_directory(theme_dir, preferred_locale));
}

std::vector<VitaThemeInfo> enumerate_installed_vita_themes(
    const fs::path &themes_root,
    const std::string &preferred_locale) {
    std::vector<VitaThemeInfo> themes;
    for (auto &theme_state : vita_shell::enumerate_themes(themes_root, preferred_locale)) {
        if (auto theme = to_vita_theme_info(std::move(theme_state)))
            themes.push_back(std::move(*theme));
    }
    return themes;
}

std::optional<fs::path> synthesize_vita_theme_qss(
    const VitaThemeInfo &theme,
    const std::string &background_option_id,
    const int readability_percent,
    const bool normalize_font_colors,
    const std::vector<fs::path> &background_image_paths,
    const int cycle_interval_seconds,
    const bool cycle_enabled,
    const fs::path &vita_fs_path,
    const fs::path &gui_settings_dir) {
    const auto *selected_background = find_background_option(theme, background_option_id);
    if (!selected_background) {
        LOG_ERROR("Unable to find Vita theme background option '{}' for theme '{}'",
            background_option_id,
            theme.theme_id);
        return std::nullopt;
    }

    if (!fs::exists(selected_background->asset_path)) {
        LOG_ERROR("Vita theme asset is missing: {}", selected_background->asset_path);
        return std::nullopt;
    }

    const fs::path output_directory = gui_settings_dir / "vita-themes" / theme.theme_id;
    const fs::path qss_path = output_directory / "generated.qss";
    const std::string stylesheet = build_stylesheet(
        theme,
        *selected_background,
        std::clamp(readability_percent, 0, 100),
        normalize_font_colors,
        background_image_paths.empty() ? std::vector<fs::path>{ selected_background->asset_path } : background_image_paths,
        cycle_interval_seconds,
        cycle_enabled,
        vita_fs_path);

    std::error_code error_code;
    fs::create_directories(output_directory, error_code);
    if (error_code) {
        LOG_ERROR("Failed to create Vita theme output directory {}: {}",
            output_directory,
            error_code.message());
        return std::nullopt;
    }

    std::ofstream output(qss_path, std::ofstream::binary | std::ofstream::trunc);
    if (!output) {
        LOG_ERROR("Failed to open generated Vita theme stylesheet path {}", qss_path);
        return std::nullopt;
    }

    output.write(stylesheet.data(), static_cast<std::streamsize>(stylesheet.size()));
    if (!output.good()) {
        LOG_ERROR("Failed to write generated Vita theme stylesheet {}", qss_path);
        return std::nullopt;
    }

    return qss_path;
}

} // namespace gui
