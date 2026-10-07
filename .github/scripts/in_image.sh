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
#
# CI is what the checks that must not skip read: the installer suite refuses to
# configure without bats, and its fish cases fail rather than skip. Both were
# written against a variable that stopped at the container wall.
#
# SEN_IN_IMAGE_DOCKER mounts the daemon socket, for the integration suites whose
# driver starts containers of its own. Off unless asked: it hands the container
# the daemon's full authority, which no other caller needs. Those suites also read
# SEN_INTEGRATION_TEST_IMAGE, at configure time and again when they run, so it is
# forwarded for the same reason the compiler is. SEN_GCC_VERSION is what the gcc
# profile reads to pick a compiler, and it falls back to 12 when unset: without it
# a lane asking for a newer gcc builds with 12 and passes.
set -euo pipefail

: "${SEN_CI_IMAGE:?set SEN_CI_IMAGE to the image tag}"
: "${GITHUB_WORKSPACE:?only meaningful inside a job}"

mkdir -p "$HOME/.conan2" "$HOME/.ccache"

# The core limit is lifted for every caller rather than on request: it costs
# nothing where the host's core_pattern discards cores, which is everywhere that
# has not deliberately arranged otherwise.
# A worktree keeps its .git as a file naming a directory outside the checkout, so with only the
# workspace mounted git inside the container reads nothing: the build bakes no commit into the
# binary, `git describe` finds no tag, and the tests that check for either fail for a reason that
# has nothing to do with the code. Mounting the directory it names, at the path it names, is what
# makes a worktree behave like a clone in here.
git_dir_mount=()
if [ -f "$GITHUB_WORKSPACE/.git" ]; then
    worktree_git_dir=$(sed -n 's/^gitdir: *//p' "$GITHUB_WORKSPACE/.git")
    if [ -d "$worktree_git_dir" ]; then
        # The worktree's own directory lives under the common one, so mounting the common one
        # brings both. Read-write because git writes its index while it reads the status.
        common_git_dir=$(cd "$worktree_git_dir" && cd "$(cat commondir 2>/dev/null || echo .)" && pwd)
        git_dir_mount=(--volume "$common_git_dir:$common_git_dir")
    fi
fi

docker_socket=()
if [ -n "${SEN_IN_IMAGE_DOCKER:-}" ]; then
    # The socket is mode 660 and a container process gets no supplementary groups,
    # so mounting it alone hands over a socket the caller can see and cannot open.
    # The group has to be the one the container sees rather than the one the host
    # does: they are the same number on a Linux runner, and Docker Desktop maps the
    # owner, so asking the host answers 1 where the container reads 0.
    socket_group=$(docker run --rm --volume /var/run/docker.sock:/var/run/docker.sock \
        "$SEN_CI_IMAGE" stat -c %g /var/run/docker.sock)
    docker_socket=(
        --volume /var/run/docker.sock:/var/run/docker.sock
        --group-add "$socket_group"
    )
fi

docker run --rm --interactive \
    --user "$(id -u):$(id -g)" \
    ${docker_socket[@]+"${docker_socket[@]}"} \
    ${git_dir_mount[@]+"${git_dir_mount[@]}"} \
    --cap-add=SYS_PTRACE \
    --volume "$GITHUB_WORKSPACE:$GITHUB_WORKSPACE" \
    --volume "$HOME/.conan2:/conan" \
    --volume "$HOME/.ccache:/ccache" \
    --workdir "$GITHUB_WORKSPACE" \
    --env HOME=/tmp \
    --env CONAN_HOME=/conan \
    --env CCACHE_DIR=/ccache \
    --env CC \
    --env CXX \
    --env SEN_INTEGRATION_TEST_IMAGE \
    --env SEN_GCC_VERSION \
    --env SEN_DOC_REF \
    --env SEN_DOC_RUN \
    --env CI \
    --security-opt seccomp=unconfined \
    --ulimit core=-1 \
    "$SEN_CI_IMAGE" \
    bash -seuo pipefail
