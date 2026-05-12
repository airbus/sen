#!/bin/sh
# === test_syntax.sh  ==================================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================
#
# Parse-only syntax checks for install.sh.

set -eu

HERE=$(cd "$(dirname "$0")" && pwd)
INSTALLER=$(cd "$HERE/.." && pwd)
RC=0

check() {
    label="$1"
    shift
    if "$@" >/dev/null 2>&1; then
        printf '  ok    %s\n' "$label"
    else
        printf '  FAIL  %s\n' "$label" >&2
        RC=1
    fi
}

check "bash -n install.sh"  bash -n "$INSTALLER/install.sh"

# install.sh's POSIX-sh contract: dash is the strictest common /bin/sh and
# will reject bashisms (process substitution, [[ ]], etc.).
if command -v dash >/dev/null 2>&1; then
    check "dash -n install.sh" dash -n "$INSTALLER/install.sh"
else
    printf '  skip  dash -n install.sh (dash not installed)\n'
fi

if command -v zsh >/dev/null 2>&1; then
    check "zsh -n install.sh" zsh -n "$INSTALLER/install.sh"
else
    printf '  skip  zsh -n install.sh (zsh not installed)\n'
fi

# Generate a sample activate pair from the embedded templates and parse-check.
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT
SAMPLE_PREFIX="$TMP/0.0.0-sample"
mkdir -p "$SAMPLE_PREFIX/bin"
SENV_INSTALLER_NORUN=1 SEN_INSTALL_HOME="$TMP" \
    sh -c ". '$INSTALLER/install.sh' && write_activate_scripts '$SAMPLE_PREFIX'"

if [ -f "$SAMPLE_PREFIX/activate" ]; then
    check "bash -n activate (generated)" bash -n "$SAMPLE_PREFIX/activate"
    if command -v dash >/dev/null 2>&1; then
        check "dash -n activate (generated)" dash -n "$SAMPLE_PREFIX/activate"
    fi
    if command -v zsh >/dev/null 2>&1; then
        check "zsh -n activate (generated)" zsh -n "$SAMPLE_PREFIX/activate"
    fi
else
    printf '  FAIL  activate template did not generate\n' >&2
    RC=1
fi

if [ -f "$SAMPLE_PREFIX/activate.fish" ]; then
    if command -v fish >/dev/null 2>&1; then
        check "fish -n activate.fish (generated)" fish -n "$SAMPLE_PREFIX/activate.fish"
    else
        printf '  skip  fish -n activate.fish (fish not installed)\n'
    fi
else
    printf '  FAIL  activate.fish template did not generate\n' >&2
    RC=1
fi

exit "$RC"
