// === util.cpp ========================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "util.h"

// sen
#include "sen/core/base/assert.h"
#include "sen/core/base/compiler_macros.h"
#include "sen/core/base/hash32.h"
#include "sen/core/base/numbers.h"
#include "sen/core/base/span.h"
#include "sen/core/meta/class_type.h"
#include "sen/core/meta/event.h"
#include "sen/core/meta/property.h"
#include "sen/core/meta/type.h"

// spdlog
#include <spdlog/sinks/stdout_color_sinks.h>

// lz4
#include "lz4.h"

// extra cstd
// NOLINTNEXTLINE(hicpp-deprecated-headers,modernize-deprecated-headers)
#include "stdio.h"  // fwrite_unlocked is not part of <stdio>

// std
#include <spdlog/logger.h>

#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <memory>
#include <string>
#include <tuple>
#include <vector>

#if defined(_MSC_VER) && !defined(_CRT_SECURE_NO_WARNINGS)
#  define _CRT_SECURE_NO_WARNINGS  // NOLINT
#  pragma warning(disable: 4996)   // NOLINT
#endif

namespace sen::db
{

void doWrite(const Span<const uint8_t>& span, FILE* file)
{
#ifdef WIN32
  const auto error = span.size() != _fwrite_nolock(span.data(), 1U, span.size(), file);
#elif defined(__APPLE__)
  const auto error = span.size() != fwrite(span.data(), 1U, span.size(), file);
#elif defined(__linux__)
  const auto error = span.size() != fwrite_unlocked(span.data(), 1U, span.size(), file);
#else
#  error "OS not supported"
#endif

  if (error)
  {
    throwRuntimeError(strerror(errno));
  }
}

std::shared_ptr<spdlog::logger> getLogger()
{
  static auto logger = spdlog::stdout_color_mt("sen.db");
  return logger;
}

void writeToCompressedBuffer(const std::vector<uint8_t>& memBuf, std::vector<uint8_t>& lz4Buf)
{
  // allocate space for the compressed data
  lz4Buf.resize(LZ4_compressBound(static_cast<int>(memBuf.size())));

  auto lz4BufSize = LZ4_compress_default(reinterpret_cast<const char*>(memBuf.data()),  // NOLINT NOSONAR
                                         reinterpret_cast<char*>(lz4Buf.data()),        // NOLINT NOSONAR
                                         static_cast<int>(memBuf.size()),
                                         static_cast<int>(lz4Buf.size()));
  if (lz4BufSize < 0)
  {
    throwRuntimeError("could not compress buffer");
  }

  // trim excess
  lz4Buf.resize(lz4BufSize);
}

void uncompressBuffer(const std::vector<uint8_t>& lz4Buf, std::vector<uint8_t>& memBuf, std::size_t decompressedSize)
{
  // allocate space for uncompressing the data
  memBuf.resize(decompressedSize);

  auto lz4BufSize = LZ4_decompress_safe(reinterpret_cast<const char*>(lz4Buf.data()),  // NOLINT NOSONAR
                                        reinterpret_cast<char*>(memBuf.data()),        // NOLINT NOSONAR
                                        static_cast<int>(lz4Buf.size()),
                                        static_cast<int>(memBuf.size()));
  if (lz4BufSize != static_cast<int>(decompressedSize))
  {
    throwRuntimeError("could not correctly decompress buffer");
  }
}

FILE* openFile(const std::filesystem::path& path)
{
  if (!std::filesystem::exists(path))
  {
    std::string err;
    err.append("file '");
    err.append(path.string());
    err.append("' is not present");
    throwRuntimeError(err);
  }

  if (std::filesystem::is_empty(path))
  {
    std::string err;
    err.append("file '");
    err.append(path.string());
    err.append("' is empty");
    throwRuntimeError(err);
  }

  auto* file = fopen(path.string().c_str(), "rb");  // NOLINT
  if (file == nullptr)
  {
    const auto pathStr = path.string();
    auto* description = strerror(errno);
    getLogger()->error("could not open file {}. {}", pathStr, description);

    std::string err;
    err.append("could not open file '");
    err.append(pathStr);
    err.append("'. ");
    err.append(description);
    throwRuntimeError(err);
  }

  return file;
}

const ::sen::ClassType* searchOwner(const ::sen::ClassType* classType, const ::sen::Property* property) noexcept
{
  if (const auto* prop = classType->searchPropertyById(property->getId(), ClassType::SearchMode::doNotIncludeParents);
      prop != nullptr)
  {
    return classType;
  }

  for (const auto& parent: classType->getParents())
  {
    return searchOwner(parent.type(), property);
  }

  return nullptr;
}

const ::sen::ClassType* searchOwner(const ::sen::ClassType* classType, const ::sen::Event* event) noexcept
{
  if (const auto* ev = classType->searchEventById(event->getId(), ClassType::SearchMode::doNotIncludeParents);
      ev != nullptr)
  {
    return classType;
  }

  for (const auto& parent: classType->getParents())
  {
    return searchOwner(parent.type(), event);
  }

  return nullptr;
}

MemberHash computePlatformDependentPropertyId(const ::sen::ClassType* classType, const ::sen::Property* property)
{
  const auto* owner = searchOwner(classType, property);
  if (owner == nullptr)
  {
    std::string err;
    {
      err.append("could not find the class owning the property '");
      err.append(property->getName());
      err.append("' in any of the parents of '");
      err.append(classType->getName());
      err.append("'. Unable to compute platform-dependent property ID.");
    }

    getLogger()->error(err);
    throwRuntimeError(err);
  }

  return platformDependentHashCombine(propertyHashSeed, crc32(property->getName()), crc32(owner->getQualifiedName()));
}

MemberHash computePlatformDependentEventId(const ::sen::ClassType* classType, const ::sen::Event* event)
{
  const u32 eventSeed = crc32("events");

  const auto* owner = searchOwner(classType, event);
  if (owner == nullptr)
  {
    std::string err;
    {
      err.append("could not find the class owning the event '");
      err.append(event->getName());
      err.append("' in any of the parents of '");
      err.append(classType->getName());
      err.append("'. Unable to compute platform-dependent event ID.");
    }

    getLogger()->error(err);
    throwRuntimeError(err);
  }

  return platformDependentHashCombine(eventSeed, crc32(owner->getQualifiedName()), crc32(event->getName()));
}

}  // namespace sen::db

/// This function is needed when other packages consume the db as a package.
/// The hex numbers encode the crc32 of the "db" string.
extern "C" SEN_EXPORT void get0x2a909459Types(sen::ExportedTypesList& types) { std::ignore = types; }
