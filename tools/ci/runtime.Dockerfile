# === runtime.Dockerfile ===============================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================
#
# Runtime image for the container-based integration tests. The test driver
# mounts the workspace with the already-built binaries and runs them here, so
# only the runtime dependencies are needed and BASE must match the OS the
# binaries were built on.
#
# Build:  docker build -f tools/ci/runtime.Dockerfile --build-arg BASE=ubuntu:22.04 -t sen-runtime:ubuntu-22.04 tools/ci
# Use:    cmake -DSEN_INTEGRATION_TEST_IMAGE=sen-runtime:ubuntu-22.04 ...

ARG BASE=ubuntu:22.04
FROM ${BASE}

# libgl1: needed by the SDL/OpenGL dependencies some components link against.
# python3 and its shared library: the py component embeds the interpreter, so
# libpy.so links libpython. The version is read from the base image, because
# each Ubuntu release carries a different one and no unversioned package
# exists.
RUN apt-get update \
    && apt-get install -y --no-install-recommends libgl1 python3 \
    && apt-get install -y --no-install-recommends \
        "libpython$(python3 -c 'import sys; print(f"{sys.version_info.major}.{sys.version_info.minor}")')" \
    && rm -rf /var/lib/apt/lists/*
