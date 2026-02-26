// === proxy_object.h ==================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_CORE_OBJ_DETAIL_PROXY_OBJECT_H
#define SEN_CORE_OBJ_DETAIL_PROXY_OBJECT_H

// sen
#include "sen/core/base/compiler_macros.h"
#include "sen/core/obj/object.h"

// std
#include <mutex>

namespace sen::kernel::impl
{

class LocalParticipant;
class RemoteParticipant;

}  // namespace sen::kernel::impl

namespace sen::impl
{

class RemoteObject;

/// Proxy interface
class SEN_EXPORT ProxyObject: public Object
{
  SEN_NOCOPY_NOMOVE(ProxyObject)

public:  // special members
  ProxyObject() = default;
  ~ProxyObject() override = default;

public:
  /// Used by the transport mechanism
  virtual void drainInputs() = 0;

  /// True if the proxy represents a remote object
  [[nodiscard]] virtual bool isRemote() const noexcept = 0;

  /// This, if this is a remote object
  [[nodiscard]] virtual RemoteObject* asRemoteObject() noexcept = 0;

  /// This, if this is a remote object
  [[nodiscard]] virtual const RemoteObject* asRemoteObject() const noexcept = 0;

public:  // lock interface to externally/internally lock a proxy object during updates
  void lock() const { selfMutex_.lock(); }

  void unlock() const { selfMutex_.unlock(); }

private:
  mutable std::mutex selfMutex_;
};

}  // namespace sen::impl

#endif  // SEN_CORE_OBJ_DETAIL_PROXY_OBJECT_H
