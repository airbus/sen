// === util.h ==========================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_APPS_CLI_PACKAGE_UTIL_H
#define SEN_APPS_CLI_PACKAGE_UTIL_H

// cli
#include <CLI/App.hpp>

// inja
#include <inja/json.hpp>

// std
#include <filesystem>

void setupInitPackage(CLI::App& app);
void setupInitComponent(CLI::App& app);

void writeFile(std::filesystem::path path,
               const unsigned* templateData,
               unsigned templateDataSize,
               const inja::json& packageData);

#endif  // SEN_APPS_CLI_PACKAGE_UTIL_H
