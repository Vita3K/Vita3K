#include <gxm/functions.h>

namespace gxm {
bool is_stream_instancing(SceGxmIndexSource source) {
    return (source == SCE_GXM_INDEX_SOURCE_EACH_INSTANCE_16BIT) || (source == SCE_GXM_INDEX_SOURCE_EACH_INSTANCE_32BIT);
}
} // namespace gxm
