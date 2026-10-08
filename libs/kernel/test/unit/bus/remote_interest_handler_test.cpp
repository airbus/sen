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
/// Registers a remote's subscription to a brand-new interest and maps no object update to it, since the
/// interest has no objects yet.
/// @requirements(SEN-369)
TEST(ARemoteInterestHandler, processANewRemoteSubscriptionWhenThereAreNoObjects)
{
  RemoteInterestsHandler remoteInterestHandler;
  RemoteParticipant remote;

  remoteInterestHandler.addSubscriber(InterestId(1), &remote, true);

  ASSERT_EQ(remoteInterestHandler.remotesUpdatesBMMap.size<RemoteParticipant*>(), 0);
  ASSERT_TRUE(remoteInterestHandler.remotesInterestsBMMap.contains(&remote));
}

/// @test
/// Fans an object added to an interest out to the already-subscribed remote, mapping the update to both the
/// interest and the remote.
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
  ASSERT_TRUE(remoteInterestHandler.remotesInterestsBMMap.contains(&remote));
}

/// @test
/// Maps each of two objects added to a subscribed interest to both the interest and the remote, so the remote
/// is due both updates.
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
/// Backfills a remote that subscribes to an interest already holding an object with that object's update,
/// alongside recording the subscription.
/// @requirements(SEN-369)
TEST(ARemoteInterestHandler, registersANewRemoteSubscriptionAfterObjectWasAdded)
{
  RemoteInterestsHandler remoteInterestHandler;
  RemoteParticipant remote;
  ObjectUpdate update;
  remoteInterestHandler.addObject(InterestId(1), &update);

  remoteInterestHandler.addSubscriber(InterestId(1), &remote);

  EXPECT_TRUE(remoteInterestHandler.remotesUpdatesBMMap.contains(&remote));
  EXPECT_TRUE(remoteInterestHandler.remotesInterestsBMMap.contains(InterestId(1)));
}

/// @test
/// Backfills each of two remotes subscribing to the same interest with the object update the interest already
/// holds.
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
  EXPECT_EQ(remoteInterestHandler.remotesInterestsBMMap.size<RemoteParticipant*>(), 2);
}

/// @test
/// Clears the updates mapped to a remote when its only subscription is removed, leaving no update due to it.
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
}

/// @test
/// Keeps the remaining remote's subscription and pending update, and the interest's link to the object, when
/// the other remote's subscription is removed.
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
  EXPECT_TRUE(remoteInterestHandler.remotesInterestsBMMap.contains(&remote2));
  EXPECT_TRUE(remoteInterestHandler.interestsUpdatesBMMap.contains(InterestId(1)));
}

/// @test
/// Clears a removed object's update from the interest and from every subscribed remote.
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
  EXPECT_TRUE(remoteInterestHandler.interestsUpdatesBMMap.empty());
}

/// @test
/// Removes a single update when one of two objects is dropped, leaving both remotes
/// registered and the interest holding one update.
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
  EXPECT_EQ(remoteInterestHandler.remotesInterestsBMMap.size<RemoteParticipant*>(), 2);

  {
    const auto* ptr = remoteInterestHandler.interestsUpdatesBMMap.tryGet(InterestId(1));
    ASSERT_NE(nullptr, ptr);
    EXPECT_EQ(ptr->size(), 1);
  }
}

/// @test
/// Maps an object registered under two interests to the remote subscribing to both, recording both
/// subscriptions.
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

  EXPECT_TRUE(remoteInterestHandler.remotesUpdatesBMMap.contains(&update));
  EXPECT_TRUE(remoteInterestHandler.remotesInterestsBMMap.contains(InterestId(1)));
  EXPECT_TRUE(remoteInterestHandler.remotesInterestsBMMap.contains(InterestId(2)));
}

/// @test
/// Keeps the update due to a remote that drops one of two interests covering the same object, since the other
/// subscription still covers it.
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

  EXPECT_TRUE(remoteInterestHandler.remotesUpdatesBMMap.contains(&update));
  EXPECT_TRUE(remoteInterestHandler.remotesInterestsBMMap.contains(InterestId(2)));
  EXPECT_TRUE(!remoteInterestHandler.remotesInterestsBMMap.contains(InterestId(1)));
}

/// @test
/// Clears every update due to the remote once it has dropped both interests covering the object.
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
}

/// @test
/// Drops all of a removed remote's subscriptions and pending updates while keeping the interest-to-object
/// links.
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
  EXPECT_TRUE(remoteInterestHandler.remotesInterestsBMMap.empty());
  EXPECT_FALSE(remoteInterestHandler.interestsUpdatesBMMap.empty());
}
