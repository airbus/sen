# === influx_tcp_tester.py =============================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================
"""Drives the influx component against a TCP server this test provides.

The component opens its connection while it loads, which is before any component of its own could
have opened a socket, so the listener cannot live inside the configuration: a module that binds the
port in its own run() is already too late and the kernel ends with "connection refused". Binding
here, before the kernel starts, is what makes the TCP transport reachable at all.
"""

import socket
import struct
import subprocess
import threading
from pathlib import Path

PORT = 18087
CONFIG = Path(__file__).parent / "config" / "record_to_tcp.yaml"


def _recv(connection):
    """One block, with a reset or a timeout read as the end of the connection."""
    try:
        return connection.recv(65536)
    except OSError:
        return b""


def _read_one(connection, received, hang_up_after_first):
    """Reads from one connection, optionally hanging up as soon as something arrives.

    The hang-up is a reset, not an ordinary close. A close sends a FIN, and the component can go
    on writing into a half-closed connection for some time without an error, so the recovery
    being tested is never reached. SO_LINGER with a zero timeout makes the close send RST, which
    fails the very next write.
    """
    with connection:
        connection.settimeout(30)
        while True:
            block = _recv(connection)
            if not block:
                return
            received.append(block)
            if hang_up_after_first:
                connection.setsockopt(socket.SOL_SOCKET, socket.SO_LINGER, struct.pack("ii", 1, 0))
                return


def _collect(listener, first, second):
    """Takes a connection, drops it once data has arrived, and takes the one that replaces it.

    Dropping the connection and not the listener is the point: the component's write fails, its
    reconnect finds the port still open, and the recording goes on. This used to end the whole
    process, because the write rethrew and nothing on the way out caught it.
    """
    try:
        connection, _ = listener.accept()
    except OSError:
        return
    _read_one(connection, first, hang_up_after_first=True)

    try:
        again, _ = listener.accept()
    except OSError:
        return
    _read_one(again, second, hang_up_after_first=False)


def test_the_component_sends_line_protocol_over_tcp():
    """The recorded points reach a TCP server, which is the half of the transport UDP cannot show."""
    first, second = [], []
    listener = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    listener.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    listener.bind(("127.0.0.1", PORT))
    listener.listen(4)

    reader = threading.Thread(target=_collect, args=(listener, first, second), daemon=True)
    reader.start()

    try:
        run = subprocess.run(["./cli_run", str(CONFIG)], capture_output=True, text=True, timeout=120, check=False)
    finally:
        listener.close()

    reader.join(timeout=10)

    assert run.returncode == 0, f"the kernel exited {run.returncode}\n{run.stdout}\n{run.stderr}"

    sent = b"".join(first)
    assert sent, "the component connected but sent nothing over TCP"
    # The object tag, so this fails on an empty frame as well as on none.
    assert b"recorded_object" in sent, f"what arrived does not name the recorded object: {sent[:200]!r}"

    assert second, "the component did not come back after the server dropped its connection"
