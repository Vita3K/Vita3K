#include <renderer/vulkan/crash_isolation.h>

#ifdef __ANDROID__
#include <csetjmp>
#include <csignal>
#include <mutex>
#endif

namespace renderer::vulkan::crash_isolation {

#ifdef __ANDROID__
namespace {

thread_local sigjmp_buf *active_jump_target = nullptr;
struct sigaction previous_segv_action {};
std::once_flag handler_installed;

void forward_to_previous_handler(int signal_number, siginfo_t *info, void *context) {
    if (previous_segv_action.sa_flags & SA_SIGINFO) {
        previous_segv_action.sa_sigaction(signal_number, info, context);
    } else if (previous_segv_action.sa_handler != SIG_DFL && previous_segv_action.sa_handler != SIG_IGN) {
        previous_segv_action.sa_handler(signal_number);
    } else {
        signal(signal_number, SIG_DFL);
        raise(signal_number);
    }
}

void guard_handler(int signal_number, siginfo_t *info, void *context) {
    if (active_jump_target)
        siglongjmp(*active_jump_target, 1);
    forward_to_previous_handler(signal_number, info, context);
}

void install_guard_handler() {
    struct sigaction action {};
    action.sa_sigaction = guard_handler;
    action.sa_flags = SA_SIGINFO | SA_ONSTACK | SA_NODEFER;
    sigemptyset(&action.sa_mask);
    sigaction(SIGSEGV, &action, &previous_segv_action);
}

} // namespace

bool run_guarded(const std::function<void()> &callable) {
    std::call_once(handler_installed, install_guard_handler);
    sigjmp_buf jump_target;
    struct TargetReset {
        ~TargetReset() { active_jump_target = nullptr; }
    } reset_on_exit;
    if (sigsetjmp(jump_target, 1) == 0) {
        active_jump_target = &jump_target;
        callable();
        return true;
    }
    return false;
}

#else

bool run_guarded(const std::function<void()> &callable) {
    callable();
    return true;
}

#endif

} // namespace renderer::vulkan::crash_isolation
