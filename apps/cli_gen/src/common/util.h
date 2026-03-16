// === util.h ==========================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_APPS_CLI_GEN_SRC_CPP_UTIL_H
#define SEN_APPS_CLI_GEN_SRC_CPP_UTIL_H

// sen
#include "sen/core/base/assert.h"
#include "sen/core/lang/stl_resolver.h"
#include "sen/core/meta/custom_type.h"

// cli11
#include <CLI/App.hpp>
// NOLINTNEXTLINE (misc-include-cleaner): cli11 needs all headers to correctly link required vtables
#include <CLI/CLI.hpp>

// inja
#include <inja/environment.hpp>

// std
#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <memory>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace impl
{

constexpr std::size_t maxCommentLineLength = 80U;

struct StlArgs
{
  std::vector<std::filesystem::path> inputs;
  std::vector<std::filesystem::path> includePaths;
  std::string basePath;
  std::string codegenOptionsFile;
};

struct FomArgs
{
  std::vector<std::filesystem::path> paths;
  std::vector<std::filesystem::path> mappingFiles;
  std::string codegenOptionsFile;
};

}  // namespace impl

CLI::App* setupStlInput(CLI::App& app, std::function<void(std::shared_ptr<impl::StlArgs>)>&& action);

CLI::App* setupFomInput(CLI::App& app, std::function<void(std::shared_ptr<impl::FomArgs>)>&& action);

/// Appends a prefix to the beginning of a given string
[[nodiscard]] std::string prepend(const std::string& prefix, const std::string str);

/// Appends a prefix to the end of a given string
[[nodiscard]] std::string append(const std::string& lhs, const std::string rhs);

/// Transform a string to uppercase
[[nodiscard]] std::string capitalize(const std::string& str);

/// Transform a string_view to uppercase
[[nodiscard]] std::string capitalize(std::string_view str);

/// Splits a string into substrings by a delimiter, skipping empty results
[[nodiscard]] std::vector<std::string> tokenize(const std::string& str, const char delim);

/// Registers functions used by the templates.
void configureEnv(inja::Environment& env);

/// Opens file
void openFile(const std::filesystem::path& path, std::ofstream& stream);

/// Reads file to contents buffer
void readFile(const std::filesystem::path& fileName, std::string& contents);

/// Appends a header extension to a file's filename to generate a source path
[[nodiscard]] std::filesystem::path typesSourceFileFromSTLFile(const std::filesystem::path& file);

/// Appends a source extension to a file's filename to generate a source path
[[nodiscard]] std::filesystem::path implSourceFileFromSTLFile(const std::filesystem::path& file);

/// Generates a hex-encoded CRC32 hash from a file path
[[nodiscard]] std::string computeFileId(const std::string& fileName);

/// Reads the type settings out of a JSON file.
[[nodiscard]] sen::lang::TypeSettings readTypeSettings(const std::filesystem::path& path);

/// Computes C++ namespace from a string package
[[nodiscard]] std::string computeCppNamespace(const std::vector<std::string>& package);

/// Computes C++ namespace from a type set
[[nodiscard]] std::string computeCppNamespace(const sen::lang::TypeSet& set);

/// Computes C++ namespace from a custom type
[[nodiscard]] std::string computeCppNamespace(const sen::CustomType& type);

template <typename T>
std::string intToHex(T i);

template <typename T>
std::string intToHex2(T i);

template <typename F>
void writeFile(const std::filesystem::path& path, std::vector<std::filesystem::path>& writtenFiles, F&& generator);

//----------------------------------------------------------------------------------------------------------------------
// Inline implementation
//----------------------------------------------------------------------------------------------------------------------

template <typename F>
void writeFile(const std::filesystem::path& path, std::vector<std::filesystem::path>& writtenFiles, F&& generator)
{
  // don't do anything if already written
  if (std::find(writtenFiles.begin(), writtenFiles.end(), path) != writtenFiles.end())
  {
    return;
  }

  const auto fileContents = generator();
  writtenFiles.push_back(path);

  if (const auto parentPath = path.parent_path(); !parentPath.empty() && !std::filesystem::exists(parentPath))
  {
    std::filesystem::create_directories(parentPath);
  }

  // do not rewrite the file if it has the same content
  if (std::filesystem::exists(path))
  {
    std::ifstream in(path);
    std::string existingContents;

    // reserve required memory in one go
    in.seekg(0U, std::ios::end);
    existingContents.reserve(static_cast<std::size_t>(in.tellg()));
    in.seekg(0U, std::ios::beg);

    // read contents
    existingContents.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());

    if (existingContents == fileContents)
    {
      return;
    }
  }

  std::ofstream stream;
  openFile(path, stream);
  stream << fileContents;
  SEN_ENSURE(stream.good());
  stream.close();
}

template <typename T>
[[nodiscard]] std::string intToHex(T i)
{
  std::stringstream stream;
  stream << "0x" << std::setfill('0') << std::setw(sizeof(T) + sizeof(T)) << std::hex << i;
  return stream.str();
}

template <typename T>
[[nodiscard]] std::string intToHex2(T i)
{
  std::stringstream stream;
  stream << "0x" << std::setfill('0') << std::setw(sizeof(T)) << std::hex << i;
  return stream.str();
}

#endif  // SEN_APPS_CLI_GEN_SRC_CPP_UTIL_H
