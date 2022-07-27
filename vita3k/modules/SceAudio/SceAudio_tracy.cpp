// Vita3K emulator project
// Copyright (C) 2022 Vita3K team
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

/**
 * @file SceAudio_tracy.cpp
 * @brief Tracy argument serializing functions for SceAudio
 */

#ifdef TRACY_ENABLE

#include "SceAudio_tracy.h"

void tracy_sceAudioOutGetAdopt(tracy::ScopedZone *___tracy_scoped_zone, bool module_activation, SceAudioOutPortType type) {
    // Tracy activation state check to log arguments
    if (module_activation) {
        struct TracyLogSettings {
            struct args {
                std::string type = "type: ";
            } args;

        } tracy_settings;

        // Log enumerated args
        switch (type) {
        case SCE_AUDIO_OUT_PORT_TYPE_MAIN:
            tracy_settings.args.type += "SCE_AUDIO_OUT_PORT_TYPE_MAIN";
            break;
        case SCE_AUDIO_OUT_PORT_TYPE_BGM:
            tracy_settings.args.type += "SCE_AUDIO_OUT_PORT_TYPE_BGM";
            break;
        case SCE_AUDIO_OUT_PORT_TYPE_VOICE:
            tracy_settings.args.type += "SCE_AUDIO_OUT_PORT_TYPE_VOICE";
            break;
        }

        // Send args
        ___tracy_scoped_zone->Text(tracy_settings.args.type.c_str(), tracy_settings.args.type.size());
    }
}

void tracy_sceAudioOutGetConfig(tracy::ScopedZone *___tracy_scoped_zone, bool module_activation, int port, SceAudioOutConfigType type) {
    // Tracy activation state check to log arguments
    if (module_activation) {
        struct TracyLogSettings {
            struct args {
                std::string port = "port: ";
                std::string type = "type: ";
            } args;

        } tracy_settings;

        // Collect function call args to be logged
        tracy_settings.args.port += std::to_string(port);

        // Collect function call enumerated args
        switch (type) {
        case SCE_AUDIO_OUT_CONFIG_TYPE_LEN:
            tracy_settings.args.type += "SCE_AUDIO_OUT_CONFIG_TYPE_LEN";
            break;
        case SCE_AUDIO_OUT_CONFIG_TYPE_FREQ:
            tracy_settings.args.type += "SCE_AUDIO_OUT_CONFIG_TYPE_FREQ";
            break;
        case SCE_AUDIO_OUT_CONFIG_TYPE_MODE:
            tracy_settings.args.type += "SCE_AUDIO_OUT_CONFIG_TYPE_MODE";
            break;
        }

        // Send args
        ___tracy_scoped_zone->Text(tracy_settings.args.port.c_str(), tracy_settings.args.port.size());
        ___tracy_scoped_zone->Text(tracy_settings.args.type.c_str(), tracy_settings.args.type.size());
    }
}

// void tracy_sceAudioOutGetPortVolume_forUser();

void tracy_sceAudioOutOpenPort(tracy::ScopedZone *___tracy_scoped_zone, bool module_activation, SceAudioOutPortType type, int len, int freq, SceAudioOutMode mode) {
    // Tracy activation state check to log arguments
    if (module_activation) {
        struct TracyLogSettings {
            struct args {
                std::string type = "type: ";
                std::string len = "len: ";
                std::string freq = "freq: ";
                std::string mode = "mode: ";
            } args;

        } tracy_settings;

        // Collect function call args to be logged
        tracy_settings.args.len += std::to_string(len);
        tracy_settings.args.freq += std::to_string(freq);

        // Collect function call enumerated args
        switch (type) {
        case SCE_AUDIO_OUT_PORT_TYPE_MAIN:
            tracy_settings.args.type += "SCE_AUDIO_OUT_PORT_TYPE_MAIN";
            break;
        case SCE_AUDIO_OUT_PORT_TYPE_BGM:
            tracy_settings.args.type += "SCE_AUDIO_OUT_PORT_TYPE_BGM";
            break;
        case SCE_AUDIO_OUT_PORT_TYPE_VOICE:
            tracy_settings.args.type += "SCE_AUDIO_OUT_PORT_TYPE_VOICE";
            break;
        }

        switch (mode) {
        case SCE_AUDIO_OUT_MODE_MONO:
            tracy_settings.args.mode += "SCE_AUDIO_OUT_MODE_MONO";
            break;
        case SCE_AUDIO_OUT_MODE_STEREO:
            tracy_settings.args.mode += "SCE_AUDIO_OUT_MODE_STEREO";
            break;
        }

        // Send args
        ___tracy_scoped_zone->Text(tracy_settings.args.type.c_str(), tracy_settings.args.type.size());
        ___tracy_scoped_zone->Text(tracy_settings.args.len.c_str(), tracy_settings.args.len.size());
        ___tracy_scoped_zone->Text(tracy_settings.args.freq.c_str(), tracy_settings.args.freq.size());
        ___tracy_scoped_zone->Text(tracy_settings.args.mode.c_str(), tracy_settings.args.mode.size());
    }
}

void tracy_sceAudioOutOutput(tracy::ScopedZone *___tracy_scoped_zone, bool module_activation, int port) {
    // Tracy activation state check to log arguments
    if (module_activation) {
        struct TracyLogSettings {
            struct args {
                std::string port = "port: ";
            } args;

        } tracy_settings;

        // Collect function call args to be logged
        tracy_settings.args.port += std::to_string(port);

        // Send args
        ___tracy_scoped_zone->Text(tracy_settings.args.port.c_str(), tracy_settings.args.port.size());
    }
}

void tracy_sceAudioOutGetRestSample(tracy::ScopedZone *___tracy_scoped_zone, bool module_activation, int port) {
    // Tracy activation state check to log arguments
    if (module_activation) {
        struct TracyLogSettings {
            struct args {
                std::string port = "port: ";
            } args;

        } tracy_settings;

        // Collect function call args to be logged
        tracy_settings.args.port += std::to_string(port);

        // Send args
        ___tracy_scoped_zone->Text(tracy_settings.args.port.c_str(), tracy_settings.args.port.size());
    }
}

// void tracy_sceAudioOutOpenExtPort();

void tracy_sceAudioOutReleasePort(tracy::ScopedZone *___tracy_scoped_zone, bool module_activation, int port) {
    // Tracy activation state check to log arguments
    if (module_activation) {
        struct TracyLogSettings {
            struct args {
                std::string port = "port: ";
            } args;

        } tracy_settings;

        // Collect function call args to be logged
        tracy_settings.args.port += std::to_string(port);

        // Send args
        ___tracy_scoped_zone->Text(tracy_settings.args.port.c_str(), tracy_settings.args.port.size());
    }
}

// void tracy_sceAudioOutSetAdoptMode();

// void tracy_sceAudioOutSetAdopt_forUser();

void tracy_sceAudioOutSetAlcMode(tracy::ScopedZone *___tracy_scoped_zone, bool module_activation, SceAudioOutAlcMode mode) {
    // Tracy activation state check to log arguments
    if (module_activation) {
        struct TracyLogSettings {
            struct args {
                std::string mode = "mode: ";
            } args;

        } tracy_settings;

        // Collect function call enumerated args
        switch (mode) {
        case SCE_AUDIO_ALC_OFF:
            tracy_settings.args.mode += "SCE_AUDIO_ALC_OFF";
            break;
        case SCE_AUDIO_ALC_MODE1:
            tracy_settings.args.mode += "SCE_AUDIO_ALC_MODE1";
            break;
        case SCE_AUDIO_ALC_MODE_MAX:
            tracy_settings.args.mode += "SCE_AUDIO_ALC_MODE_MAX";
            break;
        }

        // Send args
        ___tracy_scoped_zone->Text(tracy_settings.args.mode.c_str(), tracy_settings.args.mode.size());
    }
}

void tracy_sceAudioOutSetConfig(tracy::ScopedZone *___tracy_scoped_zone, bool module_activation, int port, SceSize len, int freq, SceAudioOutMode mode) {
    // Tracy activation state check to log arguments
    if (module_activation) {
        struct TracyLogSettings {
            struct args {
                std::string port = "port: ";
                std::string len = "len: ";
                std::string freq = "freq: ";
                std::string mode = "mode: ";
            } args;

        } tracy_settings;

        // Collect function call args to be logged
        tracy_settings.args.port += std::to_string(port);
        tracy_settings.args.len += std::to_string(len);
        tracy_settings.args.freq += std::to_string(freq);

        // Log enumerated args
        switch (mode) {
        case SCE_AUDIO_OUT_MODE_MONO:
            tracy_settings.args.mode += "SCE_AUDIO_OUT_MODE_MONO";
            break;
        case SCE_AUDIO_OUT_MODE_STEREO:
            tracy_settings.args.mode += "SCE_AUDIO_OUT_MODE_STEREO";
            break;
        }

        // Send args
        ___tracy_scoped_zone->Text(tracy_settings.args.port.c_str(), tracy_settings.args.port.size());
        ___tracy_scoped_zone->Text(tracy_settings.args.len.c_str(), tracy_settings.args.len.size());
        ___tracy_scoped_zone->Text(tracy_settings.args.freq.c_str(), tracy_settings.args.freq.size());
        ___tracy_scoped_zone->Text(tracy_settings.args.mode.c_str(), tracy_settings.args.mode.size());
    }
}

// void tracy_sceAudioOutSetEffectType();

void tracy_sceAudioOutSetVolume(tracy::ScopedZone *___tracy_scoped_zone, bool module_activation, int port, SceAudioOutChannelFlag ch, int *vol) {
    // Tracy activation state check to log arguments
    if (module_activation) {
        struct TracyLogSettings {
            struct args {
                std::string port = "port: ";
                std::string ch = "ch: ";
                std::string vol = "vol: ";
            } args;

        } tracy_settings;

        // Collect function call args to be logged
        tracy_settings.args.port += std::to_string(port);
        tracy_settings.args.vol += std::to_string(*vol);

        // Log enumerated args
        switch (static_cast<int>(ch)) { // static_cast to avoid warning on third case
        case SCE_AUDIO_VOLUME_FLAG_L_CH:
            tracy_settings.args.ch += "SCE_AUDIO_VOLUME_FLAG_L_CH";
            break;
        case SCE_AUDIO_VOLUME_FLAG_R_CH:
            tracy_settings.args.ch += "SCE_AUDIO_VOLUME_FLAG_R_CH";
            break;

        // Third case for special flag SCE_AUDIO_VOLUME_FLAG_L_CH | SCE_AUDIO_VOLUME_FLAG_R_CH
        // passed by games to set the volume of both channels at the same time
        case (SCE_AUDIO_VOLUME_FLAG_L_CH | SCE_AUDIO_VOLUME_FLAG_R_CH):
            tracy_settings.args.ch += "SCE_AUDIO_VOLUME_FLAG_L_CH | SCE_AUDIO_VOLUME_FLAG_R_CH";
            break;
        }

        // Send args
        ___tracy_scoped_zone->Text(tracy_settings.args.port.c_str(), tracy_settings.args.port.size());
        ___tracy_scoped_zone->Text(tracy_settings.args.ch.c_str(), tracy_settings.args.ch.size());
        ___tracy_scoped_zone->Text(tracy_settings.args.vol.c_str(), tracy_settings.args.vol.size());
    }
}

#endif // TRACY_ENABLE
