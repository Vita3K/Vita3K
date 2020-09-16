#pragma once

#if USE_DISCORD

#include <discord.h>

#include <string>

namespace discordrpc {
bool init();

void shutdown();

void update_init_status(bool discord_rich_presence, bool *discord_rich_presence_old);

void update_presence(const std::string &state = "", const std::string &details = "Idle", bool reset_timer = true);
} // namespace discordrpc

#endif
