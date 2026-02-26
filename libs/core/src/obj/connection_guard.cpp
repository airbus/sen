// === connection_guard.cpp ============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "sen/core/obj/connection_guard.h"

// sen
#include "sen/core/meta/type.h"
#include "sen/core/obj/object.h"

// std
#include <cstdint>
#include <utility>

namespace sen
{

ConnectionGuard::ConnectionGuard(): id_(0), memberHash_(0), typed_(false) {}

ConnectionGuard::ConnectionGuard(Object* source, uint32_t id, MemberHash memberHash, bool typed)
  : source_(source->weak_from_this()), id_(id), memberHash_(memberHash), typed_(typed)
{
}

ConnectionGuard::~ConnectionGuard() { destroy(); }

ConnectionGuard::ConnectionGuard(sen::ConnectionGuard&& other) noexcept
  : source_(std::move(other.source_)), id_(other.id_), memberHash_(other.memberHash_), typed_(other.typed_)
{
  other.keep();
}

void ConnectionGuard::destroy()
{
  if (auto src = source_.lock(); src)
  {
    if (typed_)
    {
      src->senImplRemoveTypedConnection(id_);
    }
    else
    {
      src->senImplRemoveUntypedConnection(id_, memberHash_);
    }
  }
}

ConnectionGuard& ConnectionGuard::operator=(sen::ConnectionGuard&& other) noexcept
{
  // destroy ourselves before taking the contents of others
  destroy();

  // take the contents of the other
  id_ = other.id_;
  source_ = std::move(other.source_);
  memberHash_ = other.memberHash_;
  typed_ = other.typed_;

  // tell the other to discard its contents
  other.keep();
  return *this;
}

void ConnectionGuard::keep() noexcept
{
  id_ = 0U;
  source_ = {};
  memberHash_ = {};
  typed_ = false;
}

}  // namespace sen
