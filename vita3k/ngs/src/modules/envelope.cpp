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

#include <ngs/modules/envelope.h>
#include <util/log.h>

#include <algorithm>
#include <cmath>
#include <limits>

namespace ngs {

void EnvelopeModule::on_state_change(const MemState &, ModuleData &data, VoiceState previous) {
    if (previous == VOICE_STATE_AVAILABLE && data.parent->state == VOICE_STATE_ACTIVE) {
        *data.get_logical_state<EnvelopeLogicalState>() = EnvelopeLogicalState{};
        *data.get_state<SceNgsEnvelopeStates>() = SceNgsEnvelopeStates{};
        data.get_state<SceNgsEnvelopeStates>()->fReleaseScale = 1.0f;
    }
}

bool EnvelopeModule::finished_triggers_callback(const ModuleData &data) const {
    const auto *state = static_cast<const EnvelopeLogicalState *>(data.logical_state.get());
    return !state || !state->stopped_without_callback;
}

bool EnvelopeModule::process(KernelState &, const MemState &mem, const SceUID, ModuleData &data, std::unique_lock<std::recursive_mutex> &, std::unique_lock<std::mutex> &) {
    const auto *params = data.get_parameters<SceNgsEnvelopeParams>(mem);
    Voice &voice = *data.parent;
    if (!params || params->desc.id != SCE_NGS_ENVELOPE_PARAMS_STRUCT_ID) {
        if (params && params->desc.id != 0)
            LOG_WARN_ONCE("Unsupported envelope parameter descriptor 0x{:X}", params->desc.id);
        return voice.is_keyed_off && !data.is_bypassed;
    }

    const uint32_t count = params->uNumPoints;
    const bool looping = params->nLoopEnd >= 0;
    if (count == 0 || count > SCE_NGS_ENVELOPE_MAX_POINTS
        || (looping && (params->uLoopStart >= static_cast<uint32_t>(params->nLoopEnd) || static_cast<uint32_t>(params->nLoopEnd) >= count))
        || (!looping && params->nLoopEnd < SCE_NGS_ENVELOPE_NO_LOOP_RELEASE)) {
        LOG_WARN_ONCE("Invalid envelope point count or loop configuration");
        return voice.is_keyed_off && !data.is_bypassed;
    }
    for (uint32_t i = 0; i < count; ++i) {
        const auto &point = params->envelopePoints[i];
        if (!std::isfinite(point.fAmplitude) || point.fAmplitude < 0.0f || point.fAmplitude > 100.0f
            || (point.eCurveType != SCE_NGS_ENVELOPE_LINEAR && point.eCurveType != SCE_NGS_ENVELOPE_CURVED)) {
            LOG_WARN_ONCE("Invalid envelope point amplitude or curve");
            return voice.is_keyed_off && !data.is_bypassed;
        }
    }

    const int32_t sample_rate = voice.rack->system->sample_rate;
    const int32_t granularity = voice.rack->system->granularity;
    if (sample_rate <= 0 || granularity <= 0) {
        LOG_ERROR_ONCE("Invalid sample rate or granularity for envelope processing");
        return voice.is_keyed_off && !data.is_bypassed;
    }

    auto &logical = *data.get_logical_state<EnvelopeLogicalState>();
    auto &state = *data.get_state<SceNgsEnvelopeStates>();
    logical.point = std::min(logical.point, count - 1);
    uint64_t loop_duration_ms = 0;
    if (looping) {
        for (uint32_t i = params->uLoopStart; i < static_cast<uint32_t>(params->nLoopEnd); ++i)
            loop_duration_ms += params->envelopePoints[i].uMsecsToNextPoint;
    }
    const uint64_t rate = static_cast<uint64_t>(sample_rate);
    const uint64_t loop_duration = loop_duration_ms > std::numeric_limits<uint64_t>::max() / rate
        ? std::numeric_limits<uint64_t>::max()
        : loop_duration_ms * rate;
    const uint64_t release_duration = uint64_t{ params->uReleaseMsecs } * rate;

    const auto height = [&]() {
        const auto &point = params->envelopePoints[logical.point];
        if (logical.point + 1 >= count || point.uMsecsToNextPoint == 0)
            return point.fAmplitude;
        const uint64_t duration = uint64_t{ point.uMsecsToNextPoint } * rate;
        double position = std::clamp(static_cast<double>(logical.position_units) / duration, 0.0, 1.0);
        if (point.eCurveType == SCE_NGS_ENVELOPE_CURVED) {
            // The SDK specifies a cubic curve but not its coefficients.
            position = position * position * (3.0 - 2.0 * position);
        }
        return static_cast<float>(point.fAmplitude + (params->envelopePoints[logical.point + 1].fAmplitude - point.fAmplitude) * position);
    };
    const auto start_release = [&]() {
        logical.release_height = height();
        logical.release_units = 0;
        logical.releasing = true;
    };
    if (voice.is_keyed_off && !logical.releasing)
        start_release();

    bool finished = false;
    // One sample advances 1000 units; a millisecond lasts sample_rate units.
    const auto advance = [&](uint64_t remaining) {
        while (!finished) {
            if (logical.releasing) {
                logical.release_units = std::min(logical.release_units, release_duration);
                logical.release_units += std::min(remaining, release_duration - logical.release_units);
                finished = logical.release_units >= release_duration;
                return;
            }

            if (looping && logical.point == params->uLoopStart && logical.position_units == 0) {
                if (loop_duration == 0) {
                    LOG_WARN_ONCE("Envelope loop has zero duration");
                    return;
                }
                if (remaining >= loop_duration)
                    remaining %= loop_duration;
            }

            const bool last_point = logical.point + 1 == count;
            if (last_point && params->nLoopEnd == SCE_NGS_ENVELOPE_NO_LOOP)
                return;

            const uint64_t duration = uint64_t{ params->envelopePoints[logical.point].uMsecsToNextPoint } * rate;
            logical.position_units = std::min(logical.position_units, duration);
            const uint64_t step = std::min(remaining, duration - logical.position_units);
            logical.position_units += step;
            remaining -= step;
            if (logical.position_units < duration)
                return;

            // The SDK specifies the bypass clock, but not whether end events fire during bypass.
            if (last_point && data.is_bypassed)
                return;
            logical.position_units = 0;
            if (last_point) {
                if (params->nLoopEnd == SCE_NGS_ENVELOPE_NO_LOOP_TERMINATE) {
                    logical.stopped_without_callback = true;
                    finished = true;
                    return;
                }
                start_release();
                voice.rack->system->voice_scheduler.off(mem, &voice);
            } else {
                ++logical.point;
                if (looping && logical.point == static_cast<uint32_t>(params->nLoopEnd))
                    logical.point = params->uLoopStart;
            }
        }
    };

    auto *samples = reinterpret_cast<float *>(voice.products[0].data);
    advance(0);
    for (int32_t frame = 0; frame < granularity; ++frame) {
        const float release_scale = logical.releasing && release_duration != 0
            ? static_cast<float>(std::max(0.0, 1.0 - static_cast<double>(logical.release_units) / release_duration))
            : (logical.releasing ? 0.0f : 1.0f);
        const float gain = finished ? 0.0f : (logical.releasing ? logical.release_height : height()) * release_scale;
        if (samples && !data.is_bypassed) {
            samples[frame * 2] *= gain;
            samples[frame * 2 + 1] *= gain;
        }
        if (!finished)
            advance(1000);
    }

    state.fCurrentHeight = logical.releasing ? logical.release_height : height();
    const uint64_t duration = uint64_t{ params->envelopePoints[logical.point].uMsecsToNextPoint } * rate;
    // The SDK does not specify the numerical scale of fPosition.
    state.fPosition = duration ? static_cast<float>(std::clamp(static_cast<double>(logical.position_units) / duration, 0.0, 1.0)) : 0.0f;
    state.fReleaseScale = logical.releasing
        ? (release_duration ? static_cast<float>(std::max(0.0, 1.0 - static_cast<double>(logical.release_units) / release_duration)) : 0.0f)
        : 1.0f;
    state.nCurrentPoint = static_cast<SceInt32>(logical.point);
    state.nReleasing = logical.releasing ? 1 : 0;
    return finished && !data.is_bypassed;
}
} // namespace ngs
