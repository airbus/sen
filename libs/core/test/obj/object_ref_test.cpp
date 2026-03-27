// === object_ref_test.cpp =============================================================================================
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
#include "sen/core/meta/property.h"
#include "sen/core/meta/type.h"
#include "sen/core/meta/var.h"
#include "sen/core/obj/callback.h"
#include "sen/core/obj/connection_guard.h"
#include "sen/core/obj/object.h"
#include "sen/core/obj/object_provider.h"
#include "sen/core/obj/object_ref.h"

// generated code
#include "stl/example_class.stl.h"

// google test
#include <gtest/gtest.h>

// std
#include <functional>
#include <memory>
#include <string>
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
/// Checks invalid default constructor of ObjectRef
/// @requirements(SEN-362)
TEST(ObjectRef, DefaultConstructorInvalid)
{
  sen::ObjectRef<DummyOwner> typedRef;
  EXPECT_FALSE(typedRef.valid());
  EXPECT_FALSE(static_cast<bool>(typedRef));

  sen::ObjectRef<sen::Object> genericRef;
  EXPECT_FALSE(genericRef.valid());
  EXPECT_FALSE(static_cast<bool>(genericRef));
}

/// @test
/// Checks to add/remove an object to an objectRef
/// @requirements(SEN-362)
TEST(ObjectRef, AddedAndRemovedObject)
{
  DummyObjectProvider provider;
  sen::ObjectRef<DummyOwner> objRef;
  provider.addListener(&objRef, false);

  const auto obj = std::make_shared<DummyOwner>("Obj1");
  provider.notifyObjectsAdded({makeAddition(obj)});

  EXPECT_TRUE(objRef.valid());
  EXPECT_EQ(&objRef.get(), obj.get());
  EXPECT_EQ(objRef.operator->(), obj.get());

  // const accessors
  const auto& constRef = objRef;
  EXPECT_EQ(constRef.operator->(), obj.get());
  EXPECT_EQ(&constRef.get(), obj.get());

  // install removal callback and remove
  bool removedTriggered = false;
  std::ignore = objRef.onRemoved([&]() { removedTriggered = true; });

  provider.notifyObjectsRemoved({makeRemoval(obj)});

  EXPECT_FALSE(objRef.valid());
  EXPECT_TRUE(removedTriggered);
}

/// @test
/// Checks ObjectRef<sen::Object> lifecycle
/// @requirements(SEN-362)
TEST(ObjectRef, AddedAndRemovedGenericObject)
{
  DummyObjectProvider provider;
  sen::ObjectRef<sen::Object> objRef;
  provider.addListener(&objRef, false);

  bool addedTriggered = false;
  bool removedTriggered = false;
  std::ignore = objRef.onAdded([&]() { addedTriggered = true; });
  std::ignore = objRef.onRemoved([&]() { removedTriggered = true; });

  const auto obj = std::make_shared<DummyOwner>("Obj1");
  provider.notifyObjectsAdded({makeAddition(obj)});

  EXPECT_TRUE(objRef.valid());
  EXPECT_TRUE(addedTriggered);
  EXPECT_EQ(&objRef.get(), obj.get());

  provider.notifyObjectsRemoved({makeRemoval(obj)});

  EXPECT_FALSE(objRef.valid());
  EXPECT_TRUE(removedTriggered);
}

/// @test
/// Checks that ignores null additions, empty removals, and unrelated objects
/// @requirements(SEN-362)
TEST(ObjectRef, UnrelatedObjects)
{
  DummyObjectProvider provider;
  sen::ObjectRef<DummyOwner> objRef;
  provider.addListener(&objRef, false);

  bool addedTriggered = false;
  bool removedTriggered = false;
  std::ignore = objRef.onAdded([&]() { addedTriggered = true; });
  std::ignore = objRef.onRemoved([&]() { removedTriggered = true; });

  // empty addition list
  provider.notifyObjectsAdded({});
  EXPECT_FALSE(addedTriggered);

  // null instance
  sen::ObjectInstanceDiscovery emptyDisc;
  emptyDisc.instance = nullptr;
  provider.notifyObjectsAdded({emptyDisc});
  EXPECT_FALSE(addedTriggered);
  EXPECT_FALSE(objRef.valid());

  // removal on an empty objRef
  const auto obj1 = std::make_shared<DummyOwner>("Obj1");
  provider.notifyObjectsRemoved({makeRemoval(obj1)});
  EXPECT_FALSE(removedTriggered);

  // add obj1 and try removing a different object
  provider.notifyObjectsAdded({makeAddition(obj1)});
  EXPECT_TRUE(objRef.valid());

  const auto obj2 = std::make_shared<DummyOwner>("Obj2");
  provider.notifyObjectsRemoved({makeRemoval(obj2)});
  EXPECT_TRUE(objRef.valid());
  EXPECT_FALSE(removedTriggered);
}

/// @test
/// Installing a new callback returns the previous one
/// @requirements(SEN-362)
TEST(ObjectRef, ReplaceCallback)
{
  sen::ObjectRef<DummyOwner> objRef;

  int counter = 0;
  auto firstAdded = objRef.onAdded([&]() { counter = 1; });
  EXPECT_EQ(firstAdded, nullptr);

  auto previousAdded = objRef.onAdded([&]() { counter = 2; });
  EXPECT_NE(previousAdded, nullptr);
  previousAdded();
  EXPECT_EQ(counter, 1);

  auto firstRemoved = objRef.onRemoved([&]() { counter = 10; });
  EXPECT_EQ(firstRemoved, nullptr);

  auto previousRemoved = objRef.onRemoved([&]() { counter = 20; });
  EXPECT_NE(previousRemoved, nullptr);
  previousRemoved();
  EXPECT_EQ(counter, 10);
}

/// @test
/// Checks exception throw when dynamic cast fails
/// @requirements(SEN-362)
TEST(ObjectRef, ThrowsOnInvalidDynamicCast)
{
  DummyObjectProvider provider;
  sen::ObjectRef<DummyOwner> objRef;
  provider.addListener(&objRef, false);

  const auto obj = std::make_shared<WrongTypeObject>(sen::ObjectId {99U});
  EXPECT_ANY_THROW({ provider.notifyObjectsAdded({makeAddition(obj)}); });
}

/// @test
/// Checks that ObjectRef<sen::Object> accepts any Object subclass without performing a dynamic cast
/// @requirements(SEN-362)
TEST(ObjectRef, GenericObjectWithoutCast)
{
  DummyObjectProvider provider;
  sen::ObjectRef<sen::Object> objRef;
  provider.addListener(&objRef, false);

  const auto obj = std::make_shared<WrongTypeObject>(sen::ObjectId {100U});
  provider.notifyObjectsAdded({makeAddition(obj)});

  EXPECT_TRUE(objRef.valid());
  EXPECT_EQ(&objRef.get(), obj.get());
}
