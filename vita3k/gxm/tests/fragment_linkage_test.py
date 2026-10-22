from pathlib import Path
import argparse
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[3]
parser = argparse.ArgumentParser()
parser.add_argument('--cxx', default='c++')
parser.add_argument('--dependencies', type=Path, default=ROOT / 'external')
parser.add_argument('--openssl-include', type=Path)
parser.add_argument('--crypto-library', default='-lcrypto')
parser.add_argument('--sanitize', action='store_true')
args = parser.parse_args()


def function(text, signature):
    start = text.index(signature)
    opening = text.index('{', start)
    depth = 1
    end = opening + 1
    while depth:
        depth += (text[end] == '{') - (text[end] == '}')
        end += 1
    return text[start:end]


mapping = function((ROOT / 'vita3k/gxm/src/gxp.cpp').read_text(),
                   'GxmTexCoordLinkMap get_texcoord_link_map(')
hash_function = function((ROOT / 'vita3k/renderer/src/creation.cpp').read_text(),
                         'static Sha256Hash get_fragment_program_hash(')
types = (ROOT / 'vita3k/renderer/include/renderer/types.h').read_text()
link_type = function(types, 'struct FragmentProgramLinkInfo') + ';\n'
program = r"""
#include <gxm/types.h>
#include <util/hash.h>
#include <array>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <set>
#include <vector>
namespace gxp {
using GxmTexCoordLinkMap = std::array<uint8_t, 10>;
static uint32_t output_mask;
static SceGxmVertexProgramOutputs get_vertex_outputs(const SceGxmProgram &) {
    return static_cast<SceGxmVertexProgramOutputs>(output_mask);
}
""" + mapping + '\n}\n' + link_type + hash_function + r"""
int main() {
    SceGxmProgram program{};
    program.size = sizeof(program);
    const auto identity = gxp::get_texcoord_link_map(nullptr);
    for (unsigned i = 0; i < 10; ++i) assert(identity[i] == i);
    for (uint32_t mask = 0; mask < 1024; ++mask) {
        gxp::output_mask = mask * SCE_GXM_VERTEX_PROGRAM_OUTPUT_TEXCOORD0;
        const auto map = gxp::get_texcoord_link_map(&program);
        uint8_t packed = 0;
        for (uint8_t semantic = 0; semantic < 10; ++semantic) {
            const auto expected = (mask & (1u << semantic)) ? packed++ : 9;
            assert(map[semantic] == expected);
        }
    }
    FragmentProgramLinkInfo link;
    const auto initial = get_fragment_program_hash(program, link);
    assert(initial == get_fragment_program_hash(program, link));
    std::set<Sha256Hash> hashes{initial};
    for (unsigned i = 0; i < 10; ++i) {
        auto changed = link;
        changed.texcoord_map[i] ^= 1;
        assert(hashes.insert(get_fragment_program_hash(program, changed)).second);
    }
    auto output = link;
    output.output_format = static_cast<SceGxmOutputRegisterFormat>(
        static_cast<uint32_t>(link.output_format) ^ 1u);
    assert(hashes.insert(get_fragment_program_hash(program, output)).second);
    auto samples = link;
    samples.multisample_mode = static_cast<SceGxmMultisampleMode>(
        static_cast<uint32_t>(link.multisample_mode) ^ 1u);
    assert(hashes.insert(get_fragment_program_hash(program, samples)).second);
    auto changed_program = program;
    changed_program.size--;
    assert(hashes.insert(get_fragment_program_hash(changed_program, link)).second);
    std::puts("1024 texcoord masks, null identity and 14 shader-cache identities passed");
}
"""
with tempfile.TemporaryDirectory(prefix='vita3k-fragment-linkage-') as folder:
    temp = Path(folder)
    (temp / 'test.cpp').write_text(program)
    flags = ['-std=c++23', '-O1', '-g', '-DSPDLOG_FMT_EXTERNAL']
    if args.sanitize:
        flags.append('-fsanitize=address,undefined')
    includes = ['-I' + str(p) for p in (ROOT / 'vita3k').glob('*/include')]
    includes += ['-I' + str(args.dependencies / p) for p in ('boost', 'fmt/include', 'spdlog/include')]
    if args.openssl_include:
        includes.append('-I' + str(args.openssl_include))
    command = [args.cxx, *flags, *includes, str(temp / 'test.cpp'),
               str(ROOT / 'vita3k/util/src/hash.cpp'), args.crypto_library,
               '-o', str(temp / 'test')]
    subprocess.run(command, check=True)
    subprocess.run([str(temp / 'test')], check=True)
