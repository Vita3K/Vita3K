#include <codec/state.h>

extern "C" {
#include <libavcodec/avcodec.h>
}

#include <cassert>

uint32_t DecoderState::get(DecoderQuery query) {
    return 0;
}

void DecoderState::configure(void *options) {
    // do nothing
}

void DecoderState::flush() {
    avcodec_flush_buffers(context);
}

DecoderState::~DecoderState() {
    avcodec_close(context);
    avcodec_free_context(&context);
}
