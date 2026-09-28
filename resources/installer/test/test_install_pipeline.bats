# === test_install_pipeline.bats  ======================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================
#
# Branches not covered by test_resolve_url: checksum verification (four outcomes), already-installed short-circuit,
# install-lock refusal, end-to-end install, rollback. The interactive menu (run_menu reads from /dev/tty) is not
# covered here: verified by hand via the menu_demo path.

load test_helpers

# Stage a SHA256SUMS body and publish its URL the way the release response does. The mock answers
# that exact URL only: a suffix match cannot tell the tag URL from a draft's untagged-<hash> one,
# which is the difference these tests turn on.
mock_curl_sums() {
    local sums="$1"
    local url="${2:-https://example/releases/download/0.5.2/SHA256SUMS}"
    local file="${SEN_TEST_TMPDIR}/sums.txt"
    printf '%s\n' "$sums" > "$file"
    SENV_SUMS_URL="$url"
    eval "curl() {
        local seen
        for a in \"\$@\"; do seen=\"\$a\"; done
        case \"\$seen\" in
            '$url') cat '$file'; return 0 ;;
            *) return 22 ;;
        esac
    }"
}

#---------------------------------------------------------------------------------------------------------------
# verify_checksum (four outcomes)
#---------------------------------------------------------------------------------------------------------------

# Soft-skip outcomes go to $_NOTES_QUEUE (flush_notes drains it later); the match case prints a step_done line.
# Each test inspects the right surface.

@test "verify_checksum: SHA256SUMS not published queues a note" {
    load_install
    local archive="$SEN_INSTALL_HOME/cache/x.tar.gz"
    mkdir -p "$SEN_INSTALL_HOME/cache" && printf 'x' > "$archive"
    # No release before 0.7.0 published one, so this stays installable.
    SENV_SUMS_URL=""
    curl() { return 22; }
    verify_checksum 0.5.2 x.tar.gz "$archive"
    [[ "$_NOTES_QUEUE" == *"SHA256SUMS"* ]]
    [ -f "$archive" ]
}

@test "verify_checksum: matching entry prints a step_done and keeps the archive" {
    load_install
    local fname="x.tar.gz"
    mkdir -p "$SEN_INSTALL_HOME/cache"
    printf 'fake-archive-content\n' > "$SEN_INSTALL_HOME/cache/$fname"
    local hex
    hex=$(sha256sum "$SEN_INSTALL_HOME/cache/$fname" | awk '{print $1}')
    mock_curl_sums "$hex  $fname"
    run verify_checksum 0.5.2 "$fname" "$SEN_INSTALL_HOME/cache/$fname"
    [ "$status" -eq 0 ]
    [[ "$output" == *"Verified"* ]]
    [ -f "$SEN_INSTALL_HOME/cache/$fname" ]
}

@test "verify_checksum: mismatch returns 1 and deletes the archive" {
    load_install
    local fname="x.tar.gz"
    mkdir -p "$SEN_INSTALL_HOME/cache"
    printf 'fake-archive-content\n' > "$SEN_INSTALL_HOME/cache/$fname"
    mock_curl_sums "0000000000000000000000000000000000000000000000000000000000000000  $fname"
    run verify_checksum 0.5.2 "$fname" "$SEN_INSTALL_HOME/cache/$fname"
    [ "$status" -ne 0 ]
    [[ "$output" == *"checksum mismatch"* ]]
    [ ! -f "$SEN_INSTALL_HOME/cache/$fname" ]
}

@test "verify_checksum: matches the BSD-style '*<file>' format too" {
    load_install
    local fname="x.tar.gz"
    mkdir -p "$SEN_INSTALL_HOME/cache"
    printf 'fake-archive-content\n' > "$SEN_INSTALL_HOME/cache/$fname"
    local hex
    hex=$(sha256sum "$SEN_INSTALL_HOME/cache/$fname" | awk '{print $1}')
    mock_curl_sums "$hex *$fname"
    run verify_checksum 0.5.2 "$fname" "$SEN_INSTALL_HOME/cache/$fname"
    [ "$status" -eq 0 ]
}

#---------------------------------------------------------------------------------------------------------------
# Already-installed short-circuit
#---------------------------------------------------------------------------------------------------------------

@test "do_install: short-circuits with 'already installed' when prefix exists" {
    load_install
    mock_curl_with_fixture "$(fixture_path release-0.5.2.json)"
    detect_compiler() { return 1; }
    make_fake_build "0.5.2-x86_64-linux-gnu-12.4.0" >/dev/null
    SENV_VERSION_ARG="0.5.2"
    SENV_COMPILER=""
    SENV_NON_INTERACTIVE=1
    run do_install
    [ "$status" -eq 0 ]
    [[ "$output" == *"already installed"* ]]
    [[ "$output" == *"remove the directory"* ]]
    # The short-circuit still re-targets `current` so re-running on an older build flips back to it.
    [ -L "$SEN_INSTALL_HOME/current" ]
    [ "$(readlink "$SEN_INSTALL_HOME/current")" = "0.5.2-x86_64-linux-gnu-12.4.0" ]
}

#---------------------------------------------------------------------------------------------------------------
# End-to-end happy path (mocked curl, real tarball)
#---------------------------------------------------------------------------------------------------------------

# Mock curl for the full pipeline: /releases/tags/* serves the JSON fixture; *-release.tar.gz copies a pre-staged
# tarball to the -o destination; *SHA256SUMS returns 22 (verify_checksum takes the soft-skip branch).
mock_curl_full_install() {
    local fixture_json="$1"
    local fixture_tarball="$2"
    eval "curl() {
        local a url='' out='' next=''
        for a in \"\$@\"; do
            if [ \"\$next\" = 'out' ]; then out=\"\$a\"; next=''; continue; fi
            if [ \"\$a\" = '-o' ]; then next='out'; continue; fi
            case \"\$a\" in http*) url=\"\$a\" ;; esac
        done
        case \"\$url\" in
            */releases/tags/*) cat '$fixture_json'; return 0 ;;
            *.tar.gz|*.zip)    cp '$fixture_tarball' \"\$out\"; return 0 ;;
            *SHA256SUMS)       return 22 ;;
            *)                 return 22 ;;
        esac
    }"
}

# Builds a tarball shaped like a Sen release and prints its path. The fake `sen` answers
# `completion <shell>` so cache_completions populates share/sen-completions/; a plain-text fake
# no-ops silently and the assertion then passes on nothing.
stage_release_tarball() {
    local stage="$SEN_TEST_TMPDIR/build-stage-$$"
    mkdir -p "$stage/bin" "$stage/lib" "$stage/include" "$stage/share"
    cat > "$stage/bin/sen" <<'SCRIPT'
#!/bin/sh
[ "$1" = "completion" ] && printf '# fake %s completions\n' "$2"
SCRIPT
    printf 'fake-library\n' > "$stage/lib/libsen.so"
    printf 'fake-header\n' > "$stage/include/sen.h"
    chmod +x "$stage/bin/sen"
    local tarball="$SEN_TEST_TMPDIR/release-$$.tar.gz"
    (cd "$stage" && tar -czf "$tarball" .)
    printf '%s' "$tarball"
}

@test "do_install: end-to-end installs prefix, manifest, both activate scripts, completion cache" {
    load_install

    # Build a real .tar.gz that looks like a Sen release. The fake `sen` responds to `completion <shell>` so
    # cache_completions actually populates share/sen-completions/ (a plain-text fake would no-op silently).
    local stage="$SEN_TEST_TMPDIR/build-stage"
    mkdir -p "$stage/bin" "$stage/lib" "$stage/include" "$stage/share"
    cat > "$stage/bin/sen" <<'SCRIPT'
#!/bin/sh
[ "$1" = "completion" ] && printf '# fake %s completions\n' "$2"
SCRIPT
    printf 'fake-library\n' > "$stage/lib/libsen.so"
    printf 'fake-header\n'  > "$stage/include/sen.h"
    chmod +x "$stage/bin/sen"
    local tarball="$SEN_TEST_TMPDIR/release.tar.gz"
    (cd "$stage" && tar -czf "$tarball" .)

    mock_curl_full_install "$(fixture_path release-0.5.2.json)" "$tarball"
    detect_compiler() { return 1; }

    SENV_VERSION_ARG="0.5.2"
    SENV_COMPILER=""
    SENV_NON_INTERACTIVE=1

    run do_install
    [ "$status" -eq 0 ]

    local prefix="$SEN_INSTALL_HOME/0.5.2-x86_64-linux-gnu-12.4.0"
    [ -d "$prefix" ]
    [ -x "$prefix/bin/sen" ]
    [ -f "$prefix/lib/libsen.so" ]
    [ -f "$prefix/include/sen.h" ]
    [ -f "$prefix/.sen-manifest.sha256" ]
    [ -f "$prefix/activate" ]
    [ -f "$prefix/activate.fish" ]

    # `current` symlink points at the freshly-installed build (relative target: readlink returns the build-id).
    [ -L "$SEN_INSTALL_HOME/current" ]
    [ "$(readlink "$SEN_INSTALL_HOME/current")" = "0.5.2-x86_64-linux-gnu-12.4.0" ]
    [ -f "$SEN_INSTALL_HOME/current/activate" ]   # symlink resolves into the build dir

    # Activate scripts must NOT be in the manifest (generated post-unpack; including them would fail sha256sum -c).
    grep -q '^[a-f0-9]* .*activate$'      "$prefix/.sen-manifest.sha256" && return 1
    grep -q '^[a-f0-9]* .*activate.fish$' "$prefix/.sen-manifest.sha256" && return 1
    return 0
}

#---------------------------------------------------------------------------------------------------------------
# Cache hit (fetch_archive reuses a previously-downloaded archive)
#---------------------------------------------------------------------------------------------------------------

@test "fetch_archive: cache hit when the archive already exists; no curl call" {
    load_install
    local fname="sen-0.5.2-x86_64-linux-gnu-12.4.0-release.tar.gz"
    mkdir -p "$SEN_INSTALL_HOME/cache"
    printf 'pre-staged\n' > "$SEN_INSTALL_HOME/cache/$fname"
    # Mock curl to fail loudly if invoked. A cache hit must not touch the network.
    curl() { printf 'curl was called\n' >&2; return 99; }
    # Call directly (not via `run`) so SENV_ARCHIVE_PATH propagates into the test scope. fetch_archive itself is
    # silent on a cache hit; the user-visible "using cached" narration is printed by do_install.
    fetch_archive "https://example/$fname" "$fname"
    [ "$SENV_ARCHIVE_PATH" = "$SEN_INSTALL_HOME/cache/$fname" ]
    [ "$(cat "$SEN_INSTALL_HOME/cache/$fname")" = "pre-staged" ]
}

@test "fetch_archive: corrupt cache hit is caught by verify_checksum, archive deleted" {
    # SHA256SUMS published + hash mismatch → verify_checksum removes the cache; a real flow would then re-download.
    load_install
    local fname="sen-0.5.2-x86_64-linux-gnu-12.4.0-release.tar.gz"
    mkdir -p "$SEN_INSTALL_HOME/cache"
    printf 'corrupt-cached-content\n' > "$SEN_INSTALL_HOME/cache/$fname"
    # Mock curl to serve a SHA256SUMS that doesn't match the cached file.
    SENV_SUMS_URL="https://example/releases/download/0.5.2/SHA256SUMS"
    eval "curl() {
        local a url=''
        for a in \"\$@\"; do case \"\$a\" in http*) url=\"\$a\" ;; esac; done
        case \"\$url\" in
            *SHA256SUMS) printf '0000000000000000000000000000000000000000000000000000000000000000  %s\n' '$fname' ;;
            *) return 22 ;;
        esac
    }"
    run verify_checksum "0.5.2" "$fname" "$SEN_INSTALL_HOME/cache/$fname"
    [ "$status" -ne 0 ]
    [[ "$output" == *"checksum mismatch"* ]]
    [ ! -f "$SEN_INSTALL_HOME/cache/$fname" ]
}

#---------------------------------------------------------------------------------------------------------------
# Rollback (failure after unpack must not leave a half-installed prefix)
#---------------------------------------------------------------------------------------------------------------

@test "do_install: rollback removes the prefix when unpack fails after creating it" {
    # Force write_activate_scripts to fail; the EXIT trap should rm -rf the prefix.
    load_install

    # Build a minimal real tarball so resolve+fetch+verify+unpack succeed.
    local stage="$SEN_TEST_TMPDIR/stage"
    mkdir -p "$stage/bin"
    printf 'fake\n' > "$stage/bin/sen"
    chmod +x "$stage/bin/sen"
    local tarball="$SEN_TEST_TMPDIR/release.tar.gz"
    (cd "$stage" && tar -czf "$tarball" .)

    eval "curl() {
        local a url='' out='' next=''
        for a in \"\$@\"; do
            if [ \"\$next\" = 'out' ]; then out=\"\$a\"; next=''; continue; fi
            if [ \"\$a\" = '-o' ]; then next='out'; continue; fi
            case \"\$a\" in http*) url=\"\$a\" ;; esac
        done
        case \"\$url\" in
            */releases/tags/*) cat '$(fixture_path release-0.5.2.json)' ;;
            *-release.tar.gz)  cp '$tarball' \"\$out\" ;;
            *SHA256SUMS)       return 22 ;;
            *)                 return 22 ;;
        esac
    }"
    detect_compiler() { return 1; }
    # Force write_activate_scripts to fail after unpack has created the prefix.
    write_activate_scripts() { return 1; }

    SENV_VERSION_ARG="0.5.2"
    SENV_COMPILER=""
    SENV_NON_INTERACTIVE=1

    local prefix="$SEN_INSTALL_HOME/0.5.2-x86_64-linux-gnu-12.4.0"

    # Run in a subshell so the EXIT trap fires on its exit. POSIX exempts AND-OR members from set -e, so we drop
    # errexit in the outer shell and re-arm it inside the subshell so write_activate_scripts' failure aborts.
    set +e
    (set -e; do_install)
    set -e

    [ ! -d "$prefix" ]
}

#---------------------------------------------------------------------------------------------------------------
# Install lock
#---------------------------------------------------------------------------------------------------------------

@test "with_install_lock: refuses to start when another install holds the lock" {
    if ! command -v flock >/dev/null 2>&1; then
        skip "flock not available"
    fi
    load_install
    mkdir -p "$SEN_INSTALL_HOME"
    # Hold an exclusive lock on the file via fd 8; flock -n 9 inside the lock function will then fail to acquire.
    exec 8>"$SEN_INSTALL_HOME/.lock-install"
    flock -x 8
    payload() { printf 'inside-lock\n'; }
    run with_install_lock payload
    [ "$status" -ne 0 ]
    [[ "$output" == *"in progress"* ]]
    [[ "$output" != *"inside-lock"* ]]
    exec 8>&-
}

#---------------------------------------------------------------------------------------------------------------
# verify_checksum: once the release lists SHA256SUMS, every failure is fatal
#---------------------------------------------------------------------------------------------------------------

@test "verify_checksum: a draft's untagged asset URL verifies" {
    # A draft serves its assets under releases/download/untagged-<hash>/, where a URL built from
    # the tag 404s.
    load_install
    local fname="x.tar.gz"
    mkdir -p "$SEN_INSTALL_HOME/cache"
    printf 'fake-archive-content\n' > "$SEN_INSTALL_HOME/cache/$fname"
    local hex
    hex=$(sha256sum "$SEN_INSTALL_HOME/cache/$fname" | awk '{print $1}')
    mock_curl_sums "$hex $fname" \
        "https://example/releases/download/untagged-e9edc999fecbaa6951d6/SHA256SUMS"
    run verify_checksum 0.7.0-rc1 "$fname" "$SEN_INSTALL_HOME/cache/$fname"
    [ "$status" -eq 0 ]
    [[ "$output" == *"Verified"* ]]
}

@test "verify_checksum: listed but unfetchable is fatal and deletes the archive" {
    load_install
    local fname="x.tar.gz"
    mkdir -p "$SEN_INSTALL_HOME/cache"
    printf 'fake-archive-content\n' > "$SEN_INSTALL_HOME/cache/$fname"
    SENV_SUMS_URL="https://example/releases/download/0.7.0-rc1/SHA256SUMS"
    curl() { return 22; }
    run verify_checksum 0.7.0-rc1 "$fname" "$SEN_INSTALL_HOME/cache/$fname"
    [ "$status" -ne 0 ]
    [[ "$output" == *"could not be fetched"* ]]
    [ ! -f "$SEN_INSTALL_HOME/cache/$fname" ]
}

@test "verify_checksum: no entry for our archive is fatal and deletes it" {
    # What a short artefact set looks like: sums present, our archive missing from them.
    load_install
    local fname="x.tar.gz"
    mkdir -p "$SEN_INSTALL_HOME/cache"
    printf 'fake-archive-content\n' > "$SEN_INSTALL_HOME/cache/$fname"
    mock_curl_sums "0000000000000000000000000000000000000000000000000000000000000000 other.tar.gz"
    run verify_checksum 0.5.2 "$fname" "$SEN_INSTALL_HOME/cache/$fname"
    [ "$status" -ne 0 ]
    [[ "$output" == *"no entry for"* ]]
    [ ! -f "$SEN_INSTALL_HOME/cache/$fname" ]
}

@test "do_install: a release candidate installs, with the hyphenated version intact" {
    # The build id is <version>-<arch>-<os>-<compiler>-<compilerver>, so a version containing a
    # hyphen has to survive the naming.
    load_install
    mock_curl_full_install "$(fixture_path release-rc.json)" "$(stage_release_tarball)"
    detect_compiler() { return 1; }
    SENV_VERSION_ARG="0.6.0-rc1"
    SENV_COMPILER=""
    SENV_NON_INTERACTIVE=1
    run do_install
    [ "$status" -eq 0 ]
    local prefix="$SEN_INSTALL_HOME/0.6.0-rc1-x86_64-linux-gnu-12.4.0"
    [ -d "$prefix" ]
    [ -x "$prefix/bin/sen" ]
    [ -f "$prefix/activate" ]
    [ "$(readlink "$SEN_INSTALL_HOME/current")" = "0.6.0-rc1-x86_64-linux-gnu-12.4.0" ]
}

@test "do_install: a build type other than the default installs beside the release, not over it" {
    # Two build types must not collide, or the already-installed check hands back whichever
    # arrived first.
    load_install
    mock_curl_full_install "$(fixture_path release-with-debug-symbols.json)" "$(stage_release_tarball)"
    detect_compiler() { return 1; }
    SENV_COMPILER=""
    SENV_NON_INTERACTIVE=1

    SENV_VERSION_ARG="0.5.2"
    SENV_BUILD_TYPE="release"
    run do_install
    [ "$status" -eq 0 ]

    SENV_BUILD_TYPE="relwithdebinfo"
    run do_install
    [ "$status" -eq 0 ]

    local dirs
    dirs=$(find "$SEN_INSTALL_HOME" -maxdepth 1 -type d -name '0.5.2*' | sort | tr '\n' ' ')
    # two distinct prefixes, and neither named after an archive
    [[ "$dirs" == *"0.5.2-x86_64-linux-gnu-12.4.0 "* ]]
    [[ "$dirs" == *"-relwithdebinfo"* ]]
    [[ "$dirs" != *".tar.gz"* ]]
}

@test "do_install: installing a candidate says so, and the activate scripts carry it" {
    # current is retargeted unconditionally and the docs say to source it from a shell rc, so the
    # variable is what a prompt or a bug report can read afterwards.
    load_install
    mock_curl_full_install "$(fixture_path release-rc.json)" "$(stage_release_tarball)"
    detect_compiler() { return 1; }
    SENV_VERSION_ARG="0.6.0-rc1"
    SENV_COMPILER=""
    SENV_NON_INTERACTIVE=1

    run do_install
    [ "$status" -eq 0 ]
    [[ "$output" == *"release candidate"* ]]

    local prefix="$SEN_INSTALL_HOME/0.6.0-rc1-x86_64-linux-gnu-12.4.0"
    grep -q "SEN_PRERELEASE='1'" "$prefix/activate"
    grep -q "SEN_PRERELEASE '1'" "$prefix/activate.fish"
}

@test "do_install: installing a supported release says nothing about candidates" {
    load_install
    mock_curl_full_install "$(fixture_path release-0.5.2.json)" "$(stage_release_tarball)"
    detect_compiler() { return 1; }
    SENV_VERSION_ARG="0.5.2"
    SENV_COMPILER=""
    SENV_NON_INTERACTIVE=1

    run do_install
    [ "$status" -eq 0 ]
    [[ "$output" != *"release candidate"* ]]
    grep -q "SEN_PRERELEASE='0'" "$SEN_INSTALL_HOME/0.5.2-x86_64-linux-gnu-12.4.0/activate"
}
