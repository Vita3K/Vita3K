#pragma once

#include <mem/ptr.h>

namespace emu {
    typedef void SceGxmDisplayQueueCallback(Ptr<const void> callbackData);

    struct SceGxmInitializeParams {
        uint32_t flags = 0;
        uint32_t displayQueueMaxPendingCount = 0;
        Ptr<SceGxmDisplayQueueCallback> displayQueueCallback;
        uint32_t displayQueueCallbackDataSize = 0;
        uint32_t parameterBufferSize = 0;
    };
}

struct GxmState {
    emu::SceGxmInitializeParams params;
};
