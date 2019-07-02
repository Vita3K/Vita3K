#pragma once

#include <string>

namespace discord {
void init(const std::string &application_id = "570296795943403530");

void shutdown();

void update_init_status(bool discord_rich_presence, bool *discord_rich_presence_old);

void update_presence(const std::string &state = "", const std::string &details = "Idle", bool reset_timer = true);
} // namespace discord
