// === generic_local_object_query_test.cpp =============================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

/// Tests a WHERE over GenericLocalObjects through an ObjectFilter, without a kernel.

// test helpers
#include "generic_local_object_helpers.h"

// sen
#include "sen/core/base/compiler_macros.h"
#include "sen/core/base/timestamp.h"
#include "sen/core/meta/class_type.h"
#include "sen/core/meta/type.h"
#include "sen/core/meta/type_registry.h"
#include "sen/core/meta/var.h"
#include "sen/core/obj/interest.h"
#include "sen/core/obj/object.h"
#include "sen/core/obj/object_filter.h"
#include "sen/core/obj/object_provider.h"
#include "sen/kernel/detail/generic_local_object.h"

// google test
#include <gtest/gtest.h>

// std
#include <algorithm>
#include <cstdint>
#include <initializer_list>
#include <memory>
#include <string>
#include <tuple>
#include <vector>

namespace
{

using generic_test::encodeFor;
using generic_test::indexOf;
using sen::kernel::impl::GenericLocalObject;

class TestObjectFilter: public sen::ObjectFilter
{
public:
  using ObjectFilter::evaluate;
  using ObjectFilter::ObjectFilter;
};

/// Records the objects added to and removed from a subscriber.
class Listener: public sen::ObjectProviderListener
{
public:
  void onObjectsAdded(const sen::ObjectAdditionList& additions) override
  {
    for (const auto& addition: additions)
    {
      if (const auto* instance = sen::getObjectInstance(addition))
      {
        added.push_back(instance->getName());
      }
    }
  }

  void onObjectsRemoved(const sen::ObjectRemovalList& removals) override
  {
    for (const auto& removal: removals)
    {
      removed.push_back(removal.objectid);
    }
  }

  [[nodiscard]] bool wasGiven(const std::string& name) const
  {
    return std::find(added.begin(), added.end(), name) != added.end();
  }

  std::vector<std::string> added;      // NOLINT(misc-non-private-member-variables-in-classes)
  std::vector<sen::ObjectId> removed;  // NOLINT(misc-non-private-member-variables-in-classes)
};

/// A filter over GenericLocalObjects of one class, with one subscriber to a query.
class QueryFixture
{
  SEN_NOCOPY_NOMOVE(QueryFixture)

public:
  explicit QueryFixture(const std::string& where): type_(generic_test::makeTrackClass("generic_test.QueriedTrack"))
  {
    sen::CustomTypeRegistry types;
    types.add(type_);
    const auto query = "SELECT generic_test.QueriedTrack FROM session.bus WHERE " + where;
    filter_.addSubscriber(sen::Interest::make(query, types), &listener_, false);
  }

  ~QueryFixture() { filter_.removeSubscriber(&listener_, false); }

public:
  [[nodiscard]] std::shared_ptr<GenericLocalObject> make(const std::string& name)
  {
    auto object = std::make_shared<GenericLocalObject>(name, type_);
    objects_.push_back(object);
    return object;
  }

  static void set(GenericLocalObject& object, const char* name, const sen::Var& value)
  {
    object.set(indexOf(object, name), encodeFor(object, name, value));
  }

  /// Commits every object and evaluates the query, as a runner's cycle does.
  void cycle()
  {
    time_ += 10;
    TestObjectFilter::ObjectSet set;
    for (const auto& object: objects_)
    {
      object->commit(sen::TimeStamp {time_});
      set.currentObjects[object->getId()] = object;
      if (std::find(seen_.begin(), seen_.end(), object->getId()) == seen_.end())
      {
        set.newObjects[object->getId()] = object;
        seen_.push_back(object->getId());
      }
    }
    filter_.evaluate(set);
  }

  [[nodiscard]] const Listener& listener() const noexcept { return listener_; }

private:
  sen::ConstTypeHandle<sen::ClassType> type_;
  TestObjectFilter filter_ {sen::ObjectOwnerId {1U}};
  Listener listener_;
  std::vector<std::shared_ptr<GenericLocalObject>> objects_;
  std::vector<sen::ObjectId> seen_;
  int64_t time_ = 0;
};

}  // namespace

/// @test
/// A WHERE adds an object when its changed value matches, and removes it when it stops matching.
TEST(GenericLocalObjectQuery, aWhereFollowsAPropertysChanges)
{
  QueryFixture fixture("altitude > 100");
  auto track = fixture.make("track");

  QueryFixture::set(*track, "altitude", 50.0);
  fixture.cycle();
  EXPECT_TRUE(fixture.listener().added.empty());

  QueryFixture::set(*track, "altitude", 150.0);
  fixture.cycle();
  EXPECT_TRUE(fixture.listener().wasGiven("track"));

  QueryFixture::set(*track, "altitude", 50.0);
  fixture.cycle();
  ASSERT_EQ(fixture.listener().removed.size(), 1U);
  EXPECT_EQ(fixture.listener().removed.front(), track->getId());
}

/// @test
/// A WHERE reads a struct field, an enum by its enumerator's name, and a static property.
TEST(GenericLocalObjectQuery, aWhereReadsAStructFieldAnEnumAndAStaticProperty)
{
  QueryFixture fixture(R"(position.lat > 10.0 AND force = "hostile" AND callsign = "viper")");
  auto match = fixture.make("match");
  auto friendly = fixture.make("friendly");

  for (const auto& track: {match, friendly})
  {
    QueryFixture::set(*track, "position", sen::VarMap {{"lat", 40.5}, {"lon", 1.0}});
    QueryFixture::set(*track, "callsign", std::string("viper"));
  }
  QueryFixture::set(*match, "force", uint8_t {2U});
  QueryFixture::set(*friendly, "force", uint8_t {1U});
  fixture.cycle();

  EXPECT_TRUE(fixture.listener().wasGiven("match"));
  EXPECT_FALSE(fixture.listener().wasGiven("friendly"));
}

/// @test
/// A property never set is missing: a comparison on it is false, and its negation true.
TEST(GenericLocalObjectQuery, aPropertyWithNoValueIsMissing)
{
  QueryFixture comparison("altitude > 100");
  std::ignore = comparison.make("track");
  comparison.cycle();
  EXPECT_TRUE(comparison.listener().added.empty());

  QueryFixture negation("NOT (altitude > 100)");
  std::ignore = negation.make("track");
  negation.cycle();
  EXPECT_TRUE(negation.listener().wasGiven("track"));
}

/// @test
/// An optional property is its value, or missing when it is empty.
TEST(GenericLocalObjectQuery, anOptionalIsItsValueOrMissing)
{
  QueryFixture fixture("fuel > 10.0");
  auto empty = fixture.make("empty");
  auto full = fixture.make("full");

  QueryFixture::set(*empty, "fuel", sen::Var {});
  QueryFixture::set(*full, "fuel", 20.0);
  fixture.cycle();

  EXPECT_TRUE(fixture.listener().wasGiven("full"));
  EXPECT_FALSE(fixture.listener().wasGiven("empty"));
}
