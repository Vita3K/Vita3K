"""The VitaAutotests suite: its tests, and their expected outcomes on Vita3K."""

from __future__ import annotations

from pathlib import Path

import pytest

OUTCOMES = ("pass", "fail", "crash", "timeout", "skip")
# The checkout, every test's source, and the tests to run with their expected outcome and its comment
SUITE = pytest.StashKey[tuple[Path, dict[str, Path], dict[str, tuple[str, str]]]]()


def find_tests(checkout: Path) -> dict[str, Path]:
    """Returns the source of every test, which is every .c file next to an .expected file."""
    return {expected.stem: expected.with_suffix(".c") for expected in sorted((checkout / "tests").rglob("*.expected"))}


def load_expectations(path: Path, tests: dict[str, Path]) -> dict[str, tuple[str, str]]:
    """Returns every test's expected outcome and the comment that says why."""
    expectations: dict[str, tuple[str, str]] = {}
    for lineno, raw_line in enumerate(path.read_text(encoding="utf-8").splitlines(), start=1):
        text, _, comment = raw_line.partition("#")
        line = text.split()
        if not line:
            continue
        if len(line) != 2 or line[1] not in OUTCOMES:
            raise pytest.UsageError(f"{path}:{lineno}: expected a test name and one of {', '.join(OUTCOMES)}: {raw_line}")
        name, outcome = line
        if name not in tests:
            raise pytest.UsageError(f"{path}:{lineno}: {name} isn't a test")
        expectations[name] = (outcome, comment.strip())

    missing = sorted(set(tests) - set(expectations))
    if missing:
        raise pytest.UsageError(f"{path} doesn't give these tests an expected outcome: {', '.join(missing)}")
    return expectations
