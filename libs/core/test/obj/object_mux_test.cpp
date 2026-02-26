// === object_mux_test.cpp =============================================================================================
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
#include "sen/core/obj/detail/proxy_object.h"
#include "sen/core/obj/interest.h"
#include "sen/core/obj/object.h"
#include "sen/core/obj/object_mux.h"
#include "sen/core/obj/object_provider.h"

// google test
#include <gtest/gtest.h>

// std
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

using sen::ClassType;
using sen::ConnectionGuard;
using sen::ConnId;
using sen::Event;
using sen::EventCallback;
using sen::EventInfo;
using sen::InterestId;
using sen::MemberHash;
using sen::Method;
using sen::MethodCallback;
using sen::MuxedProviderListener;
using sen::Object;
using sen::ObjectAddition;
using sen::ObjectAdditionList;
using sen::ObjectId;
using sen::ObjectInstanceDiscovery;
using sen::ObjectMux;
using sen::ObjectOwnerId;
using sen::ObjectProvider;
using sen::ObjectProviderListener;
using sen::ObjectRemoval;
using sen::ObjectRemovalList;
using sen::OutputStream;
using sen::Property;
using sen::TimeStamp;
using sen::Var;
using sen::VarList;

namespace
{

class MinimalObject final: public Object
{
public:
  MinimalObject(const ObjectId id, std::string name)
    : id_(id)
    , name_(std::move(name))
    , exampleType_(ClassType::make({"TestClass", "TestClass", "", {}, {}, {}, {}, {}, true, {}, {}}))
  {
  }

  [[nodiscard]] const std::string& getName() const noexcept override { return name_; }
  [[nodiscard]] ObjectId getId() const noexcept override { return id_; }
  [[nodiscard]] const std::string& getLocalName() const noexcept override { return name_; }

  [[nodiscard]] sen::ConstTypeHandle<ClassType> getClass() const noexcept override { return exampleType_; }
  [[nodiscard]] TimeStamp getLastCommitTime() const noexcept override { return TimeStamp {0}; }

  void senImplRemoveTypedConnection(ConnId /*id*/) override {}
  void senImplRemoveUntypedConnection(ConnId /*id*/, MemberHash /*memberHash*/) override {}
  void senImplEventEmitted(MemberHash /*id*/,
                           std::function<VarList()>&& /*argsGetter*/,
                           const EventInfo& /*info*/) override
  {
  }
  void senImplWriteAllPropertiesToStream(OutputStream& /*out*/) const override {}
  void senImplWriteStaticPropertiesToStream(OutputStream& /*out*/) const override {}
  void senImplWriteDynamicPropertiesToStream(OutputStream& /*out*/) const override {}

  [[nodiscard]] Var getPropertyUntyped(const Property* /*prop*/) const override { return {}; }
  void invokeUntyped(const Method* /*method*/, const VarList& /*args*/, MethodCallback<Var>&& /*onDone*/) override {}
  [[nodiscard]] ConnectionGuard onPropertyChangedUntyped(const Property* /*prop*/,
                                                         EventCallback<VarList>&& /*callback*/) override
  {
    return senImplMakeConnectionGuard(ConnId {0}, {}, false);
  }
  [[nodiscard]] ConnectionGuard onEventUntyped(const Event* /*ev*/, EventCallback<VarList>&& /*callback*/) override
  {
    return senImplMakeConnectionGuard(ConnId {0}, {}, false);
  }
  void invokeAllPropertyCallbacks() override {}

private:
  ObjectId id_;
  std::string name_;
  sen::ConstTypeHandle<ClassType> exampleType_;
};

class MockObjectProvider final: public ObjectProvider
{
public:
  using ObjectProvider::notifyObjectsAdded;
  using ObjectProvider::notifyObjectsRemoved;

  void setStoredObjects(const ObjectAdditionList& objs) { storedObjects_ = objs; }

protected:
  void notifyAddedOnExistingObjects(ObjectProviderListener* listener) override
  {
    if (!storedObjects_.empty())
    {
      callOnObjectsAdded(listener, storedObjects_);
    }
  }

  void notifyRemovedOnExistingObjects(ObjectProviderListener* listener) override
  {
    if (!storedObjects_.empty())
    {
      ObjectRemovalList removals;
      removals.reserve(storedObjects_.size());
      for (const auto& add: storedObjects_)
      {
        removals.push_back(makeRemoval(add));
      }
      callOnObjectsRemoved(listener, removals);
    }
  }

private:
  ObjectAdditionList storedObjects_;
};

struct EventLog
{
  ObjectAdditionList added;      // NOLINT(misc-non-private-member-variables-in-classes)
  ObjectRemovalList removed;     // NOLINT(misc-non-private-member-variables-in-classes)
  ObjectAdditionList readded;    // NOLINT(misc-non-private-member-variables-in-classes)
  ObjectRemovalList refReduced;  // NOLINT(misc-non-private-member-variables-in-classes)

  void clear()
  {
    added.clear();
    removed.clear();
    readded.clear();
    refReduced.clear();
  }
};

class MockMuxListener final: public MuxedProviderListener
{
public:
  EventLog log;  // NOLINT(misc-non-private-member-variables-in-classes)

  void onObjectsAdded(const ObjectAdditionList& additions) override
  {
    log.added.insert(log.added.end(), additions.begin(), additions.end());
  }

  void onObjectsRemoved(const ObjectRemovalList& removals) override
  {
    log.removed.insert(log.removed.end(), removals.begin(), removals.end());
  }

  void onExistingObjectsReadded(const ObjectAdditionList& additions) override
  {
    log.readded.insert(log.readded.end(), additions.begin(), additions.end());
  }

  void onObjectsRefCountReduced(const ObjectRemovalList& removals) override
  {
    log.refReduced.insert(log.refReduced.end(), removals.begin(), removals.end());
  }

  [[nodiscard]] sen::kernel::impl::RemoteParticipant* isRemoteParticipant() noexcept override { return nullptr; }
  [[nodiscard]] sen::kernel::impl::LocalParticipant* isLocalParticipant() noexcept override { return nullptr; }
};

class SelfDestructListener final: public MuxedProviderListener
{
public:
  ObjectMux* targetMux = nullptr;  // NOLINT(misc-non-private-member-variables-in-classes)

  void onObjectsAdded(const ObjectAdditionList& /*additions*/) override
  {
    if (targetMux != nullptr)
    {
      targetMux->removeMuxedListener(this, false);
    }
  }

  void onObjectsRemoved(const ObjectRemovalList& /*removals*/) override {}
  void onExistingObjectsReadded(const ObjectAdditionList& /*additions*/) override {}
  void onObjectsRefCountReduced(const ObjectRemovalList& /*removals*/) override {}
  [[nodiscard]] sen::kernel::impl::RemoteParticipant* isRemoteParticipant() noexcept override { return nullptr; }
  [[nodiscard]] sen::kernel::impl::LocalParticipant* isLocalParticipant() noexcept override { return nullptr; }
};

class MockStandardListener final: public ObjectProviderListener
{
public:
  EventLog log;  // NOLINT(misc-non-private-member-variables-in-classes)

  void onObjectsAdded(const ObjectAdditionList& additions) override
  {
    log.added.insert(log.added.end(), additions.begin(), additions.end());
  }

  void onObjectsRemoved(const ObjectRemovalList& removals) override
  {
    log.removed.insert(log.removed.end(), removals.begin(), removals.end());
  }

  [[nodiscard]] sen::kernel::impl::RemoteParticipant* isRemoteParticipant() noexcept override { return nullptr; }
  [[nodiscard]] sen::kernel::impl::LocalParticipant* isLocalParticipant() noexcept override { return nullptr; }
};

class ObjectMuxTest: public testing::Test
{
protected:
  ObjectMux mux;                                  // NOLINT(misc-non-private-member-variables-in-classes)
  MockMuxListener listener;                       // NOLINT(misc-non-private-member-variables-in-classes)
  std::shared_ptr<MockObjectProvider> providerA;  // NOLINT(misc-non-private-member-variables-in-classes)
  std::shared_ptr<MockObjectProvider> providerB;  // NOLINT(misc-non-private-member-variables-in-classes)

  static std::shared_ptr<MinimalObject> createObj(const uint32_t idVal, const std::string& name)
  {
    return std::make_shared<MinimalObject>(ObjectId {idVal}, name);
  }

  static ObjectAddition makeAdd(const std::shared_ptr<Object>& obj,
                                const InterestId iId = InterestId {1},
                                const ObjectOwnerId owner = ObjectOwnerId {100})
  {
    ObjectInstanceDiscovery disc;
    disc.instance = obj;
    disc.id = obj->getId();
    disc.interestId = iId;
    disc.ownerId = owner;
    return disc;
  }

  static ObjectRemoval makeRem(const std::shared_ptr<Object>& obj, const InterestId iId = InterestId {1})
  {
    return {iId, obj->getId(), ObjectOwnerId {100}};
  }

  void SetUp() override
  {
    providerA = std::make_shared<MockObjectProvider>();
    providerB = std::make_shared<MockObjectProvider>();

    providerA->addListener(&mux, false);
    providerB->addListener(&mux, false);

    mux.addMuxedListener(&listener, false);
  }

  void TearDown() override
  {
    if (mux.hasMuxedListener(&listener))
    {
      mux.removeMuxedListener(&listener, false);
    }
    if (providerA->hasListener(&mux))
    {
      providerA->removeListener(&mux, false);
    }
    if (providerB->hasListener(&mux))
    {
      providerB->removeListener(&mux, false);
    }
  }
};

}  // namespace

/// @test
/// Checks that a single provider adding and removing objects triggers the standard add/remove callbacks
/// Also verifies that ObjectOwnerId and other data fields are preserved intact.
/// @requirements(SEN-362)
TEST_F(ObjectMuxTest, SingleProviderFlow)
{
  const auto obj1 = createObj(10, "Obj1");
  constexpr auto specificOwner = ObjectOwnerId {99};

  providerA->notifyObjectsAdded({makeAdd(obj1, InterestId {1}, specificOwner)});

  ASSERT_EQ(listener.log.added.size(), 1U);
  EXPECT_EQ(getObjectId(listener.log.added[0]), obj1->getId());
  EXPECT_EQ(getObjectOwnerId(listener.log.added[0]), specificOwner);
  EXPECT_TRUE(listener.log.readded.empty());

  providerA->notifyObjectsRemoved({makeRem(obj1)});

  ASSERT_EQ(listener.log.removed.size(), 1U);
  EXPECT_EQ(listener.log.removed[0].objectid, obj1->getId());
  EXPECT_TRUE(listener.log.refReduced.empty());
}

/// @test
/// Checks that adding the same object from a second provider triggers the re-added callback
/// @requirements(SEN-362)
TEST_F(ObjectMuxTest, MultiplexingLogicAddition)
{
  const auto obj1 = createObj(10, "Obj1");
  auto addition = makeAdd(obj1);

  providerA->notifyObjectsAdded({addition});

  EXPECT_EQ(listener.log.added.size(), 1U);
  EXPECT_EQ(listener.log.readded.size(), 0U);

  providerB->notifyObjectsAdded({addition});

  EXPECT_EQ(listener.log.added.size(), 1U);
  EXPECT_EQ(listener.log.readded.size(), 1U);
  EXPECT_EQ(getObjectId(listener.log.readded[0]), obj1->getId());
}

/// @test
/// Checks that removing an object available from multiple providers triggers the ref-count-reduced callback
/// @requirements(SEN-362)
TEST_F(ObjectMuxTest, MultiplexingLogicRemoval)
{
  const auto obj1 = createObj(10, "Obj1");
  auto addition = makeAdd(obj1);
  auto removal = makeRem(obj1);

  providerA->notifyObjectsAdded({addition});
  providerB->notifyObjectsAdded({addition});
  listener.log.clear();

  providerA->notifyObjectsRemoved({removal});

  EXPECT_EQ(listener.log.removed.size(), 0U);
  EXPECT_EQ(listener.log.refReduced.size(), 1U);
  EXPECT_EQ(listener.log.refReduced[0].objectid, obj1->getId());

  providerB->notifyObjectsRemoved({removal});

  EXPECT_EQ(listener.log.removed.size(), 1U);
  EXPECT_EQ(listener.log.removed[0].objectid, obj1->getId());
}

/// @test
/// Checks that adding the same object twice in a single batch behaves as an addition followed by a re-addition
/// @requirements(SEN-362)
TEST_F(ObjectMuxTest, DuplicateAdditionInSingleBatch)
{
  const auto obj1 = createObj(10, "Obj1");
  auto addition = makeAdd(obj1);

  providerA->notifyObjectsAdded({addition, addition});

  EXPECT_EQ(listener.log.added.size(), 1U);
  EXPECT_EQ(listener.log.readded.size(), 1U);
  EXPECT_EQ(getObjectId(listener.log.readded[0]), obj1->getId());
}

/// @test
/// Checks that removing the same object twice in a single batch behaves as a ref-reduction followed by removal
/// @requirements(SEN-362)
TEST_F(ObjectMuxTest, DuplicateRemovalInSingleBatch)
{
  const auto obj1 = createObj(10, "Obj1");
  auto addition = makeAdd(obj1);
  auto removal = makeRem(obj1);

  providerA->notifyObjectsAdded({addition, addition});
  listener.log.clear();

  providerA->notifyObjectsRemoved({removal, removal});

  EXPECT_EQ(listener.log.refReduced.size(), 1U);
  EXPECT_EQ(listener.log.removed.size(), 1U);
}

/// @test
/// Checks that a batch containing a new object and a known object is split into added and re-added callbacks
/// @requirements(SEN-362)
TEST_F(ObjectMuxTest, MixedBatchAddition)
{
  const auto obj1 = createObj(10, "Obj1");
  const auto obj2 = createObj(20, "Obj2");

  providerA->notifyObjectsAdded({makeAdd(obj1)});
  listener.log.clear();

  ObjectAdditionList batch;
  batch.push_back(makeAdd(obj1));
  batch.push_back(makeAdd(obj2));

  providerB->notifyObjectsAdded(batch);

  ASSERT_EQ(listener.log.added.size(), 1U);
  EXPECT_EQ(getObjectId(listener.log.added[0]), obj2->getId());
  ASSERT_EQ(listener.log.readded.size(), 1U);
  EXPECT_EQ(getObjectId(listener.log.readded[0]), obj1->getId());
}

/// @test
/// Checks that adding a listener with notification enabled receives current objects and ref-counts correctly
/// @requirements(SEN-362)
TEST_F(ObjectMuxTest, ListenerLateAttachmentWithDuplicates)
{
  const auto obj1 = createObj(10, "Obj1");
  const auto obj2 = createObj(20, "Obj2");

  providerA->notifyObjectsAdded({makeAdd(obj1), makeAdd(obj2)});
  providerB->notifyObjectsAdded({makeAdd(obj2)});

  MockMuxListener lateListener;
  mux.addMuxedListener(&lateListener, true);

  ASSERT_EQ(lateListener.log.added.size(), 2U);
  ASSERT_EQ(lateListener.log.readded.size(), 1U);

  EXPECT_EQ(getObjectId(lateListener.log.readded[0]), obj2->getId());

  mux.removeMuxedListener(&lateListener, false);
}

/// @test
/// Checks that removing a listener with notification enabled receives removal and ref-reduced callbacks
/// @requirements(SEN-362)
TEST_F(ObjectMuxTest, ListenerDetachmentWithNotification)
{
  const auto obj1 = createObj(10, "Obj1");
  const auto obj2 = createObj(20, "Obj2");

  providerA->notifyObjectsAdded({makeAdd(obj1), makeAdd(obj2)});
  providerB->notifyObjectsAdded({makeAdd(obj2)});

  listener.log.clear();

  mux.removeMuxedListener(&listener, true);

  ASSERT_EQ(listener.log.removed.size(), 2U);
  ASSERT_EQ(listener.log.refReduced.size(), 1U);
  EXPECT_EQ(listener.log.refReduced[0].objectid, obj2->getId());
}

/// @test
/// Checks that a standard ObjectProviderListener only receives logical add/remove and no multiplexing details
/// @requirements(SEN-362)
TEST_F(ObjectMuxTest, StandardListenerCompatibility)
{
  MockStandardListener stdListener;
  mux.addListener(&stdListener, false);

  const auto obj1 = createObj(10, "Obj1");

  providerA->notifyObjectsAdded({makeAdd(obj1)});
  providerB->notifyObjectsAdded({makeAdd(obj1)});

  EXPECT_EQ(stdListener.log.added.size(), 1U);

  providerA->notifyObjectsRemoved({makeRem(obj1)});
  EXPECT_EQ(stdListener.log.removed.size(), 0U);

  providerB->notifyObjectsRemoved({makeRem(obj1)});
  EXPECT_EQ(stdListener.log.removed.size(), 1U);

  mux.removeListener(&stdListener, false);
}

/// @test
/// Checks that a late-connecting standard listener receives only unique existing objects
/// @requirements(SEN-362)
TEST_F(ObjectMuxTest, StandardListenerLateAttachment)
{
  const auto obj1 = createObj(10, "Obj1");
  providerA->notifyObjectsAdded({makeAdd(obj1)});
  providerB->notifyObjectsAdded({makeAdd(obj1)});

  MockStandardListener stdListener;
  mux.addListener(&stdListener, true);

  ASSERT_EQ(stdListener.log.added.size(), 1U);
  EXPECT_EQ(getObjectId(stdListener.log.added[0]), obj1->getId());

  mux.removeListener(&stdListener, true);
  ASSERT_EQ(stdListener.log.removed.size(), 1U);
}

/// @test
/// Checks that a late-connecting provider triggers addition for new objects on the mux
/// @requirements(SEN-362)
TEST_F(ObjectMuxTest, LateProviderConnection)
{
  const auto obj1 = createObj(10, "Obj1");

  providerA->removeListener(&mux, false);
  providerA->setStoredObjects({makeAdd(obj1)});
  providerA->addListener(&mux, true);

  ASSERT_EQ(listener.log.added.size(), 1U);
  EXPECT_EQ(getObjectId(listener.log.added[0]), obj1->getId());
}

/// @test
/// Checks that removing an object with a non-matching InterestId is ignored
/// @requirements(SEN-362)
TEST_F(ObjectMuxTest, RemovalInterestMismatch)
{
  const auto obj1 = createObj(10, "Obj1");

  providerA->notifyObjectsAdded({makeAdd(obj1, InterestId {1})});
  providerA->notifyObjectsRemoved({makeRem(obj1, InterestId {2})});
  EXPECT_EQ(listener.log.removed.size(), 0U);

  providerA->notifyObjectsRemoved({makeRem(obj1, InterestId {1})});
  EXPECT_EQ(listener.log.removed.size(), 1U);
}

/// @test
/// Checks that empty input lists do not trigger callbacks
/// @requirements(SEN-362)
TEST_F(ObjectMuxTest, EmptyInputSafety)
{
  providerA->notifyObjectsAdded({});
  EXPECT_EQ(listener.log.added.size(), 0U);

  providerA->notifyObjectsRemoved({});
  EXPECT_EQ(listener.log.removed.size(), 0U);
}

/// @test
/// Checks logic for determining if the mux has specific types of listeners attached
/// @requirements(SEN-362)
TEST_F(ObjectMuxTest, ListenerQueries)
{
  EXPECT_TRUE(mux.hasListeners());
  EXPECT_TRUE(mux.hasMuxedListener(&listener));

  MockMuxListener otherMuxListener;
  EXPECT_FALSE(mux.hasMuxedListener(&otherMuxListener));

  MockStandardListener stdListener;
  EXPECT_FALSE(mux.hasListener(&stdListener));

  mux.removeMuxedListener(&listener, false);
  EXPECT_FALSE(mux.hasListeners());

  mux.addListener(&stdListener, false);
  EXPECT_TRUE(mux.hasListeners());
  EXPECT_TRUE(mux.hasListener(&stdListener));

  mux.removeListener(&stdListener, false);
}

/// @test
/// Checks that adding or removing the same listener multiple times is safe and ignored
/// @requirements(SEN-362)
TEST_F(ObjectMuxTest, IdempotencyChecks)
{
  mux.addMuxedListener(&listener, false);

  const auto obj1 = createObj(10, "Obj1");
  providerA->notifyObjectsAdded({makeAdd(obj1)});

  EXPECT_EQ(listener.log.added.size(), 1U);

  mux.removeMuxedListener(&listener, false);
  mux.removeMuxedListener(&listener, false);

  EXPECT_FALSE(mux.hasMuxedListener(&listener));
}

/// @test
/// Checks that the mux cleans up internal references when a listener is destroyed externally
/// @requirements(SEN-362)
TEST_F(ObjectMuxTest, ListenerDestructionCleanup)
{
  mux.removeMuxedListener(&listener, false);
  EXPECT_FALSE(mux.hasListeners());

  {
    MockMuxListener tempListener;
    mux.addMuxedListener(&tempListener, false);
    EXPECT_TRUE(mux.hasListeners());
  }

  EXPECT_FALSE(mux.hasListeners());
}

/// @test
/// Checks that a listener removing itself during a callback does not crash the mux
/// @requirements(SEN-362)
TEST_F(ObjectMuxTest, ListenerSelfRemoval)
{
  SelfDestructListener riskyListener;
  riskyListener.targetMux = &mux;
  mux.addMuxedListener(&riskyListener, false);

  const auto obj1 = createObj(10, "Obj1");

  EXPECT_NO_THROW(providerA->notifyObjectsAdded({makeAdd(obj1)}));
  EXPECT_FALSE(mux.hasMuxedListener(&riskyListener));
}
