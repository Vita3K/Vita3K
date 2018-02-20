#pragma once

#include <mem/ptr.h>

namespace emu {
    typedef void SceGxmDisplayQueueCallback(Ptr<const void> callbackData);

    struct SceGxmInitializeParams {
        u32 flags = 0;
        u32 displayQueueMaxPendingCount = 0;
        Ptr<SceGxmDisplayQueueCallback> displayQueueCallback;
        u32 displayQueueCallbackDataSize = 0;
        u32 parameterBufferSize = 0;
    };
}

struct GxmState {
    emu::SceGxmInitializeParams params;
    bool isInScene = false;
};
