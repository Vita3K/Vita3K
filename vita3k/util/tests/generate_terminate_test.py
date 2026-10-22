from pathlib import Path
import argparse
import re

parser = argparse.ArgumentParser()
parser.add_argument('--source', type=Path, required=True)
parser.add_argument('--output', type=Path, required=True)
args = parser.parse_args()
source = args.source.read_text()


def function(text, signature):
    start = text.index(signature)
    opening = text.index('{', start)
    depth, end = 1, opening + 1
    while depth:
        depth += (text[end] == '{') - (text[end] == '}')
        end += 1
    return text[start:end]


if 'static void terminate_handler()' in source:
    declarations = '\n'.join(re.findall(r'^static .*s_(?:terminate_handler_once|previous_terminate).*;$', source, re.MULTILINE))
    handler = function(source, 'static void terminate_handler()')
    start = source.index('    std::call_once(s_terminate_handler_once')
else:
    declarations, handler = '', ''
    start = source.index('    static std::terminate_handler old_terminate')
end = source.index('\n    });', start) + len('\n    });')
install = source[start:end]
program = r"""
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <mutex>
#include <stdexcept>
#include <string>
static int critical_count, flush_count;
static bool previous_returns;
static void critical(const char *message, ...) {
    std::fprintf(stderr, "critical: %s\n", message);
    if (++critical_count > 1) std::_Exit(88);
}
#define LOG_CRITICAL(...) critical(__VA_ARGS__)
static void flush() {
    ++flush_count;
    std::fputs("flush\n", stderr);
}
static void previous() {
    if (critical_count != 1 || flush_count != 1) std::_Exit(89);
    std::fputs("previous\n", stderr);
    if (!previous_returns) std::_Exit(77);
}
static void normal_exit_marker() {
    std::fputs("normal-exit\n", stderr);
}
""" + declarations + '\n' + handler + '\nstatic void install() {\n' + install + r"""
}
int main(int argc, char **argv) {
    if (argc != 3) return 90;
    const std::string mode = argv[2];
    if (std::atexit(normal_exit_marker) != 0) return 90;
    previous_returns = mode == "return";
    std::set_terminate(previous);
    for (int i = 0; i < std::atoi(argv[1]); ++i) install();
    if (mode == "exception") {
        try { throw std::runtime_error("test exception"); }
        catch (...) { std::terminate(); }
    }
    std::terminate();
}
"""
args.output.write_text(program)
