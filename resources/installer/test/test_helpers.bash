# === test_helpers.bash  ===============================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================
#
# Sourced by every *.bats file in this directory.

INSTALLER_DIR=$(cd "$BATS_TEST_DIRNAME/.." && pwd)
FIXTURES_DIR="$BATS_TEST_DIRNAME/fixtures"

# Fresh SEN_INSTALL_HOME per test. SEN_HOST_ARCH=x86_64 makes the fixture JSONs (x86_64-only) match on aarch64 dev hosts;
# SEN_INSTALLER_NO_SELF_COPY skips the post-install network refetch of install.sh.
setup() {
    SEN_INSTALL_HOME=$(mktemp -d)
    export SEN_INSTALL_HOME
    # Scratch space for staging directories and mock payloads, kept outside the install prefix so that an
    # end-to-end install does not find them. We make our own instead of using BATS_TEST_TMPDIR: bats 1.2,
    # which Ubuntu 22.04 ships, does not set that variable, and the tests then write to the root directory.
    SEN_TEST_TMPDIR=$(mktemp -d)
    export SEN_TEST_TMPDIR
    # fish keeps session state under XDG_RUNTIME_DIR; with none set it falls back to /tmp/fish.$USER,
    # which a container user with no passwd entry cannot name, and writes "Runtime path not available"
    # into whatever the test captured. One set per test, so two cases cannot share it either.
    XDG_RUNTIME_DIR="$SEN_TEST_TMPDIR/run"
    XDG_CONFIG_HOME="$SEN_TEST_TMPDIR/config"
    XDG_DATA_HOME="$SEN_TEST_TMPDIR/data"
    XDG_CACHE_HOME="$SEN_TEST_TMPDIR/cache"
    mkdir -p "$XDG_RUNTIME_DIR" "$XDG_CONFIG_HOME" "$XDG_DATA_HOME" "$XDG_CACHE_HOME"
    chmod 700 "$XDG_RUNTIME_DIR"
    export XDG_RUNTIME_DIR XDG_CONFIG_HOME XDG_DATA_HOME XDG_CACHE_HOME
    export SEN_HOST_ARCH=x86_64
    export SEN_INSTALLER_NO_SELF_COPY=1
}

teardown() {
    if [ -n "${SEN_INSTALL_HOME:-}" ] && [ -d "$SEN_INSTALL_HOME" ]; then
        rm -rf "$SEN_INSTALL_HOME"
    fi
    if [ -n "${SEN_TEST_TMPDIR:-}" ] && [ -d "$SEN_TEST_TMPDIR" ]; then
        rm -rf "$SEN_TEST_TMPDIR"
    fi
}

# Source install.sh with main() suppressed so individual functions can be called. set -u stays off (bats probes
# unset vars internally); set -e stays on so each `[ ... ]` / `[[ ... ]]` is a hard assertion. `! cmd` is exempt
# from set -e per POSIX; test bodies asserting "must NOT succeed" use `cmd && return 1` instead.
load_install() {
    SENV_INSTALLER_NORUN=1
    # shellcheck disable=SC1091
    . "$INSTALLER_DIR/install.sh"
    set +u
    set -e
}

# The build image and setup_build_context both install fish, so a skip in CI would mean the
# environment lost it and the fish half of the activation script stopped being checked. A
# workstation without fish still skips.
require_fish() {
    if command -v fish >/dev/null 2>&1; then
        return 0
    fi
    if [ -n "${CI:-}" ] || [ -n "${SEN_DEV_STRICT:-}" ]; then
        printf 'fish not found, and this environment is meant to carry it\n' >&2
        return 1
    fi
    skip "fish not installed"
}

fixture_path() {
    printf '%s/%s' "$FIXTURES_DIR" "$1"
}

# Mock curl: serve $fixture for any /releases/tags/ URL, fail otherwise. URL is identified by the http* prefix
# (not argument position) so the mock works for both `curl -fsSL <url>` and `curl -fL <url> -o <out>`.
mock_curl_with_fixture() {
    local fixture="$1"
    eval "curl() {
        local a url=''
        for a in \"\$@\"; do
            case \"\$a\" in http*) url=\"\$a\" ;; esac
        done
        case \"\$url\" in
            */releases/tags/*) cat '$fixture'; return 0 ;;
            *) return 22 ;;
        esac
    }"
}

# Mock curl for the version listing. The fixture-based mock above serves /releases/tags/ only and
# returns 22 for everything else, so the listing endpoint was unreachable from the tests and
# ls_remote had no coverage at all. Kept separate because the shapes differ: the listing returns
# an array of releases, every other fixture is one release object.
mock_curl_with_listing() {
    local fixture="$1"
    eval "curl() {
        local a url=''
        for a in \"\$@\"; do
            case \"\$a\" in http*) url=\"\$a\" ;; esac
        done
        case \"\$url\" in
            */releases\\?*|*/releases) cat '$fixture'; return 0 ;;
            *) return 22 ;;
        esac
    }"
}

# Synthetic build directory under SEN_INSTALL_HOME; prints the prefix path.
make_fake_build() {
    local build="$1"
    local prefix="$SEN_INSTALL_HOME/$build"
    mkdir -p "$prefix/bin" "$prefix/lib" "$prefix/include"
    printf 'fake-binary\n'  > "$prefix/bin/sen"
    printf 'fake-library\n' > "$prefix/lib/libsen.so"
    printf 'fake-header\n'  > "$prefix/include/sen.h"
    chmod +x "$prefix/bin/sen"
    printf '%s' "$prefix"
}
