// === object_list_test.cpp ============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

// sen
#include "sen/core/base/timestamp.h"
#include "sen/core/io/output_stream.h"
#include "sen/core/meta/event.h"
#include "sen/core/meta/method.h"
#include "sen/core/meta/type.h"
#include "sen/core/meta/var.h"
#include "sen/core/obj/callback.h"
#include "sen/core/obj/connection_guard.h"
#include "sen/core/obj/object.h"
#include "sen/core/obj/object_list.h"
#include "sen/core/obj/object_provider.h"

// generated code
#include "stl/example_class.stl.h"

// google test
#include <gtest/gtest.h>

// std
#include <functional>
#include <memory>
#include <string>
#include <tuple>
#include <utility>

namespace
{

class DummyObjectProvider final: public sen::ObjectProvider
{
public:
  using ObjectProvider::notifyObjectsAdded;
  using ObjectProvider::notifyObjectsRemoved;

protected:
  void notifyAddedOnExistingObjects(sen::ObjectProviderListener* /*listener*/) override {}
  void notifyRemovedOnExistingObjects(sen::ObjectProviderListener* /*listener*/) override {}
};

struct DummyOwner final: example_class::ExampleClassBase
{
  explicit DummyOwner(std::string name): ExampleClassBase(std::move(name)) {}
};

class WrongTypeObject final: public sen::Object
{
public:
  explicit WrongTypeObject(const sen::ObjectId id): id_(id) {}
  [[nodiscard]] const std::string& getName() const noexcept override { return name_; }
  [[nodiscard]] sen::ObjectId getId() const noexcept override { return id_; }
  [[nodiscard]] sen::ConstTypeHandle<sen::ClassType> getClass() const noexcept override
  {
    return example_class::ExampleClassInterface::meta();
  }
  [[nodiscard]] const std::string& getLocalName() const noexcept override { return name_; }
  [[nodiscard]] sen::TimeStamp getLastCommitTime() const noexcept override { return sen::TimeStamp {0}; }
  [[nodiscard]] sen::Var getPropertyUntyped(const sen::Property* /*prop*/) const override { return {}; }
  void invokeUntyped(const sen::Method* /*method*/,
                     const sen::VarList& /*args*/,
                     sen::MethodCallback<sen::Var>&& /*onDone*/) override
  {
  }
  [[nodiscard]] sen::ConnectionGuard onPropertyChangedUntyped(const sen::Property* /*prop*/,
                                                              sen::EventCallback<sen::VarList>&& /*callback*/) override
  {
    return {};
  }
  [[nodiscard]] sen::ConnectionGuard onEventUntyped(const sen::Event* /*ev*/,
                                                    sen::EventCallback<sen::VarList>&& /*callback*/) override
  {
    return {};
  }
  void invokeAllPropertyCallbacks() override {}

protected:
  void senImplRemoveTypedConnection(sen::ConnId /*id*/) override {}
  void senImplRemoveUntypedConnection(sen::ConnId /*id*/, sen::MemberHash /*memberHash*/) override {}
  void senImplEventEmitted(sen::MemberHash /*id*/,
                           std::function<sen::VarList()>&& /*argsGetter*/,
                           const sen::EventInfo& /*info*/) override
  {
  }
  void senImplWriteAllPropertiesToStream(sen::OutputStream& /*out*/) const override {}
  void senImplWriteStaticPropertiesToStream(sen::OutputStream& /*out*/) const override {}
  void senImplWriteDynamicPropertiesToStream(sen::OutputStream& /*out*/) const override {}

private:
  sen::ObjectId id_;
  std::string name_ = "WrongTypeObject";
};

sen::ObjectAddition makeAddition(const std::shared_ptr<sen::Object>& obj)
{
  sen::ObjectInstanceDiscovery disc;
  disc.instance = obj;
  disc.id = obj->getId();
  disc.interestId = sen::InterestId {1};
  disc.ownerId = sen::ObjectOwnerId {1};
  return disc;
}

sen::ObjectRemoval makeRemoval(const std::shared_ptr<sen::Object>& obj)
{
  return {sen::InterestId {1}, obj->getId(), sen::ObjectOwnerId {1}};
}

}  // namespace

/// @test
/// Verifies the successful detection and typed casting of added objects, invoking the onAdded callback
/// @requirements(SEN-362)
TEST(ObjectList, TracksAndInvokesOnAdded)
{
  DummyObjectProvider provider;
  sen::ObjectList<DummyOwner> list(provider, 10U);

  bool addedTriggered = false;
  std::ignore = list.onAdded(
    [&](const auto& iterators)
    {
      addedTriggered = true;
      EXPECT_NE(iterators.typedBegin, iterators.typedEnd);
      EXPECT_NE(iterators.untypedBegin, iterators.untypedEnd);
    });

  const auto obj1 = std::make_shared<DummyOwner>("Obj1");
  provider.notifyObjectsAdded({makeAddition(obj1)});

  EXPECT_TRUE(addedTriggered);
  EXPECT_EQ(list.getObjects().size(), 1U);
  EXPECT_EQ(list.getUntypedObjects().size(), 1U);
}

/// @test
/// Verifies the successful detection of added objects for the generic Object instantiation
/// @requirements(SEN-362)
TEST(ObjectList, TracksAndInvokesOnAdded_GenericObject)
{
  DummyObjectProvider provider;
  sen::ObjectList<sen::Object> list(provider, 10U);

  bool addedTriggered = false;
  std::ignore = list.onAdded(
    [&](const auto& iterators)
    {
      addedTriggered = true;
      EXPECT_NE(iterators.typedBegin, iterators.typedEnd);
      EXPECT_NE(iterators.untypedBegin, iterators.untypedEnd);
    });

  const auto obj1 = std::make_shared<DummyOwner>("Obj1");
  provider.notifyObjectsAdded({makeAddition(obj1)});

  EXPECT_TRUE(addedTriggered);
  EXPECT_EQ(list.getObjects().size(), 1U);
  EXPECT_EQ(list.getUntypedObjects().size(), 1U);
}

/// @test
/// Checks that updating the onAdded callback triggers immediately if objects are already registered
/// @requirements(SEN-362)
TEST(ObjectList, InstallsCallbackAndTriggersForExisting)
{
  DummyObjectProvider provider;
  sen::ObjectList<DummyOwner> list(provider, 10U);

  const auto obj1 = std::make_shared<DummyOwner>("Obj1");
  provider.notifyObjectsAdded({makeAddition(obj1)});

  bool triggered = false;
  std::ignore = list.onAdded(
    [&](const auto& iterators)
    {
      triggered = true;
      EXPECT_NE(iterators.typedBegin, iterators.typedEnd);
    });

  EXPECT_TRUE(triggered);
}

/// @test
/// Checks that updating the onAdded callback triggers immediately for the generic Object instantiation
/// @requirements(SEN-362)
TEST(ObjectList, InstallsCallbackAndTriggersForExisting_GenericObject)
{
  DummyObjectProvider provider;
  sen::ObjectList<sen::Object> list(provider, 10U);

  const auto obj1 = std::make_shared<DummyOwner>("Obj1");
  provider.notifyObjectsAdded({makeAddition(obj1)});

  bool triggered = false;
  std::ignore = list.onAdded(
    [&](const auto& iterators)
    {
      triggered = true;
      EXPECT_NE(iterators.typedBegin, iterators.typedEnd);
    });

  EXPECT_TRUE(triggered);
}

/// @test
/// Verifies successful detection of removed objects and properly un-tracks them
/// @requirements(SEN-362)
TEST(ObjectList, TracksAndInvokesOnRemoved)
{
  DummyObjectProvider provider;
  sen::ObjectList<DummyOwner> list(provider, 10U);

  bool removedTriggered = false;
  std::ignore = list.onRemoved(
    [&](const auto& iterators)
    {
      removedTriggered = true;
      EXPECT_NE(iterators.typedBegin, iterators.typedEnd);
    });

  const auto obj1 = std::make_shared<DummyOwner>("Obj1");
  provider.notifyObjectsAdded({makeAddition(obj1)});

  EXPECT_EQ(list.getObjects().size(), 1U);

  provider.notifyObjectsRemoved({makeRemoval(obj1)});

  EXPECT_TRUE(removedTriggered);
  EXPECT_TRUE(list.getObjects().empty());
  EXPECT_TRUE(list.getUntypedObjects().empty());
}

/// @test
/// Verifies successful detection of removed objects for the generic Object instantiation
/// @requirements(SEN-362)
TEST(ObjectList, TracksAndInvokesOnRemoved_GenericObject)
{
  DummyObjectProvider provider;
  sen::ObjectList<sen::Object> list(provider, 10U);

  bool removedTriggered = false;
  std::ignore = list.onRemoved(
    [&](const auto& iterators)
    {
      removedTriggered = true;
      EXPECT_NE(iterators.typedBegin, iterators.typedEnd);
    });

  const auto obj1 = std::make_shared<DummyOwner>("Obj1");
  provider.notifyObjectsAdded({makeAddition(obj1)});

  EXPECT_EQ(list.getObjects().size(), 1U);

  provider.notifyObjectsRemoved({makeRemoval(obj1)});

  EXPECT_TRUE(removedTriggered);
  EXPECT_TRUE(list.getObjects().empty());
  EXPECT_TRUE(list.getUntypedObjects().empty());
}

/// @test
/// Checks that removing an object without a registered callback safely deletes the object mapping
/// @requirements(SEN-362)
TEST(ObjectList, RemovesWithoutCallbackRegistered)
{
  DummyObjectProvider provider;
  const sen::ObjectList<DummyOwner> list(provider, 10U);

  const auto obj1 = std::make_shared<DummyOwner>("Obj1");
  provider.notifyObjectsAdded({makeAddition(obj1)});

  EXPECT_EQ(list.getObjects().size(), 1U);

  provider.notifyObjectsRemoved({makeRemoval(obj1)});

  EXPECT_TRUE(list.getObjects().empty());
}

/// @test
/// Checks that removing an object without a registered callback deletes mapping for generic Object instantiation
/// @requirements(SEN-362)
TEST(ObjectList, RemovesWithoutCallbackRegistered_GenericObject)
{
  DummyObjectProvider provider;
  const sen::ObjectList<sen::Object> list(provider, 10U);

  const auto obj1 = std::make_shared<DummyOwner>("Obj1");
  provider.notifyObjectsAdded({makeAddition(obj1)});

  EXPECT_EQ(list.getObjects().size(), 1U);

  provider.notifyObjectsRemoved({makeRemoval(obj1)});

  EXPECT_TRUE(list.getObjects().empty());
}

/// @test
/// Checks that attempting to remove an unknown object ignores it gracefully
/// @requirements(SEN-362)
TEST(ObjectList, IgnoresUnknownRemovals)
{
  DummyObjectProvider provider;
  sen::ObjectList<DummyOwner> list(provider, 10U);

  bool removedTriggered = false;
  std::ignore = list.onRemoved([&](const auto&) { removedTriggered = true; });

  const auto obj1 = std::make_shared<DummyOwner>("Obj1");
  provider.notifyObjectsRemoved({makeRemoval(obj1)});

  EXPECT_FALSE(removedTriggered);
  EXPECT_TRUE(list.getObjects().empty());
}

/// @test
/// Checks that attempting to remove an unknown object ignores it gracefully for generic Object instantiation
/// @requirements(SEN-362)
TEST(ObjectList, IgnoresUnknownRemovals_GenericObject)
{
  DummyObjectProvider provider;
  sen::ObjectList<sen::Object> list(provider, 10U);

  bool removedTriggered = false;
  std::ignore = list.onRemoved([&](const auto&) { removedTriggered = true; });

  const auto obj1 = std::make_shared<DummyOwner>("Obj1");
  provider.notifyObjectsRemoved({makeRemoval(obj1)});

  EXPECT_FALSE(removedTriggered);
  EXPECT_TRUE(list.getObjects().empty());
}

/// @test
/// Checks that sending empty or null additions does not trigger callbacks or corrupt data
/// @requirements(SEN-362)
TEST(ObjectList, IgnoresEmptyOrNullAdditions)
{
  DummyObjectProvider provider;
  sen::ObjectList<DummyOwner> list(provider, 10U);

  bool addedTriggered = false;
  std::ignore = list.onAdded([&](const auto&) { addedTriggered = true; });

  provider.notifyObjectsAdded({});
  EXPECT_FALSE(addedTriggered);

  sen::ObjectInstanceDiscovery emptyDisc;
  emptyDisc.instance = nullptr;
  provider.notifyObjectsAdded({emptyDisc});
  EXPECT_FALSE(addedTriggered);
}

/// @test
/// Checks that sending empty or null additions does not trigger callbacks for generic Object instantiation
/// @requirements(SEN-362)
TEST(ObjectList, IgnoresEmptyOrNullAdditions_GenericObject)
{
  DummyObjectProvider provider;
  sen::ObjectList<sen::Object> list(provider, 10U);

  bool addedTriggered = false;
  std::ignore = list.onAdded([&](const auto&) { addedTriggered = true; });

  provider.notifyObjectsAdded({});
  EXPECT_FALSE(addedTriggered);

  sen::ObjectInstanceDiscovery emptyDisc;
  emptyDisc.instance = nullptr;
  provider.notifyObjectsAdded({emptyDisc});
  EXPECT_FALSE(addedTriggered);
}

/// @test
/// Checks that using a generic Object template type bypasses dynamic casting
/// @requirements(SEN-362)
TEST(ObjectList, HandlesGenericObjectBypassingCast)
{
  DummyObjectProvider provider;
  const sen::ObjectList<sen::Object> list(provider, 10U);

  const auto obj1 = std::make_shared<WrongTypeObject>(sen::ObjectId {100U});
  provider.notifyObjectsAdded({makeAddition(obj1)});

  EXPECT_EQ(list.getObjects().size(), 1U);
}

/// @test
/// Confirms an exception is thrown when an incoming object fails the strong typed dynamic cast expected by the
/// ObjectList
/// @requirements(SEN-362)
TEST(ObjectList, ThrowsOnInvalidDynamicCast)
{
  DummyObjectProvider provider;
  sen::ObjectList<DummyOwner> list(provider, 10U);

  const auto obj1 = std::make_shared<WrongTypeObject>(sen::ObjectId {99U});

  EXPECT_ANY_THROW({ provider.notifyObjectsAdded({makeAddition(obj1)}); });
}
