# === test_parse.bats  =================================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================
#
# Pure-helper tests: parse_toolchain, host_arch, host_os. No network, no filesystem mutation.

load test_helpers

#---------------------------------------------------------------------------------------------------------------
# parse_toolchain (legacy gnu, gcc, clang, msvc; rc-tagged versions)
#---------------------------------------------------------------------------------------------------------------

@test "parse_toolchain: legacy 0.5.2 linux asset (gnu means gcc)" {
    load_install
    result=$(parse_toolchain \
        "https://github.com/airbus/sen/releases/download/0.5.2/sen-0.5.2-x86_64-linux-gnu-12.4.0-release.tar.gz")
    [ "$result" = "gcc 12.4.0" ]
}

@test "parse_toolchain: explicit gcc slot" {
    load_install
    result=$(parse_toolchain "https://example/sen-0.6.0-x86_64-linux-gcc-12.4.0-release.tar.gz")
    [ "$result" = "gcc 12.4.0" ]
}

@test "parse_toolchain: clang" {
    load_install
    result=$(parse_toolchain "https://example/sen-0.6.0-x86_64-linux-clang-16.0.0-release.tar.gz")
    [ "$result" = "clang 16.0.0" ]
}

@test "parse_toolchain: msvc on Windows" {
    load_install
    result=$(parse_toolchain "https://example/sen-0.5.2-amd64-windows-msvc-19.44.35223.0-release.zip")
    [ "$result" = "msvc 19.44.35223.0" ]
}

@test "parse_toolchain: rc-tagged version with hyphen, legacy gnu" {
    load_install
    result=$(parse_toolchain "https://example/sen-0.6.0-rc1-x86_64-linux-gnu-13.2.0-release.tar.gz")
    [ "$result" = "gcc 13.2.0" ]
}

@test "parse_toolchain: filename without URL prefix" {
    load_install
    result=$(parse_toolchain "sen-0.5.2-x86_64-linux-gnu-12.4.0-release.tar.gz")
    [ "$result" = "gcc 12.4.0" ]
}

#---------------------------------------------------------------------------------------------------------------
# host_arch / host_os overrides
#---------------------------------------------------------------------------------------------------------------

@test "host_arch: SEN_HOST_ARCH override wins over uname -m" {
    load_install
    SEN_HOST_ARCH=ppc64le
    [ "$(host_arch)" = "ppc64le" ]
}

@test "host_arch: amd64 alias normalises to x86_64" {
    load_install
    unset SEN_HOST_ARCH
    # We can't override uname; redefine it as a function.
    uname() { [ "$1" = "-m" ] && printf 'amd64' || printf 'Linux'; }
    [ "$(host_arch)" = "x86_64" ]
}

@test "host_arch: arm64 alias normalises to aarch64" {
    load_install
    unset SEN_HOST_ARCH
    uname() { [ "$1" = "-m" ] && printf 'arm64' || printf 'Linux'; }
    [ "$(host_arch)" = "aarch64" ]
}

@test "host_os: SEN_HOST_OS override wins over uname -s" {
    load_install
    SEN_HOST_OS=freebsd
    [ "$(host_os)" = "freebsd" ]
}
