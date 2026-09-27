# === test_ls_remote.bats ==============================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================
#
# The version listing. It had no tests and the fixture mock could not reach it: that mock serves
# /releases/tags/ and fails everything else, and every other fixture is a single release object
# where this endpoint returns an array.
#
# The fixture mirrors the live response's shape, checked rather than assumed: tag_name appears
# once per release and nowhere nested, so the grep in ls_remote cannot pick up a key from an
# author or an asset. It also carries the immutable key GitHub has since added, which the older
# fixtures predate.

load test_helpers

@test "ls_remote: prints one tag per release, newest first, as the api ordered them" {
    load_install
    mock_curl_with_listing "${BATS_TEST_DIRNAME}/fixtures/releases-listing.json"
    run ls_remote
    [ "$status" -eq 0 ]
    [ "${lines[0]}" = "0.7.0-rc1" ]
    [ "${lines[1]}" = "0.6.0" ]
    [ "${lines[2]}" = "0.5.2" ]
    [ "${#lines[@]}" -eq 3 ]
}

@test "ls_remote: asks for a full page so older releases cannot drop off" {
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

@test "ls_remote: a failed query reports it and returns non-zero" {
    load_install
    curl() { return 22; }
    run ls_remote
    [ "$status" -ne 0 ]
    [[ "$output" == *"could not query"* ]]
}

@test "ls_remote: a response with no releases prints nothing and succeeds" {
    # An empty array is what an unreleased repository returns. Distinguishing it from a failure
    # matters: they reach the caller as the same empty string today.
    load_install
    eval "curl() { printf '[]'; }"
    run ls_remote
    [ "$status" -eq 0 ]
    [ -z "$output" ]
}
