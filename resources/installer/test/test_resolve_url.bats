# === test_resolve_url.bats ============================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================
#
# resolve_url: API + filter + match selection. Menu UI is not exercised here (driven through non_interactive=1
# or --compiler); the menu is verified by hand.

load test_helpers

# Stub curl to serve $fixture for any release-tag URL; pretend no compilers are detected so warn_compat stays quiet.
mock_curl_resolve() {
    local fixture="$1"
    eval "curl() {
        local url
        for a in \"\$@\"; do url=\"\$a\"; done
        case \"\$url\" in
            */releases/tags/*) cat \"$fixture\"; return 0 ;;
            *) return 22 ;;
        esac
    }"
    detect_compiler() { return 1; }
}

@test "picks the only matching build without a menu" {
    load_install
    mock_curl_resolve "$(fixture_path release-0.5.2.json)"
    resolve_url "0.5.2" "" "0"
    [[ "$SENV_RESOLVED_URL" == *"sen-0.5.2-x86_64-linux-gnu-12.4.0-release.tar.gz" ]]
    [ "$SENV_RESOLVED_USED_MENU" = "0" ]
}

@test "--compiler picks its build when several are available" {
    load_install
    mock_curl_resolve "$(fixture_path release-multi-build.json)"
    resolve_url "0.6.0" "13.2.0" "0"
    [[ "$SENV_RESOLVED_URL" == *"-13.2.0-release.tar.gz" ]]
}

@test "fails when --compiler matches no build and lists the compilers that exist" {
    load_install
    mock_curl_resolve "$(fixture_path release-multi-build.json)"
    run resolve_url "0.6.0" "99.99.99" "0"
    [ "$status" -ne 0 ]
    [[ "$output" == *"no build for 0.6.0 with compiler 99.99.99"* ]]
    [[ "$output" == *"12.4.0"* ]]
    [[ "$output" == *"13.2.0"* ]]
    [[ "$output" == *"14.1.0"* ]]
}

@test "refuses to choose among several builds in a non-interactive run" {
    load_install
    mock_curl_resolve "$(fixture_path release-multi-build.json)"
    run resolve_url "0.6.0" "" "1"
    [ "$status" -ne 0 ]
    [[ "$output" == *"multiple builds available"* ]]
}

@test "--compiler gcc-X.Y picks the gcc build among several compilers" {
    load_install
    mock_curl_resolve "$(fixture_path release-multi-compiler.json)"
    resolve_url "0.7.0" "gcc-13.2.0" "0"
    [[ "$SENV_RESOLVED_URL" == *"-gcc-13.2.0-release.tar.gz" ]]
}

@test "--compiler clang-X.Y picks the clang build among several compilers" {
    load_install
    mock_curl_resolve "$(fixture_path release-multi-compiler.json)"
    resolve_url "0.7.0" "clang-16.0.0" "0"
    [[ "$SENV_RESOLVED_URL" == *"-clang-16.0.0-release.tar.gz" ]]
}

@test "a bare --compiler X.Y is treated as gcc" {
    load_install
    mock_curl_resolve "$(fixture_path release-multi-compiler.json)"
    resolve_url "0.7.0" "12.4.0" "0"
    [[ "$SENV_RESOLVED_URL" == *"-gcc-12.4.0-release.tar.gz" ]]
}

@test "a non-interactive refusal lists the compilers as name-version pairs" {
    load_install
    mock_curl_resolve "$(fixture_path release-multi-compiler.json)"
    run resolve_url "0.7.0" "" "1"
    [ "$status" -ne 0 ]
    [[ "$output" == *"gcc-12.4.0"* ]]
    [[ "$output" == *"gcc-13.2.0"* ]]
    [[ "$output" == *"clang-16.0.0"* ]]
}

@test "an aarch64 host filters out x86_64 builds and picks its own" {
    load_install
    SEN_HOST_ARCH=aarch64
    mock_curl_resolve "$(fixture_path release-multi-compiler.json)"
    resolve_url "0.7.0" "" "0"
    [[ "$SENV_RESOLVED_URL" == *"aarch64-linux-clang-17.0.6"* ]]
}

@test "reports no build for the host when only other architectures exist" {
    load_install
    SEN_HOST_ARCH=x86_64
    cat > "$SEN_TEST_TMPDIR/aarch64-only.json" <<'EOF'
{
  "tag_name": "0.7.0",
  "assets": [
    {"browser_download_url": "https://github.com/airbus/sen/releases/download/0.7.0/sen-0.7.0-aarch64-linux-clang-17.0.6-release.tar.gz"}
  ]
}
EOF
    mock_curl_resolve "$SEN_TEST_TMPDIR/aarch64-only.json"
    run resolve_url "0.7.0" "" "0"
    [ "$status" -ne 0 ]
    [[ "$output" == *"x86_64-linux"* ]]
}

@test "resolves an rc-tagged version to its release asset" {
    load_install
    mock_curl_resolve "$(fixture_path release-rc.json)"
    resolve_url "0.6.0-rc1" "" "0"
    [[ "$SENV_RESOLVED_URL" == *"sen-0.6.0-rc1-x86_64-linux-gnu-12.4.0-release.tar.gz" ]]
}

@test "says which release could not be queried when the API call fails" {
    load_install
    curl() { return 22; }
    run resolve_url "9.9.9" "" "0"
    [ "$status" -ne 0 ]
    [[ "$output" == *"could not query release '9.9.9'"* ]]
}

@test "reports no builds for the host when the release has no assets" {
    load_install
    SEN_HOST_OS=linux
    curl() { printf '{}'; return 0; }
    detect_compiler() { return 1; }
    run resolve_url "0.5.2" "" "0"
    [ "$status" -ne 0 ]
    [[ "$output" == *"x86_64-linux"* ]] && [[ "$output" == *"no"* ]] && [[ "$output" == *"builds"* ]]
}

#---------------------------------------------------------------------------------------------------------------
# warn_compat_if_needed
#---------------------------------------------------------------------------------------------------------------

# warn_compat_if_needed now queues notes via defer_note; these tests inspect $_NOTES_QUEUE (the buffer flush_notes
# drains at the end of the install) instead of $output.

@test "queues no compatibility note after an interactive menu choice" {
    load_install
    detect_compiler() { return 1; }
    SENV_RESOLVED_USED_MENU=1
    warn_compat_if_needed \
        "https://example/sen-0.5.2-x86_64-linux-gnu-12.4.0-release.tar.gz"
    [ -z "$_NOTES_QUEUE" ]
}

@test "queues no note when the build's compiler matches the host's" {
    load_install
    detect_compiler() { [ "$1" = gcc ] && printf '12.4.0' || return 1; }
    SENV_RESOLVED_USED_MENU=0
    warn_compat_if_needed \
        "https://example/sen-0.5.2-x86_64-linux-gnu-12.4.0-release.tar.gz"
    [ -z "$_NOTES_QUEUE" ]
}

@test "queues a note when the host compiler's version differs from the build's" {
    load_install
    detect_compiler() { [ "$1" = gcc ] && printf '13.2.0' || return 1; }
    SENV_RESOLVED_USED_MENU=0
    warn_compat_if_needed \
        "https://example/sen-0.5.2-x86_64-linux-gnu-12.4.0-release.tar.gz"
    [[ "$_NOTES_QUEUE" == *"gcc"* ]]
    [[ "$_NOTES_QUEUE" == *"12.4.0"* ]]
    [[ "$_NOTES_QUEUE" == *"13.2.0"* ]]
}

@test "queues a note when the build's compiler is not on the host's PATH" {
    load_install
    detect_compiler() { [ "$1" = gcc ] && printf '12.4.0' || return 1; }
    SENV_RESOLVED_USED_MENU=0
    warn_compat_if_needed \
        "https://example/sen-0.6.0-x86_64-linux-clang-16.0.0-release.tar.gz"
    [[ "$_NOTES_QUEUE" == *"clang 16.0.0"* ]]
    [[ "$_NOTES_QUEUE" == *"not on your PATH"* ]]
}

@test "a plain run picks the release build" {
    load_install
    mock_curl_resolve "$(fixture_path release-with-debug-symbols.json)"
    resolve_url "0.5.2" "" "0"
    [[ "$SENV_RESOLVED_URL" == *"-release.tar.gz" ]]
}

@test "--debug-symbols picks the archive carrying the debug information" {
    load_install
    mock_curl_resolve "$(fixture_path release-with-debug-symbols.json)"
    SENV_BUILD_TYPE=relwithdebinfo
    resolve_url "0.5.2" "" "0"
    [[ "$SENV_RESOLVED_URL" == *"-relwithdebinfo.tar.gz" ]]
}

@test "two archives for one platform do not force a menu" {
    load_install
    mock_curl_resolve "$(fixture_path release-with-debug-symbols.json)"
    resolve_url "0.5.2" "" "0"
    [ "$SENV_RESOLVED_USED_MENU" = "0" ]
}

# The fixture here is the asset list a release actually published, so these pin the
# resolution against the real names and not against names invented for a test.

@test "--symbols picks the release build's own debug information" {
    load_install
    SEN_HOST_ARCH=x86_64
    SEN_HOST_OS=linux
    mock_curl_resolve "$(fixture_path release-full-set.json)"
    SENV_BUILD_TYPE=symbols
    resolve_url "0.0.0-rc1" "" "0"
    [[ "$SENV_RESOLVED_URL" == *"-symbols.tar.gz" ]]
}

@test "--debug picks the unoptimised build" {
    load_install
    SEN_HOST_ARCH=x86_64
    SEN_HOST_OS=linux
    mock_curl_resolve "$(fixture_path release-full-set.json)"
    SENV_BUILD_TYPE=debug
    resolve_url "0.0.0-rc1" "" "0"
    [[ "$SENV_RESOLVED_URL" == *"-debug.tar.gz" ]]
}

@test "a plain run does not pick up the symbols archive" {
    load_install
    SEN_HOST_ARCH=x86_64
    SEN_HOST_OS=linux
    mock_curl_resolve "$(fixture_path release-full-set.json)"
    resolve_url "0.0.0-rc1" "" "0"
    [[ "$SENV_RESOLVED_URL" == *"-release.tar.gz" ]]
    [[ "$SENV_RESOLVED_URL" != *"symbols"* ]]
}

@test "--debug does not settle for the relwithdebinfo build" {
    load_install
    SEN_HOST_ARCH=x86_64
    SEN_HOST_OS=linux
    mock_curl_resolve "$(fixture_path release-full-set.json)"
    SENV_BUILD_TYPE=debug
    resolve_url "0.0.0-rc1" "" "0"
    [[ "$SENV_RESOLVED_URL" != *"relwithdebinfo"* ]]
}

#---------------------------------------------------------------------------------------------------------------
# What resolve_url has to leave behind for do_install
#---------------------------------------------------------------------------------------------------------------

@test "leaves the release's SHA256SUMS URL behind for the checksum step" {
    # The verify_checksum tests set SENV_SUMS_URL themselves, so only this covers the path that
    # supplies it. Read inside build_candidates it would not survive the command substitution.
    load_install
    mock_curl_with_fixture "${BATS_TEST_DIRNAME}/fixtures/release-full-set.json"
    SENV_SUMS_URL=""
    resolve_url "0.0.0-rc1" "" "1"
    [ -n "$SENV_SUMS_URL" ]
    [[ "$SENV_SUMS_URL" == *"/SHA256SUMS" ]]
}

@test "takes the SHA256SUMS URL from the response, so a draft's untagged URL works" {
    # A draft serves assets under releases/download/untagged-<hash>/, where a url built from the
    # tag 404s.
    load_install
    mock_curl_with_fixture "${BATS_TEST_DIRNAME}/fixtures/release-full-set.json"
    resolve_url "0.0.0-rc1" "" "1"
    [[ "$SENV_SUMS_URL" == *"/untagged-"*"/SHA256SUMS" ]]
}

@test "says when the requested flavour is missing and names the ones that exist" {
    load_install
    mock_curl_with_fixture "${BATS_TEST_DIRNAME}/fixtures/release-0.5.2.json"
    SENV_BUILD_TYPE=symbols
    run resolve_url "0.5.2" "" "1"
    [ "$status" -ne 0 ]
    [[ "$output" == *"no symbols archive"* ]]
    [[ "$output" == *"it has: release"* ]]
}

@test "refuses when it cannot prompt between builds and advises --compiler" {
    # A piped run in CI has no terminal, and the useful answer there is the --compiler list.
    load_install
    mock_curl_with_fixture "${BATS_TEST_DIRNAME}/fixtures/release-multi-compiler.json"
    SENV_FORCE_CAN_PROMPT=0
    run resolve_url "0.6.0" "" "0"
    [ "$status" -ne 0 ]
    [[ "$output" == *"pass --compiler"* ]]
}

@test "a reachable terminal enables the menu, not stdin" {
    # stdin is a pipe under `curl | sh`, the documented invocation, so testing stdin refuses the
    # case /dev/tty exists for.
    load_install
    mock_curl_with_fixture "${BATS_TEST_DIRNAME}/fixtures/release-multi-compiler.json"
    SENV_FORCE_CAN_PROMPT=1
    # No menu input is supplied, so the read fails; what matters is which message we reach.
    run resolve_url "0.6.0" "" "0"
    [[ "$output" != *"pass --compiler"* ]]
}

@test "marks a release candidate as prerelease when resolving it" {
    load_install
    mock_curl_with_fixture "${BATS_TEST_DIRNAME}/fixtures/release-rc.json"
    SENV_IS_PRERELEASE=
    resolve_url "0.6.0-rc1" "" "1"
    [ "$SENV_IS_PRERELEASE" = "1" ]
}

@test "does not mark a supported release as prerelease" {
    load_install
    mock_curl_with_fixture "${BATS_TEST_DIRNAME}/fixtures/release-0.5.2.json"
    SENV_IS_PRERELEASE=
    resolve_url "0.5.2" "" "1"
    [ "$SENV_IS_PRERELEASE" = "0" ]
}
