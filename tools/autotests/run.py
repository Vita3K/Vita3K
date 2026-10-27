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

The tests run under pytest, from test_autotests.py next to this script, and with --jobs above 1 under
pytest-xdist, that many at once, each worker on an emulated Vita of its own with the firmware of the
default one linked in if it has one. pytest can also run them directly, with its own options, like
  pytest tools/autotests --checkout ../vita-autotests --expectations tools/autotests/expectations.txt -n 4

VITA3K is the Vita3K binary to run, VITA3K_ARGS adds options to every run, like "-B Vulkan", and
TIMEOUT is the seconds a test may take, 60 by default. Exits with 0 when every test has the outcome
it is expected to have.
"""

from __future__ import annotations

import argparse
import importlib.util
import os
from pathlib import Path
import sys

# No __pycache__ next to the tests, pytest's assertion rewriting included, also from the
# pytest-xdist workers, which are new interpreters
sys.dont_write_bytecode = True
os.environ["PYTHONDONTWRITEBYTECODE"] = "1"

import pytest


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--expectations", type=Path, help="file with the expected outcome of every test")
    parser.add_argument("--jobs", "-j", type=int, default=1, help="how many tests to run at once, 1 by default")
    parser.add_argument("checkout", type=Path, help="VitaAutotests checkout with its vpk built in build/")
    parser.add_argument("names", nargs="*", help="tests to run, all of them by default")
    args = parser.parse_args()

    checkout = args.checkout.resolve()
    here = Path(__file__).resolve().parent
    pytest_args = [
        str(here / "test_autotests.py"),
        # So the test IDs are short, test_autotests.py::test_autotest[<name>]
        f"--rootdir={here}",
        f"--checkout={checkout}",
        # Each test's result as it ends, the failures and unexpected passes at the end, and the slowest tests
        "-v",
        "--tb=short",
        "-rfEX",
        "--durations=20",
        # The cache goes with the logs, not into the Vita3K tree
        "-o",
        f"cache_dir={checkout / 'build' / 'pytest_cache'}",
    ]
    if args.expectations:
        pytest_args.append(f"--expectations={args.expectations.resolve()}")
    pytest_args += [f"--autotest={name}" for name in args.names]
    if args.jobs > 1:
        if importlib.util.find_spec("xdist") is None:
            raise SystemExit("--jobs above 1 needs pytest-xdist")
        # worksteal hands the tests still queued behind a long one to the workers that are idle
        pytest_args += ["-n", str(args.jobs), "--dist", "worksteal"]
    return int(pytest.main(pytest_args))


if __name__ == "__main__":
    sys.exit(main())
