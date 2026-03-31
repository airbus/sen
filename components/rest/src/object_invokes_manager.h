// === object_invokes_manager.h ========================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_COMPONENTS_REST_SRC_OBJECT_INVOKES_MANAGER_H
#define SEN_COMPONENTS_REST_SRC_OBJECT_INVOKES_MANAGER_H

// component
#include "notifications.h"
#include "types.h"

// sen
#include "sen/core/base/compiler_macros.h"
#include "sen/core/meta/var.h"
#include "sen/core/obj/callback.h"

// std
#include <atomic>
#include <optional>
#include <unordered_map>

namespace sen::components::rest
{

/// ObjectInvokesManager class allows clients to manage
/// invocations to Sen object methods.
/// As method invocations are asynchronous by design, an InvokieId is
/// used to handle the life cycle of the method invocation.
/// It extends the Notifier class to provide notification capabilities when
/// invocations are updated.
/// Note: This class is not reentrant
class ObjectInvokesManager: public Notifier
{
  SEN_NOCOPY_NOMOVE(ObjectInvokesManager)

public:
  ObjectInvokesManager() = default;
  ~ObjectInvokesManager() = default;

public:
  /// Creates a new invoke with an unique ID.
  [[nodiscard]] Invoke newInvoke(const InterestName& interest);

  /// Finds and returns the invoke details for a given invoke ID.
  [[nodiscard]] std::optional<Invoke> findInvoke(const InvokeId& id);

  /// Updates the status and result of an existing invoke.
  bool updateInvoke(const InterestName& interest, const InvokeId& id, const MethodResult<Var>& result);

  /// Releases a specific invoke by ID.
  void releaseInvoke(const InvokeId& id);

  /// Releases all invokes.
  void releaseAllInvokes();

private:
  std::atomic<InvokeId> lastInvokeId_ = -1;
  std::unordered_map<InvokeId, Invoke> invokes_;
};

}  // namespace sen::components::rest

#endif  // SEN_COMPONENTS_REST_SRC_OBJECT_INVOKES_MANAGER_H
