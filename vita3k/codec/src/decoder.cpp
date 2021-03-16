#include <codec/state.h>

extern "C" {
#include <libavcodec/avcodec.h>
}

#include <util/log.h>

#include <cassert>

uint32_t DecoderState::get(DecoderQuery query) {
    return 0;
}

void DecoderState::configure(void *options) {
    // do nothing
}

uint32_t DecoderState::get_es_size(const uint8_t *data) {
    return std::numeric_limits<uint32_t>::max();
}

void DecoderState::flush() {
    avcodec_flush_buffers(context);
}

DecoderState::~DecoderState() {
    avcodec_close(context);
    avcodec_free_context(&context);
}

// Handy to have this in logs, some debuggers dont seem to be able to evaluate there error macros properly.
std::string codec_error_name(int error) {
    switch (error) {
    case AVERROR(EAGAIN):
        return "Requires Another Call (AVERROR(EAGAIN))";
    case AVERROR_EOF:
        return "End of File (AVERROR_EOF)";
    case AVERROR(EINVAL):
        return "Invalid Call (AVERROR(EINVAL))";
    case AVERROR(ENOMEM):
        return "Out of Memory (AVERROR(ENOMEM))";
    case AVERROR_INVALIDDATA:
        return "Invalid Data (AVERROR_INVALIDDATA)";
    default:
        return log_hex(static_cast<uint32_t>(error));
    }
}
