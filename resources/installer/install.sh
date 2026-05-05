#!/bin/sh
# === install.sh  ======================================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================
#
# Sen quick installer (POSIX sh). End-user usage lives in `docs/getting_started/install.md`; the install pipeline,
# layout, exit codes, env-var contract, security model, and test harness are documented in `architecture.md`
# alongside this file. This file's own conventions:
#
#   - Every function declares its working vars with `local`. Not POSIX, but dash/bash/ash/zsh all support it, and
#     the activate scripts already rely on it. Avoids accidental name collisions in this flat namespace.
#   - Strict mode (`set -eu`) is enabled only for script execution; the bats harness sources this file with
#     SENV_INSTALLER_NORUN=1 to call individual functions, where `set -u` would clash with bats probing unset vars.

if [ "${SENV_INSTALLER_NORUN:-0}" != "1" ]; then
    set -eu
fi

SENV_INSTALLER_VERSION="0.2.0"

: "${SEN_INSTALL_HOME:=$HOME/.sen}"
: "${SEN_BASE_URL:=https://github.com/airbus/sen}"
: "${SEN_API_URL:=https://api.github.com/repos/airbus/sen}"
: "${SEN_RAW_URL:=https://raw.githubusercontent.com/airbus/sen/main}"

# Parsed-arg globals. Top-level so the test harness can inspect them after calling parse_args directly.
SENV_VERSION_ARG=""
SENV_COMPILER=""
SENV_NON_INTERACTIVE=0
SENV_ALLOW_ROOT=0

# Resolve-stage scratch globals. Functions set these instead of using command substitution so callers don't have to
# round-trip through subshells (where set -e behaviour is harder to reason about).
SENV_RESOLVED_URL=""
SENV_RESOLVED_USED_MENU=0
SENV_ARCHIVE_PATH=""

# Color and decoration globals.
_C_BOLD=""    ; _C_DIM=""     ; _C_ULINE=""   ; _C_RED=""
_C_GREEN=""   ; _C_YELLOW=""  ; _C_CYAN=""
_C_MAGENTA="" ; _C_RESET=""
_G_OK=""      ; _G_ERR=""     ; _G_WARN=""    ; _G_INFO=""
_G_BAR="="    ; _G_STEP=">"   ; _G_RULE="-"

#---------------------------------------------------------------------------------------------------------------
# Color and message helpers
#---------------------------------------------------------------------------------------------------------------

# Populate colors (TTY + tput + >= 8 colors + NO_COLOR unset) and UTF-8 glyphs (also requires UTF-8 in the locale).
init_colors() {
    local n
    [ -n "${NO_COLOR:-}" ] && return 0
    [ -t 1 ] || return 0
    command -v tput >/dev/null 2>&1 || return 0
    n=$(tput colors 2>/dev/null || echo 0)
    [ "$n" -ge 8 ] 2>/dev/null || return 0
    _C_BOLD=$(tput bold)
    _C_DIM=$(tput dim)
    _C_ULINE=$(tput smul)
    _C_RED=$(tput setaf 1)
    _C_GREEN=$(tput setaf 2)
    _C_YELLOW=$(tput setaf 3)
    _C_CYAN=$(tput setaf 6)
    _C_MAGENTA=$(tput setaf 5)
    _C_RESET=$(tput sgr0)
    case "${LC_ALL:-${LC_CTYPE:-${LANG:-}}}" in
        *UTF-8*|*utf-8*|*UTF8*|*utf8*)
            _G_OK="✓ "
            _G_ERR="✗ "
            _G_WARN="⚠ "
            _G_INFO="ℹ "
            _G_BAR="▬"
            _G_STEP="▸"
            _G_RULE="─"
            ;;
    esac
}

# Inline colored text (no newline). Last argument is the text; preceding arguments compose styles.
paint() {
    local prefix=""
    while [ $# -gt 1 ]; do
        case "$1" in
            bold)      prefix="$prefix$_C_BOLD" ;;
            dim)       prefix="$prefix$_C_DIM" ;;
            underline) prefix="$prefix$_C_ULINE" ;;
            red)       prefix="$prefix$_C_RED" ;;
            green)     prefix="$prefix$_C_GREEN" ;;
            yellow)    prefix="$prefix$_C_YELLOW" ;;
            cyan)      prefix="$prefix$_C_CYAN" ;;
            magenta)   prefix="$prefix$_C_MAGENTA" ;;
        esac
        shift
    done
    printf '%s%s%s' "$prefix" "$1" "$_C_RESET"
}

# Message helpers. First arg is a short label, second is the body.
err()  { printf '%s %s\n' "$(paint red bold    "${_G_ERR}$1")"  "$2" >&2; }
warn() { printf '%s %s\n' "$(paint yellow bold "${_G_WARN}$1")" "$2" >&2; }
note() { printf '%s %s\n' "$(paint yellow      "${_G_INFO}$1")" "$2" >&2; }
ok()   { printf '%s %s\n' "$(paint green bold  "${_G_OK}$1")"   "$2"; }

#---------------------------------------------------------------------------------------------------------------
# Display helpers (banner, configuration block, step marker, rule, closing panel)
#---------------------------------------------------------------------------------------------------------------

# Brand banner
banner() {
    local indent="  " width=78 segs=6 segchar="$_G_BAR" colors="1 2 3 6 5 4"
    printf '\n%s%s  %s\n' \
        "$indent" \
        "$(paint bold "Sen Installer")" \
        "$(paint dim "v$SENV_INSTALLER_VERSION")"
    local seglen=$((width / segs)) c i seg bar=""
    if [ -n "$_C_BOLD" ]; then
        for c in $colors; do
            seg=""; i=0
            while [ "$i" -lt "$seglen" ]; do seg="$seg$segchar"; i=$((i+1)); done
            bar="$bar$(tput setaf "$c")$seg"
        done
        printf '%s%s%s\n\n' "$indent" "$bar" "$_C_RESET"
    else
        seg=""; i=0
        while [ "$i" -lt "$width" ]; do seg="$seg$segchar"; i=$((i+1)); done
        printf '%s%s\n\n' "$indent" "$seg"
    fi
}

# Pre-install summary: print the resolved choices so the user can sanity-check before the download starts.
config_block() {
    local toolchain="$1" prefix="$2"
    printf '  %s\n' "$(paint bold underline 'Configuration')"
    printf '    %-10s %s\n' "Version"   "$(paint cyan "$SENV_VERSION_ARG")"
    printf '    %-10s %s\n' "Toolchain" "$(paint cyan "$toolchain")"
    printf '    %-10s %s\n' "Arch / OS" "$(paint cyan "$(host_arch)-$(host_os)")"
    printf '    %-10s %s\n' "Prefix"    "$(paint dim  "$prefix")"
    printf '\n\n'
}

# In-progress step marker.
step() {
    printf '  %s %s\n' "$(paint cyan bold "$_G_STEP")" "$(paint bold "$1")"
}

# Standalone done line: green checkmark + message.
step_done() {
    printf '  %s %s\n' "$(paint green bold "${_G_OK% }")" "$1"
}

# Finalize a previous step() with intermediate output below it (the curl bar).
complete_step() {
    local msg="$1"
    if [ -n "$_C_BOLD" ]; then
        printf '\033[2A\r\033[2K  %s %s\n\033[2K' \
            "$(paint green bold "${_G_OK% }")" \
            "$msg"
    else
        step_done "$msg"
    fi
}

# Notes queue: soft skips and warnings buffered during the install and flushed as a single "Notes" block at the end.
_NOTES_QUEUE=""

defer_note() { _NOTES_QUEUE="${_NOTES_QUEUE}note	$1
"; }
defer_warn() { _NOTES_QUEUE="${_NOTES_QUEUE}warn	$1
"; }

# Emit the queued notes as a "Notes" block. No-op on an empty queue. Messages may use "; " to separate a headline
# from a trailing explanation; the trailing portion is rendered dim so the eye lands on the headline first.
flush_notes() {
    [ -z "$_NOTES_QUEUE" ] && return 0
    printf '\n  %s\n' "$(paint bold underline 'Notes')"
    printf '%s' "$_NOTES_QUEUE" | while IFS='	' read -r kind msg; do
        [ -z "$kind" ] && continue
        local head tail
        if [ "${msg#*; }" != "$msg" ]; then
            head="${msg%%; *}"
            tail="; ${msg#*; }"
        else
            head="$msg"
            tail=""
        fi
        case "$kind" in
            note) printf '    %s %s%s\n' "$(paint yellow      "${_G_INFO% }")" "$head" "$(paint dim "$tail")" ;;
            warn) printf '    %s %s%s\n' "$(paint yellow bold "${_G_WARN% }")" "$head" "$(paint dim "$tail")" ;;
        esac
    done
    _NOTES_QUEUE=""
}

# Horizontal ruler.
rule() {
    local width=78 i=0 line=""
    while [ "$i" -lt "$width" ]; do line="$line$_G_RULE"; i=$((i+1)); done
    printf '  %s\n' "$(paint dim "$line")"
}

# Closing panel: rule + success line + activate hints + rc tip.
done_panel() {
    local build="$1" lbl
    local current="${SEN_INSTALL_HOME}/current"
    printf '\n'
    rule
    printf '\n  %s\n' "$(paint green bold "${_G_OK}Sen $build installed.")"
    printf '\n  %s\n' "$(paint bold 'To activate this build:')"
    lbl=$(printf '%-10s' 'bash/zsh')
    printf '    %s %s\n' "$(paint cyan "$lbl")" ". $current/activate"
    lbl=$(printf '%-10s' 'fish')
    printf '    %s %s\n' "$(paint cyan "$lbl")" "source $current/activate.fish"
    printf '\n  %s\n' "$(paint dim "Add the same line to your shell rc")"
    printf '  %s\n' "$(paint dim "'$current' points at the latest install.")"
}

#---------------------------------------------------------------------------------------------------------------
# Network helpers
#---------------------------------------------------------------------------------------------------------------

# curl wrapper with retry/timeout defaults applied to every network call.
_curl() {
    curl --retry 3 --retry-delay 2 --retry-connrefused --connect-timeout 10 "$@"
}

#---------------------------------------------------------------------------------------------------------------
# Argument parsing
#---------------------------------------------------------------------------------------------------------------

print_usage() {
    cat <<EOF
$(paint bold "Sen quick installer") $(paint dim "v$SENV_INSTALLER_VERSION")

$(paint bold "Usage:") sh install.sh [<version>] [options]

$(paint bold "Options:")
  --compiler <name>-<ver>     pick a specific toolchain (gcc-12.4.0, clang-16.0.0, msvc-19.X)
  --compiler=<name>-<ver>     same, equals form
  -y, --yes                   non-interactive (refuse rather than open the menu)
  --allow-root                allow running as root (containers)
  -h, --help                  show this help
  -V, --version               print installer version

If <version> is omitted, the script prints the available release tags and
exits. Multiple builds for one version (different compilers / archs) trigger
an interactive menu unless --compiler resolves to exactly one or --yes is set.
EOF
}

parse_args() {
    SENV_VERSION_ARG=""
    SENV_COMPILER=""
    SENV_NON_INTERACTIVE=0
    SENV_ALLOW_ROOT=0
    while [ $# -gt 0 ]; do
        case "$1" in
            --compiler)     SENV_COMPILER="${2:-}"
                            [ -n "$SENV_COMPILER" ] || { err "install.sh:" "--compiler needs a value."; return 1; }
                            shift 2 ;;
            --compiler=*)   SENV_COMPILER="${1#*=}"
                            [ -n "$SENV_COMPILER" ] || { err "install.sh:" "--compiler needs a value."; return 1; }
                            shift ;;
            -y|--yes)       SENV_NON_INTERACTIVE=1; shift ;;
            --allow-root)   SENV_ALLOW_ROOT=1; shift ;;
            -h|--help)      print_usage; exit 0 ;;
            -V|--version)   printf 'install.sh %s\n' "$SENV_INSTALLER_VERSION"; exit 0 ;;
            -*)             err "install.sh:" "unknown option: $1"; return 1 ;;
            *)
                if [ -z "$SENV_VERSION_ARG" ]; then
                    SENV_VERSION_ARG="$1"
                else
                    err "install.sh:" "unexpected argument: $1"
                    return 1
                fi
                shift ;;
        esac
    done
}

#---------------------------------------------------------------------------------------------------------------
# Pre-flight checks
#---------------------------------------------------------------------------------------------------------------

ensure_not_root() {
    [ "$(id -u 2>/dev/null || echo 0)" = "0" ] || return 0
    [ -n "${SEN_ALLOW_ROOT:-}" ] && return 0
    [ "$SENV_ALLOW_ROOT" = "1" ] && return 0
    err "install.sh:" "refusing to run as root (Sen installs into \$HOME)."
    printf '%s\n' \
        '       If you really mean it (e.g. inside a container), re-run with: sh install.sh --allow-root' \
        '       or set SEN_ALLOW_ROOT=1.' \
        >&2
    return 1
}

ensure_linux() {
    case "$(host_os)" in
        linux) ;;
        *)
            err "install.sh:" "Sen quick install supports Linux only (got: $(host_os))."
            return 1 ;;
    esac
}

# Verify the tools we need are on PATH.
ensure_tools() {
    local tool missing=""
    for tool in curl tar sha256sum awk sed grep find xargs sort; do
        command -v "$tool" >/dev/null 2>&1 || missing="$missing $tool"
    done
    [ -z "$missing" ] && return 0
    err "install.sh:" "missing required tools:$missing"
    printf '%s\n' \
        '       install them and re-run; for example:' \
        '         Debian/Ubuntu: sudo apt install curl coreutils tar findutils' \
        '         Fedora/RHEL:   sudo dnf install curl coreutils tar findutils' \
        '         Arch:          sudo pacman -S curl coreutils tar findutils' \
        >&2
    return 1
}

#---------------------------------------------------------------------------------------------------------------
# Host detection
#---------------------------------------------------------------------------------------------------------------

# Detect the host's architecture slot as it appears in Sen release filenames (x86_64 / aarch64 / ...). amd64 / arm64
# are aliased to the slots Sen uses; anything else is passed through. SEN_HOST_ARCH overrides it (for tests).
host_arch() {
    local m
    if [ -n "${SEN_HOST_ARCH:-}" ]; then
        printf '%s' "$SEN_HOST_ARCH"
        return 0
    fi
    m=$(uname -m 2>/dev/null || echo unknown)
    case "$m" in
        x86_64|amd64)   printf 'x86_64' ;;
        aarch64|arm64)  printf 'aarch64' ;;
        *)              printf '%s' "$m" ;;
    esac
}

# Detect the host's OS slot as it appears in Sen release filenames. SEN_HOST_ARCH overrides it (for tests).
host_os() {
    if [ -n "${SEN_HOST_OS:-}" ]; then
        printf '%s' "$SEN_HOST_OS"
        return 0
    fi
    case "$(uname -s 2>/dev/null)" in
        Linux) printf 'linux' ;;
        *)     printf 'unknown' ;;
    esac
}

#---------------------------------------------------------------------------------------------------------------
# GitHub releases: ls-remote + asset filter + toolchain parser
#---------------------------------------------------------------------------------------------------------------

# Print every release tag from GitHub, one per line.
ls_remote() {
    local json
    if ! json=$(_curl -fsSL "$SEN_API_URL/releases" 2>/dev/null); then
        err "install.sh:" "could not query $SEN_API_URL/releases"
        return 1
    fi
    printf '%s' "$json" \
        | grep -o '"tag_name"[[:space:]]*:[[:space:]]*"[^"]*"' \
        | sed -E 's/.*"([^"]+)"$/\1/'
}

# Reads a GitHub Releases API response on stdin. Emits one tarball download URL per line, filtered by (arch, os).
extract_assets() {
    local arch os
    arch=$(host_arch)
    os=$(host_os)
    grep -o '"browser_download_url"[[:space:]]*:[[:space:]]*"[^"]*"' \
        | sed -E 's/.*"(https:[^"]+)".*/\1/' \
        | grep -E "sen-.*-${arch}-${os}-.*-release\.tar\.gz$" \
        || true
}

# Extract the toolchain (compiler name + version) from a Sen release URL or filename. Prints "<name> <version>".
# Example: "sen-0.5.2-x86_64-linux-gnu-12.4.0-release.tar.gz" -> "gcc 12.4.0"
parse_toolchain() {
    local name stem ver rest tag
    name="${1##*/}"
    stem="${name%-release.tar.gz}"
    stem="${stem%-release.zip}"
    ver="${stem##*-}"
    rest="${stem%-*}"
    tag="${rest##*-}"
    case "$tag" in
        gnu) printf '%s %s' gcc "$ver" ;;
        *)   printf '%s %s' "$tag" "$ver" ;;
    esac
}

#---------------------------------------------------------------------------------------------------------------
# Compiler detection (for the menu's per-line compat hint)
#---------------------------------------------------------------------------------------------------------------

# Version of one named compiler on this system. Return non-zero (no output) if it isn't on PATH or unresponsive.
detect_compiler() {
    local v
    case "$1" in
        gcc)
            command -v gcc >/dev/null 2>&1 || return 1
            v=$(gcc -dumpfullversion -dumpversion 2>/dev/null) || return 1
            printf '%s' "$v" ;;
        clang)
            command -v clang >/dev/null 2>&1 || return 1
            v=$(clang -dumpversion 2>/dev/null) || return 1
            printf '%s' "$v" ;;
        *)
            return 1 ;;
    esac
}

# One "<name> <version>" line per detected system compiler. Empty output means "nothing detected".
detect_compilers() {
    local v
    v=$(detect_compiler gcc 2>/dev/null)   && printf 'gcc %s\n'   "$v"
    v=$(detect_compiler clang 2>/dev/null) && printf 'clang %s\n' "$v"
    return 0
}

#---------------------------------------------------------------------------------------------------------------
# Resolve URL: fetch JSON, build candidates, pick one
#---------------------------------------------------------------------------------------------------------------

# Emit one "<name> <version>\t<url>" line per host-matching Linux build for $version. Empty stdout = no matching
# builds. Network or API errors return non-zero with a message on stderr.
build_candidates() {
    local version="$1"
    local api_url="$SEN_API_URL/releases/tags/$version"
    local json urls url toolchain
    if ! json=$(_curl -fsSL "$api_url" 2>/dev/null); then
        err "install.sh:" "could not query release '$version'"
        printf '%s\n' "$(paint dim "run 'sh install.sh' (no args) to see available versions.")" >&2
        return 1
    fi
    urls=$(printf '%s' "$json" | extract_assets)
    [ -z "$urls" ] && return 0
    while IFS= read -r url; do
        [ -z "$url" ] && continue
        toolchain=$(parse_toolchain "$url") || continue
        printf '%s\t%s\n' "$toolchain" "$url"
    done <<EOF
$urls
EOF
}

# Pick a URL according to the input policy. Sets SENV_RESOLVED_URL on success, plus SENV_RESOLVED_USED_MENU=1 iff
# the user picked from the interactive menu.
resolve_url() {
    local version="$1" compiler="$2" non_interactive="$3"
    local candidates user_name user_ver count
    SENV_RESOLVED_URL=""
    SENV_RESOLVED_USED_MENU=0

    candidates=$(build_candidates "$version") || return 1
    if [ -z "$candidates" ]; then
        err "install.sh:" "no $(host_arch)-$(host_os) builds in release $version"
        return 1
    fi

    # 1) Explicit --compiler. gcc-X.Y / clang-X.Y / msvc-X.Y; bare X.Y is treated as gcc-X.Y for backward
    # compatibility with the pre-0.6 names.
    if [ -n "$compiler" ]; then
        case "$compiler" in
            gcc-*)   user_name=gcc   ; user_ver="${compiler#gcc-}" ;;
            clang-*) user_name=clang ; user_ver="${compiler#clang-}" ;;
            msvc-*)  user_name=msvc  ; user_ver="${compiler#msvc-}" ;;
            *)       user_name=gcc   ; user_ver="$compiler" ;;
        esac
        SENV_RESOLVED_URL=$(printf '%s\n' "$candidates" \
            | awk -v want="$user_name $user_ver" -F'\t' '$1==want {print $2; exit}')
        if [ -z "$SENV_RESOLVED_URL" ]; then
            err "install.sh:" "no build for $version with compiler $compiler"
            printf 'available toolchains for %s:\n' "$version" >&2
            printf '%s\n' "$candidates" \
                | awk -F'\t' '{ split($1, p, " "); print "  " p[1] "-" p[2] }' >&2
            return 1
        fi
        warn_compat_if_needed "$SENV_RESOLVED_URL"
        return 0
    fi

    # 2) Single match: auto-pick.
    count=$(printf '%s\n' "$candidates" | grep -c .)
    if [ "$count" = "1" ]; then
        SENV_RESOLVED_URL=$(printf '%s\n' "$candidates" | awk -F'\t' '{print $2}')
        warn_compat_if_needed "$SENV_RESOLVED_URL"
        return 0
    fi

    # 3) Multi match, non-interactive: refuse with the toolchain list.
    if [ "$non_interactive" = "1" ] || [ ! -t 0 ]; then
        err "install.sh:" "multiple builds available; pass --compiler <name>-<ver>:"
        printf '%s\n' "$candidates" \
            | awk -F'\t' '{ split($1, p, " "); print "  " p[1] "-" p[2] }' >&2
        return 1
    fi

    # 4) Multi match, interactive: menu.
    run_menu "$version" "$candidates"
}

# Show the interactive selection menu and read the user's choice.
run_menu() {
    local version="$1" candidates="$2"
    local detected i line field1 cand_name cand_ver marker user_ver choice default_idx=""

    detected=$(detect_compilers | paste -sd, - | sed 's/,/, /g')

    printf '%s\n' "$(paint bold "Multiple builds available for Sen $version.")"
    if [ -n "$detected" ]; then
        printf '%s\n' "$(paint dim "Detected on this system: $detected")"
    fi
    printf '\n'

    # Render each candidate line and remember the first one whose compiler+version matches the host: that becomes
    # the default-on-Enter pick.
    i=1
    while IFS= read -r line; do
        field1="${line%%	*}"
        cand_name="${field1%% *}"
        cand_ver="${field1##* }"
        marker=""
        if user_ver=$(detect_compiler "$cand_name" 2>/dev/null); then
            if [ "$user_ver" = "$cand_ver" ]; then
                marker="  $(paint green "(matches your $cand_name)")"
                [ -z "$default_idx" ] && default_idx="$i"
            else
                marker="  $(paint dim "(your $cand_name: $user_ver)")"
            fi
        else
            marker="  $(paint dim "(no $cand_name on your system)")"
        fi
        printf '  %s %s %s%s\n' \
            "$(paint bold "$i)")" \
            "$(paint cyan "$cand_name")" \
            "$(paint cyan "$cand_ver")" \
            "$marker"
        i=$((i+1))
    done <<EOF
$candidates
EOF
    if [ -n "$default_idx" ]; then
        printf '\n%s ' "$(paint bold "select a build [1-$((i-1)), default: $default_idx]:")"
    else
        printf '\n%s ' "$(paint bold "select a build [1-$((i-1))]:")"
    fi
    choice=""
    if ! read -r choice </dev/tty 2>/dev/null; then
        err "aborted:" "cannot read from terminal."
        return 1
    fi
    # Empty input picks the default if we have one; otherwise it's still an abort.
    if [ -z "$choice" ] && [ -n "$default_idx" ]; then
        choice="$default_idx"
    fi
    case "$choice" in
        ''|*[!0-9]*) err "aborted:" "invalid selection."; return 1 ;;
    esac
    if [ "$choice" -lt 1 ] || [ "$choice" -ge "$i" ]; then
        err "aborted:" "selection out of range."
        return 1
    fi
    SENV_RESOLVED_URL=$(printf '%s\n' "$candidates" \
        | sed -n "${choice}p" \
        | awk -F'\t' '{print $2}')
    SENV_RESOLVED_USED_MENU=1
}

# Soft compatibility note. Suppressed when the menu was used (it already showed per-line compat). Otherwise queues
# a note for the closing Notes block: compiler missing from PATH, or present at a different version (ABI risk).
warn_compat_if_needed() {
    local url="$1"
    local toolchain build_name build_ver user_ver
    [ "$SENV_RESOLVED_USED_MENU" = "1" ] && return 0
    toolchain=$(parse_toolchain "$url") || return 0
    build_name="${toolchain%% *}"
    build_ver="${toolchain##* }"
    if ! user_ver=$(detect_compiler "$build_name" 2>/dev/null); then
        defer_note "this build needs $build_name $build_ver; '$build_name' is not on your PATH."
        return 0
    fi
    [ "$user_ver" = "$build_ver" ] && return 0
    defer_note "this build was compiled with $build_name $build_ver, your $build_name is $user_ver; \
ABI mismatches may show up at link time."
}

#---------------------------------------------------------------------------------------------------------------
# Fetch + checksum verification
#---------------------------------------------------------------------------------------------------------------

# Place $url's archive at $SEN_INSTALL_HOME/cache/$fname. Cache hit = file already present from a previous locked run;
# verify_checksum catches corruption when SHA256SUMS is published, otherwise we trust the cache.
# Sets SENV_ARCHIVE_PATH on success.
fetch_archive() {
    local url="$1" fname="$2"
    local archive="$SEN_INSTALL_HOME/cache/$fname"
    mkdir -p "$SEN_INSTALL_HOME/cache"
    if [ -f "$archive" ]; then
        SENV_ARCHIVE_PATH="$archive"
        return 0
    fi
    if ! _curl -fL --no-progress-meter --no-progress-bar "$url" -o "${archive}.part"; then
        rm -f "${archive}.part"
        err "install.sh:" "download failed."
        return 1
    fi
    mv "${archive}.part" "$archive"
    SENV_ARCHIVE_PATH="$archive"
}

# Best-effort SHA256SUMS verification. Returns 0 with a soft note when the release doesn't publish SHA256SUMS or when
# there's no entry for $fname. Returns 1 (and deletes $archive) on a real mismatch.
verify_checksum() {
    local version="$1" fname="$2" archive="$3"
    local sums_url="$SEN_BASE_URL/releases/download/$version/SHA256SUMS"
    local sums expected actual=""
    if ! sums=$(_curl -fsSL "$sums_url" 2>/dev/null); then
        defer_note "SHA256SUMS not published for $version; checksum verification skipped."
        return 0
    fi
    expected=$(printf '%s\n' "$sums" \
        | awk -v f="$fname" '$2==f || $2=="*"f {print $1; exit}')
    if [ -z "$expected" ]; then
        defer_warn "no checksum entry for $fname in SHA256SUMS; verification skipped."
        return 0
    fi
    if command -v sha256sum >/dev/null 2>&1; then
        actual=$(sha256sum "$archive" | awk '{print $1}')
    elif command -v shasum >/dev/null 2>&1; then
        actual=$(shasum -a 256 "$archive" | awk '{print $1}')
    else
        defer_warn "no sha256sum tool found; verification skipped."
        return 0
    fi
    if [ "$actual" = "$expected" ]; then
        step_done "Verified sha256 checksum"
        return 0
    fi
    err "install.sh:" "checksum mismatch for $fname"
    printf '      expected: %s\n' "$expected" >&2
    printf '      got:      %s\n' "$actual" >&2
    rm -f "$archive"
    return 1
}

#---------------------------------------------------------------------------------------------------------------
# Extract + manifest + completion cache
#---------------------------------------------------------------------------------------------------------------

# Extract $archive into $prefix (promoting a single top-level dir if present).
unpack() {
    local archive="$1" prefix="$2"
    local tmp="$SEN_INSTALL_HOME/cache/.extract.$$"
    local entries top_count top
    rm -rf "$tmp"
    mkdir -p "$tmp"
    if ! tar -xf "$archive" -C "$tmp"; then
        rm -rf "$tmp"
        err "install.sh:" "extraction failed."
        return 1
    fi

    entries=$(ls -1 "$tmp")
    top_count=$(printf '%s\n' "$entries" | grep -c .)
    if [ "$top_count" = "1" ] && [ -d "$tmp/$entries" ]; then
        top="$tmp/$entries"
    else
        top="$tmp"
    fi

    mkdir -p "$(dirname "$prefix")"
    if ! mv "$top" "$prefix"; then
        rm -rf "$tmp"
        err "install.sh:" "could not move build into place at $prefix"
        return 1
    fi
    rm -rf "$tmp"
}

# Write a sha256sum-format manifest at $prefix root (one "<hex>  <relative-path>" per line, ready for `sha256sum -c`).
generate_manifest() {
    local prefix="$1"
    local manifest="$prefix/.sen-manifest.sha256"
    command -v sha256sum >/dev/null 2>&1 || return 1
    (cd "$prefix" \
        && find . -type f \
            ! -name '.sen-manifest.sha256' \
            ! -name 'activate' \
            ! -name 'activate.fish' \
            -printf '%P\0' \
        | LC_ALL=C sort -z \
        | xargs -0 -r sha256sum > "$manifest" 2>/dev/null) \
        || { rm -f "$manifest"; return 1; }
    [ -s "$manifest" ] || { rm -f "$manifest"; return 1; }
}

#---------------------------------------------------------------------------------------------------------------
# Activate-script generation
#---------------------------------------------------------------------------------------------------------------

# Write $prefix/activate and $prefix/activate.fish. Both: bake $SEN_PREFIX as a literal; strip prior
# $SEN_INSTALL_HOME/ entries from PATH, CMAKE_PREFIX_PATH, and LD_LIBRARY_PATH (so re-sourcing or build-switching is
# idempotent); prepend the new build's <prefix>/bin and <prefix>.
write_activate_scripts() {
    local prefix="$1"
    local sen_root="$SEN_INSTALL_HOME"

    cat > "$prefix/activate" <<EOF
# This file was automatically generated by sen. Do not modify.

SEN_PREFIX='$prefix'
export SEN_PREFIX
_sen_root='$sen_root'

_sen_strip() {
    # Drop colon-separated entries under \$_sen_root/ from \$1; print the rest.
    # POSIX-only parameter expansion (no awk, no fork). 'local' keeps the
    # working vars from leaking into the caller's interactive shell.
    local _in="\$1" _out="" _rest="\$1" _entry
    [ -z "\$_in" ] && return 0
    while [ -n "\$_rest" ]; do
        if [ "\${_rest#*:}" != "\$_rest" ]; then
            _entry="\${_rest%%:*}"
            _rest="\${_rest#*:}"
        else
            _entry="\$_rest"
            _rest=""
        fi
        case "\$_entry" in
            "\$_sen_root"/*) ;;
            "") ;;
            *) _out="\${_out:+\$_out:}\$_entry" ;;
        esac
    done
    printf '%s' "\$_out"
}

PATH=\$(_sen_strip "\$PATH")
CMAKE_PREFIX_PATH=\$(_sen_strip "\${CMAKE_PREFIX_PATH:-}")
LD_LIBRARY_PATH=\$(_sen_strip "\${LD_LIBRARY_PATH:-}")

# Sen installs binaries, shared libs, and archives into <prefix>/bin (CMAKE_INSTALL_BINDIR), not <prefix>/lib.
PATH="\$SEN_PREFIX/bin\${PATH:+:\$PATH}"
CMAKE_PREFIX_PATH="\$SEN_PREFIX\${CMAKE_PREFIX_PATH:+:\$CMAKE_PREFIX_PATH}"
LD_LIBRARY_PATH="\$SEN_PREFIX/bin\${LD_LIBRARY_PATH:+:\$LD_LIBRARY_PATH}"

export PATH CMAKE_PREFIX_PATH LD_LIBRARY_PATH

unset _sen_root
unset -f _sen_strip 2>/dev/null || true

# Sen CLI completion. Prefer the install-time cache; fall back to the binary.
if [ -n "\${BASH_VERSION:-}" ]; then
    if [ -r "\$SEN_PREFIX/share/sen-completions/bash" ]; then
        # shellcheck disable=SC1091
        . "\$SEN_PREFIX/share/sen-completions/bash" 2>/dev/null || true
    elif command -v sen >/dev/null 2>&1; then
        eval "\$(sen completion bash 2>/dev/null)" 2>/dev/null || true
    fi
elif [ -n "\${ZSH_VERSION:-}" ]; then
    if [ -r "\$SEN_PREFIX/share/sen-completions/zsh" ]; then
        # shellcheck disable=SC1091
        . "\$SEN_PREFIX/share/sen-completions/zsh" 2>/dev/null || true
    elif command -v sen >/dev/null 2>&1; then
        eval "\$(sen completion zsh 2>/dev/null)" 2>/dev/null || true
    fi
fi
EOF

    cat > "$prefix/activate.fish" <<EOF
# This file was automatically generated by sen. Do not modify.

set -gx SEN_PREFIX '$prefix'
set -l _sen_root '$sen_root'

function _sen_strip --no-scope-shadowing -a input
    # Drop entries under \$_sen_root/ from a colon-separated string; print the rest.
    if test -z "\$input"
        return 0
    end
    set -l out
    for e in (string split ':' -- \$input)
        switch \$e
            case "\$_sen_root/*"
            case ""
            case '*'
                set -a out \$e
        end
    end
    string join ':' -- \$out
end

set -l _sen_path       (_sen_strip "\$PATH")
set -l _sen_cmake_path (_sen_strip "\$CMAKE_PREFIX_PATH")
set -l _sen_ldlib_path (_sen_strip "\$LD_LIBRARY_PATH")

# Sen installs binaries, shared libs, and archives into <prefix>/bin (CMAKE_INSTALL_BINDIR), not <prefix>/lib.
set -gx PATH \$SEN_PREFIX/bin (string split ':' -- \$_sen_path)

if test -n "\$_sen_cmake_path"
    set -gx CMAKE_PREFIX_PATH "\$SEN_PREFIX:\$_sen_cmake_path"
else
    set -gx CMAKE_PREFIX_PATH "\$SEN_PREFIX"
end
if test -n "\$_sen_ldlib_path"
    set -gx LD_LIBRARY_PATH "\$SEN_PREFIX/bin:\$_sen_ldlib_path"
else
    set -gx LD_LIBRARY_PATH "\$SEN_PREFIX/bin"
end

functions -e _sen_strip

# Sen CLI completion. Prefer the install-time cache; fall back to the binary.
if test -r "\$SEN_PREFIX/share/sen-completions/fish"
    source "\$SEN_PREFIX/share/sen-completions/fish" 2>/dev/null
else if command -v sen >/dev/null 2>&1
    sen completion fish 2>/dev/null | source
end
EOF
    chmod 0644 "$prefix/activate" "$prefix/activate.fish"
}

#---------------------------------------------------------------------------------------------------------------
# Self-copy: refetch install.sh into $SEN_INSTALL_HOME so future installs don't need to round-trip to the bootstrap URL.
#---------------------------------------------------------------------------------------------------------------

# Refresh $SEN_INSTALL_HOME/install.sh from the bootstrap URL. Returns 0 on real refresh, 1 on bypass or fetch failure.
self_copy_installer() {
    local target tmp
    [ -n "${SEN_INSTALLER_NO_SELF_COPY:-}" ] && return 1
    target="$SEN_INSTALL_HOME/install.sh"
    tmp="${target}.tmp.$$"
    if _curl -fsSL "$SEN_RAW_URL/resources/installer/install.sh" -o "$tmp" 2>/dev/null; then
        mv "$tmp" "$target"
        chmod 0755 "$target"
        return 0
    fi
    rm -f "$tmp"
    return 1
}

#---------------------------------------------------------------------------------------------------------------
# Install lock
#---------------------------------------------------------------------------------------------------------------

# Run "$@" under an exclusive non-blocking flock on $SEN_INSTALL_HOME/.lock-install.
with_install_lock() {
    if ! command -v flock >/dev/null 2>&1; then
        "$@"
        return $?
    fi
    mkdir -p "$SEN_INSTALL_HOME"
    (
        flock -n 9 || {
            err "install.sh:" "another install is in progress."
            exit 99
        }
        "$@"
    ) 9>"$SEN_INSTALL_HOME/.lock-install"
}

#---------------------------------------------------------------------------------------------------------------
# Orchestrator
#---------------------------------------------------------------------------------------------------------------

# Install pipeline (under with_install_lock when flock is present): resolve URL -> already-installed short-circuit
# -> download (or cache hit) -> verify -> extract + completions + manifest -> activate scripts -> self-copy ->
# done panel. Rollback: once unpack creates the build directory, any later failure removes it via an EXIT trap.
do_install() {
    local fname stem build prefix toolchain
    resolve_url "$SENV_VERSION_ARG" "$SENV_COMPILER" "$SENV_NON_INTERACTIVE" || return $?

    fname="${SENV_RESOLVED_URL##*/}"
    stem="${fname%-release.tar.gz}"
    build="${stem#sen-}"
    prefix="$SEN_INSTALL_HOME/$build"

    if [ -d "$prefix" ]; then
        warn "already installed:" "$build"
        # Re-targeting the `current` symlink even on the short-circuit lets `sh install.sh <old-version>` flip
        # back to a previously-installed build without forcing a re-install.
        ln -sfn "$build" "$SEN_INSTALL_HOME/current"
        printf "  prefix: %s\n" "$prefix"
        printf "  current: %s\n" "$SEN_INSTALL_HOME/current"
        printf "  remove the directory and re-run if you want to reinstall.\n"
        return 0
    fi

    toolchain=$(parse_toolchain "$SENV_RESOLVED_URL")
    config_block "$toolchain" "$prefix"

    # Download phase. Cache hit: single done line. Cold fetch: active arrow + curl bar, then complete_step
    # collapses both into one done line on success.
    if [ -f "$SEN_INSTALL_HOME/cache/$fname" ]; then
        fetch_archive "$SENV_RESOLVED_URL" "$fname" || return $?
        step_done "Using cached archive  $(paint dim "$fname")"
    else
        step "Downloading  $(paint dim "$fname")"
        fetch_archive "$SENV_RESOLVED_URL" "$fname" || return $?
        local size
        size=$(du -h "$SENV_ARCHIVE_PATH" 2>/dev/null | awk '{print $1}')
        complete_step "Downloaded  $(paint dim "$fname")${size:+  $(paint dim "($size)")}"
    fi

    verify_checksum "$SENV_VERSION_ARG" "$fname" "$SENV_ARCHIVE_PATH" || return $?

    # Arm the rollback. Disarm with `trap - EXIT` once the install is fully written.
    trap "rm -rf '$prefix'" EXIT

    unpack "$SENV_ARCHIVE_PATH" "$prefix" || return $?
    step_done "Extracted into  $(paint dim "$prefix")"

    if generate_manifest "$prefix"; then
        step_done "Wrote integrity manifest"
    else
        defer_warn "could not generate integrity manifest; sha256sum -c will not be available for this build."
    fi

    write_activate_scripts "$prefix"
    step_done "Wrote activate scripts"
    if self_copy_installer; then
        step_done "Refreshed cached installer"
    fi

    # Update the `current` symlink to this build. `-n` keeps the symlink itself as the target rather than
    # following it into the previous build's directory; `-f` replaces the existing symlink in one step.
    ln -sfn "$build" "$SEN_INSTALL_HOME/current"
    step_done "Updated 'current' to  $(paint dim "$build")"

    trap - EXIT

    flush_notes
    done_panel "$build"
}

main() {
    parse_args "$@"
    init_colors
    banner
    ensure_not_root
    ensure_linux
    ensure_tools

    mkdir -p "$SEN_INSTALL_HOME"

    if [ -z "$SENV_VERSION_ARG" ]; then
        local tags
        tags=$(ls_remote) || return $?
        printf '  %s\n' "$(paint bold "Available Sen releases:")"
        printf '%s\n' "$tags" | sed 's/^/    /'
        printf '\n  %s\n' "$(paint dim "Re-run with a version: sh install.sh <version>")"
        return 0
    fi

    with_install_lock do_install
}

# Allow the test harness to source this file without executing main.
[ "${SENV_INSTALLER_NORUN:-0}" = "1" ] || main "$@"
