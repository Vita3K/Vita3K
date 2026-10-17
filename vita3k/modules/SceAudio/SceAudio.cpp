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

#include "audio_process.h"

#include <audio/state.h>
#include <cpu/state.h>
#include <kernel/state.h>
#include <kernel/thread/thread_state.h>
#include <util/tracy.h>

#include <limits>
#include <optional>
#include <vector>

TRACY_MODULE_NAME(SceAudio);

struct AudioCaller {
    ThreadStatePtr thread;
    SceUID pid;
};

static std::optional<AudioCaller> get_audio_caller(EmuEnvState &emuenv, SceUID thread_id) {
    const auto thread = emuenv.kernel.get_thread(thread_id);
    if (!thread) {
        LOG_ERROR("Audio: unknown thread {}", thread_id);
        return std::nullopt;
    }
    const auto identity = emuenv.kernel.get_process_identity(thread->cpu->mem);
    if (!identity) {
        LOG_ERROR("Audio: no process for thread {}", thread_id);
        return std::nullopt;
    }
    return AudioCaller{ thread, identity->pid };
}

static bool valid_audio_buffer_size(int len, int channels) {
    return len > 0 && len <= std::numeric_limits<int>::max() / (channels * static_cast<int>(sizeof(int16_t)));
}

enum SceAudioOutMode {
    SCE_AUDIO_OUT_MODE_MONO = 0,
    SCE_AUDIO_OUT_MODE_STEREO = 1
};

enum SceAudioOutAlcMode {
    SCE_AUDIO_ALC_OFF,
    SCE_AUDIO_ALC_MODE1,
    SCE_AUDIO_ALC_MODE_MAX
};

enum SceAudioOutPortType {
    //! Used for main audio output, freq must be set to 48000 Hz
    SCE_AUDIO_OUT_PORT_TYPE_MAIN = 0,
    //! Used for Background Music port
    SCE_AUDIO_OUT_PORT_TYPE_BGM = 1,
    //! Used for voice chat port
    SCE_AUDIO_OUT_PORT_TYPE_VOICE = 2
};

enum SceAudioOutErrorCode : uint32_t {
    SCE_AUDIO_OUT_ERROR_NOT_OPENED = 0x80260001,
    SCE_AUDIO_OUT_ERROR_BUSY = 0x80260002,
    SCE_AUDIO_OUT_ERROR_INVALID_PORT = 0x80260003,
    SCE_AUDIO_OUT_ERROR_INVALID_POINTER = 0x80260004,
    SCE_AUDIO_OUT_ERROR_PORT_FULL = 0x80260005,
    SCE_AUDIO_OUT_ERROR_INVALID_SIZE = 0x80260006,
    SCE_AUDIO_OUT_ERROR_INVALID_FORMAT = 0x80260007,
    SCE_AUDIO_OUT_ERROR_INVALID_SAMPLE_FREQ = 0x80260008,
    SCE_AUDIO_OUT_ERROR_INVALID_VOLUME = 0x80260009,
    SCE_AUDIO_OUT_ERROR_INVALID_PORT_TYPE = 0x8026000A,
    SCE_AUDIO_OUT_ERROR_INVALID_FX_TYPE = 0x8026000B,
    SCE_AUDIO_OUT_ERROR_INVALID_CONF_TYPE = 0x8026000C,
    SCE_AUDIO_OUT_ERROR_OUT_OF_MEMORY = 0x8026000D
};

enum SceAudioOutChannelFlag {
    SCE_AUDIO_VOLUME_FLAG_L_CH = 1, //!< Left Channel
    SCE_AUDIO_VOLUME_FLAG_R_CH = 2 //!< Right Channel
};

enum SceAudioOutConfigType {
    SCE_AUDIO_OUT_CONFIG_TYPE_LEN,
    SCE_AUDIO_OUT_CONFIG_TYPE_FREQ,
    SCE_AUDIO_OUT_CONFIG_TYPE_MODE
};

template <>
std::string to_debug_str<SceAudioOutPortType>(const MemState &mem, SceAudioOutPortType type) {
    switch (type) {
    case SCE_AUDIO_OUT_PORT_TYPE_MAIN:
        return "SCE_AUDIO_OUT_PORT_TYPE_MAIN";
    case SCE_AUDIO_OUT_PORT_TYPE_BGM:
        return "SCE_AUDIO_OUT_PORT_TYPE_BGM";
    case SCE_AUDIO_OUT_PORT_TYPE_VOICE:
        return "SCE_AUDIO_OUT_PORT_TYPE_VOICE";
    }
    return std::to_string(type);
}

template <>
std::string to_debug_str<SceAudioOutConfigType>(const MemState &mem, SceAudioOutConfigType type) {
    switch (type) {
    case SCE_AUDIO_OUT_CONFIG_TYPE_LEN:
        return "SCE_AUDIO_OUT_CONFIG_TYPE_LEN";
    case SCE_AUDIO_OUT_CONFIG_TYPE_FREQ:
        return "SCE_AUDIO_OUT_CONFIG_TYPE_FREQ";
    case SCE_AUDIO_OUT_CONFIG_TYPE_MODE:
        return "SCE_AUDIO_OUT_CONFIG_TYPE_MODE";
    }
    return std::to_string(type);
}

template <>
std::string to_debug_str<SceAudioOutMode>(const MemState &mem, SceAudioOutMode type) {
    switch (type) {
    case SCE_AUDIO_OUT_MODE_MONO:
        return "SCE_AUDIO_OUT_MODE_MONO";
    case SCE_AUDIO_OUT_MODE_STEREO:
        return "SCE_AUDIO_OUT_MODE_STEREO";
    }
    return std::to_string(type);
}

template <>
std::string to_debug_str<SceAudioOutAlcMode>(const MemState &mem, SceAudioOutAlcMode type) {
    switch (type) {
    case SCE_AUDIO_ALC_OFF:
        return "SCE_AUDIO_ALC_OFF";
    case SCE_AUDIO_ALC_MODE1:
        return "SCE_AUDIO_ALC_MODE1";
    case SCE_AUDIO_ALC_MODE_MAX:
        return "SCE_AUDIO_ALC_MODE_MAX";
    }
    return std::to_string(type);
}

template <>
std::string to_debug_str<SceAudioOutChannelFlag>(const MemState &mem, SceAudioOutChannelFlag type) {
    switch (static_cast<uint32_t>(type)) {
    case SCE_AUDIO_VOLUME_FLAG_L_CH:
        return "SCE_AUDIO_VOLUME_FLAG_L_CH";
    case SCE_AUDIO_VOLUME_FLAG_R_CH:
        return "SCE_AUDIO_VOLUME_FLAG_R_CH";
        // Third case for special flag SCE_AUDIO_VOLUME_FLAG_L_CH | SCE_AUDIO_VOLUME_FLAG_R_CH
        // passed by games to set the volume of both channels at the same time
    case (SCE_AUDIO_VOLUME_FLAG_L_CH | SCE_AUDIO_VOLUME_FLAG_R_CH):
        return "SCE_AUDIO_VOLUME_FLAG_L_CH | SCE_AUDIO_VOLUME_FLAG_R_CH";
    }
    return std::to_string(type);
}

EXPORT(int, sceAudioOutGetAdopt, SceAudioOutPortType type) {
    TRACY_FUNC(sceAudioOutGetAdopt, type);
    // this is used in case the vita is using voice chat, not useful for us
    // all types of audio ports are always enabled
    return 1;
}

EXPORT(int, sceAudioOutGetConfig, int port, SceAudioOutConfigType type) {
    TRACY_FUNC(sceAudioOutGetConfig, type);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAudioOutGetPortVolume_forUser) {
    TRACY_FUNC(sceAudioOutGetPortVolume_forUser);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAudioOutOpenPort, SceAudioOutPortType type, int len, int freq, SceAudioOutMode mode) {
    TRACY_FUNC(sceAudioOutOpenPort, type, len, freq, mode);
    const auto caller = get_audio_caller(emuenv, thread_id);
    if (!caller)
        return RET_ERROR(SCE_AUDIO_OUT_ERROR_NOT_OPENED);
    if (type < SCE_AUDIO_OUT_PORT_TYPE_MAIN || type > SCE_AUDIO_OUT_PORT_TYPE_VOICE) {
        return RET_ERROR(SCE_AUDIO_OUT_ERROR_INVALID_PORT_TYPE);
    }
    if (freq <= 0 || (type == SCE_AUDIO_OUT_PORT_TYPE_MAIN && freq != 48000)) {
        return RET_ERROR(SCE_AUDIO_OUT_ERROR_INVALID_SAMPLE_FREQ);
    }
    if ((mode != SCE_AUDIO_OUT_MODE_MONO) && (mode != SCE_AUDIO_OUT_MODE_STEREO)) {
        return RET_ERROR(SCE_AUDIO_OUT_ERROR_INVALID_FORMAT);
    }
    const int channels = (mode == SCE_AUDIO_OUT_MODE_MONO) ? 1 : 2;
    if (!valid_audio_buffer_size(len, channels)) {
        return RET_ERROR(SCE_AUDIO_OUT_ERROR_INVALID_SIZE);
    }

    AudioOutPortPtr port = emuenv.audio.open_port(channels, freq, len);
    if (!port)
        return RET_ERROR(SCE_AUDIO_OUT_ERROR_NOT_OPENED);

    // Save the port configuration
    port->type = type;
    port->len = len;
    port->freq = freq;
    port->mode = mode;
    port->owner_pid = caller->pid;

    const std::lock_guard<std::mutex> lock(emuenv.audio.mutex);
    const int port_id = emuenv.audio.next_port_id++;
    emuenv.audio.out_ports.emplace(port_id, port);

    return port_id;
}

EXPORT(int, sceAudioOutOutput, int port, Ptr<const uint8_t> buf) {
    TRACY_FUNC(sceAudioOutOutput, port, buf);
    const auto caller = get_audio_caller(emuenv, thread_id);
    if (!caller)
        return RET_ERROR(SCE_AUDIO_OUT_ERROR_INVALID_PORT);
    const AudioOutPortPtr prt = audio_process::find_port(emuenv.audio, port, caller->pid);
    if (!prt) {
        return RET_ERROR(SCE_AUDIO_OUT_ERROR_INVALID_PORT);
    }

    // Empty "buf" variable is valid. It mean wait until sound output is completed.
    // Because this function always returns when all sound is out, then on empty buf it returns immediately.
    // Return value is the number of samples (value of 0 or greater) registered to the audio driver for normal termination.
    if (!buf)
        return 0;

    try {
        std::vector<uint8_t> buffer(static_cast<size_t>(prt->len_bytes));
        if (!audio_process::copy_guest_buffer(caller->thread->cpu->mem, buf.address(), buffer.data(), static_cast<uint32_t>(buffer.size())))
            return RET_ERROR(SCE_AUDIO_OUT_ERROR_INVALID_POINTER);
        const ThreadState::HostWait waiting(*caller->thread);
        emuenv.audio.audio_output(*prt, buffer.data());
        return prt->len;
    } catch (const std::bad_alloc &) {
        LOG_ERROR("{}: unable to copy audio buffer for port {}", export_name, port);
        return RET_ERROR(SCE_AUDIO_OUT_ERROR_OUT_OF_MEMORY);
    }
}

EXPORT(int, sceAudioOutGetRestSample, int port) {
    TRACY_FUNC(sceAudioOutGetRestSample, port);
    const auto caller = get_audio_caller(emuenv, thread_id);
    if (!caller)
        return RET_ERROR(SCE_AUDIO_OUT_ERROR_INVALID_PORT);
    const AudioOutPortPtr prt = audio_process::find_port(emuenv.audio, port, caller->pid);
    if (!prt) {
        return RET_ERROR(SCE_AUDIO_OUT_ERROR_INVALID_PORT);
    }
    int samples_available = emuenv.audio.get_rest_sample(*prt);
    if (prt->type == SCE_AUDIO_OUT_PORT_TYPE_MAIN) {
        samples_available = std::clamp(samples_available, 0, prt->len);
    } else {
        // for other port types only the granularity value set for the len argument or 0 can be returned.
        if (samples_available < prt->len / 2)
            samples_available = 0;
        else
            samples_available = prt->len;
    }
    return samples_available;
}

EXPORT(int, sceAudioOutOpenExtPort) {
    TRACY_FUNC(sceAudioOutOpenExtPort);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAudioOutReleasePort, int port) {
    TRACY_FUNC(sceAudioOutReleasePort, port);
    const auto caller = get_audio_caller(emuenv, thread_id);
    if (!caller || !audio_process::release_port(emuenv.audio, port, caller->pid)) {
        return RET_ERROR(SCE_AUDIO_OUT_ERROR_INVALID_PORT);
    }

    return 0;
}

EXPORT(int, sceAudioOutSetAdoptMode) {
    TRACY_FUNC(sceAudioOutSetAdoptMode);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAudioOutSetAdopt_forUser) {
    TRACY_FUNC(sceAudioOutSetAdopt_forUser);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAudioOutSetAlcMode, SceAudioOutAlcMode mode) {
    TRACY_FUNC(sceAudioOutSetAlcMode, mode);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAudioOutSetCompress) {
    TRACY_FUNC(sceAudioOutSetCompress);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAudioOutSetConfig, int port, int len, int freq, int mode) {
    TRACY_FUNC(sceAudioOutSetConfig, port, len, freq, mode);
    const auto caller = get_audio_caller(emuenv, thread_id);
    if (!caller)
        return RET_ERROR(SCE_AUDIO_OUT_ERROR_INVALID_PORT);
    if (len == 0)
        return RET_ERROR(SCE_AUDIO_OUT_ERROR_INVALID_SIZE);

    if ((mode >= 0) && (mode != SCE_AUDIO_OUT_MODE_MONO) && (mode != SCE_AUDIO_OUT_MODE_STEREO))
        return RET_ERROR(SCE_AUDIO_OUT_ERROR_INVALID_FORMAT);

    AudioOutPortPtr prt = audio_process::find_port(emuenv.audio, port, caller->pid);
    if (!prt)
        return RET_ERROR(SCE_AUDIO_OUT_ERROR_INVALID_PORT);

    if ((freq >= 0) && (prt->type == SCE_AUDIO_OUT_PORT_TYPE_MAIN) && (freq != 48000))
        return RET_ERROR(SCE_AUDIO_OUT_ERROR_INVALID_SAMPLE_FREQ);

    const auto set_len = len > 0 ? len : prt->len;
    const auto set_freq = freq >= 0 ? freq : prt->freq;
    const auto set_mode = mode >= 0 ? mode : prt->mode;
    const auto channels = (set_mode == SCE_AUDIO_OUT_MODE_MONO) ? 1 : 2;
    if (set_freq <= 0)
        return RET_ERROR(SCE_AUDIO_OUT_ERROR_INVALID_SAMPLE_FREQ);
    if (!valid_audio_buffer_size(set_len, channels))
        return RET_ERROR(SCE_AUDIO_OUT_ERROR_INVALID_SIZE);

    if ((set_len == prt->len) && (set_freq == prt->freq) && (set_mode == prt->mode))
        return 0;

    AudioOutPortPtr replacement = emuenv.audio.open_port(channels, set_freq, set_len);
    if (!replacement)
        return RET_ERROR(SCE_AUDIO_OUT_ERROR_NOT_OPENED);

    replacement->type = prt->type;
    replacement->freq = set_freq;
    replacement->len = set_len;
    replacement->mode = set_mode;
    replacement->owner_pid = caller->pid;
    replacement->left_channel_volume = prt->left_channel_volume;
    replacement->right_channel_volume = prt->right_channel_volume;
    emuenv.audio.set_volume(*replacement, prt->volume);

    const std::lock_guard<std::mutex> lock(emuenv.audio.mutex);
    const auto it = emuenv.audio.out_ports.find(port);
    if (it == emuenv.audio.out_ports.end() || it->second != prt)
        return RET_ERROR(SCE_AUDIO_OUT_ERROR_INVALID_PORT);
    prt->stopping = true;
    if (emuenv.audio.adapter)
        emuenv.audio.adapter->wake_all_ports();
    it->second = std::move(replacement);

    return 0;
}

EXPORT(int, sceAudioOutSetEffectType) {
    TRACY_FUNC(sceAudioOutSetEffectType);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAudioOutSetPortVolume_forUser) {
    TRACY_FUNC(sceAudioOutSetPortVolume_forUser);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAudioOutSetVolume, int port, SceAudioOutChannelFlag ch, Ptr<const int> vol) {
    TRACY_FUNC(sceAudioOutSetVolume, port, ch, vol);
    if (ch == 0) // no channel selected, no changes
        return 0;

    const auto caller = get_audio_caller(emuenv, thread_id);
    if (!caller)
        return RET_ERROR(SCE_AUDIO_OUT_ERROR_INVALID_PORT);
    const AudioOutPortPtr prt = audio_process::find_port(emuenv.audio, port, caller->pid);

    if (!prt)
        return RET_ERROR(SCE_AUDIO_OUT_ERROR_INVALID_PORT);

    int volumes[2]{};
    try {
        if (!vol || !audio_process::copy_guest_buffer(caller->thread->cpu->mem, vol.address(), volumes, sizeof(volumes)))
            return RET_ERROR(SCE_AUDIO_OUT_ERROR_INVALID_POINTER);
    } catch (const std::bad_alloc &) {
        LOG_ERROR("{}: unable to pin volume buffer for port {}", export_name, port);
        return RET_ERROR(SCE_AUDIO_OUT_ERROR_OUT_OF_MEMORY);
    }
    const int left = (ch & SCE_AUDIO_VOLUME_FLAG_L_CH) ? volumes[0] : prt->left_channel_volume;
    const int right = (ch & SCE_AUDIO_VOLUME_FLAG_R_CH) ? volumes[1] : prt->right_channel_volume;
    const float volume_level = (static_cast<float>(left + right) / static_cast<float>(SCE_AUDIO_VOLUME_0DB * 2));

    // then update channel volumes in case there was a change
    prt->left_channel_volume = left;
    prt->right_channel_volume = right;
    emuenv.audio.set_volume(*prt, volume_level);

    return 0;
}
