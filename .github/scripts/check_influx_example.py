# === check_influx_example.py ==========================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================

"""Checks the influx example's halves agree.

The example spans four files: the Sen configuration, Telegraf's configuration, the container's port
mapping and Grafana's provisioning. Nothing builds them together, so a port changed in one and not
the others leaves an example that starts and records nothing.
"""

from __future__ import annotations

import json
import re
import sys
from pathlib import Path

EXAMPLE = Path("examples/config/8_influx")


def fail(message: str) -> None:
    """Reports one disagreement, named so a reader knows which file to change."""
    print(f"influx example: {message}", file=sys.stderr)


def main() -> int:
    """Returns non-zero when the example's files disagree."""
    problems = 0

    config = (EXAMPLE / "1_influx_school.yaml").read_text(encoding="utf-8")
    telegraf = (EXAMPLE / "docker/config/telegraf.conf").read_text(encoding="utf-8")
    start = (EXAMPLE / "docker/start.sh").read_text(encoding="utf-8")

    sen_port = re.search(r"^\s*port:\s*(\d+)", config, re.M)
    listener = re.search(r'service_address\s*=\s*"tcp://:(\d+)"', telegraf)
    mapping = re.search(rf"-p\s+{sen_port.group(1) if sen_port else 0}:(\d+)", start)

    if not sen_port:
        fail("the Sen configuration names no port")
        problems += 1
    if not listener:
        fail("telegraf.conf has no tcp socket listener")
        problems += 1

    if sen_port and listener:
        if not mapping:
            fail(f"start.sh publishes no host port {sen_port.group(1)}, which the Sen config sends to")
            problems += 1
        elif mapping.group(1) != listener.group(1):
            fail(
                f"start.sh maps {sen_port.group(1)} to {mapping.group(1)}, but telegraf listens on {listener.group(1)}"
            )
            problems += 1

    # The readme tells the reader to switch the listener to udp://, and Docker publishes a port as
    # TCP unless the mapping says otherwise. Without the second mapping the datagrams are dropped at
    # the container boundary, and because UDP reports nothing both sides look healthy.
    if sen_port and not re.search(rf"-p\s+{sen_port.group(1)}:\d+/udp", start):
        fail(f"start.sh publishes {sen_port.group(1)} for TCP only, so a udp:// listener would receive nothing")
        problems += 1

    for dashboard in (EXAMPLE / "docker/config/grafana/dashboards").glob("*.json"):
        try:
            loaded = json.loads(dashboard.read_text(encoding="utf-8"))
        except json.JSONDecodeError as error:
            fail(f"{dashboard} is not valid json: {error}")
            problems += 1
            continue

        if not loaded.get("panels"):
            fail(f"{dashboard} declares no panels, so it would load empty")
            problems += 1

    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main())
