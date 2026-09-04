# === Dockerfile =======================================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================
#
# Build environment for Sen CI and the devcontainer.
# .github/actions/setup_build_context installs the same toolchain directly
# on runners; keep the two in sync.
#
# cmake and ninja are installed at the versions conanfile.py tool-requires. A Conan
# tool_requires reaches the sen package only, so a package built from source in here
# finds neither. Sen's own build still uses Conan's copies: a step calling cmake or
# ninja directly must source the generated conanbuild.sh first.
#
# Two stages. `base` is what CI validates and what the pipeline is described
# against. `dev` adds what an editor needs to recognise a toolchain, and is what
# the devcontainer builds. Build `base` explicitly: a plain `docker build` takes
# the last stage.

# Pinned by digest: the tag moves whenever Canonical republishes it, which made
# two developers building a fortnight apart hold different toolchains. Update it
# deliberately; the scheduled cold build is what notices when it should be.
FROM ubuntu:22.04@sha256:2edbbc5dc405e9612ba3584ce95480277e3eb374407b5505fe26f17df77c7dbc AS base

ARG DEBIAN_FRONTEND=noninteractive
ARG LLVM_VERSION=20
ARG CONAN_VERSION=2.28.1
ARG JUNITPARSER_VERSION=5.0.1
ARG PYTEST_VERSION=9.1.1
ARG PRECOMMIT_VERSION=4.5.1
# Both must equal the versions conanfile.py tool-requires, and nothing checks that
# they do. The header says why the image carries them at all.
ARG CMAKE_VERSION=3.31.10
ARG NINJA_VERSION=1.13.2

LABEL org.opencontainers.image.source="https://github.com/airbus/sen" \
      org.opencontainers.image.description="Sen CI build environment" \
      org.opencontainers.image.licenses="Apache-2.0"

ENV LANG=C.UTF-8 \
    LC_ALL=C.UTF-8 \
    CCACHE_DIR=/home/sen/.ccache

# Apt packages are not version-pinned (waived hadolint DL3008): the Ubuntu
# archive drops superseded versions, so exact pins break within weeks. This
# file's revision identifies the environment; only version-defining tools
# (conan, the LLVM major) are pinned here.
#
# libgl1-mesa-dev and libxext-dev exist for the imgui explorer's GUI
# dependency chain (SDL needs libXext, the explorer needs libGL). make
# serves the autotools-style Conan recipes; python3-dev serves pybind11;
# bats runs the installer test suite.
# Every apt and curl call in this file talks to a mirror that can stall or refuse a
# connection; one such refusal from apt.llvm.org failed an image build. Retries turn
# those into a pause instead of a red pipeline.
RUN printf 'Acquire::Retries "3";\nAcquire::http::Timeout "30";\nAcquire::https::Timeout "30";\n' \
        > /etc/apt/apt.conf.d/99-sen-retries

# apt gives up on the first refusal by default, and a fetch from apt.llvm.org that
# cannot resolve fails the whole image. The action this image replaces on the runners
# carried the same settings for the same reason: the hosts involved do go quiet.
RUN printf '%s\n' \
        'Acquire::Retries "3";' \
        'Acquire::http::Timeout "30";' \
        'Acquire::https::Timeout "30";' \
        'DPkg::Lock::Timeout "120";' \
        > /etc/apt/apt.conf.d/99-sen-timeouts \
    && apt-get update && apt-get install -y --no-install-recommends \
        bats \
        ca-certificates \
        ccache \
        curl \
        g++-12 \
        gcc-12 \
        gcovr \
        git \
        lcov \
        libgl1-mesa-dev \
        libxext-dev \
        make \
        pkg-config \
        python3 \
        python3-dev \
        python3-pip \
        sudo \
    && update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-12 100 \
    && update-alternatives --install /usr/bin/g++ g++ /usr/bin/g++-12 100 \
    && rm -rf /var/lib/apt/lists/*

# clang from apt.llvm.org: the matrix's clang leg and the clang-tidy lane
# both use this major version. The repository is added directly instead of
# running llvm.sh, which needs add-apt-repository.
RUN install -m 0755 -d /etc/apt/keyrings \
    && curl -fsSL --retry 3 --retry-delay 2 --retry-connrefused \
        https://apt.llvm.org/llvm-snapshot.gpg.key -o /etc/apt/keyrings/llvm.asc \
    && echo "deb [signed-by=/etc/apt/keyrings/llvm.asc] https://apt.llvm.org/jammy/ llvm-toolchain-jammy-${LLVM_VERSION} main" \
        > /etc/apt/sources.list.d/llvm.list \
    && apt-get update \
    && apt-get install -y --no-install-recommends \
        clang-${LLVM_VERSION} \
        clang-tidy-${LLVM_VERSION} \
        clang-tools-${LLVM_VERSION} \
        libclang-rt-${LLVM_VERSION}-dev \
        llvm-${LLVM_VERSION} \
    && rm -rf /var/lib/apt/lists/*


# cmake/util/test.cmake invokes junitparser from the run_tests targets; pytest runs
# the python test suites. cmake comes from pip because Ubuntu 22.04 ships 3.22, which
# cannot read the CMakeUserPresets.json Conan writes at version 4.
#
# pre-commit builds each hook its own environment, so the lint and format tools are not
# installed here and their versions keep coming from .pre-commit-config.yaml. Nothing is
# baked at build time, which would reintroduce that drift; the environments are cached in
# a volume instead, because building them clones a repository per hook and GitHub refuses
# a burst of unauthenticated clones.
RUN pip3 install --no-cache-dir \
        cmake==${CMAKE_VERSION} \
        conan==${CONAN_VERSION} \
        junitparser==${JUNITPARSER_VERSION} \
        ninja==${NINJA_VERSION} \
        pre-commit==${PRECOMMIT_VERSION} \
        pytest==${PYTEST_VERSION}

# Work as a normal user: on a Linux host the editor remaps this user's ids to
# the developer's own, so files written into the mounted checkout belong to
# them rather than to root. The conan and ccache directories live in this
# user's home, which is where the devcontainer mounts its volumes. sudo is
# here because that remapping can leave the mounted volumes owned by the
# original ids, and the container then has to correct them itself.
RUN groupadd --gid 1000 sen \
    && useradd --uid 1000 --gid 1000 --create-home --shell /bin/bash sen \
    && mkdir -p /home/sen/.conan2 /home/sen/.ccache /home/sen/.cache/pre-commit \
    && chown -R sen:sen /home/sen \
    && echo "sen ALL=(root) NOPASSWD:ALL" > /etc/sudoers.d/sen \
    && chmod 0440 /etc/sudoers.d/sen

# The name is created two lines up with uid 1000, so it resolves inside the
# image; the devcontainer refers to the same user by name.
# hadolint ignore=DL3066
USER sen

WORKDIR /workspace


# CLion, VS Code and Visual Studio detect a toolchain by looking for cmake, a build
# tool and a debugger inside the environment; base carries the first two, so this
# stage adds the debugger. graphviz is here for the cmake_target_graph target, which
# otherwise warns that 'dot' is missing.
FROM base AS dev

# Names again, for the same reason as in the base stage: this user exists in
# the image, created there with uid 1000.
# hadolint ignore=DL3066
USER root
RUN apt-get update && apt-get install -y --no-install-recommends \
        gdb \
        graphviz \
    && rm -rf /var/lib/apt/lists/*
# hadolint ignore=DL3066
USER sen
