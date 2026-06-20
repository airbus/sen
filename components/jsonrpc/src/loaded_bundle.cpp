// === loaded_bundle.cpp ===============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "loaded_bundle.h"

// sen
#include "sen/core/base/hash32.h"

// std
#include <algorithm>
#include <charconv>
#include <cstdint>
#include <system_error>
#include <utility>

namespace sen::components::jsonrpc
{

//--------------------------------------------------------------------------------------------------------------
// Helpers
//--------------------------------------------------------------------------------------------------------------

namespace
{

constexpr char hexDigits[] = "0123456789abcdef";

[[nodiscard]] std::string computeEtag(std::string_view contents)
{
  // Strong ETag in `"<hex>"` form.
  const u32 hash = sen::crc32(contents);
  std::string out;
  out.reserve(2U + sizeof(hash) * 2U);
  out.push_back('"');
  for (int i = static_cast<int>(sizeof(hash)) - 1; i >= 0; --i)
  {
    const auto byte = static_cast<unsigned char>(hash >> (i * 8U));
    out.push_back(hexDigits[byte >> 4U]);
    out.push_back(hexDigits[byte & 0x0FU]);
  }
  out.push_back('"');
  return out;
}

[[nodiscard]] bool isControlOrBackslash(char c) noexcept
{
  const auto u = static_cast<unsigned char>(c);
  return c == '\\' || u < 0x20U || u == 0x7FU;
}

[[nodiscard]] std::optional<std::string> percentDecode(std::string_view in)
{
  std::string out;
  out.reserve(in.size());

  for (std::size_t i = 0; i < in.size(); ++i)
  {
    if (in[i] != '%')
    {
      out.push_back(in[i]);
      continue;
    }

    if (i + 2U >= in.size())
    {
      return std::nullopt;
    }

    const char* const begin = in.data() + i + 1U;
    const char* const end = begin + 2U;
    unsigned int byte = 0;

    const auto [ptr, ec] = std::from_chars(begin, end, byte, 16);
    if (ec != std::errc {} || ptr != end)
    {
      return std::nullopt;
    }

    out.push_back(static_cast<char>(byte));
    i += 2U;
  }
  return out;
}

}  // namespace

//--------------------------------------------------------------------------------------------------------------
// LoadedBundle
//--------------------------------------------------------------------------------------------------------------

sen::Result<LoadedBundle, std::string> LoadedBundle::make(LoadedBundleInput input)
{
  if (input.urlPrefix.empty() || input.urlPrefix.front() != '/')
  {
    return sen::Err(std::string {"LoadedBundle: urlPrefix must start with '/'"});
  }

  if (input.indexFileName.empty())
  {
    return sen::Err(std::string {"LoadedBundle: indexFileName must be non-empty"});
  }

  std::vector<LoadedFile> files;
  files.reserve(input.files.size());
  for (auto& file: input.files)
  {
    if (file.path.empty())
    {
      return sen::Err(std::string {"LoadedBundle: file path must be non-empty"});
    }

    LoadedFile out;
    out.path = std::move(file.path);
    out.contentType = std::move(file.contentType);
    out.contents = std::move(file.contents);
    out.etag = computeEtag(out.contents);
    files.push_back(std::move(out));
  }

  std::sort(files.begin(), files.end(), [](const LoadedFile& a, const LoadedFile& b) { return a.path < b.path; });

  for (std::size_t i = 1; i < files.size(); ++i)
  {
    if (files[i].path == files[i - 1U].path)
    {
      return sen::Err(std::string {"LoadedBundle: duplicate file path"});
    }
  }

  std::sort(files.begin(), files.end(), [](const LoadedFile& a, const LoadedFile& b) { return a.path < b.path; });

  const auto indexIt = std::lower_bound(files.begin(),
                                        files.end(),
                                        input.indexFileName,
                                        [](const LoadedFile& f, const std::string& name) { return f.path < name; });
  if (indexIt == files.end() || indexIt->path != input.indexFileName)
  {
    return sen::Err("LoadedBundle: indexFileName '" + input.indexFileName + "' not present in files");
  }

  return sen::Ok(
    LoadedBundle {std::move(input.urlPrefix), std::move(input.indexFileName), std::move(files), Private {}});
}

LoadedBundle::LoadedBundle(std::string urlPrefix,
                           std::string indexFileName,
                           std::vector<LoadedFile> files,
                           Private /*notUsable*/)
  : urlPrefix_(std::move(urlPrefix)), indexFileName_(std::move(indexFileName)), files_(std::move(files))
{
}

const LoadedFile* LoadedBundle::find(std::string_view pathInBundle) const
{
  // lower_bound + string_view comparator avoids the per-lookup std::string allocation that
  // unordered_map<std::string, ...> forces under C++17.
  const auto it = std::lower_bound(files_.begin(),
                                   files_.end(),
                                   pathInBundle,
                                   [](const LoadedFile& f, std::string_view path) { return f.path < path; });
  if (it == files_.end() || it->path != pathInBundle)
  {
    return nullptr;
  }
  return &*it;
}

const LoadedFile* LoadedBundle::indexFile() const { return find(indexFileName_); }

std::optional<std::string> stripAndNormalize(std::string_view prefix, std::string_view url)
{
  for (char c: url)
  {
    if (isControlOrBackslash(c))
    {
      return std::nullopt;
    }
  }

  if (const auto qpos = url.find('?'); qpos != std::string_view::npos)
  {
    url = url.substr(0, qpos);
  }

  if (url.size() < prefix.size() || url.substr(0, prefix.size()) != prefix)
  {
    return std::nullopt;
  }

  auto rest = url.substr(prefix.size());

  // Require either exact prefix match or a `/` boundary so `/explorerfoo` does not match `/explorer`.
  if (!rest.empty() && rest.front() != '/')
  {
    return std::nullopt;
  }

  if (!rest.empty())
  {
    rest.remove_prefix(1U);
  }

  auto decoded = percentDecode(rest);
  if (!decoded)
  {
    return std::nullopt;
  }

  for (char c: *decoded)
  {
    if (isControlOrBackslash(c))
    {
      return std::nullopt;
    }
  }

  // Walk segments, drop empty and `.`, reject `..` outright.
  std::string out;
  out.reserve(decoded->size());
  std::size_t start = 0;

  const std::string_view view {*decoded};
  for (std::size_t i = 0; i <= view.size(); ++i)
  {
    if (i != view.size() && view[i] != '/')
    {
      continue;
    }

    const auto seg = view.substr(start, i - start);
    start = i + 1U;

    if (seg == "..")
    {
      return std::nullopt;
    }

    if (seg.empty() || seg == ".")
    {
      continue;
    }

    if (!out.empty())
    {
      out.push_back('/');
    }

    out.append(seg);
  }
  return out;
}

}  // namespace sen::components::jsonrpc
