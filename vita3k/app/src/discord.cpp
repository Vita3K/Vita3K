// Vita3K emulator project
// Copyright (C) 2021 Vita3K team
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

#if USE_DISCORD

#include <app/discord.h>
#include <util/log.h>

struct DiscordState {
    std::unique_ptr<discord::Core> core;
    bool running = false;
};

namespace discordrpc {

DiscordState discord_state{};

bool init() {
    discord::Core *core{};
    auto result = discord::Core::Create(570296795943403530, DiscordCreateFlags_NoRequireDiscord, &core);
    if (result != discord::Result::Ok) {
        return false;
    }
    discord_state.core.reset(core);
    if (!discord_state.core) {
        LOG_ERROR("Failed to instantiate discord core, err_code: {}", static_cast<int>(result));
        return false;
    }
    discord_state.running = true;
    return true;
}

void shutdown() {
    discord_state.running = false;
    discord_state.core.reset();
}

void update_init_status(bool discord_rich_presence, bool *discord_rich_presence_old) {
    if (*discord_rich_presence_old != discord_rich_presence) {
        if (discord_rich_presence) {
            discordrpc::init();
            discordrpc::update_presence();
        } else {
            discordrpc::shutdown();
        }
        *discord_rich_presence_old = discord_rich_presence;
    }
    if (discord_state.running) {
        discord_state.core->RunCallbacks();
    }
}

void update_presence(const std::string &state, const std::string &details, bool reset_timer) {
    discord::Activity activity{};

    if (discord_state.running) {
        activity.SetDetails(details.c_str());
        activity.SetState(state.c_str());
        activity.GetAssets().SetLargeImage("vita3k-logo");
        activity.GetAssets().SetLargeText("Vita3K is the world's first functional PlayStation Vita emulator");
        activity.SetType(discord::ActivityType::Playing);
        if (reset_timer) {
            activity.GetTimestamps().SetStart(time(nullptr));
        }
        discord_state.core->ActivityManager().UpdateActivity(activity, [](discord::Result result) {
            if (result != discord::Result::Ok) {
                LOG_ERROR("Error updating discord rich presence, err_code: {}", static_cast<int>(result));
            }
        });
    }
}
} // namespace discordrpc
#endif
