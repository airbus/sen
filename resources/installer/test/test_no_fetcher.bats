# === test_no_fetcher.bats  ============================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================
#
# ensure_tools uses only shell builtins (printf), so emptying PATH is enough to simulate a barren container.

load test_helpers

@test "no-tools: ensure_tools lists every missing tool and exits non-zero" {
    load_install
    local saved_path="$PATH"
    PATH=/nonexistent
    export PATH
    run ensure_tools
    PATH="$saved_path"   # restore so bats teardown can find `rm`
    export PATH
    [ "$status" -ne 0 ]
    [[ "$output" == *"missing required tools"* ]]
    [[ "$output" == *"curl"* ]]
    [[ "$output" == *"tar"* ]]
    [[ "$output" == *"sha256sum"* ]]
    [[ "$output" == *"apt install curl"* ]]
}

@test "no-tools: ensure_tools is silent when everything is on PATH" {
    load_install
    run ensure_tools
    [ "$status" -eq 0 ]
    [ -z "$output" ]
}
