
// === cli_archive_setup.h =============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_APPS_CLI_PACKAGE_ARCHIVE_SETUP_H
#define SEN_APPS_CLI_PACKAGE_ARCHIVE_SETUP_H

// sen
#include "sen/core/base/duration.h"
#include "sen/core/base/numbers.h"

// cli11
#include <CLI/App.hpp>
#include <CLI/CLI.hpp>  // NOLINT (misc-include-cleaner): to correctly link

float64_t toSeconds(const sen::Duration& duration);

void setupInfo(CLI::App& app);

void setupIndexed(CLI::App& app);

void setupMerger(CLI::App& app);

#endif  // SEN_APPS_CLI_PACKAGE_ARCHIVE_SETUP_H
