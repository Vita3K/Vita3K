from pathlib import Path
import argparse
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[3]
parser = argparse.ArgumentParser()
parser.add_argument('--cxx', default='c++')
parser.add_argument('--sanitize', action='store_true')
args = parser.parse_args()
source = (ROOT / 'vita3k/renderer/src/state_set.cpp').read_text()
start = source.index('COMMAND_SET_STATE(region_clip) {')
opening = source.index('{', start)
depth, end = 1, opening + 1
while depth:
    depth += (source[end] == '{') - (source[end] == '}')
    end += 1
region = source[start:end]
scene = (ROOT / 'vita3k/renderer/src/vulkan/scene.cpp').read_text()
start = scene.index('{', scene.index('void draw(')) + 1
prefix = scene[start:scene.index('    void *indices_ptr', start)]
program = r"""
#include <array>
#include <cassert>
#include <cstdint>
#include <cstdio>
using SceInt = int32_t;
using SceGxmRegionClipMode = uint32_t;
constexpr uint32_t SCE_GXM_TILE_SIZEX = 32, SCE_GXM_TILE_SIZEY = 32;
enum class Backend { OpenGL, Vulkan };
struct State { Backend current_backend; struct { bool use_mask_bit; } features; };
struct SceGxmFragmentProgram { bool is_maskupdate; };
struct ProgramPtr {
    SceGxmFragmentProgram *value;
    const SceGxmFragmentProgram *get(int &) const { return value; }
};
struct Record {
    SceGxmRegionClipMode region_clip_mode;
    struct { bool downscale; } color_surface;
    bool is_maskupdate;
    struct { int x, y; } region_clip_min, region_clip_max;
    ProgramPtr fragment_program;
};
struct Target { bool multisample_mode; };
struct Context { Record record; Target *current_render_target; State state; unsigned work = 0; };
struct Helper {
    std::array<uint32_t, 5> values;
    unsigned index = 0;
    template<class T> T pop() { return static_cast<T>(values[index++]); }
};
static unsigned sync_calls;
namespace gl { using GLState = State; using GLContext = Context; void sync_clipping(State &, Context &) { ++sync_calls; } }
namespace vulkan { using VKContext = Context; void sync_clipping(Context &) { ++sync_calls; } }
static uint32_t align_down(uint32_t value, uint32_t alignment) { return value & ~(alignment - 1); }
static uint32_t align(uint32_t value, uint32_t alignment) { return (value + alignment - 1) & ~(alignment - 1); }
#define TRACY_FUNC_COMMANDS_SET_STATE(...) ((void)0)
#define REPORT_MISSING(...) assert(false)
#define COMMAND_SET_STATE(name) void name(State &renderer, Context *render_context, Helper &helper)
""" + region + r"""
static void draw_prefix(Context &context, int &mem) {
""" + prefix + r"""
    ++context.work;
}
int main() {
    unsigned cases = 0;
    for (Backend backend : {Backend::OpenGL, Backend::Vulkan})
        for (bool mask : {false, true})
            for (bool msaa : {false, true})
                for (bool downscale : {false, true}) {
                    State state{backend};
                    Target target{msaa};
                    Context context{};
                    context.current_render_target = &target;
                    context.record.is_maskupdate = mask;
                    context.record.color_surface.downscale = downscale;
                    Helper helper{{0, 13, 23, 19, 27}};
                    sync_calls = 0;
                    region_clip(state, &context, helper);
                    assert(helper.index == 5 && sync_calls == 1);
                    const int factor = msaa && !downscale ? 2 : 1;
                    if (backend == Backend::Vulkan && mask) {
                        assert(context.record.region_clip_min.x == 13 * factor);
                        assert(context.record.region_clip_max.x == 23 * factor);
                        assert(context.record.region_clip_min.y == 19 * factor);
                        assert(context.record.region_clip_max.y == 27 * factor);
                    } else {
                        assert(context.record.region_clip_min.x == (13 * factor / 32) * 32);
                        assert(context.record.region_clip_max.x == ((23 * factor + 31) / 32) * 32 - 1);
                        assert(context.record.region_clip_min.y == (19 * factor / 32) * 32);
                        assert(context.record.region_clip_max.y == ((27 * factor + 31) / 32) * 32 - 1);
                    }
                    ++cases;
                }
    for (bool supported : {false, true})
        for (bool mask : {false, true}) {
            Context context{};
            SceGxmFragmentProgram fragment{mask};
            context.record.fragment_program = {&fragment};
            context.state.features.use_mask_bit = supported;
            int mem = 0;
            draw_prefix(context, mem);
            assert(context.work == (mask && !supported ? 0u : 1u));
            ++cases;
        }
    std::printf("%u clipping/early-mask-draw cases passed\n", cases);
}
"""
with tempfile.TemporaryDirectory(prefix='vita3k-mask-update-') as folder:
    temp = Path(folder)
    (temp / 'test.cpp').write_text(program)
    flags = ['-std=c++23', '-O1', '-g']
    if args.sanitize:
        flags.append('-fsanitize=address,undefined')
    subprocess.run([args.cxx, *flags, str(temp / 'test.cpp'), '-o', str(temp / 'test')], check=True)
    subprocess.run([str(temp / 'test')], check=True)
