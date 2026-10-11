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
#include <string>

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
std::string escapedForDepFile(const std::filesystem::path& path)
{
  std::string escaped;
  for (const char character: path.string())
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

void writeDepFile(const std::filesystem::path& path,
                  const std::vector<std::filesystem::path>& targets,
                  const std::set<std::filesystem::path>& dependencies)
{
  // A rule with no target, or with an empty one, is not a depfile the build system can read,
  // and the empty case does not fail quietly: absolute("") is the current directory, so the
  // rule would name a plausible-looking path that depends on the models. `-o` is optional on
  // every subcommand, so this is reachable from the command line rather than only from a bug.
  SEN_ENSURE(!targets.empty());

  std::string body;
  for (const auto& target: targets)
  {
    SEN_ENSURE(!target.empty());
    body += (body.empty() ? "" : " ") + escapedForDepFile(std::filesystem::absolute(target));
  }
  body += ":";
  for (const auto& dependency: dependencies)
  {
    body += " " + escapedForDepFile(std::filesystem::absolute(dependency));
  }
  body += "\n";

  // Not writeFile: that skips an identical write, and a depfile whose content did not
  // change must still be newer than the output it describes.
  std::ofstream stream;
  openFile(path, stream);
  stream << body;
  SEN_ENSURE(stream.good());
  stream.close();
}
