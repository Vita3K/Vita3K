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

#include "SceAudio.h"
#include "SceAudio_tracy.h"
#include "Tracy.hpp"

#include <audio/state.h>
#include <kernel/state.h>
#include <util/lock_and_find.h>

EXPORT(int, sceAudioOutGetAdopt, SceAudioOutPortType type) {
#ifdef TRACY_ENABLE
    // --- Tracy logging --- START
    bool _tracy_activation_state = config::is_tracy_advanced_profiling_active_for_module(emuenv.cfg.tracy_advanced_profiling_modules, tracy_module_name);
    ZoneNamedN(___tracy_scoped_zone, "sceAudioOutGetAdopt", _tracy_activation_state);
    tracy_sceAudioOutGetAdopt(&___tracy_scoped_zone, _tracy_activation_state, type);
    // --- Tracy logging --- END
#endif

    // this is used in case the vita is using voice chat, not useful for us
    // all types of audio ports are always enabled
    return 1;
}

EXPORT(int, sceAudioOutGetConfig, int port, SceAudioOutConfigType type) {
#ifdef TRACY_ENABLE
    // --- Tracy logging --- START
    bool _tracy_activation_state = config::is_tracy_advanced_profiling_active_for_module(emuenv.cfg.tracy_advanced_profiling_modules, tracy_module_name);
    ZoneNamedN(___tracy_scoped_zone, "sceAudioOutGetConfig", _tracy_activation_state);
    tracy_sceAudioOutGetConfig(&___tracy_scoped_zone, _tracy_activation_state, port, type);
    // --- Tracy logging --- END
#endif

    return UNIMPLEMENTED();
}

EXPORT(int, sceAudioOutGetPortVolume_forUser) {
#ifdef TRACY_ENABLE
    // --- Tracy logging --- START
    bool _tracy_activation_state = config::is_tracy_advanced_profiling_active_for_module(emuenv.cfg.tracy_advanced_profiling_modules, tracy_module_name);
    ZoneNamedN(___tracy_scoped_zone, "sceAudioOutGetPortVolume_forUser", _tracy_activation_state);
    // --- Tracy logging --- END
#endif

    return UNIMPLEMENTED();
}

EXPORT(int, sceAudioOutOpenPort, SceAudioOutPortType type, int len, int freq, SceAudioOutMode mode) {
#ifdef TRACY_ENABLE
    // Tracy logging --- START
    bool _tracy_activation_state = config::is_tracy_advanced_profiling_active_for_module(emuenv.cfg.tracy_advanced_profiling_modules, tracy_module_name);
    ZoneNamedN(___tracy_scoped_zone, "sceAudioOutOpenPort", _tracy_activation_state);
    tracy_sceAudioOutOpenPort(&___tracy_scoped_zone, _tracy_activation_state, type, len, freq, mode);
    // Tracy logging --- END
#endif

    if (type < SCE_AUDIO_OUT_PORT_TYPE_MAIN || type > SCE_AUDIO_OUT_PORT_TYPE_VOICE) {
        return RET_ERROR(SCE_AUDIO_OUT_ERROR_INVALID_PORT_TYPE);
    }
    if (type == SCE_AUDIO_OUT_PORT_TYPE_MAIN && freq != 48000) {
        return RET_ERROR(SCE_AUDIO_OUT_ERROR_INVALID_SAMPLE_FREQ);
    }
    if ((mode != SCE_AUDIO_OUT_MODE_MONO) && (mode != SCE_AUDIO_OUT_MODE_STEREO)) {
        return RET_ERROR(SCE_AUDIO_OUT_ERROR_INVALID_FORMAT);
    }
    if (len <= 0) {
        return RET_ERROR(SCE_AUDIO_OUT_ERROR_INVALID_SIZE);
    }

    const int channels = (mode == SCE_AUDIO_OUT_MODE_MONO) ? 1 : 2;
    const AudioStreamPtr stream(SDL_NewAudioStream(AUDIO_S16LSB, channels, freq, emuenv.audio.ro.spec.format, emuenv.audio.ro.spec.channels, emuenv.audio.ro.spec.freq), SDL_FreeAudioStream);
    if (!stream) {
        return RET_ERROR(SCE_AUDIO_OUT_ERROR_NOT_OPENED);
    }

    const AudioOutPortPtr port = std::make_shared<AudioOutPort>();
    port->ro.len_bytes = len * channels * sizeof(int16_t);
    port->shared.stream = stream;

    const std::lock_guard<std::mutex> lock(emuenv.audio.shared.mutex);
    const int port_id = emuenv.audio.shared.next_port_id++;
    emuenv.audio.shared.out_ports.emplace(port_id, port);

    return port_id;
}

EXPORT(int, sceAudioOutOutput, int port, const void *buf) {
#ifdef TRACY_ENABLE
    // --- Tracy logging --- START
    bool _tracy_activation_state = config::is_tracy_advanced_profiling_active_for_module(emuenv.cfg.tracy_advanced_profiling_modules, tracy_module_name);
    ZoneNamedN(___tracy_scoped_zone, "sceAudioOutOutput", _tracy_activation_state);
    tracy_sceAudioOutOutput(&___tracy_scoped_zone, _tracy_activation_state, port);
    // --- Tracy logging --- END
#endif

    const AudioOutPortPtr prt = lock_and_find(port, emuenv.audio.shared.out_ports, emuenv.audio.shared.mutex);
    if (!prt) {
        return RET_ERROR(SCE_AUDIO_OUT_ERROR_INVALID_PORT);
    }

    const ThreadStatePtr thread = lock_and_find(thread_id, emuenv.kernel.threads, emuenv.kernel.mutex);
    if (!thread) {
        return RET_ERROR(SCE_AUDIO_OUT_ERROR_INVALID_PORT);
    }

    // Put audio to the port's stream and see how much is left to play.
    std::unique_lock<std::mutex> lock(prt->shared.mutex);
    SDL_AudioStreamPut(prt->shared.stream.get(), buf, prt->ro.len_bytes);
    const int available = SDL_AudioStreamAvailable(prt->shared.stream.get());

    // If there's lots of audio left to play, stop this thread.
    // The audio callback will wake it up later when it's running out of data.
    // the 3*emuenv.audio.ro.spec.size is needed for some games with an 480 host audiobuffer
    // sample size (what SDL audio gives us) to make sure this does not happen
    // we are supposed to wait for the existing samples to be processed (except the ones just passed)
    // but this would give a bad audio because the host buffer size is different compared to the guest buffer size
    // so we need to cache more data to make sure we always have enough
    if (available >= 3 * emuenv.audio.ro.spec.size) {
        prt->shared.thread = thread_id;

        std::unique_lock<std::mutex> mlock(thread->mutex);
        thread->update_status(ThreadStatus::wait);
        lock.unlock();
        thread->status_cond.wait(mlock, [&]() { return thread->status == ThreadStatus::run; });
    }

    return 0;
}

EXPORT(int, sceAudioOutGetRestSample, int port) {
#ifdef TRACY_ENABLE
    // --- Tracy logging --- START
    bool _tracy_activation_state = config::is_tracy_advanced_profiling_active_for_module(emuenv.cfg.tracy_advanced_profiling_modules, tracy_module_name);
    ZoneNamedN(___tracy_scoped_zone, "sceAudioOutGetRestSample", _tracy_activation_state);
    tracy_sceAudioOutGetRestSample(&___tracy_scoped_zone, _tracy_activation_state, port);
    // --- Tracy logging --- END
#endif

    const AudioOutPortPtr prt = lock_and_find(port, emuenv.audio.shared.out_ports, emuenv.audio.shared.mutex);
    if (!prt) {
        return RET_ERROR(SCE_AUDIO_OUT_ERROR_INVALID_PORT);
    }

    const int bytes_available = SDL_AudioStreamAvailable(prt->shared.stream.get());
    assert(emuenv.audio.ro.spec.format == AUDIO_S16LSB);

    // we have the number of bytes left, we can convert it back to the number of samples left
    return bytes_available / (emuenv.audio.ro.spec.channels * sizeof(int16_t));
}

EXPORT(int, sceAudioOutOpenExtPort) {
#ifdef TRACY_ENABLE
    // --- Tracy logging --- START
    bool _tracy_activation_state = config::is_tracy_advanced_profiling_active_for_module(emuenv.cfg.tracy_advanced_profiling_modules, tracy_module_name);
    ZoneNamedN(___tracy_scoped_zone, "sceAudioOutOpenExtPort", _tracy_activation_state);
    // --- Tracy logging --- END
#endif

    return UNIMPLEMENTED();
}

EXPORT(int, sceAudioOutReleasePort, int port) {
#ifdef TRACY_ENABLE
    // --- Tracy logging --- START
    bool _tracy_activation_state = config::is_tracy_advanced_profiling_active_for_module(emuenv.cfg.tracy_advanced_profiling_modules, tracy_module_name);
    ZoneNamedN(___tracy_scoped_zone, "sceAudioOutReleasePort", _tracy_activation_state);
    tracy_sceAudioOutReleasePort(&___tracy_scoped_zone, _tracy_activation_state, port);
    // --- Tracy logging --- END
#endif

    const std::lock_guard<std::mutex> guard(emuenv.audio.shared.mutex);
    if (!emuenv.audio.shared.out_ports.erase(port)) {
        return RET_ERROR(SCE_AUDIO_OUT_ERROR_INVALID_PORT);
    }

    return 0;
}

EXPORT(int, sceAudioOutSetAdoptMode) {
#ifdef TRACY_ENABLE
    // --- Tracy logging --- START
    bool _tracy_activation_state = config::is_tracy_advanced_profiling_active_for_module(emuenv.cfg.tracy_advanced_profiling_modules, tracy_module_name);
    ZoneNamedN(___tracy_scoped_zone, "sceAudioOutSetAdoptMode", _tracy_activation_state);
    // --- Tracy logging --- END
#endif

    return UNIMPLEMENTED();
}

EXPORT(int, sceAudioOutSetAdopt_forUser) {
#ifdef TRACY_ENABLE
    // --- Tracy logging --- START
    bool _tracy_activation_state = config::is_tracy_advanced_profiling_active_for_module(emuenv.cfg.tracy_advanced_profiling_modules, tracy_module_name);
    ZoneNamedN(___tracy_scoped_zone, "sceAudioOutSetAdopt_forUser", _tracy_activation_state);
    // --- Tracy logging --- END
#endif

    return UNIMPLEMENTED();
}

EXPORT(int, sceAudioOutSetAlcMode, SceAudioOutAlcMode mode) {
#ifdef TRACY_ENABLE
    // --- Tracy logging --- START
    bool _tracy_activation_state = config::is_tracy_advanced_profiling_active_for_module(emuenv.cfg.tracy_advanced_profiling_modules, tracy_module_name);
    ZoneNamedN(___tracy_scoped_zone, "sceAudioOutSetAlcMode", _tracy_activation_state);
    tracy_sceAudioOutSetAlcMode(&___tracy_scoped_zone, _tracy_activation_state, mode);
    // --- Tracy logging --- END
#endif

    return UNIMPLEMENTED();
}

EXPORT(int, sceAudioOutSetCompress) {
#ifdef TRACY_ENABLE
    // --- Tracy logging --- START
    bool _tracy_activation_state = config::is_tracy_advanced_profiling_active_for_module(emuenv.cfg.tracy_advanced_profiling_modules, tracy_module_name);
    ZoneNamedN(___tracy_scoped_zone, "sceAudioOutSetCompress", _tracy_activation_state);
    // --- Tracy logging --- END
#endif

    return UNIMPLEMENTED();
}

EXPORT(int, sceAudioOutSetConfig, int port, SceSize len, int freq, SceAudioOutMode mode) {
#ifdef TRACY_ENABLE
    // --- Tracy logging --- START
    bool _tracy_activation_state = config::is_tracy_advanced_profiling_active_for_module(emuenv.cfg.tracy_advanced_profiling_modules, tracy_module_name);
    ZoneNamedN(___tracy_scoped_zone, "sceAudioOutSetConfig", _tracy_activation_state);
    tracy_sceAudioOutSetConfig(&___tracy_scoped_zone, _tracy_activation_state, port, len, freq, mode);
#endif
    // --- Tracy logging --- END

    return UNIMPLEMENTED();
}

EXPORT(int, sceAudioOutSetEffectType) {
#ifdef TRACY_ENABLE
    // --- Tracy logging --- START
    bool _tracy_activation_state = config::is_tracy_advanced_profiling_active_for_module(emuenv.cfg.tracy_advanced_profiling_modules, tracy_module_name);
    ZoneNamedN(___tracy_scoped_zone, "sceAudioOutSetEffectType", _tracy_activation_state);
    // --- Tracy logging --- END
#endif

    return UNIMPLEMENTED();
}

EXPORT(int, sceAudioOutSetPortVolume_forUser) {
#ifdef TRACY_ENABLE
    // --- Tracy logging --- START
    bool _tracy_activation_state = config::is_tracy_advanced_profiling_active_for_module(emuenv.cfg.tracy_advanced_profiling_modules, tracy_module_name);
    ZoneNamedN(___tracy_scoped_zone, "sceAudioOutSetPortVolume_forUser", _tracy_activation_state);
    // --- Tracy logging --- END
#endif

    return UNIMPLEMENTED();
}

EXPORT(int, sceAudioOutSetVolume, int port, SceAudioOutChannelFlag ch, int *vol) {
#ifdef TRACY_ENABLE
    // --- Tracy logging --- START
    bool _tracy_activation_state = config::is_tracy_advanced_profiling_active_for_module(emuenv.cfg.tracy_advanced_profiling_modules, tracy_module_name);
    ZoneNamedN(___tracy_scoped_zone, "sceAudioOutSetVolume", _tracy_activation_state);
    tracy_sceAudioOutSetVolume(&___tracy_scoped_zone, _tracy_activation_state, port, ch, vol);
    // --- Tracy logging --- END
#endif

    if (!ch) // no channel selected, no changes
        return 0;

    const AudioOutPortPtr prt = lock_and_find(port, emuenv.audio.shared.out_ports, emuenv.audio.shared.mutex);

    if (!prt)
        return RET_ERROR(SCE_AUDIO_OUT_ERROR_INVALID_PORT);

    // Unsure of what happens if only one channel is selected, this will break if program passes a size 1 int array
    const int left = (ch & SCE_AUDIO_VOLUME_FLAG_L_CH) ? vol[0] : prt->left_channel_volume;
    const int right = (ch & SCE_AUDIO_VOLUME_FLAG_R_CH) ? vol[1] : prt->right_channel_volume;
    const float volume_level = (static_cast<float>(left + right) / static_cast<float>(SCE_AUDIO_VOLUME_0DB * 2));
    const int volume = static_cast<int>(SDL_MIX_MAXVOLUME * volume_level);

    prt->volume = volume;
    // then update channel volumes in case there was a change
    prt->left_channel_volume = left;
    prt->right_channel_volume = right;

    return 0;
}

BRIDGE_IMPL(sceAudioOutGetAdopt)
BRIDGE_IMPL(sceAudioOutGetConfig)
BRIDGE_IMPL(sceAudioOutGetPortVolume_forUser)
BRIDGE_IMPL(sceAudioOutGetRestSample)
BRIDGE_IMPL(sceAudioOutOpenExtPort)
BRIDGE_IMPL(sceAudioOutOpenPort)
BRIDGE_IMPL(sceAudioOutOutput)
BRIDGE_IMPL(sceAudioOutReleasePort)
BRIDGE_IMPL(sceAudioOutSetAdoptMode)
BRIDGE_IMPL(sceAudioOutSetAdopt_forUser)
BRIDGE_IMPL(sceAudioOutSetAlcMode)
BRIDGE_IMPL(sceAudioOutSetCompress)
BRIDGE_IMPL(sceAudioOutSetConfig)
BRIDGE_IMPL(sceAudioOutSetEffectType)
BRIDGE_IMPL(sceAudioOutSetPortVolume_forUser)
BRIDGE_IMPL(sceAudioOutSetVolume)
