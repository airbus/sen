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

/// Writes a make-style depfile naming `target` and every path in `dependencies`.
///
/// One target, written exactly as given: ninja reads the first and refuses the file unless it
/// matches the edge's first output character for character. Only the build system has that
/// form, so it supplies it. Dependency paths are absolute, which CMake resolves against the
/// build directory.
void writeDepFile(const std::filesystem::path& path,
                  const std::string& target,
                  const std::set<std::filesystem::path>& dependencies);

/// The target to name: `given` when the build system supplied one, else the first file
/// written, resolved. `written` must not be empty.
std::string depFileTargetOr(const std::string& given, const std::vector<std::filesystem::path>& written);

#endif  // SEN_APPS_CLI_GEN_SRC_IO_H
