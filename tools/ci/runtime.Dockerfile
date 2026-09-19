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
# binutils: for addr2line, which the sanitizers are pointed at here. They look for
# llvm-symbolizer by name on PATH, and without a symbolizer they print every frame as
# <null> and refuse to fall back. binutils takes the image from 406 to 430 MB;
# llvm-symbolizer needs libLLVM, whose shared-library closure alone is 182 MB.
# python3 and its shared library: the py component embeds the interpreter, so
# libpy.so links libpython. The version is read from the base image, because
# each Ubuntu release carries a different one and no unversioned package
# exists.
# apt bounds, as in Dockerfile: an archive that answers with a 520 or goes quiet
# otherwise fails the whole build on the first refusal.
RUN printf '%s\n' \
        'Acquire::Retries "3";' \
        'Acquire::http::Timeout "30";' \
        'Acquire::https::Timeout "30";' \
        'DPkg::Lock::Timeout "120";' \
        > /etc/apt/apt.conf.d/99-sen-timeouts \
    && apt-get update \
    && apt-get install -y --no-install-recommends libgl1 python3 binutils \
    && apt-get install -y --no-install-recommends \
        "libpython$(python3 -c 'import sys; print(f"{sys.version_info.major}.{sys.version_info.minor}")')" \
    && rm -rf /var/lib/apt/lists/*
