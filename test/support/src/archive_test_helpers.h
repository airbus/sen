// === archive_test_helpers.h ==========================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_TEST_SUPPORT_ARCHIVE_TEST_HELPERS_H
#define SEN_TEST_SUPPORT_ARCHIVE_TEST_HELPERS_H

// sen
#include "sen/core/base/compiler_macros.h"
#include "sen/core/base/timestamp.h"
#include "sen/core/base/uuid.h"
#include "sen/core/meta/class_type.h"
#include "sen/core/meta/type.h"
#include "sen/db/output.h"

// generated code
#include "stl/sen/db/basic_types.stl.h"

// std
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>

namespace sen::test
{

/// Creates a unique temporary directory and deletes it on destruction.
class TempDir
{
public:
  explicit TempDir(std::string prefix = "sen_test_")
    : path_(std::filesystem::temp_directory_path() / (std::move(prefix) + getRandomPathPostFix()))
  {
    std::filesystem::create_directories(path_);
  }

  SEN_NOCOPY_NOMOVE(TempDir)

  ~TempDir()
  {
    std::error_code errorCode;
    std::filesystem::remove_all(path_, errorCode);
  }

  /// Returns the absolute path to the generated directory.
  [[nodiscard]] const std::filesystem::path& path() const noexcept { return path_; }

private:
  /// Returns a random post fix for temporary files paths.
  static std::string getRandomPathPostFix() { return sen::UuidRandomGenerator()().toString(); }

private:
  std::filesystem::path path_;
};

/// Creates a TimeStamp from the specified number of seconds.
[[nodiscard]] inline sen::TimeStamp makeTime(int64_t seconds) { return sen::TimeStamp(std::chrono::seconds(seconds)); }

/// Generates output settings to write an archive using the path folder.
[[nodiscard]] inline sen::db::OutSettings makeArchiveSettings(std::string_view name,
                                                              const std::filesystem::path& folder,
                                                              bool indexKeyframes = true)
{
  sen::db::OutSettings settings;
  settings.name = std::string(name);
  settings.folder = folder.string();
  settings.indexKeyframes = indexKeyframes;
  return settings;
}

/// Generates output settings to write an archive using a TempDir object.
[[nodiscard]] inline sen::db::OutSettings makeArchiveSettings(std::string_view name,
                                                              const TempDir& tempDir,
                                                              bool indexKeyframes = true)
{
  return makeArchiveSettings(name, tempDir.path(), indexKeyframes);
}

/// Constructs the full archive path by appending the archive name to the base folder.
[[nodiscard]] inline std::filesystem::path makeArchivePath(std::string_view name, const std::filesystem::path& folder)
{
  return folder / std::string(name);
}

/// Constructs the archive path within a TempDir.
[[nodiscard]] inline std::filesystem::path makeArchivePath(std::string_view name, const TempDir& tempDir)
{
  return makeArchivePath(name, tempDir.path());
}

/// Creates object metadata for database registration, assigning a session and bus.
template <typename ObjectType>
[[nodiscard]] inline sen::db::ObjectInfo makeObjectInfo(ObjectType* object,
                                                        std::string_view session = "test_session",
                                                        std::string_view bus = "test_bus")
{
  return sen::db::ObjectInfo {object, std::string(session), std::string(bus)};
}

/// Creates object metadata for database registration accepting a std::shared_ptr.
template <typename ObjectType>
[[nodiscard]] inline sen::db::ObjectInfo makeObjectInfo(const std::shared_ptr<ObjectType>& object,
                                                        std::string_view session = "test_session",
                                                        std::string_view bus = "test_bus")
{
  return makeObjectInfo(object.get(), session, bus);
}

/// Retrieves the MemberHash id of the first property defined in the object's class.
template <typename ObjectType>
[[nodiscard]] inline sen::MemberHash firstPropertyId(const ObjectType& object)
{
  return object.getClass()->getProperties(sen::ClassType::SearchMode::includeParents).front()->getId();
}

/// Retrieves the MemberHash id of the first event defined in the object's class.
template <typename ObjectType>
[[nodiscard]] inline sen::MemberHash firstEventId(const ObjectType& object)
{
  return object.getClass()->getEvents(sen::ClassType::SearchMode::includeParents).front()->getId();
}

}  // namespace sen::test

#endif  // SEN_TEST_SUPPORT_ARCHIVE_TEST_HELPERS_H
