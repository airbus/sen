// === connection_guard.h ==============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_CORE_OBJ_CONNECTION_GUARD_H
#define SEN_CORE_OBJ_CONNECTION_GUARD_H

// sen
#include "sen/core/base/compiler_macros.h"
#include "sen/core/meta/type.h"

// std
#include <cstdint>
#include <memory>

namespace sen
{

namespace impl
{

template <typename... T>
class EventBuffer;

}

/// \addtogroup obj
/// @{

class Object;

/// Unregisters a callback on destruction, unless keep() is called.
class [[nodiscard]] ConnectionGuard
{
public:
  SEN_COPY_CONSTRUCT(ConnectionGuard) = delete;
  SEN_COPY_ASSIGN(ConnectionGuard) = delete;

public:
  SEN_MOVE_CONSTRUCT(ConnectionGuard);
  SEN_MOVE_ASSIGN(ConnectionGuard);

public:
  /// Constructs an emtpy guard.
  ConnectionGuard();

  /// Removes the associated callback from the object if the object has not been destructed and
  /// keep() has not been called.
  ~ConnectionGuard();

  /// Tells this class to keep the callback even when this guard is destroyed.
  void keep() noexcept;

private:
  friend class Object;

  template <typename... T>
  friend class impl::EventBuffer;

  /// Only objects can create callback guards.
  ConnectionGuard(Object* source, uint32_t id, MemberHash memberHash, bool typed);

  /// Destructor logic
  void destroy();

private:
  std::weak_ptr<Object> source_;
  uint32_t id_;
  MemberHash memberHash_;
  bool typed_;
};

/// @}

}  // namespace sen

#endif  // SEN_CORE_OBJ_CONNECTION_GUARD_H
