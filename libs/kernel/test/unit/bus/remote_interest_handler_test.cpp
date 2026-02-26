// === remote_interest_handler_test.cpp ================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

// sen
#include "bus/remote_interest_handler.h"
#include "sen/core/obj/interest.h"

// gtest
#include <gtest/gtest.h>

using sen::InterestId;
using sen::kernel::impl::RemoteInterestsHandler;

// Redefinitions for testing purposes
namespace sen::kernel::impl
{

class RemoteParticipant
{
};

class ObjectUpdate
{
};

}  // namespace sen::kernel::impl

using sen::kernel::impl::ObjectUpdate;
using sen::kernel::impl::RemoteParticipant;

/// @test
/// Check Remote interest subscription when there are no objects
/// @requirements(SEN-369)
TEST(ARemoteInterestHandler, processANewRemoteSubscriptionWhenThereAreNoObjects)
{
  RemoteInterestsHandler remoteInterestHandler;
  RemoteParticipant remote;

  remoteInterestHandler.addSubscriber(InterestId(1), &remote, true);

  ASSERT_EQ(remoteInterestHandler.remotesUpdatesBMMap.size<RemoteParticipant*>(), 0);
  ASSERT_EQ(remoteInterestHandler.remotesToInterestsUpdatesMap.size(), 0);
  ASSERT_TRUE(remoteInterestHandler.remotesInterestsBMMap.contains(&remote));
}

/// @test
/// Check Remote interest object when a subscriber was added
/// @requirements(SEN-369)
TEST(ARemoteInterestHandler, processANewObjectWhenASubscriberWasAdded)
{
  RemoteInterestsHandler remoteInterestHandler;
  RemoteParticipant remote;
  ObjectUpdate update;
  remoteInterestHandler.addSubscriber(InterestId(1), &remote);

  remoteInterestHandler.addObject(InterestId(1), &update);

  ASSERT_TRUE(remoteInterestHandler.interestsUpdatesBMMap.contains(InterestId(1)));
  ASSERT_TRUE(remoteInterestHandler.remotesUpdatesBMMap.contains(&remote));
  ASSERT_EQ(remoteInterestHandler.remotesToInterestsUpdatesMap.size(), 1);
}

/// @test
/// Check Remote interest with two objects when a subscriber was added
/// @requirements(SEN-369)
TEST(ARemoteInterestHandler, processTwoObjectWhenASubscriberWasAdded)
{
  RemoteInterestsHandler remoteInterestHandler;
  RemoteParticipant remote;
  ObjectUpdate update1;
  ObjectUpdate update2;
  remoteInterestHandler.addSubscriber(InterestId(1), &remote);

  remoteInterestHandler.addObject(InterestId(1), &update1);
  remoteInterestHandler.addObject(InterestId(1), &update2);

  const auto* const ptr1 = remoteInterestHandler.interestsUpdatesBMMap.tryGet(InterestId(1));
  ASSERT_NE(nullptr, ptr1);
  EXPECT_EQ(ptr1->size(), 2);

  const auto* const ptr2 = remoteInterestHandler.remotesUpdatesBMMap.tryGet(&remote);
  ASSERT_NE(nullptr, ptr2);
  EXPECT_EQ(ptr2->size(), 2);
}

/// @test
/// Check Remote interest subscription when an object was added
/// @requirements(SEN-369)
TEST(ARemoteInterestHandler, registersANewRemoteSubscriptionAfterObjectWasAdded)
{
  RemoteInterestsHandler remoteInterestHandler;
  RemoteParticipant remote;
  ObjectUpdate update;
  remoteInterestHandler.addObject(InterestId(1), &update);

  remoteInterestHandler.addSubscriber(InterestId(1), &remote);

  EXPECT_TRUE(remoteInterestHandler.remotesUpdatesBMMap.contains(&remote));
  EXPECT_TRUE(remoteInterestHandler.remotesToInterestsUpdatesMap.at(&remote).contains(InterestId(1)));
}

/// @test
/// Check Remote interest with two subscription when an object was added
/// @requirements(SEN-369)
TEST(ARemoteInterestHandler, registersTwoSubscriptionsAfterObjectWasAdded)
{
  RemoteInterestsHandler remoteInterestHandler;
  RemoteParticipant remote1;
  RemoteParticipant remote2;
  ObjectUpdate update;
  remoteInterestHandler.addObject(InterestId(1), &update);

  remoteInterestHandler.addSubscriber(InterestId(1), &remote1);
  remoteInterestHandler.addSubscriber(InterestId(1), &remote2);

  EXPECT_EQ(remoteInterestHandler.remotesUpdatesBMMap.size<RemoteParticipant*>(), 2);
  EXPECT_EQ(remoteInterestHandler.remotesToInterestsUpdatesMap.size(), 2);
}

/// @test
/// Check the removal of Remote interest object and subscriber
/// @requirements(SEN-369)
TEST(ARemoteInterestHandler, removesSubscriptionWithUpdates)
{
  RemoteInterestsHandler remoteInterestHandler;
  RemoteParticipant remote;
  ObjectUpdate update;
  remoteInterestHandler.addObject(InterestId(1), &update);
  remoteInterestHandler.addSubscriber(InterestId(1), &remote);

  remoteInterestHandler.removeSubscriber(InterestId(1), &remote);

  EXPECT_TRUE(remoteInterestHandler.remotesUpdatesBMMap.empty());
  EXPECT_TRUE(remoteInterestHandler.remotesToInterestsUpdatesMap.empty());
}

/// @test
/// Check Remote interest object and subscription when one subscriber is removed
/// @requirements(SEN-369)
TEST(ARemoteInterestHandler, keepsTheObjectInterestWhenAInterestedSubscriptionRemains)
{
  RemoteInterestsHandler remoteInterestHandler;
  RemoteParticipant remote1;
  RemoteParticipant remote2;
  ObjectUpdate update;

  remoteInterestHandler.addObject(InterestId(1), &update);
  remoteInterestHandler.addSubscriber(InterestId(1), &remote1);
  remoteInterestHandler.addSubscriber(InterestId(1), &remote2);

  remoteInterestHandler.removeSubscriber(InterestId(1), &remote1);

  EXPECT_TRUE(remoteInterestHandler.remotesUpdatesBMMap.contains(&remote2));
  EXPECT_TRUE(!remoteInterestHandler.remotesUpdatesBMMap.contains(&remote1));
  EXPECT_EQ(remoteInterestHandler.remotesToInterestsUpdatesMap.size(), 1);
  EXPECT_TRUE(remoteInterestHandler.interestsUpdatesBMMap.contains(InterestId(1)));
}

/// @test
/// Check Remote interest when an object is removed
/// @requirements(SEN-369)
TEST(ARemoteInterestHandler, deregistersObjectAfterObjectRemoval)
{
  RemoteInterestsHandler remoteInterestHandler;
  RemoteParticipant remote1;
  RemoteParticipant remote2;
  ObjectUpdate update;
  remoteInterestHandler.addObject(InterestId(1), &update);
  remoteInterestHandler.addSubscriber(InterestId(1), &remote1);
  remoteInterestHandler.addSubscriber(InterestId(1), &remote2);

  remoteInterestHandler.removeObject(&update);

  EXPECT_TRUE(remoteInterestHandler.remotesUpdatesBMMap.empty());
  EXPECT_TRUE(remoteInterestHandler.remotesToInterestsUpdatesMap.empty());
  EXPECT_TRUE(remoteInterestHandler.interestsUpdatesBMMap.empty());
}

/// @test
/// Check Remote interest deregistration from one object of two
/// @requirements(SEN-369)
TEST(ARemoteInterestHandler, deregistersOneOfTwoObjectFromInterestedRemotes)
{
  RemoteInterestsHandler remoteInterestHandler;
  RemoteParticipant remote1;
  RemoteParticipant remote2;
  ObjectUpdate update1;
  ObjectUpdate update2;
  remoteInterestHandler.addObject(InterestId(1), &update1);
  remoteInterestHandler.addObject(InterestId(1), &update2);
  remoteInterestHandler.addSubscriber(InterestId(1), &remote1);
  remoteInterestHandler.addSubscriber(InterestId(1), &remote2);

  {
    const auto* ptr = remoteInterestHandler.interestsUpdatesBMMap.tryGet(InterestId(1));
    ASSERT_NE(nullptr, ptr);
    EXPECT_EQ(ptr->size(), 2);
  }

  remoteInterestHandler.removeObject(&update1);

  EXPECT_EQ(remoteInterestHandler.remotesUpdatesBMMap.size<RemoteParticipant*>(), 2);
  EXPECT_EQ(remoteInterestHandler.remotesToInterestsUpdatesMap.size(), 2);

  {
    const auto* ptr = remoteInterestHandler.interestsUpdatesBMMap.tryGet(InterestId(1));
    ASSERT_NE(nullptr, ptr);
    EXPECT_EQ(ptr->size(), 1);
  }
}

/// @test
/// Check Remote interest registration of two interests to the same object
/// @requirements(SEN-369)
TEST(ARemoteInterestHandler, registerTwoInterestsToTheSameObject)
{
  RemoteInterestsHandler remoteInterestHandler;
  RemoteParticipant remote;
  ObjectUpdate update;
  remoteInterestHandler.addObject(InterestId(1), &update);
  remoteInterestHandler.addObject(InterestId(2), &update);

  remoteInterestHandler.addSubscriber(InterestId(1), &remote);
  remoteInterestHandler.addSubscriber(InterestId(2), &remote);

  auto& registeredUpdateInterests = remoteInterestHandler.remotesToInterestsUpdatesMap.at(&remote);
  EXPECT_TRUE(registeredUpdateInterests.contains(InterestId(1)));
  EXPECT_TRUE(registeredUpdateInterests.contains(InterestId(2)));
}

/// @test
/// Check that Remote interest keeps an object when an interest remains
/// @requirements(SEN-369)
TEST(ARemoteInterestHandler, keepsObjectForRemoteWhenAInterestRemains)
{
  RemoteInterestsHandler remoteInterestHandler;
  RemoteParticipant remote;
  ObjectUpdate update;
  remoteInterestHandler.addObject(InterestId(1), &update);
  remoteInterestHandler.addObject(InterestId(2), &update);
  remoteInterestHandler.addSubscriber(InterestId(1), &remote);
  remoteInterestHandler.addSubscriber(InterestId(2), &remote);
  remoteInterestHandler.removeSubscriber(InterestId(1), &remote);

  auto& registeredUpdateInterests = remoteInterestHandler.remotesToInterestsUpdatesMap.at(&remote);
  EXPECT_TRUE(registeredUpdateInterests.contains(&update));
  EXPECT_TRUE(registeredUpdateInterests.contains(InterestId(2)));
  EXPECT_TRUE(!registeredUpdateInterests.contains(InterestId(1)));
  EXPECT_TRUE(remoteInterestHandler.remotesUpdatesBMMap.contains(&update));
}

/// @test
/// Check Remote interest deregistration object after no interests remains
/// @requirements(SEN-369)
TEST(ARemoteInterestHandler, deregisterObjectForRemoteAfterNoInterestsRemain)
{
  RemoteInterestsHandler remoteInterestHandler;
  RemoteParticipant remote;
  ObjectUpdate update;
  remoteInterestHandler.addObject(InterestId(1), &update);
  remoteInterestHandler.addObject(InterestId(2), &update);
  remoteInterestHandler.addSubscriber(InterestId(1), &remote);
  remoteInterestHandler.addSubscriber(InterestId(2), &remote);

  remoteInterestHandler.removeSubscriber(InterestId(1), &remote);
  remoteInterestHandler.removeSubscriber(InterestId(2), &remote);

  EXPECT_TRUE(remoteInterestHandler.remotesUpdatesBMMap.empty());
  EXPECT_TRUE(remoteInterestHandler.remotesToInterestsUpdatesMap.empty());
}

/// @test
/// Check that Remote interest deregistration from a remote has its references empty
/// @requirements(SEN-369)
TEST(ARemoteInterestHandler, deregistersRemoteAndItsReferencesWhenRemoved)
{
  RemoteInterestsHandler remoteInterestHandler;
  RemoteParticipant remote;
  ObjectUpdate update1;
  ObjectUpdate update2;

  remoteInterestHandler.addObject(InterestId(1), &update1);
  remoteInterestHandler.addObject(InterestId(2), &update2);
  remoteInterestHandler.addSubscriber(InterestId(1), &remote);
  remoteInterestHandler.addSubscriber(InterestId(2), &remote);

  remoteInterestHandler.removeSubscriber(&remote);

  EXPECT_TRUE(remoteInterestHandler.remotesUpdatesBMMap.empty());
  EXPECT_TRUE(remoteInterestHandler.remotesToInterestsUpdatesMap.empty());
  EXPECT_TRUE(remoteInterestHandler.remotesInterestsBMMap.empty());
  EXPECT_FALSE(remoteInterestHandler.interestsUpdatesBMMap.empty());
}
