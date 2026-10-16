"""Runs the VitaAutotests suite on Vita3K headless and checks each test against its expectation.

Each test is launched with the URI psgm:play?titleid=VTST00000&arg=<name>, as another app would
launch it on a Vita, and its output is compared with its .expected file, which holds what a real
Vita prints. The output is the file the test writes every line to in the emulated Vita, since
Vita3K's log drops repeated lines. Logs and outputs go to build/logs/ in the VitaAutotests checkout.

The expectations file gives every test one outcome, one "name outcome" per line with # comments:
  pass     it exits normally and prints its expected output
  fail     it exits normally with other output
  crash    Vita3K exits with an error, like after a host crash or a failed assert
  timeout  it doesn't end within the timeout
  skip     it isn't run, for a test whose outcome on Vita3K depends on how busy the host is
Without an expectations file, every test is expected to pass.

A test whose precondition doesn't hold, one a Vita doesn't promise, ends with test_skip() and prints
only "SKIP: <reason>" and DONE. That run is reported as skipped whatever the test's expectation, which
then says how the test does when it runs.

VITA3K is the Vita3K binary to run, VITA3K_ARGS adds options to every run, like "-B Vulkan", and
TIMEOUT is the seconds a test may take, 60 by default. Exits with 0 when every test has the outcome
it is expected to have.
"""

from __future__ import annotations

import argparse
import difflib
import os
from pathlib import Path
import re
import shlex
import subprocess
import sys

TITLE_ID = "VTST00000"
OUTCOMES = ("pass", "fail", "crash", "timeout", "skip")
OUTPUT_RE = re.compile(rb"\[export_sceClibPrintf\]: (.*)")
# Where in the emulated Vita each test writes its output
OUTPUT_DIR = Path("ux0/data/vita-autotests/output")


def find_tests(checkout: Path) -> dict[str, Path]:
    """Returns the source of every test, which is every .c file next to an .expected file."""
    return {expected.stem: expected.with_suffix(".c") for expected in sorted((checkout / "tests").rglob("*.expected"))}


def load_expectations(path: Path, tests: dict[str, Path]) -> dict[str, str]:
    expectations: dict[str, str] = {}
    for lineno, raw_line in enumerate(path.read_text(encoding="utf-8").splitlines(), start=1):
        line = raw_line.split("#", 1)[0].split()
        if not line:
            continue
        if len(line) != 2 or line[1] not in OUTCOMES:
            raise SystemExit(f"{path}:{lineno}: expected a test name and one of {', '.join(OUTCOMES)}: {raw_line}")
        name, outcome = line
        if name not in tests:
            raise SystemExit(f"{path}:{lineno}: {name} isn't a test")
        expectations[name] = outcome

    missing = sorted(set(tests) - set(expectations))
    if missing:
        raise SystemExit(f"{path} doesn't give these tests an expected outcome: {', '.join(missing)}")
    return expectations


def run_vita3k(vita3k: str, extra_args: list[str], args: list[str], log: Path, timeout: int) -> int | None:
    """Runs Vita3K without a window and returns its exit code, or None if it timed out."""
    env = dict(os.environ, SDL_VIDEO_DRIVER="offscreen", SDL_AUDIO_DRIVER="dummy")
    env.pop("DISPLAY", None)
    env.pop("WAYLAND_DISPLAY", None)
    with log.open("wb") as out:
        try:
            return subprocess.run([vita3k, *extra_args, "--frontend", "sdl", *args], stdout=out, stderr=subprocess.STDOUT, env=env, timeout=timeout).returncode
        except subprocess.TimeoutExpired:
            return None


def output_file(name: str) -> Path:
    """Returns the file the test writes its output to in the emulated Vita Vita3K uses."""
    data = Path(os.environ.get("XDG_DATA_HOME") or Path.home() / ".local" / "share")
    return data / "Vita3K" / "Vita3K" / OUTPUT_DIR / f"{name}.txt"


def guest_output(name: str, log: Path) -> bytes:
    """Returns what the test printed: its output file, or the lines Vita3K logs for each sceClibPrintf if it wrote none."""
    path = output_file(name)
    if path.exists():
        return path.read_bytes()
    lines = (OUTPUT_RE.search(line) for line in log.read_bytes().splitlines())
    return b"".join(match.group(1) + b"\n" for match in lines if match)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--expectations", type=Path, help="file with the expected outcome of every test")
    parser.add_argument("checkout", type=Path, help="VitaAutotests checkout with its vpk built in build/")
    parser.add_argument("names", nargs="*", help="tests to run, all of them by default")
    args = parser.parse_args()

    vita3k = os.environ.get("VITA3K", "Vita3K")
    extra_args = shlex.split(os.environ.get("VITA3K_ARGS", ""))
    timeout = int(os.environ.get("TIMEOUT", "60"))

    checkout = args.checkout.resolve()
    tests = find_tests(checkout)
    unknown = [name for name in args.names if name not in tests]
    if unknown:
        raise SystemExit(f"not tests: {', '.join(unknown)}")
    expectations = load_expectations(args.expectations, tests) if args.expectations else dict.fromkeys(tests, "pass")

    logs = checkout / "build" / "logs"
    logs.mkdir(parents=True, exist_ok=True)
    vpk = checkout / "build" / "tests" / "vita-autotests.vpk"

    # Installing runs the vpk once, with "list" so it only prints the test names
    install_log = logs / "install.log"
    if run_vita3k(vita3k, extra_args, ["--launch-uri", f"psgm:play?titleid={TITLE_ID}&arg=list", str(vpk)], install_log, timeout) != 0:
        print(f"installing {vpk} failed, see {install_log}")
        return 1

    counts = {"passed": 0, "failed as expected": 0, "skipped": 0, "unexpected": 0}
    for name in args.names or sorted(tests):
        if expectations[name] == "skip":
            print(f"SKIP {name}")
            counts["skipped"] += 1
            continue
        log = logs / f"{name}.log"
        # So a run that never starts the test can't pass with an older output
        output_file(name).unlink(missing_ok=True)
        code = run_vita3k(vita3k, extra_args, ["-r", TITLE_ID, "--launch-uri", f"psgm:play?titleid={TITLE_ID}&arg={name}"], log, timeout)
        output = guest_output(name, log)
        (logs / f"{name}.output").write_bytes(output)
        expected_output = tests[name].with_suffix(".expected").read_bytes()

        lines = output.decode("latin-1").splitlines()
        if code == 0 and len(lines) == 2 and lines[0].startswith("SKIP: ") and lines[1] == "DONE":
            print(f"SKIP {name} ({lines[0][len('SKIP: '):]})")
            counts["skipped"] += 1
            continue

        if code is None:
            outcome = "timeout"
        elif code != 0:
            outcome = "crash"
        else:
            outcome = "pass" if output == expected_output else "fail"
        expected = expectations[name]
        how = {"timeout": " (timed out)", "crash": f" (exit {code})"}.get(outcome, "")

        if outcome == expected == "pass":
            print(f"PASS {name}")
            counts["passed"] += 1
        elif outcome == expected:
            print(f"XFAIL {name}{how}")
            counts["failed as expected"] += 1
        else:
            print(f"UNEXPECTED {name}: expected {expected}, got {outcome}{how}")
            counts["unexpected"] += 1
            if outcome == "pass":
                print("    it passes, change its expectation to pass")
            else:
                diff = difflib.unified_diff(expected_output.decode("latin-1").splitlines(), output.decode("latin-1").splitlines(), "expected", "output", lineterm="")
                for line in list(diff)[2:]:
                    if line.startswith(("-", "+")):
                        print(f"    {line}")
            print(f"    log: {log}")

    print(", ".join(f"{count} {what}" for what, count in counts.items()))
    return 1 if counts["unexpected"] else 0


if __name__ == "__main__":
    sys.exit(main())
