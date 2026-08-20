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
