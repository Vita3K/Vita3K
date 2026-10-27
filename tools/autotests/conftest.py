"""The command line options of test_autotests.py, checked before any test runs."""

from __future__ import annotations

from pathlib import Path

import pytest

from suite import SUITE, find_tests, load_expectations


def pytest_addoption(parser: pytest.Parser) -> None:
    group = parser.getgroup("vita-autotests")
    group.addoption("--checkout", type=Path, help="VitaAutotests checkout with its vpk built in build/")
    group.addoption("--expectations", type=Path, help="file with the expected outcome of every test, all pass without one")
    group.addoption("--autotest", action="append", default=[], metavar="NAME", help="a test to run, all of them by default, can be given more than once")


def pytest_configure(config: pytest.Config) -> None:
    # Here, so a bad option or expectations file stops the run with its message before any worker starts
    if config.getoption("help"):
        return
    checkout = config.getoption("checkout")
    if checkout is None:
        raise pytest.UsageError("--checkout is needed, the VitaAutotests checkout with its vpk built in build/")
    checkout = checkout.resolve()
    tests = find_tests(checkout)
    if not tests:
        raise pytest.UsageError(f"{checkout} has no tests, no .expected files under tests/")
    # Each test once, since two runs of one test would share its log
    names = list(dict.fromkeys(config.getoption("autotest"))) or sorted(tests)
    unknown = [name for name in names if name not in tests]
    if unknown:
        raise pytest.UsageError(f"not tests: {', '.join(unknown)}")
    path = config.getoption("expectations")
    expectations = load_expectations(path, tests) if path else dict.fromkeys(tests, ("pass", ""))
    config.stash[SUITE] = (checkout, tests, {name: expectations[name] for name in names})
