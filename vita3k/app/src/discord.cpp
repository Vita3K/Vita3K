#if DISCORD_RPC

#include <app/discord.h>
#include <discord_rpc.h>

#include <ctime>

// Credits to the RPCS3 Project

namespace discord {
void init(const std::string &application_id) {
    DiscordEventHandlers handlers = {};
    Discord_Initialize(application_id.c_str(), &handlers, 1, NULL);
}

void shutdown() {
    Discord_Shutdown();
}

void update_init_status(bool discord_rich_presence, bool *discord_rich_presence_old) {
    if (*discord_rich_presence_old != discord_rich_presence) {
        if (discord_rich_presence) {
            discord::init();
            discord::update_presence("", "Idle");
        } else {
            discord::shutdown();
        }
        *discord_rich_presence_old = discord_rich_presence;
    }
}

void update_presence(const std::string &state, const std::string &details, bool reset_timer) {
    DiscordRichPresence discord_presence = {};
    discord_presence.details = details.c_str();
    discord_presence.state = state.c_str();
    discord_presence.largeImageKey = "vita3k-logo";
    discord_presence.largeImageText = "Vita3K is the world's first functional PlayStation Vita emulator";

    if (reset_timer) {
        discord_presence.startTimestamp = time(0);
    }

    Discord_UpdatePresence(&discord_presence);
}
} // namespace discord
#endif
