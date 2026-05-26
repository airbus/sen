# === run.py ===========================================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================
"""Module that implements the container test harness runner."""

# std
import os
import sys
import time
from pathlib import Path
from threading import Thread

import docker
from testcontainers.core.container import DockerContainer

# testcontainers
from testcontainers.core.network import Network

# constants
# TODO (SEN-1681) replace with a lighter runtime image
IMAGE_NAME = "sim-csr-docker.pforgeipt-docker.intra.airbusds.corp/sen-build-debian12:0.1.0-31-ge5dd492"
TIMEOUT = 5


def stream_logs(cont: DockerContainer) -> None:
    """Formats and streams logs from the containers."""
    w = cont.get_wrapped_container()
    for line in w.logs(stream=True, follow=True):
        if line:
            print(f"[{w.short_id}] {line.decode('utf-8', 'replace').strip()}")


def is_container() -> bool:
    """Checks whether the script is running inside a container."""
    try:
        return os.path.exists("/.dockerenv") or any(
            k in open("/proc/self/cgroup", encoding="utf-8").read() for k in ("docker", "containerd")
        )
    except (FileNotFoundError, PermissionError):
        return False


def get_repo_root(start_path: Path) -> Path:
    """Returns the repo root path from a given path."""
    for p in [start_path] + list(start_path.parents):
        if (p / ".git").exists():
            return p
    raise FileNotFoundError(f"Error: '{start_path}' is not inside a Git repo.")


def find_host_mount(container_path: Path) -> Path | None:
    """Finds the directory in the host where a certain container path is mounted."""
    if not (mount_file := Path("/proc/self/mountinfo")).exists():
        raise FileNotFoundError("Could not access /proc/self/mountinfo. Host might not be a docker")

    ws = os.environ.get("WORKSPACE")

    mounts = list(
        {
            Path(ws if ws else parts[3])
            for line in open(mount_file, encoding="utf-8")
            if str(container_path) in line and (parts := line.split())
        }
    )

    if len(mounts) > 1:
        raise RuntimeError(f"Error: Multiple Sen repository mounts detected! {mounts}.")
    return mounts[0] if mounts else (print("No Sen repository mounts found.") or None)


def check_image_availability(image: str) -> None:
    """Reports an error if the container image is not found in the docker cache."""
    try:
        client = docker.from_env()
        client.images.get(image)
    except docker.errors.ImageNotFound as err:
        raise RuntimeError("Container Image not found in the local cache. Please pull the image first.") from err
    except docker.errors.DockerException as e:
        raise RuntimeError("Could not connect to the local Docker daemon.") from e


def abort(container_list: list[DockerContainer], thread_list: list[Thread]) -> None:
    """Stops containers and joins log threads before aborting with error."""
    for container in container_list:
        container.stop()

    for t in thread_list:
        if isinstance(t, Thread):
            t.join()

    sys.exit(1)


if __name__ == "__main__":
    if len(sys.argv) < 3:
        sys.exit("Usage: python run.py <workdir> <config_yaml> ...")

    cmake_workdir = sys.argv[1]
    configs = sys.argv[2:]

    check_image_availability(IMAGE_NAME)

    # repo root dir in the environment used in cmake (it can be the host machine or a container)
    cmake_repo_root = get_repo_root(Path(__file__).parent)

    # repo root dir in the host machine (if cmake is being configured in a container)
    host_repo_root = find_host_mount(cmake_repo_root) if is_container() else cmake_repo_root

    # repo root dir in the container used to execute the integration tests
    test_repo_root = Path("/home/builder/sen")
    test_workdir = test_repo_root / Path(cmake_workdir).relative_to(cmake_repo_root)

    # container paths
    with Network() as network:
        containers = []
        log_threads = []

        for i, config in enumerate(configs):
            aliases = ["sen-hub"] if i == 0 else []

            # container definition
            container = (
                DockerContainer(IMAGE_NAME)
                .with_network(network)
                .with_network_aliases(*aliases)
                .with_volume_mapping(host_repo_root, "/home/builder/sen", mode="rw")
                .with_command(f"./cli_run {test_repo_root / Path(config).relative_to(cmake_repo_root)}")
                .with_kwargs(working_dir=str(test_workdir), cap_add=["SYS_ADMIN"], security_opt=["seccomp=unconfined"])
            )

            # start the container (with the host environment) and the log thread
            container.env.update(os.environ)
            container.start()
            log_threads.append(Thread(target=stream_logs, args=(container,), daemon=True).start())
            containers.append(container)

        deadline = time.time() + TIMEOUT
        while time.time() < deadline:
            wrapped = [c.get_wrapped_container() for c in containers]
            for w in wrapped:
                w.reload()

            # abort if any of the processes has exited with error
            if any(w.status == "exited" and w.wait()["StatusCode"] != 0 for w in wrapped):
                abort(containers, log_threads)

            # pass the test if all processes have exited successfully
            if all(w.status == "exited" for w in wrapped):
                # join the logger threads
                list(map(Thread.join, [t for t in log_threads if isinstance(t, Thread)]))
                break

            time.sleep(0.2)
        else:
            print(f"\nError: timeout of {TIMEOUT}s reached!")
            abort(containers, log_threads)
