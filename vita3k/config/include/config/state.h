// Vita3K emulator project
// Copyright (C) 2018 Vita3K team
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

#pragma once

#include <config/config.h>
#include <config/yaml.h>

#include <util/fs.h>
#include <util/vector_utils.h>

#include <boost/optional.hpp>
#include <boost/optional/optional_io.hpp>

using boost::optional;

// Enum based on members in Config file
// Used for easier getting of options and their names for config files
enum file_config {
#define GENERATE_ENUM(option_type, option_name, option_default, member_name) \
    e_##member_name,

    CONFIG_LIST(GENERATE_ENUM)

        _INVALID
#undef GENERATE_ENUM
};

// Configuration File options
struct Config : YamlLoader {
private:
    // Privately update members
    // Use this function when the YAML file is updated before the members.
    void update_members() {
#define UPDATE_MEMBERS(option_type, option_name, option_default, member_name) \
    member_name = get_member<option_type>(option_name);

        CONFIG_LIST(UPDATE_MEMBERS)
#undef UPDATE_MEMBERS
    }

    // Perform comparisons with optional settings
    void check_members(const Config &rhs) {
        if (rhs.vpk_path.is_initialized())
            vpk_path = rhs.vpk_path;
        if (rhs.run_title_id.is_initialized())
            run_title_id = rhs.run_title_id;
        if (rhs.recompile_shader_path.is_initialized())
            recompile_shader_path = rhs.recompile_shader_path;
        if (rhs.delete_title_id.is_initialized())
            delete_title_id = rhs.delete_title_id;
        if (rhs.pkg_path.is_initialized())
            pkg_path = rhs.pkg_path;
        if (rhs.pkg_zrif.is_initialized())
            pkg_zrif = rhs.pkg_zrif;

        if (!rhs.config_path.empty())
            config_path = rhs.config_path;

        overwrite_config = rhs.overwrite_config;
        load_config = rhs.load_config;
        fullscreen = rhs.fullscreen;
        console = rhs.console;
        console_arguments = rhs.console_arguments;
    }

public:
    // Optional config settings
    optional<std::string> vpk_path;
    optional<std::string> run_title_id;
    optional<std::string> recompile_shader_path;
    optional<std::string> delete_title_id;
    optional<std::string> pkg_path;
    optional<std::string> pkg_zrif;

    // Setting not present in the YAML file
    fs::path config_path = {};
    std::string console_arguments;
    bool overwrite_config = true;
    bool load_config = false;
    bool fullscreen = false;
    bool console = false;

    Config() {
        update_yaml();
    }

    ~Config() = default;

    Config(const Config &rhs) {
        yaml_node = rhs.get();
        check_members(rhs);
        update_members();
    }

    friend bool operator==(const Config &&lhs, const Config &rhs);

    //Compare members of 2 Configs
    bool operator==(const Config &rhs) {
        if (this != &rhs) {
            /*for (file_config fc = (file_config)0; fc != _INVALID; fc = (file_config)(fc + 1)) {
                auto option = Config::operator[](fc);
                auto element = this->yaml_node[option].Scalar();
                auto temp = rhs.yaml_node[option].Scalar();
                if (element != temp)
                    return false;
            }*/
            // clang-format off
            if (this->log_imports != rhs.log_imports ||
                this->stack_traceback != rhs.stack_traceback ||
                this->log_exports != rhs.log_exports ||
                this->log_active_shaders != rhs.log_active_shaders ||
                this->log_uniforms != rhs.log_uniforms ||
                this->pstv_mode != rhs.pstv_mode ||
                this->show_gui != rhs.show_gui ||
                this->apps_list_grid != rhs.apps_list_grid ||
                this->show_live_area_screen != rhs.show_live_area_screen ||
                this->icon_size != rhs.icon_size ||
                this->archive_log != rhs.archive_log ||
                this->texture_cache != rhs.texture_cache ||
                this->disable_ngs != rhs.disable_ngs ||
                this->sys_button != rhs.sys_button ||
                this->sys_lang != rhs.sys_lang ||
                this->auto_lle != rhs.auto_lle ||
                this->delay_background != rhs.delay_background ||
                this->delay_start != rhs.delay_start ||
                this->background_alpha != rhs.background_alpha ||
                this->log_level != rhs.log_level ||
                this->pref_path != rhs.pref_path ||
                this->last_app != rhs.last_app ||
                this->discord_rich_presence != rhs.discord_rich_presence ||
                this->wait_for_debugger != rhs.wait_for_debugger ||
                this->color_surface_debug != rhs.color_surface_debug ||
                this->hardware_flip != rhs.hardware_flip ||
                this->use_ubo != rhs.use_ubo ||
                this->performance_overlay != rhs.performance_overlay ||
                this->backend_renderer != rhs.backend_renderer ||
                this->keyboard_button_select != rhs.keyboard_button_select ||
                this->keyboard_button_start != rhs.keyboard_button_start ||
                this->keyboard_button_up != rhs.keyboard_button_up ||
                this->keyboard_button_right != rhs.keyboard_button_right ||
                this->keyboard_button_down != rhs.keyboard_button_down ||
                this->keyboard_button_left != rhs.keyboard_button_left ||
                this->keyboard_button_l1 != rhs.keyboard_button_l1 ||
                this->keyboard_button_r1 != rhs.keyboard_button_r1 ||
                this->keyboard_button_l2 != rhs.keyboard_button_l2 ||
                this->keyboard_button_r2 != rhs.keyboard_button_r2 ||
                this->keyboard_button_l3 != rhs.keyboard_button_l3 ||
                this->keyboard_button_r3 != rhs.keyboard_button_r3 ||
                this->keyboard_button_triangle != rhs.keyboard_button_triangle ||
                this->keyboard_button_circle != rhs.keyboard_button_circle ||
                this->keyboard_button_cross != rhs.keyboard_button_cross ||
                this->keyboard_button_square != rhs.keyboard_button_square ||
                this->keyboard_leftstick_left != rhs.keyboard_leftstick_left ||
                this->keyboard_leftstick_right != rhs.keyboard_leftstick_right ||
                this->keyboard_leftstick_up != rhs.keyboard_leftstick_up ||
                this->keyboard_leftstick_down != rhs.keyboard_leftstick_down ||
                this->keyboard_rightstick_left != rhs.keyboard_rightstick_left ||
                this->keyboard_rightstick_right != rhs.keyboard_rightstick_right ||
                this->keyboard_rightstick_up != rhs.keyboard_rightstick_up ||
                this->keyboard_rightstick_down != rhs.keyboard_rightstick_down ||
                this->keyboard_button_psbutton != rhs.keyboard_button_psbutton ||
                this->user_id != rhs.user_id ||
                this->auto_user_login != rhs.auto_user_login ||
                this->dump_textures != rhs.dump_textures ||
                this->show_welcome != rhs.show_welcome ||
                this->lle_modules != rhs.lle_modules)
                return false;
            // clang-format on
        }
        return true;
    }

    //Opposite compare between 2 configs
    const bool operator!=(const Config &rhs) {
        return !(*this == rhs);
    }

    Config(Config &&rhs) noexcept {
        yaml_node = rhs.get();
        check_members(rhs);
        update_members();
    }

    Config &operator=(const Config &rhs) {
        if (this != &rhs) {
            yaml_node = rhs.get();
            check_members(rhs);
            update_members();
        }
        return *this;
    }

    Config &operator=(Config &&rhs) noexcept {
        yaml_node = rhs.get();
        check_members(rhs);
        update_members();
        return *this;
    }

    // Merge two Config nodes, respecting both options and preferring the left-hand side
    Config &operator+=(const Config &rhs) {
        bool init = false;
        if (rhs.yaml_node == Config{}.get()) {
            init = true;
        }

        // if (log_imports != rhs.log_imports && (init || rhs.log_imports != false))
#define COMBINE_INDIVIDUAL(option_type, option_name, option_default, member_name)      \
    if (member_name != rhs.member_name && (init || rhs.member_name != option_default)) \
        member_name = rhs.member_name;

        CONFIG_INDIVIDUAL(COMBINE_INDIVIDUAL)
#undef COMBINE_INDIVIDUAL

#define COMBINE_VECTOR(option_type, option_name, option_default, member_name) \
    if (member_name != rhs.member_name && (init || !rhs.member_name.empty())) \
        vector_utils::merge_vectors(member_name, rhs.member_name);

        CONFIG_VECTOR(COMBINE_VECTOR)
#undef COMBINE_VECTOR

        check_members(rhs);
        update_yaml();
        return *this;
    }

    // Return the name of the configuration as named in the config file
    std::string operator[](const file_config &name) const {
#define SWITCH_NAMES(option_type, option_name, option_default, member_name) \
    case file_config::e_##member_name:                                      \
        return option_name;

        switch (name) {
            CONFIG_LIST(SWITCH_NAMES)

        case _INVALID:
        default: {
            return nullptr;
        }
        }
#undef SWITCH_NAMES
    }

    // Return the value of the YAML node
    // If the YAML looks outdated, call update_yaml() first, and then use this operator
    template <typename T>
    T get_from_yaml(const file_config &name) const {
        return get_member<T>(this[name]);
    }

    // Generate a YAML node based on the current values of the members.
    YAML::Node get() const {
        YAML::Node out;

#define GEN_VALUES(option_type, option_name, option_default, member_name) \
    out[option_name] = member_name;

        CONFIG_LIST(GEN_VALUES)
#undef GEN_VALUES

        return out;
    }

    // Update the YAML node based on the current values of the members
    void update_yaml() {
        yaml_node = get();
    }

    // Load a function to the node network, and then update the members
    void load_new_config(const fs::path &path) override {
        yaml_node = YAML::LoadFile(path.generic_path().string());
        update_members();
    }

    // Generate members in the preprocessor with default values
#define CREATE_MEMBERS(option_type, option_name, option_default, member_name) \
    YAML_MEMBER(option_type, member_name, option_default)

    CONFIG_LIST(CREATE_MEMBERS)
#undef GENERATE_MEMBERS
};
