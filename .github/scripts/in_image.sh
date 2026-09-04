#!/usr/bin/env bash
# Runs a script inside the image this repository builds, so that a lane and a
# developer use the same toolchain rather than two kept in agreement by hand.
# The script is read from standard input, which keeps the caller free of the
# quoting a command-line argument would need.
#
# As the caller's own user, because the checkout belongs to them: the image's
# user would leave build output they cannot delete, and root would leave it
# owned by root. That user is uid 1000 and a runner is 1001, so there is no
# passwd entry inside and HOME has to be given -- without it conan gets HOME=/
# and cannot write its cache.
#
# The workspace is mounted at the path it already has. Conan writes absolute
# paths into what it generates, so a build folder belongs to the path that
# configured it.
#
# CONAN_HOME and CCACHE_DIR are redirected for the same reason as HOME: the
# image sets CCACHE_DIR to a directory under its own user, which is mode 750
# and unreadable to anyone else, so a build fails on the first compile.
set -euo pipefail

: "${SEN_CI_IMAGE:?set SEN_CI_IMAGE to the image tag}"
: "${GITHUB_WORKSPACE:?only meaningful inside a job}"

mkdir -p "$HOME/.conan2" "$HOME/.ccache"

docker run --rm --interactive \
    --user "$(id -u):$(id -g)" \
    --volume "$GITHUB_WORKSPACE:$GITHUB_WORKSPACE" \
    --volume "$HOME/.conan2:/conan" \
    --volume "$HOME/.ccache:/ccache" \
    --workdir "$GITHUB_WORKSPACE" \
    --env HOME=/tmp \
    --env CONAN_HOME=/conan \
    --env CCACHE_DIR=/ccache \
    --env CC \
    --env CXX \
    --security-opt seccomp=unconfined \
    "$SEN_CI_IMAGE" \
    bash -seuo pipefail
