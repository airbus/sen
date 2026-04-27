# === test_activate.bats  ==============================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================

load test_helpers

BUILD_A="0.5.2-x86_64-linux-gnu-12.4.0"
BUILD_B="0.6.0-x86_64-linux-gcc-13.2.0"

setup_two_builds() {
    make_fake_build "$BUILD_A" >/dev/null
    make_fake_build "$BUILD_B" >/dev/null
    write_activate_scripts "$SEN_INSTALL_HOME/$BUILD_A"
    write_activate_scripts "$SEN_INSTALL_HOME/$BUILD_B"
}

#---------------------------------------------------------------------------------------------------------------
# Generation: file presence and parseability
#---------------------------------------------------------------------------------------------------------------

@test "activate: write_activate_scripts writes both activate and activate.fish" {
    load_install
    make_fake_build "$BUILD_A" >/dev/null
    write_activate_scripts "$SEN_INSTALL_HOME/$BUILD_A"
    [ -f "$SEN_INSTALL_HOME/$BUILD_A/activate" ]
    [ -f "$SEN_INSTALL_HOME/$BUILD_A/activate.fish" ]
}

@test "activate: generated activate parses under bash" {
    load_install
    make_fake_build "$BUILD_A" >/dev/null
    write_activate_scripts "$SEN_INSTALL_HOME/$BUILD_A"
    bash -n "$SEN_INSTALL_HOME/$BUILD_A/activate"
}

@test "activate: generated activate parses under dash" {
    if ! command -v dash >/dev/null 2>&1; then skip "dash not installed"; fi
    load_install
    make_fake_build "$BUILD_A" >/dev/null
    write_activate_scripts "$SEN_INSTALL_HOME/$BUILD_A"
    dash -n "$SEN_INSTALL_HOME/$BUILD_A/activate"
}

@test "activate: generated activate.fish parses under fish" {
    if ! command -v fish >/dev/null 2>&1; then skip "fish not installed"; fi
    load_install
    make_fake_build "$BUILD_A" >/dev/null
    write_activate_scripts "$SEN_INSTALL_HOME/$BUILD_A"
    fish -n "$SEN_INSTALL_HOME/$BUILD_A/activate.fish"
}

#---------------------------------------------------------------------------------------------------------------
# Sourcing: env-var setup, idempotence, build switching
#---------------------------------------------------------------------------------------------------------------

@test "activate: source sets SEN_PREFIX, PATH, CMAKE_PREFIX_PATH, LD_LIBRARY_PATH" {
    load_install
    make_fake_build "$BUILD_A" >/dev/null
    write_activate_scripts "$SEN_INSTALL_HOME/$BUILD_A"
    PATH="/usr/local/bin:/usr/bin:/bin"
    CMAKE_PREFIX_PATH="/some/prior/path"
    unset LD_LIBRARY_PATH
    # shellcheck disable=SC1091
    . "$SEN_INSTALL_HOME/$BUILD_A/activate"
    [ "$SEN_PREFIX" = "$SEN_INSTALL_HOME/$BUILD_A" ]
    [[ "$PATH" == "$SEN_INSTALL_HOME/$BUILD_A/bin:"* ]]
    [[ "$PATH" == *":/usr/local/bin:"* ]]
    [ "$CMAKE_PREFIX_PATH" = "$SEN_INSTALL_HOME/$BUILD_A:/some/prior/path" ]
    # Sen installs shared libs into <prefix>/bin (CMAKE_INSTALL_BINDIR), so LD_LIBRARY_PATH points there too.
    [ "$LD_LIBRARY_PATH" = "$SEN_INSTALL_HOME/$BUILD_A/bin" ]
}

@test "activate: re-sourcing the same activate doesn't accumulate entries" {
    load_install
    make_fake_build "$BUILD_A" >/dev/null
    write_activate_scripts "$SEN_INSTALL_HOME/$BUILD_A"
    PATH="/usr/bin"
    # shellcheck disable=SC1091
    . "$SEN_INSTALL_HOME/$BUILD_A/activate"
    local once="$PATH"
    # shellcheck disable=SC1091
    . "$SEN_INSTALL_HOME/$BUILD_A/activate"
    [ "$PATH" = "$once" ]
}

@test "activate: switching builds drops the previous build's bin/ from PATH" {
    load_install
    setup_two_builds
    PATH="/usr/bin"
    # shellcheck disable=SC1091
    . "$SEN_INSTALL_HOME/$BUILD_A/activate"
    [[ "$PATH" == *"$SEN_INSTALL_HOME/$BUILD_A/bin"* ]]
    # shellcheck disable=SC1091
    . "$SEN_INSTALL_HOME/$BUILD_B/activate"
    [[ "$PATH" != *"$BUILD_A"* ]]
    [[ "$PATH" == *"$SEN_INSTALL_HOME/$BUILD_B/bin"* ]]
}

@test "activate: non-Sen entries in PATH are preserved across activations" {
    load_install
    setup_two_builds
    PATH="/usr/local/bin:/usr/bin:/bin"
    # shellcheck disable=SC1091
    . "$SEN_INSTALL_HOME/$BUILD_A/activate"
    [[ "$PATH" == *":/usr/local/bin:"* ]]
    # shellcheck disable=SC1091
    . "$SEN_INSTALL_HOME/$BUILD_B/activate"
    [[ "$PATH" == *":/usr/local/bin:"* ]]
}

#---------------------------------------------------------------------------------------------------------------
# fish-native activate (only run when fish is installed)
#---------------------------------------------------------------------------------------------------------------

@test "activate.fish: source sets SEN_PREFIX (under fish)" {
    if ! command -v fish >/dev/null 2>&1; then skip "fish not installed"; fi
    load_install
    make_fake_build "$BUILD_A" >/dev/null
    write_activate_scripts "$SEN_INSTALL_HOME/$BUILD_A"
    run fish -c "source $SEN_INSTALL_HOME/$BUILD_A/activate.fish; echo \$SEN_PREFIX"
    [ "$status" -eq 0 ]
    [ "$output" = "$SEN_INSTALL_HOME/$BUILD_A" ]
}

@test "activate.fish: source prepends prefix/bin to PATH (under fish)" {
    if ! command -v fish >/dev/null 2>&1; then skip "fish not installed"; fi
    load_install
    make_fake_build "$BUILD_A" >/dev/null
    write_activate_scripts "$SEN_INSTALL_HOME/$BUILD_A"
    run fish -c "source $SEN_INSTALL_HOME/$BUILD_A/activate.fish; echo \$PATH[1]"
    [ "$status" -eq 0 ]
    [ "$output" = "$SEN_INSTALL_HOME/$BUILD_A/bin" ]
}

@test "activate.fish: switching builds drops the previous build's bin/ from PATH (under fish)" {
    if ! command -v fish >/dev/null 2>&1; then skip "fish not installed"; fi
    load_install
    setup_two_builds
    run fish -c "
        source $SEN_INSTALL_HOME/$BUILD_A/activate.fish
        source $SEN_INSTALL_HOME/$BUILD_B/activate.fish
        for p in \$PATH; echo \$p; end
    "
    [ "$status" -eq 0 ]
    [[ "$output" != *"$BUILD_A"* ]]
    [[ "$output" == *"$BUILD_B/bin"* ]]
}
