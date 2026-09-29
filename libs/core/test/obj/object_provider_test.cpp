// === object_provider_test.cpp ========================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

// sen
#include "sen/core/obj/detail/proxy_object.h"
#include "sen/core/obj/object_provider.h"

// google test
#include <gtest/gtest.h>

// std
#include <algorithm>
#include <cstddef>
#include <functional>
#include <tuple>
#include <vector>

using sen::ObjectAdditionList;
using sen::ObjectProvider;
using sen::ObjectProviderListener;
using sen::ObjectRemovalList;

namespace
{

/// The smallest concrete provider: the two notify hooks are pure virtual and nothing here needs them.
class TestProvider final: public ObjectProvider
{
public:
  using ObjectProvider::forEachListener;

protected:
  void notifyAddedOnExistingObjects(ObjectProviderListener* listener) override { std::ignore = listener; }
  void notifyRemovedOnExistingObjects(ObjectProviderListener* listener) override { std::ignore = listener; }
};

class TestListener final: public ObjectProviderListener
{
public:
  void onObjectsAdded(const ObjectAdditionList& additions) override { std::ignore = additions; }
  void onObjectsRemoved(const ObjectRemovalList& removals) override { std::ignore = removals; }

  [[nodiscard]] sen::kernel::impl::RemoteParticipant* isRemoteParticipant() noexcept override { return nullptr; }
  [[nodiscard]] sen::kernel::impl::LocalParticipant* isLocalParticipant() noexcept override { return nullptr; }
};

}  // namespace

/// @test
/// Verifies that forEachListener reaches every registered listener.
TEST(ObjectProviderTest, ForEachListenerVisitsEveryRegisteredListener)
{
  TestProvider provider;
  TestListener first;
  TestListener second;

  provider.addListener(&first, false);
  provider.addListener(&second, false);

  std::vector<ObjectProviderListener*> visited;
  provider.forEachListener([&visited](ObjectProviderListener* listener) { visited.push_back(listener); });

  EXPECT_EQ(visited.size(), 2U);
  EXPECT_NE(std::find(visited.begin(), visited.end(), &first), visited.end());
  EXPECT_NE(std::find(visited.begin(), visited.end(), &second), visited.end());
}

/// @test
/// Verifies that forEachListener calls nothing when no listener is registered.
TEST(ObjectProviderTest, ForEachListenerVisitsNothingWithoutListeners)
{
  TestProvider provider;

  size_t calls = 0;
  provider.forEachListener(
    [&calls](ObjectProviderListener* listener)
    {
      std::ignore = listener;
      ++calls;
    });

  EXPECT_EQ(calls, 0U);
}

/// @test
/// Verifies that a listener removed during the walk is not visited afterwards. The registry's
/// iterators survive a concurrent erase; a walk over a copy would call the removed one instead,
/// which is a use-after-free for a subclass that removed it so as to destroy it.
TEST(ObjectProviderTest, ForEachListenerDoesNotVisitAListenerRemovedDuringTheWalk)
{
  TestProvider provider;
  TestListener first;
  TestListener second;

  provider.addListener(&first, false);
  provider.addListener(&second, false);

  size_t calls = 0;
  ObjectProviderListener* reachedListener = nullptr;
  provider.forEachListener(
    [&](ObjectProviderListener* reached)
    {
      ++calls;
      reachedListener = reached;
      // Whichever came first, remove the other. Iteration order follows the pointers, so naming a
      // fixed one would make this test depend on the addresses the allocator happened to hand out.
      provider.removeListener(reached == &first ? &second : &first, false);
    });

  // One call, not two: the other listener was gone before the walk reached it.
  EXPECT_EQ(calls, 1U);
  ASSERT_NE(reachedListener, nullptr);
  EXPECT_TRUE(provider.hasListener(reachedListener));
}
