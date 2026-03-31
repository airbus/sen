// === object_interests_manager.h ======================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_COMPONENTS_REST_SRC_OBJECT_INTEREST_MANAGER_H
#define SEN_COMPONENTS_REST_SRC_OBJECT_INTEREST_MANAGER_H

// component
#include "locators.h"
#include "notifications.h"
#include "types.h"

// generated code
#include "stl/types.stl.h"

// sen
#include "sen/core/base/compiler_macros.h"
#include "sen/core/base/result.h"
#include "sen/core/obj/connection_guard.h"
#include "sen/core/obj/interest.h"
#include "sen/core/obj/subscription.h"
#include "sen/kernel/component_api.h"

// std
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>

namespace sen::components::rest
{

enum class InterestError
{
  invalidBus,
  nameNotUnique,
  creatingSubscription,
};

/// Represents a subscription to a specific interest. It stores
/// all the objects currently live for the interest.
struct InterestSubscription
{
  /// Interest
  std::shared_ptr<sen::Interest> interest;

  /// Objects subscriptions
  std::shared_ptr<sen::Subscription<sen::Object>> subscription;

  /// Bus subscription of this interest
  BusLocator busLocator;
};

using InterestMap = std::unordered_map<InterestName, InterestSubscription>;
using InterestMapIterator = std::unordered_map<InterestName, InterestSubscription>::iterator;

/// Interests class allows clients to manage subscriptions that track objects
/// matching specific queries on Sen buses.
/// The class manages the lifecycle of these subscriptions and provides notification
/// capabilities when objects are added or removed from the interest.
/// It extends the Notifier class to provide this notification capabilities.
/// Note: This class is not reentrant
class ObjectInterestsManager: public Notifier
{
  SEN_NOCOPY_NOMOVE(ObjectInterestsManager)

public:
  ObjectInterestsManager() = default;
  ~ObjectInterestsManager() = default;

public:
  /// Creates a new interest subscription with the specified parameters.
  Result<InterestName, InterestError> createInterest(sen::kernel::RunApi& runApi,
                                                     const BusLocator& busLocator,
                                                     const InterestName& interestName,
                                                     const std::string& query,
                                                     InterestCallback&& onObjectRemoved);

  /// Removes an existing interest subscription by name.
  bool removeInterest(InterestName interestName);

  /// Finds and returns the interest subscription details for a given interest name.
  [[nodiscard]] std::optional<InterestSubscription> findInterest(const InterestName& interestName) const;

  /// Retrieves a summary of a specific interest by name.
  /// InterestSummary is a JSON serializable representation of an interest
  [[nodiscard]] std::optional<InterestSummary> getInterestSummary(const InterestName& interestName);

  /// Retrieves a list of summaries for all registered interests.
  /// InterestSummary is a JSON serializable representation of an interest///
  [[nodiscard]] InterestsSummary getAllInterestsSummary();

  /// Releases all interests.
  void releaseAllInterests();

private:
  InterestMap interests_;
};

}  // namespace sen::components::rest

#endif  // SEN_COMPONENTS_REST_SRC_OBJECT_INTEREST_MANAGER_H
