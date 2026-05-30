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
    export SEN_HOST_ARCH=x86_64
    export SEN_INSTALLER_NO_SELF_COPY=1
}

teardown() {
    if [ -n "${SEN_INSTALL_HOME:-}" ] && [ -d "$SEN_INSTALL_HOME" ]; then
        rm -rf "$SEN_INSTALL_HOME"
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
