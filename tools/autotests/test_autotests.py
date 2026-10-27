"""The VitaAutotests suite on Vita3K, one pytest test per VitaAutotests test. run.py runs it.

A test whose expected outcome is fail, crash or timeout is a strict xfail that only that outcome
satisfies, so another outcome or a pass is a failure. A skip isn't run. With pytest-xdist each worker
runs its tests on an emulated Vita of its own.
"""

from __future__ import annotations

import difflib
import os
from pathlib import Path
import re
import shlex
import subprocess

import pytest

from suite import SUITE

TITLE_ID = "VTST00000"
OUTPUT_RE = re.compile(rb"\[export_sceClibPrintf\]: (.*)")
# Where in the emulated Vita each test writes its output
OUTPUT_DIR = Path("ux0/data/vita-autotests/output")
# What the emulated Vita of a worker links from the default one, so every worker has the same firmware
FIRMWARE_PARTITIONS = ("os0", "vs0", "sa0", "pd0")

VITA3K = os.environ.get("VITA3K", "Vita3K")
VITA3K_ARGS = shlex.split(os.environ.get("VITA3K_ARGS", ""))
TIMEOUT = int(os.environ.get("TIMEOUT", "60"))


class OutputDiffers(Exception):
    """The test exited normally with other output than its .expected file."""


class Vita3KCrashed(Exception):
    """Vita3K exited with an error."""


class TimedOut(Exception):
    """The test didn't end within the timeout."""


OUTCOME_ERRORS = {"fail": OutputDiffers, "crash": Vita3KCrashed, "timeout": TimedOut}


def pytest_generate_tests(metafunc: pytest.Metafunc) -> None:
    if "autotest" not in metafunc.fixturenames:
        return
    _, _, expectations = metafunc.config.stash[SUITE]

    params = []
    for name, (outcome, comment) in expectations.items():
        reason = f"expected {outcome}" + (f", {comment}" if comment else "")
        if outcome == "skip":
            marks = [pytest.mark.skip(reason=reason)]
        elif outcome in OUTCOME_ERRORS:
            marks = [pytest.mark.xfail(strict=True, raises=OUTCOME_ERRORS[outcome], reason=reason)]
        else:
            marks = []
        params.append(pytest.param(name, marks=marks, id=name))
    metafunc.parametrize("autotest", params)


def default_data_home() -> Path:
    """Returns the XDG data dir of the default emulated Vita, the one of the environment."""
    return Path(os.environ.get("XDG_DATA_HOME") or Path.home() / ".local" / "share")


def new_emulated_vita(root: Path) -> dict[str, str]:
    """Returns the XDG dirs of a new emulated Vita under root, with the default one's firmware linked in."""
    dirs = {"XDG_CONFIG_HOME": root / "config", "XDG_DATA_HOME": root / "data", "XDG_CACHE_HOME": root / "cache"}
    vita = dirs["XDG_DATA_HOME"] / "Vita3K" / "Vita3K"
    vita.mkdir(parents=True)
    for partition in FIRMWARE_PARTITIONS:
        source = default_data_home() / "Vita3K" / "Vita3K" / partition
        if source.exists():
            (vita / partition).symlink_to(source.resolve())
    return {name: str(path) for name, path in dirs.items()}


def run_vita3k(args: list[str], log: Path, vita: dict[str, str]) -> int | None:
    """Runs Vita3K without a window on the emulated Vita of the XDG dirs vita, the default one if it is
    empty, and returns its exit code, or None if it timed out."""
    env = dict(os.environ, SDL_VIDEO_DRIVER="offscreen", SDL_AUDIO_DRIVER="dummy", **vita)
    env.pop("DISPLAY", None)
    env.pop("WAYLAND_DISPLAY", None)
    with log.open("wb") as out:
        try:
            return subprocess.run([VITA3K, *VITA3K_ARGS, "--frontend", "sdl", *args], stdout=out, stderr=subprocess.STDOUT, env=env, timeout=TIMEOUT).returncode
        except subprocess.TimeoutExpired:
            return None


def output_file(name: str, vita: dict[str, str]) -> Path:
    """Returns the file the test writes its output to in the emulated Vita of vita."""
    data = Path(vita["XDG_DATA_HOME"]) if vita else default_data_home()
    return data / "Vita3K" / "Vita3K" / OUTPUT_DIR / f"{name}.txt"


def guest_output(name: str, vita: dict[str, str], log: Path) -> bytes:
    """Returns what the test printed: its output file, or the lines Vita3K logs for each sceClibPrintf if it wrote none."""
    path = output_file(name, vita)
    if path.exists():
        return path.read_bytes()
    # Line by line, since a test that runs away can log gigabytes
    with log.open("rb") as lines:
        matches = (OUTPUT_RE.search(line) for line in lines)
        return b"".join(match.group(1) + b"\n" for match in matches if match)


@pytest.fixture(scope="session")
def checkout(pytestconfig: pytest.Config) -> Path:
    """The VitaAutotests checkout."""
    return pytestconfig.stash[SUITE][0]


@pytest.fixture(scope="session")
def tests(pytestconfig: pytest.Config) -> dict[str, Path]:
    """The source of every test in the checkout."""
    return pytestconfig.stash[SUITE][1]


@pytest.fixture(scope="session")
def logs(checkout: Path) -> Path:
    """The directory the logs and outputs go to, build/logs/ in the VitaAutotests checkout."""
    path = checkout / "build" / "logs"
    path.mkdir(parents=True, exist_ok=True)
    return path


@pytest.fixture(scope="session")
def vita(checkout: Path, logs: Path, tmp_path_factory: pytest.TempPathFactory) -> dict[str, str]:
    """The emulated Vita this process runs its tests on, with the vpk installed: the default one, or one
    of its own for a pytest-xdist worker."""
    worker = os.environ.get("PYTEST_XDIST_WORKER")
    vita = new_emulated_vita(tmp_path_factory.mktemp("vita")) if worker else {}
    vpk = checkout / "build" / "tests" / "vita-autotests.vpk"
    install_log = logs / (f"install-{worker}.log" if worker else "install.log")
    # Installing runs the vpk once, with "list" so it only prints the test names
    if run_vita3k(["--launch-uri", f"psgm:play?titleid={TITLE_ID}&arg=list", str(vpk)], install_log, vita) != 0:
        raise RuntimeError(f"installing {vpk} failed, see {install_log}")
    return vita


def test_autotest(autotest: str, vita: dict[str, str], logs: Path, tests: dict[str, Path]) -> None:
    __tracebackhide__ = True
    name = autotest
    log = logs / f"{name}.log"
    # So a run that never starts the test can't pass with an older output
    output_file(name, vita).unlink(missing_ok=True)
    code = run_vita3k(["-r", TITLE_ID, "--launch-uri", f"psgm:play?titleid={TITLE_ID}&arg={name}"], log, vita)
    output = guest_output(name, vita, log)
    (logs / f"{name}.output").write_bytes(output)

    # A test whose precondition doesn't hold, one a Vita doesn't promise, ends with test_skip() and
    # prints only "SKIP: <reason>" and DONE, whatever its expectation
    lines = output.decode("latin-1").splitlines()
    if code == 0 and len(lines) == 2 and lines[0].startswith("SKIP: ") and lines[1] == "DONE":
        pytest.skip(lines[0][len("SKIP: "):])

    if code is None:
        raise TimedOut(f"no end within {TIMEOUT} s, log: {log}")
    if code != 0:
        raise Vita3KCrashed(f"exit {code}, log: {log}")
    expected_output = tests[name].with_suffix(".expected").read_bytes()
    if output != expected_output:
        diff = difflib.unified_diff(expected_output.decode("latin-1").splitlines(), lines, "expected", "output", lineterm="")
        changed = "\n".join(line for line in list(diff)[2:] if line.startswith(("-", "+")))
        raise OutputDiffers(f"the output differs from the .expected file, log: {log}\n{changed}")
