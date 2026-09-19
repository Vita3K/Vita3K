#pragma once

#include <functional>

namespace renderer::vulkan::crash_isolation {

// Runs the callable on this thread; a SIGSEGV raised on this thread inside it is turned into `false`. Only implemented on Android.
bool run_guarded(const std::function<void()> &callable);

} // namespace renderer::vulkan::crash_isolation
