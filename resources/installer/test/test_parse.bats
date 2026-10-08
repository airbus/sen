# === test_parse.bats ==================================================================================================
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

@test "reads gcc and its version from a legacy asset name that says gnu" {
    load_install
    result=$(parse_toolchain \
        "https://github.com/airbus/sen/releases/download/0.5.2/sen-0.5.2-x86_64-linux-gnu-12.4.0-release.tar.gz")
    [ "$result" = "gcc 12.4.0" ]
}

@test "reads the toolchain from an asset name with an explicit gcc slot" {
    load_install
    result=$(parse_toolchain "https://example/sen-0.6.0-x86_64-linux-gcc-12.4.0-release.tar.gz")
    [ "$result" = "gcc 12.4.0" ]
}

@test "reads a clang toolchain from the asset name" {
    load_install
    result=$(parse_toolchain "https://example/sen-0.6.0-x86_64-linux-clang-16.0.0-release.tar.gz")
    [ "$result" = "clang 16.0.0" ]
}

@test "reads an msvc toolchain from a windows zip name" {
    load_install
    result=$(parse_toolchain "https://example/sen-0.5.2-amd64-windows-msvc-19.44.35223.0-release.zip")
    [ "$result" = "msvc 19.44.35223.0" ]
}

@test "reads the toolchain despite a hyphenated rc version in a legacy name" {
    load_install
    result=$(parse_toolchain "https://example/sen-0.6.0-rc1-x86_64-linux-gnu-13.2.0-release.tar.gz")
    [ "$result" = "gcc 13.2.0" ]
}

@test "reads the toolchain from a bare filename with no URL" {
    load_install
    result=$(parse_toolchain "sen-0.5.2-x86_64-linux-gnu-12.4.0-release.tar.gz")
    [ "$result" = "gcc 12.4.0" ]
}

#---------------------------------------------------------------------------------------------------------------
# host_arch / host_os overrides
#---------------------------------------------------------------------------------------------------------------

@test "SEN_HOST_ARCH overrides the detected machine architecture" {
    load_install
    SEN_HOST_ARCH=ppc64le
    [ "$(host_arch)" = "ppc64le" ]
}

@test "normalises the amd64 architecture alias to x86_64" {
    load_install
    unset SEN_HOST_ARCH
    # We can't override uname; redefine it as a function.
    uname() { [ "$1" = "-m" ] && printf 'amd64' || printf 'Linux'; }
    [ "$(host_arch)" = "x86_64" ]
}

@test "normalises the arm64 architecture alias to aarch64" {
    load_install
    unset SEN_HOST_ARCH
    uname() { [ "$1" = "-m" ] && printf 'arm64' || printf 'Linux'; }
    [ "$(host_arch)" = "aarch64" ]
}

@test "SEN_HOST_OS overrides the detected operating system" {
    load_install
    SEN_HOST_OS=freebsd
    [ "$(host_os)" = "freebsd" ]
}

#---------------------------------------------------------------------------------------------------------------
# split_archive_name: every build type, and the names it has to refuse
#---------------------------------------------------------------------------------------------------------------

@test "splits every known build type off the archive stem" {
    load_install
    for bt in release debug relwithdebinfo symbols; do
        result=$(split_archive_name "sen-0.7.0-rc1-x86_64-linux-gnu-12.4.0-$bt.tar.gz")
        [ "$result" = "sen-0.7.0-rc1-x86_64-linux-gnu-12.4.0 $bt" ]
    done
}

@test "splits a windows zip name the same way as a tarball" {
    load_install
    result=$(split_archive_name "sen-0.7.0-rc1-amd64-windows-msvc-19.44.0-symbols.zip")
    [ "$result" = "sen-0.7.0-rc1-amd64-windows-msvc-19.44.0 symbols" ]
}

@test "refuses an archive name with an unknown build type" {
    load_install
    run split_archive_name "sen-0.7.0-rc1-x86_64-linux-gnu-12.4.0-nonsense.tar.gz"
    [ "$status" -eq 1 ]
}

@test "refuses an archive name with an unknown extension" {
    load_install
    run split_archive_name "sen-0.7.0-rc1-x86_64-linux-gnu-12.4.0-release.tar.bz2"
    [ "$status" -eq 1 ]
}

@test "refuses an archive name with a two-segment build type" {
    # Two segments where the scheme allows one: this parses as a toolchain called
    # "12.4.0 release" if the guard is removed.
    load_install
    run split_archive_name "sen-0.7.0-rc1-x86_64-linux-gnu-12.4.0-release-symbols.tar.gz"
    [ "$status" -eq 1 ]
}

@test "reads a symbols archive as carrying its build's toolchain" {
    load_install
    result=$(parse_toolchain "sen-0.7.0-rc1-x86_64-linux-gnu-12.4.0-symbols.tar.gz")
    [ "$result" = "gcc 12.4.0" ]
}
