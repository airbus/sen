# === check_container_capabilities.py ==================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================
"""Reports what the container actually granted, so a missing permission is visible.

Some behaviour only runs when the container was started with it. A configured thread
priority requests a real-time policy, and the cpu idle latency control writes to
/dev/cpu_dma_latency. Without them the product takes its fallback path and logs a warning,
so a lane goes green having exercised the fallback rather than the feature.

Not everyone can grant them. `--device` refuses to start the container at all when the
device is absent, and a workstation under someone else's administration may allow neither.
So the checked-in configuration asks only for what cannot prevent a start, the rest is
opt-in, and this says which of them are in force.

Reporting is the default because a developer who cannot grant these still has to work.
SEN_REQUIRE_CAPABILITIES makes it judge: name the ones the caller believes it asked for and
this exits non-zero if any is missing. A gating lane sets it, so a grant cannot lapse
silently -- which is the only thing that makes the grant worth having.
"""

import argparse
import os
import sys
from pathlib import Path

try:
    import resource
except ImportError:  # Windows has no resource module, and is a supported development host.
    resource = None  # type: ignore[assignment]

CPU_DMA_LATENCY = "/dev/cpu_dma_latency"
YAMA_PTRACE_SCOPE = "/proc/sys/kernel/yama/ptrace_scope"
PROC_SELF_STATUS = "/proc/self/status"
# CAP_SYS_PTRACE is bit 19 of the capability mask /proc/self/status reports as hex.
CAP_SYS_PTRACE_BIT = 19


def real_time_priority() -> tuple[str, str]:
    """Whether an unprivileged process here may ask for a real-time policy.

    RLIMIT_RTPRIO is the ceiling, and it is zero unless the container was started
    with one. CAP_SYS_NICE is the other route and does not work for a non-root
    user: --cap-add sets only the bounding set, so the process never holds it.
    """
    # Fetched rather than named: Windows has no resource module and macOS no
    # RLIMIT_RTPRIO, and both run this suite. Neither is a container, so on neither
    # is the answer a missing grant.
    ceiling = getattr(resource, "RLIMIT_RTPRIO", None)
    if ceiling is None:
        return "unavailable", "not a Linux container"
    try:
        limit = resource.getrlimit(ceiling)[0]
    except (ValueError, OSError):
        return "unavailable", "the real-time priority ceiling could not be read"
    if limit == 0:
        return "absent", "rtprio ceiling is 0; start with --ulimit rtprio=99"
    return "granted", f"rtprio ceiling is {limit}"


def cpu_dma_latency() -> tuple[str, str]:
    """Whether the idle-latency control can be written.

    Absent covers both the device missing and it being unwritable: from here they
    are the same thing, which is that the feature cannot be exercised.
    """
    if not os.path.exists(CPU_DMA_LATENCY):
        return "absent", f"no {CPU_DMA_LATENCY}; start with --device={CPU_DMA_LATENCY}"
    if not os.access(CPU_DMA_LATENCY, os.W_OK):
        return "absent", f"{CPU_DMA_LATENCY} is not writable by this user"
    return "granted", f"{CPU_DMA_LATENCY} is writable"


def yama_ptrace_scope() -> tuple[str, str]:
    """What Yama allows, distinguishing "not loaded" from "loaded and unrestricted".

    Those two permit the same things and are different facts. A container whose kernel
    never loaded Yama has no file at all, and a process there can call
    prctl(PR_SET_PTRACER) and have it fail harmlessly; where Yama is loaded that call is
    what makes tracing work. Reporting the absent case as 0 would erase the difference
    between an environment that cannot restrict tracing and one that has chosen not to.
    """
    try:
        scope = Path(YAMA_PTRACE_SCOPE).read_text(encoding="utf-8").strip()
    except FileNotFoundError:
        return "unavailable", "no Yama LSM here, so nothing restricts tracing"
    except OSError as error:
        return "unavailable", f"{YAMA_PTRACE_SCOPE} could not be read: {error}"

    meaning = {
        "0": "unrestricted",
        "1": "a tracer must be an ancestor, or the tracee must call prctl(PR_SET_PTRACER)",
        "2": "only a process holding CAP_SYS_PTRACE may trace",
        "3": "no tracing at all",
    }.get(scope, "unrecognised value")
    return "granted" if scope == "0" else "restricted", f"ptrace_scope is {scope}: {meaning}"


def sys_ptrace() -> tuple[str, str]:
    """Whether this process actually holds CAP_SYS_PTRACE, not whether it was asked for.

    The effective set is what matters: --cap-add puts a capability in the bounding set,
    and a non-root user never gains it from there.
    """
    try:
        status = Path(PROC_SELF_STATUS).read_text(encoding="utf-8")
    except OSError:
        return "unavailable", "not a Linux container"

    for line in status.splitlines():
        if line.startswith("CapEff:"):
            effective = int(line.split()[1], 16)
            held = bool(effective & (1 << CAP_SYS_PTRACE_BIT))
            return ("granted", "CAP_SYS_PTRACE is held") if held else ("absent", "CAP_SYS_PTRACE is not held")
    return "unavailable", f"{PROC_SELF_STATUS} reported no CapEff line"


CAPABILITIES = {
    "rtprio": real_time_priority,
    "cpu_dma_latency": cpu_dma_latency,
    "yama_ptrace_scope": yama_ptrace_scope,
    "sys_ptrace": sys_ptrace,
}


def required(environ: dict) -> list[str]:
    """The capabilities the caller says it asked the container for.

    An unknown name is kept rather than dropped: a typo that silently required
    nothing would be the failure this exists to prevent.
    """
    raw = environ.get("SEN_REQUIRE_CAPABILITIES", "")
    return [name.strip() for name in raw.replace(",", " ").split() if name.strip()]


def report(environ: dict) -> tuple[str, int]:
    """The state of each capability, and whether anything the caller required is missing."""
    wanted = required(environ)
    lines, missing = [], []

    for name, probe in CAPABILITIES.items():
        state, detail = probe()
        mark = " (required)" if name in wanted else ""
        lines.append(f"  {name:<18} {state:<12} {detail}{mark}")
        if name in wanted and state != "granted":
            missing.append(name)

    for name in wanted:
        if name not in CAPABILITIES:
            lines.append(f"  {name:<18} {'unknown':<12} no such capability, so nothing checked it")
            missing.append(name)

    lines.insert(0, "Container capabilities:")
    if missing:
        lines.append("")
        lines.append(f"Required but not granted: {', '.join(missing)}.")
        lines.append("The tests that need them would pass over their fallback path, so this fails instead.")
    return "\n".join(lines) + "\n", 1 if missing else 0


def main() -> int:
    """Prints the report; exits non-zero only for a required capability that is missing."""
    argparse.ArgumentParser(
        prog="check_container_capabilities",
        description="Reports the container permissions that decide whether some tests exercise the feature.",
    ).parse_args()

    text, status = report(dict(os.environ))
    print(text, end="")
    return status


if __name__ == "__main__":
    sys.exit(main())
