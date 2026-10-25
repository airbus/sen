// === generic_local_object_test.cpp ===================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

/// Tests GenericLocalObject without a kernel: when a value becomes current, what it writes, untyped
/// access, the query getter and method calls.

// test helpers
#include "generic_local_object_helpers.h"

// sen
#include "sen/core/base/result.h"
#include "sen/core/base/span.h"
#include "sen/core/base/timestamp.h"
#include "sen/core/lang/vm.h"
#include "sen/core/meta/class_type.h"
#include "sen/core/meta/method.h"
#include "sen/core/meta/native_types.h"
#include "sen/core/meta/property.h"
#include "sen/core/meta/struct_type.h"
#include "sen/core/meta/type.h"
#include "sen/core/meta/var.h"
#include "sen/core/obj/callback.h"
#include "sen/core/obj/detail/native_object_impl.h"
#include "sen/core/obj/detail/work_queue.h"
#include "sen/core/obj/native_object.h"
#include "sen/kernel/detail/generic_local_object.h"

// generated code
#include "bus/stl/rich.stl.h"

// google test
#include <gtest/gtest.h>

// std
#include <cstddef>
#include <cstdint>
#include <exception>
#include <memory>
#include <optional>
#include <string>
#include <tuple>
#include <variant>
#include <vector>

namespace
{

using generic_test::encodeFor;
using generic_test::indexOf;
using sen::kernel::impl::GenericLocalObject;

/// A GenericLocalObject of a new class, with its work queue enabled as a runner enables it.
std::shared_ptr<GenericLocalObject> makeTrack()
{
  auto object = std::make_shared<GenericLocalObject>("track", generic_test::makeTrackClass("generic_test.Track"));
  sen::impl::getWorkQueue(object.get())->enable();
  return object;
}

std::vector<uint8_t> toVector(sen::Span<const uint8_t> bytes) { return {bytes.begin(), bytes.end()}; }

void drain(sen::impl::WorkQueue& queue)
{
  while (queue.executeAll())
  {
  }
}

}  // namespace

/// @test
/// A value set becomes current and changed at the next commit; the commit after clears the mark.
TEST(GenericLocalObject, aValueSetIsCurrentAndChangedFromTheNextCommitOnly)
{
  auto track = makeTrack();
  const auto altitude = indexOf(*track, "altitude");
  const auto value = encodeFor(*track, "altitude", 1500.0);

  track->set(altitude, value);
  EXPECT_TRUE(track->current(altitude).empty());
  EXPECT_FALSE(track->flags(altitude).changedInLastCycle());

  track->commit(sen::TimeStamp {100});
  EXPECT_EQ(toVector(track->current(altitude)), value);
  EXPECT_TRUE(track->flags(altitude).changedInLastCycle());

  track->commit(sen::TimeStamp {200});
  EXPECT_EQ(toVector(track->current(altitude)), value);
  EXPECT_FALSE(track->flags(altitude).changedInLastCycle());
}

/// @test
/// Setting the current value changes nothing; setting twice before a commit keeps the last value.
TEST(GenericLocalObject, theSameValueChangesNothingAndTheLastValueSetWins)
{
  auto track = makeTrack();
  const auto altitude = indexOf(*track, "altitude");

  track->set(altitude, encodeFor(*track, "altitude", 10.0));
  track->commit(sen::TimeStamp {100});
  track->commit(sen::TimeStamp {200});

  track->set(altitude, encodeFor(*track, "altitude", 10.0));
  track->commit(sen::TimeStamp {300});
  EXPECT_FALSE(track->flags(altitude).changedInLastCycle());
  EXPECT_TRUE(generic_test::collectChanges(*track).multicast.empty());

  track->set(altitude, encodeFor(*track, "altitude", 20.0));
  track->set(altitude, encodeFor(*track, "altitude", 30.0));
  track->commit(sen::TimeStamp {400});
  EXPECT_EQ(toVector(track->current(altitude)), encodeFor(*track, "altitude", 30.0));
  EXPECT_TRUE(track->flags(altitude).changedInLastCycle());
}

/// @test
/// A static property is current once set, never written as a change, and written with the statics.
TEST(GenericLocalObject, aStaticPropertyIsSetInPlace)
{
  auto track = makeTrack();
  const auto callsign = indexOf(*track, "callsign");
  const auto value = encodeFor(*track, "callsign", std::string("viper"));

  track->set(callsign, value);
  EXPECT_EQ(toVector(track->current(callsign)), value);

  track->commit(sen::TimeStamp {100});
  EXPECT_FALSE(track->flags(callsign).changedInLastCycle());
  EXPECT_TRUE(generic_test::collectChanges(*track).confirmed.empty());

  const auto statics = generic_test::collectRecords(*track, generic_test::Access::writeStatic());
  ASSERT_EQ(statics.size(), 1U);
  EXPECT_EQ(statics.at(track->properties()[callsign]->getId().get()), value);
  EXPECT_TRUE(generic_test::collectRecords(*track, generic_test::Access::writeDynamic()).empty());
}

/// @test
/// Writes as changes what the last commit made current, each in its property's transport mode.
TEST(GenericLocalObject, writesAsChangesExactlyWhatTheLastCommitMadeCurrent)
{
  auto track = makeTrack();
  const auto idOf = [&track](const char* name) { return track->properties()[indexOf(*track, name)]->getId().get(); };

  const auto altitude = encodeFor(*track, "altitude", 100.0);
  const auto force = encodeFor(*track, "force", uint8_t {2U});
  const auto status = encodeFor(*track, "status", std::string("climbing"));
  track->set(indexOf(*track, "altitude"), altitude);
  track->set(indexOf(*track, "force"), force);
  track->set(indexOf(*track, "status"), status);
  track->commit(sen::TimeStamp {100});

  const auto first = generic_test::collectChanges(*track);
  EXPECT_EQ(first.multicast, (generic_test::Records {{idOf("altitude"), altitude}}));
  EXPECT_EQ(first.confirmed, (generic_test::Records {{idOf("force"), force}}));
  EXPECT_EQ(first.unicast, (generic_test::Records {{idOf("status"), status}}));

  track->commit(sen::TimeStamp {200});
  const auto idle = generic_test::collectChanges(*track);
  EXPECT_TRUE(idle.multicast.empty() && idle.confirmed.empty() && idle.unicast.empty());

  const auto higher = encodeFor(*track, "altitude", 200.0);
  track->set(indexOf(*track, "altitude"), higher);
  track->commit(sen::TimeStamp {300});
  const auto third = generic_test::collectChanges(*track);
  EXPECT_EQ(third.multicast, (generic_test::Records {{idOf("altitude"), higher}}));
  EXPECT_TRUE(third.confirmed.empty() && third.unicast.empty());
}

/// @test
/// Writes only the properties that have a value, split into static and dynamic ones.
TEST(GenericLocalObject, writesOnlyThePropertiesThatHaveAValue)
{
  auto track = makeTrack();
  EXPECT_TRUE(generic_test::collectAll(*track).empty());

  track->set(indexOf(*track, "callsign"), encodeFor(*track, "callsign", std::string("viper")));
  track->set(indexOf(*track, "altitude"), encodeFor(*track, "altitude", 100.0));
  track->commit(sen::TimeStamp {100});

  EXPECT_EQ(generic_test::collectAll(*track).size(), 2U);
  EXPECT_EQ(generic_test::collectRecords(*track, generic_test::Access::writeStatic()).size(), 1U);
  EXPECT_EQ(generic_test::collectRecords(*track, generic_test::Access::writeDynamic()).size(), 1U);
}

/// @test
/// The constructor throws for a class with proxy makers and accepts the same class without them.
TEST(GenericLocalObject, refusesAClassWithProxyMakersOfItsOwn)
{
  const auto generated = rich::RichInterface::meta();
  ASSERT_TRUE(generated->hasProxyMakers());

  try
  {
    std::ignore = std::make_shared<GenericLocalObject>("rich", generated);
    FAIL() << "a class with proxy makers of its own must be refused";
  }
  catch (const std::exception& err)
  {
    EXPECT_NE(std::string(err.what()).find("rich.Rich"), std::string::npos) << err.what();
  }

  EXPECT_NO_THROW(std::ignore = std::make_shared<GenericLocalObject>("rich", generic_test::runtimeCopyOf(*generated)));
}

/// @test
/// set() and apply() reject an empty value, and the property keeps the value it had.
TEST(GenericLocalObject, refusesAnEmptyValueAndKeepsTheOneItHas)
{
  auto track = makeTrack();
  const auto altitude = indexOf(*track, "altitude");
  const auto value = encodeFor(*track, "altitude", 300.0);
  track->set(altitude, value);
  track->commit(sen::TimeStamp {100});

  try
  {
    track->set(altitude, {});
    FAIL() << "an empty value must be refused";
  }
  catch (const std::exception& err)
  {
    EXPECT_NE(std::string(err.what()).find("altitude"), std::string::npos) << err.what();
  }

  std::vector<uint8_t> emptyRecord;
  generic_test::appendRecord(emptyRecord, track->properties()[altitude]->getId().get(), {});
  EXPECT_FALSE(track->apply(emptyRecord));

  track->commit(sen::TimeStamp {200});
  EXPECT_EQ(toVector(track->current(altitude)), value);
  EXPECT_FALSE(track->flags(altitude).changedInLastCycle());
  EXPECT_TRUE(generic_test::collectChanges(*track).multicast.empty());
}

/// @test
/// Throws for a property whose type encodes to no bytes, which would read as no value.
TEST(GenericLocalObject, refusesATypeWithNoBytes)
{
  const auto nothing = sen::StructType::make({"Nothing", "generic_test.Nothing", "", {}});
  const sen::ClassSpec spec {"Empty",
                             "generic_test.Empty",
                             "",
                             {{"nothing", "", nothing}},
                             {},
                             {},
                             sen::MethodSpec {{"constructorHeld", "", {}}, sen::VoidType::get()},
                             {},
                             false,
                             false,
                             nullptr,
                             nullptr};
  auto object = std::make_shared<GenericLocalObject>("empty", sen::ClassType::make(spec));

  try
  {
    object->setNextPropertyUntyped(object->properties().front().get(), sen::VarMap {});
    FAIL() << "a value with no bytes must be refused";
  }
  catch (const std::exception& err)
  {
    EXPECT_NE(std::string(err.what()).find("nothing"), std::string::npos) << err.what();
  }
  EXPECT_TRUE(object->current(0U).empty());
}

/// @test
/// apply() sets every record of a list, by property id, for the next commit.
TEST(GenericLocalObject, applySetsEveryRecordOfAList)
{
  auto track = makeTrack();
  const auto altitude = indexOf(*track, "altitude");
  const auto force = indexOf(*track, "force");

  std::vector<uint8_t> records;
  const auto altitudeId = track->properties()[altitude]->getId().get();
  const auto forceId = track->properties()[force]->getId().get();
  generic_test::appendRecord(records, altitudeId, encodeFor(*track, "altitude", 350.0));
  generic_test::appendRecord(records, forceId, encodeFor(*track, "force", uint8_t {1U}));

  ASSERT_TRUE(track->apply(records));
  EXPECT_TRUE(track->current(altitude).empty());

  track->commit(sen::TimeStamp {100});
  EXPECT_EQ(toVector(track->current(altitude)), encodeFor(*track, "altitude", 350.0));
  EXPECT_EQ(toVector(track->current(force)), encodeFor(*track, "force", uint8_t {1U}));
  EXPECT_TRUE(track->flags(altitude).changedInLastCycle());
  EXPECT_TRUE(track->flags(force).changedInLastCycle());
}

/// @test
/// apply() sets nothing when one record names an unknown property or runs past the end.
TEST(GenericLocalObject, applyRefusesABadListAndSetsNothing)
{
  auto track = makeTrack();
  const auto altitude = indexOf(*track, "altitude");
  const auto altitudeId = track->properties()[altitude]->getId().get();

  std::vector<uint8_t> good;
  generic_test::appendRecord(good, altitudeId, encodeFor(*track, "altitude", 350.0));

  auto unknownId = good;
  generic_test::appendRecord(unknownId, 12345U, encodeFor(*track, "altitude", 1.0));
  EXPECT_FALSE(track->apply(unknownId));

  auto pastTheEnd = good;
  generic_test::appendRecord(pastTheEnd, altitudeId, encodeFor(*track, "altitude", 1.0));
  pastTheEnd.pop_back();
  EXPECT_FALSE(track->apply(pastTheEnd));

  auto shortHeader = good;
  shortHeader.push_back(1U);
  EXPECT_FALSE(track->apply(shortHeader));

  track->commit(sen::TimeStamp {100});
  EXPECT_TRUE(track->current(altitude).empty());
  EXPECT_FALSE(track->flags(altitude).changedInLastCycle());

  EXPECT_TRUE(track->apply({}));
}

/// @test
/// Untyped access reads the next value at once, the current one after a commit, and none if unset.
TEST(GenericLocalObject, untypedAccessGoesThroughTheEncoding)
{
  auto track = makeTrack();
  const auto* altitude = track->properties()[indexOf(*track, "altitude")].get();
  const auto* status = track->properties()[indexOf(*track, "status")].get();

  track->setNextPropertyUntyped(altitude, 42.5);
  EXPECT_EQ(track->getNextPropertyUntyped(altitude).getCopyAs<double>(), 42.5);
  EXPECT_TRUE(track->getPropertyUntyped(altitude).isEmpty());

  track->commit(sen::TimeStamp {100});
  EXPECT_EQ(track->getPropertyUntyped(altitude).getCopyAs<double>(), 42.5);
  EXPECT_TRUE(track->getPropertyUntyped(status).isEmpty());
}

/// @test
/// A commit notifies the property's subscribers once, with the commit's time.
TEST(GenericLocalObject, aCommitTellsThePropertysSubscribers)
{
  auto track = makeTrack();
  auto* queue = sen::impl::getWorkQueue(track.get());
  const auto* altitude = track->properties()[indexOf(*track, "altitude")].get();

  std::vector<sen::TimeStamp> told;
  auto guard = track->onPropertyChangedUntyped(
    altitude,
    sen::EventCallback<sen::VarList>(
      queue, [&told](const sen::EventInfo& info, const sen::VarList& /*args*/) { told.push_back(info.creationTime); }));

  track->set(indexOf(*track, "altitude"), encodeFor(*track, "altitude", 1.0));
  track->commit(sen::TimeStamp {100});
  drain(*queue);
  ASSERT_EQ(told.size(), 1U);
  EXPECT_EQ(told.front(), sen::TimeStamp {100});

  track->commit(sen::TimeStamp {200});
  drain(*queue);
  EXPECT_EQ(told.size(), 1U);
}

/// @test
/// A query getter reads the current value and change flags, including a field of a struct.
TEST(GenericLocalObject, aQueryGetterReadsTheCurrentValueInPlace)
{
  auto track = makeTrack();
  const auto altitude = indexOf(*track, "altitude");
  const auto position = indexOf(*track, "position");
  const auto callsign = indexOf(*track, "callsign");
  const auto getterOf = [&track](std::size_t index, std::vector<uint16_t> fields)
  {
    return (static_cast<const sen::NativeObject&>(*track).*generic_test::Access::fieldGetter())(
      track->properties()[index]->getId(), sen::Span<uint16_t>(fields.data(), fields.size()));
  };

  const auto altitudeGetter = getterOf(altitude, {});
  EXPECT_EQ(altitudeGetter.flags, &track->flags(altitude));
  EXPECT_TRUE(std::holds_alternative<sen::lang::VariantAccessError>(altitudeGetter.getterFunc()));

  track->set(altitude, encodeFor(*track, "altitude", 12.0));
  track->set(position, encodeFor(*track, "position", sen::VarMap {{"lat", 1.5}, {"lon", -2.5}}));
  track->set(callsign, encodeFor(*track, "callsign", std::string("viper")));
  EXPECT_TRUE(std::holds_alternative<sen::lang::VariantAccessError>(altitudeGetter.getterFunc()));

  track->commit(sen::TimeStamp {100});
  EXPECT_EQ(std::get<double>(altitudeGetter.getterFunc()), 12.0);
  EXPECT_EQ(std::get<double>(getterOf(position, {1U}).getterFunc()), -2.5);

  const auto callsignGetter = getterOf(callsign, {});
  EXPECT_EQ(callsignGetter.flags, nullptr);
  EXPECT_EQ(std::get<std::string>(callsignGetter.getterFunc()), "viper");

  EXPECT_THROW(std::ignore = (static_cast<const sen::NativeObject&>(*track).*generic_test::Access::fieldGetter())(
                 sen::MemberHash {12345U}, {}),
               std::exception);
}

/// @test
/// A getter call returns the current value; any other method call returns an error.
TEST(GenericLocalObject, answersAGetterCallAndRefusesOtherMethods)
{
  auto track = makeTrack();
  auto* queue = sen::impl::getWorkQueue(track.get());
  const auto* altitude = track->properties()[indexOf(*track, "altitude")].get();
  const auto* status = track->properties()[indexOf(*track, "status")].get();

  track->set(indexOf(*track, "altitude"), encodeFor(*track, "altitude", 640.0));
  track->commit(sen::TimeStamp {100});

  std::optional<sen::MethodResult<sen::Var>> getterResult;
  track->invokeUntyped(&altitude->getGetterMethod(),
                       {},
                       sen::MethodCallback<sen::Var>(
                         queue,
                         [&getterResult](const sen::MethodCallInfo& /*info*/, const sen::MethodResult<sen::Var>& result)
                         { getterResult = result; }));
  drain(*queue);
  ASSERT_TRUE(getterResult.has_value());
  ASSERT_TRUE(getterResult->isOk());
  EXPECT_EQ(getterResult->getValue().getCopyAs<double>(), 640.0);

  // a getter of a property with no value returns an error
  std::optional<sen::MethodResult<sen::Var>> noValue;
  track->invokeUntyped(&status->getGetterMethod(),
                       {},
                       sen::MethodCallback<sen::Var>(
                         queue,
                         [&noValue](const sen::MethodCallInfo& /*info*/, const sen::MethodResult<sen::Var>& result)
                         { noValue = result; }));
  drain(*queue);
  ASSERT_TRUE(noValue.has_value());
  EXPECT_TRUE(noValue->isError());

  std::optional<sen::MethodResult<sen::Var>> setterResult;
  track->invokeUntyped(&status->getSetterMethod(),
                       {sen::Var(std::string("descending"))},
                       sen::MethodCallback<sen::Var>(
                         queue,
                         [&setterResult](const sen::MethodCallInfo& /*info*/, const sen::MethodResult<sen::Var>& result)
                         { setterResult = result; }));
  drain(*queue);
  ASSERT_TRUE(setterResult.has_value());
  ASSERT_TRUE(setterResult->isError());
  try
  {
    std::rethrow_exception(setterResult->getError());
  }
  catch (const std::exception& err)
  {
    EXPECT_NE(std::string(err.what()).find("track"), std::string::npos) << err.what();
    EXPECT_NE(std::string(err.what()).find("getters"), std::string::npos) << err.what();
  }
}
