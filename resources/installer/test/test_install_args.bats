# === test_install_args.bats ===========================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================
#
# parse_args edge cases; observes the parsed-arg globals without touching the network.

load test_helpers

@test "accepts the --compiler=<value> equals form" {
    load_install
    parse_args 0.5.2 --compiler=gcc-12.4.0
    [ "$SENV_VERSION_ARG" = "0.5.2" ]
    [ "$SENV_COMPILER" = "gcc-12.4.0" ]
    [ "$SENV_NON_INTERACTIVE" = "0" ]
}

@test "accepts --compiler with its value as a separate argument" {
    load_install
    parse_args 0.5.2 --compiler clang-16.0.0
    [ "$SENV_COMPILER" = "clang-16.0.0" ]
}

@test "both -y and --yes make the run non-interactive" {
    load_install
    parse_args 0.5.2 -y
    [ "$SENV_NON_INTERACTIVE" = "1" ]
    parse_args 0.5.2 --yes
    [ "$SENV_NON_INTERACTIVE" = "1" ]
}

@test "accepts the --allow-root flag" {
    load_install
    parse_args 0.5.2 --allow-root
    [ "$SENV_ALLOW_ROOT" = "1" ]
}

@test "rejects an unknown flag with an unknown-option error" {
    load_install
    run parse_args 0.5.2 --frobnicate
    [ "$status" -ne 0 ]
    [[ "$output" == *"unknown option"* ]]
}

@test "rejects a second positional argument as unexpected" {
    load_install
    run parse_args 0.5.2 0.5.3
    [ "$status" -ne 0 ]
    [[ "$output" == *"unexpected argument"* ]]
}

@test "accepts an invocation with no version argument" {
    load_install
    parse_args
    [ -z "$SENV_VERSION_ARG" ]
}

@test "rejects --compiler without a value" {
    load_install
    run parse_args 0.5.2 --compiler
    [ "$status" -ne 0 ]
    [[ "$output" == *"--compiler"* ]]
}

@test "rejects --compiler= with an empty value" {
    load_install
    run parse_args 0.5.2 --compiler=
    [ "$status" -ne 0 ]
    [[ "$output" == *"--compiler"* ]]
}
