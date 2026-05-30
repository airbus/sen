# === test_resolve_url.bats  ===========================================================================================
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

@test "resolve_url: single-match release auto-picks" {
    load_install
    mock_curl_resolve "$(fixture_path release-0.5.2.json)"
    resolve_url "0.5.2" "" "0"
    [[ "$SENV_RESOLVED_URL" == *"sen-0.5.2-x86_64-linux-gnu-12.4.0-release.tar.gz" ]]
    [ "$SENV_RESOLVED_USED_MENU" = "0" ]
}

@test "resolve_url: multi-build, --compiler match" {
    load_install
    mock_curl_resolve "$(fixture_path release-multi-build.json)"
    resolve_url "0.6.0" "13.2.0" "0"
    [[ "$SENV_RESOLVED_URL" == *"-13.2.0-release.tar.gz" ]]
}

@test "resolve_url: multi-build, --compiler does not match: fail and list" {
    load_install
    mock_curl_resolve "$(fixture_path release-multi-build.json)"
    run resolve_url "0.6.0" "99.99.99" "0"
    [ "$status" -ne 0 ]
    [[ "$output" == *"no build for 0.6.0 with compiler 99.99.99"* ]]
    [[ "$output" == *"12.4.0"* ]]
    [[ "$output" == *"13.2.0"* ]]
    [[ "$output" == *"14.1.0"* ]]
}

@test "resolve_url: multi-build, non-interactive refuses with toolchain list" {
    load_install
    mock_curl_resolve "$(fixture_path release-multi-build.json)"
    run resolve_url "0.6.0" "" "1"
    [ "$status" -ne 0 ]
    [[ "$output" == *"multiple builds available"* ]]
}

@test "resolve_url: multi-compiler, --compiler gcc-X.Y picks the gcc build" {
    load_install
    mock_curl_resolve "$(fixture_path release-multi-compiler.json)"
    resolve_url "0.7.0" "gcc-13.2.0" "0"
    [[ "$SENV_RESOLVED_URL" == *"-gcc-13.2.0-release.tar.gz" ]]
}

@test "resolve_url: multi-compiler, --compiler clang-X.Y picks the clang build" {
    load_install
    mock_curl_resolve "$(fixture_path release-multi-compiler.json)"
    resolve_url "0.7.0" "clang-16.0.0" "0"
    [[ "$SENV_RESOLVED_URL" == *"-clang-16.0.0-release.tar.gz" ]]
}

@test "resolve_url: multi-compiler, legacy --compiler X.Y resolves to gcc" {
    load_install
    mock_curl_resolve "$(fixture_path release-multi-compiler.json)"
    resolve_url "0.7.0" "12.4.0" "0"
    [[ "$SENV_RESOLVED_URL" == *"-gcc-12.4.0-release.tar.gz" ]]
}

@test "resolve_url: multi-compiler, non-interactive lists name-version pairs" {
    load_install
    mock_curl_resolve "$(fixture_path release-multi-compiler.json)"
    run resolve_url "0.7.0" "" "1"
    [ "$status" -ne 0 ]
    [[ "$output" == *"gcc-12.4.0"* ]]
    [[ "$output" == *"gcc-13.2.0"* ]]
    [[ "$output" == *"clang-16.0.0"* ]]
}

@test "resolve_url: aarch64 host filters out x86_64 candidates" {
    load_install
    SEN_HOST_ARCH=aarch64
    mock_curl_resolve "$(fixture_path release-multi-compiler.json)"
    resolve_url "0.7.0" "" "0"
    [[ "$SENV_RESOLVED_URL" == *"aarch64-linux-clang-17.0.6"* ]]
}

@test "resolve_url: x86_64 host with only aarch64 builds reports no match" {
    load_install
    SEN_HOST_ARCH=x86_64
    cat > "$BATS_TEST_TMPDIR/aarch64-only.json" <<'EOF'
{
  "tag_name": "0.7.0",
  "assets": [
    {"browser_download_url": "https://github.com/airbus/sen/releases/download/0.7.0/sen-0.7.0-aarch64-linux-clang-17.0.6-release.tar.gz"}
  ]
}
EOF
    mock_curl_resolve "$BATS_TEST_TMPDIR/aarch64-only.json"
    run resolve_url "0.7.0" "" "0"
    [ "$status" -ne 0 ]
    [[ "$output" == *"x86_64-linux"* ]]
}

@test "resolve_url: rc-tagged version is parsed correctly" {
    load_install
    mock_curl_resolve "$(fixture_path release-rc.json)"
    resolve_url "0.6.0-rc1" "" "0"
    [[ "$SENV_RESOLVED_URL" == *"sen-0.6.0-rc1-x86_64-linux-gnu-12.4.0-release.tar.gz" ]]
}

@test "resolve_url: API failure surfaces clean error" {
    load_install
    curl() { return 22; }
    run resolve_url "9.9.9" "" "0"
    [ "$status" -ne 0 ]
    [[ "$output" == *"could not query release '9.9.9'"* ]]
}

@test "resolve_url: empty asset list surfaces clean error" {
    load_install
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

@test "warn_compat: suppressed when menu was used" {
    load_install
    detect_compiler() { return 1; }
    SENV_RESOLVED_USED_MENU=1
    warn_compat_if_needed \
        "https://example/sen-0.5.2-x86_64-linux-gnu-12.4.0-release.tar.gz"
    [ -z "$_NOTES_QUEUE" ]
}

@test "warn_compat: silent when build's compiler matches the user's" {
    load_install
    detect_compiler() { [ "$1" = gcc ] && printf '12.4.0' || return 1; }
    SENV_RESOLVED_USED_MENU=0
    warn_compat_if_needed \
        "https://example/sen-0.5.2-x86_64-linux-gnu-12.4.0-release.tar.gz"
    [ -z "$_NOTES_QUEUE" ]
}

@test "warn_compat: queues note when versions differ" {
    load_install
    detect_compiler() { [ "$1" = gcc ] && printf '13.2.0' || return 1; }
    SENV_RESOLVED_USED_MENU=0
    warn_compat_if_needed \
        "https://example/sen-0.5.2-x86_64-linux-gnu-12.4.0-release.tar.gz"
    [[ "$_NOTES_QUEUE" == *"gcc"* ]]
    [[ "$_NOTES_QUEUE" == *"12.4.0"* ]]
    [[ "$_NOTES_QUEUE" == *"13.2.0"* ]]
}

@test "warn_compat: queues note when compiler is missing (clang build, gcc-only host)" {
    load_install
    detect_compiler() { [ "$1" = gcc ] && printf '12.4.0' || return 1; }
    SENV_RESOLVED_USED_MENU=0
    warn_compat_if_needed \
        "https://example/sen-0.6.0-x86_64-linux-clang-16.0.0-release.tar.gz"
    [[ "$_NOTES_QUEUE" == *"clang 16.0.0"* ]]
    [[ "$_NOTES_QUEUE" == *"not on your PATH"* ]]
}
