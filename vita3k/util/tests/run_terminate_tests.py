import subprocess
import sys

for repeats in (1, 2, 32):
    for mode in ('plain', 'exception', 'return'):
        result = subprocess.run([sys.argv[1], str(repeats), mode], capture_output=True, text=True)
        lines = result.stderr.splitlines()
        expected_exit = result.returncode == 77 if mode != 'return' else result.returncode not in (0, 77, 88, 89, 90)
        if not expected_exit or any(lines.count(s) != 1 for s in ('critical', 'flush', 'previous')):
            raise AssertionError(f'{repeats=} {mode=} {result.returncode=}: {result.stderr}')
print('Nine terminate-handler cases passed: repeated init, active exceptions, predecessor and abort fallback')
