# === test_shell_tests_claim_their_own_port.py =========================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================
"""Checks that the shell integration tests do not share one discovery port.

Six of them run through `shell_runner.py`, four in ls_command and two in log_prompt, and they are
the integration tests that run in the parallel ctest pass. Their configs name port 65454, so two
at once bound it and one saw the other's objects: that is what failed as `shell_ls_open_bus` under
`ctest --parallel 4`, in 0.06 seconds, on run 38096353194. It passed on the leg beside it, because
`--schedule-random` decides whether any two of them overlap. So a green run is not evidence here
and these assertions are.

The runner copies each config with the port replaced rather than editing the file in place. The
first attempt put an `@env` placeholder in the configs instead, which `check-yaml` rightly
refused: `@` cannot start a plain scalar, and the expansion would also have made a typed port
field a string. Hence the parse assertion below, which is the one that would have caught it.

Scope, so it reads as a decision rather than an oversight: other configs still name a port and
should. Those under `apps` and `components/rest/config` are a product's defaults. The kernel
integration groups carry RUN_SERIAL, so nothing runs beside them, and interest_filtering is
labelled flaky, which `-LE flaky` keeps out of the parallel pass. These six had neither.
"""

import importlib.util
import re
from pathlib import Path

import yaml

ROOT = Path(__file__).resolve().parents[2]
SHELL = ROOT / "components" / "shell" / "test" / "integration"
RUNNER = SHELL / "shell_runner.py"
CONFIGS = sorted(SHELL.glob("*/config/*.yaml"))


def runner():
    """The runner module, loaded by path because it does not sit on sys.path."""
    spec = importlib.util.spec_from_file_location("shell_runner_under_test", RUNNER)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def test_there_are_configs_and_a_runner_to_check():
    """Fails if either moves, which would leave everything below passing over nothing."""
    assert CONFIGS, f"no shell integration configs under {SHELL}"
    assert RUNNER.is_file(), f"{RUNNER} is gone"


def test_every_checked_in_config_is_valid_yaml():
    """A placeholder that only the bootloader understands is still a file other tools read."""
    for config in CONFIGS:
        yaml.safe_load(config.read_text(encoding="utf-8"))


def test_the_runner_takes_its_port_from_the_operating_system():
    """Not a number of its own choosing, which would be the same defect one layer up."""
    text = RUNNER.read_text(encoding="utf-8")
    assert re.search(r"probe\.bind\(\(.*0\)\)", text), "the port is not asked of the operating system"
    assert "getsockname" in text, "the chosen port is never read back"


def test_a_rendered_config_carries_that_port_as_a_number():
    """A string here would reach a typed port field, which is how the first attempt failed."""
    module = runner()
    rendered = Path(module.config_on_this_run_s_port(str(CONFIGS[0])))
    loaded = yaml.safe_load(rendered.read_text(encoding="utf-8"))
    ports = sorted(
        set(
            re.findall(r"^\s*(?:port|runDiscoveryHub):\s*(\d+)\s*$", rendered.read_text(encoding="utf-8"), re.MULTILINE)
        )
    )
    assert loaded, "the rendered config is empty"
    assert ports == [str(module.HUB_PORT)], f"rendered ports {ports} are not the chosen {module.HUB_PORT}"


def test_the_port_is_one_per_process_so_paired_tests_meet():
    """log_prompt starts a shell and a second process that have to find the same hub."""
    module = runner()
    rendered = [module.config_on_this_run_s_port(str(c)) for c in CONFIGS]
    found = {
        re.search(r"^\s*(?:port|runDiscoveryHub):\s*(\d+)", Path(r).read_text(encoding="utf-8"), re.MULTILINE).group(1)
        for r in rendered
    }
    assert found == {str(module.HUB_PORT)}, f"configs rendered onto different ports: {found}"


def test_the_runner_refuses_a_config_with_no_port():
    """Silence there would isolate nothing while reading as success."""
    module = runner()
    blank = ROOT / "README.md"
    try:
        module.config_on_this_run_s_port(str(blank))
    except RuntimeError:
        return
    raise AssertionError("a config naming no port was accepted")
