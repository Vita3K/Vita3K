import subprocess
import sys

for repeats in (1, 2, 32):
    for mode in ('plain', 'exception', 'return'):
        try:
            result = subprocess.run([sys.argv[1], str(repeats), mode], capture_output=True, text=True, timeout=5)
        except subprocess.TimeoutExpired as error:
            raise AssertionError(f'{repeats=} {mode=}: terminate handler did not exit within five seconds') from error
        lines = result.stderr.splitlines()
        expected_exit = result.returncode == 77 if mode != 'return' else result.returncode not in (0, 77, 88, 89, 90)
        expected_critical = 'critical: Unhandled C++ exception. {}' if mode == 'exception' else "critical: Unhandled 'std::terminate()' call"
        if (not expected_exit or 'normal-exit' in lines
                or any(lines.count(s) != 1 for s in (expected_critical, 'flush', 'previous'))):
            raise AssertionError(f'{repeats=} {mode=} {result.returncode=}: {result.stderr}')
print('Nine terminate-handler cases passed: repeated init, active exceptions, predecessor and abort fallback')
