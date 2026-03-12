// === util_test.cpp ===================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

// sen
#include "db_test_helpers.h"
#include "sen/core/meta/class_type.h"
#include "sen/core/meta/event.h"
#include "sen/core/meta/property.h"
#include "sen/core/meta/type.h"

// google test
#include <gtest/gtest.h>

// std
#include <cstdio>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <ios>
#include <tuple>

namespace sen::db
{
FILE* openFile(const std::filesystem::path& path);
const ::sen::ClassType* searchOwner(const ::sen::ClassType* classType, const ::sen::Property* property) noexcept;
const ::sen::ClassType* searchOwner(const ::sen::ClassType* classType, const ::sen::Event* event) noexcept;
MemberHash computePlatformDependentPropertyId(const ::sen::ClassType* classType, const ::sen::Property* property);
MemberHash computePlatformDependentEventId(const ::sen::ClassType* classType, const ::sen::Event* event);
}  // namespace sen::db

namespace sen::db::test
{

/// @test
/// openFile throws when the file does not exist.
/// @requirements(SEN-364)
TEST(UtilTest, OpenFileThrowsWhenFileNotPresent)
{
  TempDir tempDir;
  auto nonExistentFile = tempDir.path() / "does_not_exist";
  EXPECT_ANY_THROW(std::ignore = openFile(nonExistentFile));
}

/// @test
/// openFile throws when the file is empty.
/// @requirements(SEN-364)
TEST(UtilTest, OpenFileThrowsWhenFileIsEmpty)
{
  TempDir tempDir;
  auto emptyFile = tempDir.path() / "empty_file";
  {
    std::ofstream ofs(emptyFile, std::ios::binary);
  }
  EXPECT_ANY_THROW(std::ignore = openFile(emptyFile));
}

/// @test
/// openFile succeeds when the file exists and is non-empty.
/// @requirements(SEN-364)
TEST(UtilTest, OpenFileSucceedsWithValidFile)
{
  TempDir tempDir;
  auto validFile = tempDir.path() / "valid_file";
  {
    std::ofstream ofs(validFile, std::ios::binary);
    ofs.write("data", 4);
  }

  FILE* file = openFile(validFile);
  ASSERT_NE(file, nullptr);
  fclose(file);  // NOLINT
}

/// @test
/// searchOwner finds the class that owns a property
/// @requirements(SEN-364)
TEST(UtilTest, SearchOwnerFindsPropertyInDirectClass)
{
  SingleClassSetup setup;
  const auto* classType = setup.object->getClass().type();
  const auto* prop = classType->searchPropertyByName("speed");
  ASSERT_NE(prop, nullptr);

  const auto* owner = searchOwner(classType, prop);
  ASSERT_NE(owner, nullptr);
  EXPECT_EQ(owner->getName(), classType->getName());
}

/// @test
/// searchOwner finds the class that owns an event.
/// @requirements(SEN-364)
TEST(UtilTest, SearchOwnerFindsEventInDirectClass)
{
  SingleClassSetup setup;
  const auto* classType = setup.object->getClass().type();
  const auto* event = classType->searchEventByName("valueChanged");
  ASSERT_NE(event, nullptr);

  const auto* owner = searchOwner(classType, event);
  ASSERT_NE(owner, nullptr);
  EXPECT_EQ(owner->getName(), classType->getName());
}

/// @test
/// searchOwner returns nullptr when the property is not owned by the class.
/// @requirements(SEN-364)
TEST(UtilTest, SearchOwnerReturnsNullWhenPropertyNotInClass)
{
  DualClassSetup setup;
  const auto* classType = setup.testObject->getClass().type();
  const auto* otherClass = setup.otherObject->getClass().type();
  const auto* speedProp = classType->searchPropertyByName("speed");
  ASSERT_NE(speedProp, nullptr);

  const auto* owner = searchOwner(otherClass, speedProp);
  EXPECT_EQ(owner, nullptr);
}

/// @test
/// searchOwner returns nullptr when the event is not owned by the class.
/// @requirements(SEN-364)
TEST(UtilTest, SearchOwnerReturnsNullWhenEventNotInClass)
{
  DualClassSetup setup;
  const auto* classType = setup.testObject->getClass().type();
  const auto* otherClass = setup.otherObject->getClass().type();
  const auto* event = classType->searchEventByName("valueChanged");
  ASSERT_NE(event, nullptr);

  const auto* owner = searchOwner(otherClass, event);
  EXPECT_EQ(owner, nullptr);
}

/// @test
/// computePlatformDependentPropertyId returns a stable, non-zero hash.
/// @requirements(SEN-364)
TEST(UtilTest, ComputePlatformDependentPropertyId)
{
  SingleClassSetup setup;
  const auto* classType = setup.object->getClass().type();
  const auto* prop = classType->searchPropertyByName("speed");
  ASSERT_NE(prop, nullptr);

  auto hash = computePlatformDependentPropertyId(classType, prop);
  EXPECT_NE(hash.get(), 0U);

  auto hash2 = computePlatformDependentPropertyId(classType, prop);
  EXPECT_EQ(hash.get(), hash2.get());
}

/// @test
/// computePlatformDependentEventId returns a stable, non-zero hash.
/// @requirements(SEN-364)
TEST(UtilTest, ComputePlatformDependentEventId)
{
  SingleClassSetup setup;
  const auto* classType = setup.object->getClass().type();
  const auto* event = classType->searchEventByName("valueChanged");
  ASSERT_NE(event, nullptr);

  auto hash = computePlatformDependentEventId(classType, event);
  EXPECT_NE(hash.get(), 0U);

  auto hash2 = computePlatformDependentEventId(classType, event);
  EXPECT_EQ(hash.get(), hash2.get());
}

/// @test
/// computePlatformDependentPropertyId throws when property doesn't belong to the class.
/// @requirements(SEN-364)
TEST(UtilTest, ComputePlatformDependentPropertyIdThrowsForWrongClass)
{
  DualClassSetup setup;
  const auto* classType = setup.testObject->getClass().type();
  const auto* otherClass = setup.otherObject->getClass().type();
  const auto* speedProp = classType->searchPropertyByName("speed");
  ASSERT_NE(speedProp, nullptr);

  EXPECT_ANY_THROW(std::ignore = computePlatformDependentPropertyId(otherClass, speedProp));
}

/// @test
/// computePlatformDependentEventId throws when event doesn't belong to the class.
/// @requirements(SEN-364)
TEST(UtilTest, ComputePlatformDependentEventIdThrowsForWrongClass)
{
  DualClassSetup setup;
  const auto* classType = setup.testObject->getClass().type();
  const auto* otherClass = setup.otherObject->getClass().type();
  const auto* event = classType->searchEventByName("valueChanged");
  ASSERT_NE(event, nullptr);

  EXPECT_ANY_THROW(std::ignore = computePlatformDependentEventId(otherClass, event));
}

}  // namespace sen::db::test
