#include <module/module.h>

#include <unicorn/unicorn.h>

IMP_SIG(exit) {
    const int status = r0;
    (void)status;
    uc_emu_stop(thread->uc.get());

    return 0;
}
