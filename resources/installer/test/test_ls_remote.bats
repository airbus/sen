# === test_ls_remote.bats ==============================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================
#
# The version listing. It needs its own mock and fixture: the shared mock serves /releases/tags/
# and this endpoint returns an array where every other fixture is a single release object.
#
# The fixture mirrors the live response: tag_name appears once per release and nowhere nested, and
# it carries the immutable key GitHub added after the older fixtures were written.

load test_helpers

@test "lists one release per line, newest first, each with its stability" {
    load_install
    mock_curl_with_listing "${BATS_TEST_DIRNAME}/fixtures/releases-listing.json"
    run ls_remote
    [ "$status" -eq 0 ]
    [ "${lines[0]}" = "$(printf '0.7.0-rc1\tprerelease')" ]
    [ "${lines[1]}" = "$(printf '0.6.0\tstable')" ]
    [ "${lines[2]}" = "$(printf '0.5.2\tstable')" ]
    [ "${#lines[@]}" -eq 3 ]
}

@test "keeps each version paired with its stability when the response gains a new key" {
    # GitHub has inserted one there already.
    load_install
    local fixture="$SEN_TEST_TMPDIR/with-new-key.json"
    sed 's/"prerelease"/"something_new": false,\n    "prerelease"/' \
        "${BATS_TEST_DIRNAME}/fixtures/releases-listing.json" > "$fixture"
    mock_curl_with_listing "$fixture"
    run ls_remote
    [ "$status" -eq 0 ]
    [ "${lines[0]}" = "$(printf '0.7.0-rc1\tprerelease')" ]
    [ "${lines[1]}" = "$(printf '0.6.0\tstable')" ]
}

@test "refuses a release list it cannot read instead of printing it empty" {
    # A minified response is how this breaks in practice. Printing nothing would read as
    # "no releases"; a partial list would offer tags somebody then types.
    load_install
    local fixture="$SEN_TEST_TMPDIR/minified.json"
    tr -d '\n ' < "${BATS_TEST_DIRNAME}/fixtures/releases-listing.json" > "$fixture"
    mock_curl_with_listing "$fixture"
    run ls_remote
    [ "$status" -ne 0 ]
    [[ "$output" == *"could not read the release list"* ]]
}

@test "asks for a full page of releases so older ones cannot drop off" {
    # The endpoint pages at 30 and nothing reads the Link header.
    load_install
    local seen="$SEN_TEST_TMPDIR/url"
    eval "curl() {
        local a url=''
        for a in \"\$@\"; do case \"\$a\" in http*) url=\"\$a\" ;; esac; done
        printf '%s' \"\$url\" > '$seen'
        printf '[]'
    }"
    ls_remote >/dev/null
    [[ "$(cat "$seen")" == *"per_page=100"* ]]
}

@test "reports a failed release query and returns non-zero" {
    load_install
    curl() { return 22; }
    run ls_remote
    [ "$status" -ne 0 ]
    [[ "$output" == *"could not query"* ]]
}

@test "prints nothing and succeeds when the repository has no releases" {
    # What an unreleased repository returns, and it must not read as a failure.
    load_install
    eval "curl() { printf '[]'; }"
    run ls_remote
    [ "$status" -eq 0 ]
    [ -z "$output" ]
}

@test "the listing puts candidates in their own section, below the releases" {
    load_install
    mock_curl_with_listing "${BATS_TEST_DIRNAME}/fixtures/releases-listing.json"
    ensure_tools() { return 0; }
    run main
    [ "$status" -eq 0 ]
    [[ "$output" == *"Available Sen releases"* ]]
    [[ "$output" == *"Release candidates"* ]]
    [[ "$output" == *"not for production"* ]]
    # the candidate must not appear above the supported release
    local releases_at candidates_at
    releases_at=$(printf '%s\n' "$output" | grep -n '0.6.0' | head -1 | cut -d: -f1)
    candidates_at=$(printf '%s\n' "$output" | grep -n '0.7.0-rc1' | head -1 | cut -d: -f1)
    [ "$releases_at" -lt "$candidates_at" ]
}

@test "the listing omits the candidates section when there are none" {
    load_install
    mock_curl_with_listing "${BATS_TEST_DIRNAME}/fixtures/releases-listing-stable-only.json"
    ensure_tools() { return 0; }
    run main
    [ "$status" -eq 0 ]
    [[ "$output" == *"Available Sen releases"* ]]
    [[ "$output" != *"Release candidates"* ]]
}
