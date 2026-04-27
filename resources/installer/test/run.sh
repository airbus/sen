#!/bin/sh
# === run.sh  ==========================================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================
#
# Test runner for the Sen installer.
#
# Default behaviour: run every check we can; warn-and-skip the parts that need bats-core when bats is not installed.
# Set SEN_DEV_STRICT=1 to make any missing optional tooling a hard error (used by CI / canonical dev).

set -eu

HERE=$(cd "$(dirname "$0")" && pwd)
RC=0

c_red=""; c_yellow=""; c_green=""; c_dim=""; c_reset=""
if [ -t 1 ] && [ -z "${NO_COLOR:-}" ] && command -v tput >/dev/null 2>&1 \
   && [ "$(tput colors 2>/dev/null || echo 0)" -ge 8 ]; then
    c_red=$(tput setaf 1)
    c_yellow=$(tput setaf 3)
    c_green=$(tput setaf 2)
    c_dim=$(tput dim)
    c_reset=$(tput sgr0)
fi

say()  { printf '%s\n' "$*"; }
warn() { printf '%swarning:%s %s\n' "$c_yellow" "$c_reset" "$*" >&2; }
err()  { printf '%serror:%s %s\n' "$c_red" "$c_reset" "$*" >&2; }
ok()   { printf '%s%s%s\n' "$c_green" "$*" "$c_reset"; }

require_or_warn() {
    # require_or_warn <command> <human description>
    if command -v "$1" >/dev/null 2>&1; then
        return 0
    fi
    if [ -n "${SEN_DEV_STRICT:-}" ]; then
        err "$2 is required (SEN_DEV_STRICT=1) but was not found in PATH."
        RC=1
        return 1
    fi
    warn "skipping $2 checks (not found in PATH)."
    return 1
}

run_step() {
    # run_step <label> <command...>
    label="$1"
    shift
    printf '%s──── %s%s\n' "$c_dim" "$label" "$c_reset"
    if "$@"; then
        ok "  passed: $label"
    else
        err "  failed: $label"
        RC=1
    fi
}

# Always run: shell syntax checks.
run_step "syntax checks" sh "$HERE/test_syntax.sh"

# bats-driven unit tests (optional unless SEN_DEV_STRICT=1).
if require_or_warn bats "bats-core unit tests"; then
    bats_files=$(ls "$HERE"/test_*.bats 2>/dev/null || true)
    if [ -z "$bats_files" ]; then
        warn "no bats files found in $HERE"
    else
        # shellcheck disable=SC2086
        run_step "bats unit tests" bats $bats_files
    fi
fi

if [ "$RC" -eq 0 ]; then
    ok ""
    ok "all installer test steps passed"
else
    err ""
    err "one or more installer test steps failed"
fi
exit "$RC"
