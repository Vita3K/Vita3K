#include <module/read_arg.h>
#include <module/vargs.h>

// Workaround for old Clang and GCC
template <>
module::vargs make_vargs(const LayoutArgsState &state) {
    return module::vargs{ state };
}