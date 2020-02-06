#pragma once

#include <mem/ptr.h>
#include <threads/queue.h>

typedef void SceGxmDisplayQueueCallback(Ptr<const void> callbackData);

struct SceGxmInitializeParams {
    uint32_t flags = 0;
    uint32_t displayQueueMaxPendingCount = 0;
    Ptr<SceGxmDisplayQueueCallback> displayQueueCallback;
    uint32_t displayQueueCallbackDataSize = 0;
    uint32_t parameterBufferSize = 0;
};

struct DisplayCallback {
    Address pc;
    Address data;
    Address old_buffer;
    Address new_buffer;
};

struct GxmState {
    SceGxmInitializeParams params;
    bool is_in_scene = false;
    Queue<DisplayCallback> display_queue;
    Ptr<uint32_t> notification_region;
    SceUID display_queue_thread;
};
