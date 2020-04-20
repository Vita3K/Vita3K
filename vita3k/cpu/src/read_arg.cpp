#include <cpu/read_arg.h>
#include <cpu/vargs.h>

// Workaround for old Clang and GCC
template <>
module::vargs make_vargs(const LayoutArgsState &state) {
    return module::vargs{ state };
}
