// === io.cpp ==========================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "io.h"

// sen
#include "sen/core/base/assert.h"

// std
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <ios>
#include <iterator>
#include <set>
#include <string>
#include <vector>

void openFile(const std::filesystem::path& path, std::ofstream& stream)
{
  stream.open(path, std::ios_base::trunc | std::ios_base::out);
  if (!stream.is_open() || stream.fail())
  {
    std::string err;
    err.append("could not open file '");
    err.append(path.string());
    err.append("' for writing");
    sen::throwRuntimeError(err);
  }
}

void readFile(const std::filesystem::path& fileName, std::string& contents)
{
  std::ifstream in(fileName);

  // reserve required memory in one go
  in.seekg(0U, std::ios::end);
  contents.reserve(static_cast<std::size_t>(in.tellg()));
  in.seekg(0U, std::ios::beg);

  // read contents
  contents.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

void writeFile(const std::filesystem::path& path, const std::string& body)
{
  if (const auto parentPath = path.parent_path(); !parentPath.empty() && !std::filesystem::exists(parentPath))
  {
    std::filesystem::create_directories(parentPath);
  }

  if (std::filesystem::exists(path))
  {
    std::string existing;
    readFile(path, existing);
    if (existing == body)
    {
      return;
    }
  }

  std::ofstream stream;
  openFile(path, stream);
  stream << body;
  SEN_ENSURE(stream.good());
  stream.close();
}

namespace
{

/// Escapes the three characters a depfile reader treats as syntax rather than path.
///
/// Takes text rather than a path: the target has to reach the file unchanged, and building a
/// path from it first can normalise separators.
std::string escapedForDepFile(const std::string& path)
{
  std::string escaped;
  for (const char character: path)
  {
    switch (character)
    {
      case ' ':
        escaped += "\\ ";
        break;
      case '#':
        escaped += "\\#";
        break;
      case '$':
        escaped += "$$";
        break;
      default:
        escaped += character;
        break;
    }
  }
  return escaped;
}

}  // namespace

std::string depFileTargetOr(const std::string& given, const std::vector<std::filesystem::path>& written)
{
  if (!given.empty())
  {
    return given;
  }
  SEN_ENSURE(!written.empty());
  return std::filesystem::absolute(written.front()).string();
}

void writeDepFile(const std::filesystem::path& path,
                  const std::string& target,
                  const std::set<std::filesystem::path>& dependencies)
{
  // An empty target writes a rule with no left-hand side, which a reader takes as the whole
  // file being malformed. `-o` is optional on every subcommand, so a caller can reach this.
  SEN_ENSURE(!target.empty());

  // Written as given rather than resolved. The build graph records its outputs in a form only
  // it knows, and an absolute path matched none of them: ninja dropped the dependencies and ran
  // the generator again on every build.
  std::string body = escapedForDepFile(target);
  body += ":";
  for (const auto& dependency: dependencies)
  {
    body += " ";
    body += escapedForDepFile(std::filesystem::absolute(dependency).string());
  }
  body += "\n";

  // writeFile skips an identical write; a depfile must still be newer than its output.
  std::ofstream stream;
  openFile(path, stream);
  stream << body;
  SEN_ENSURE(stream.good());
  stream.close();
}
