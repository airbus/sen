// === io.h ============================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_APPS_CLI_GEN_SRC_IO_H
#define SEN_APPS_CLI_GEN_SRC_IO_H

// sen
#include "sen/core/base/assert.h"

// std
#include <filesystem>
#include <fstream>
#include <set>
#include <string>
#include <vector>

/// Opens file
void openFile(const std::filesystem::path& path, std::ofstream& stream);

/// Reads file to contents buffer
void readFile(const std::filesystem::path& fileName, std::string& contents);

/// Writes `body` to `path`, creating parent dirs on demand. Skips the write when the
/// existing file already has identical contents.
void writeFile(const std::filesystem::path& path, const std::string& body);

/// Writes a make-style depfile naming every path in `targets` and `dependencies`.
///
/// Imports are resolved through a search path at generation time, so a build system is
/// told about them this way or not at all. Every target is named rather than the first,
/// because which output a depfile is attached to is the build system's choice, not ours.
/// Paths are absolute: CMake resolves a relative one against the build directory, which
/// is not where the sources are.
void writeDepFile(const std::filesystem::path& path,
                  const std::vector<std::filesystem::path>& targets,
                  const std::set<std::filesystem::path>& dependencies);

#endif  // SEN_APPS_CLI_GEN_SRC_IO_H
