// === event.cpp =======================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "sen/core/meta/event.h"

// sen
#include "sen/core/base/hash32.h"
#include "sen/core/meta/callable.h"
#include "sen/core/meta/type.h"

// std
#include <memory>

namespace sen
{

bool operator==(const EventSpec& lhs, const EventSpec& rhs) noexcept
{
  return &lhs == &rhs || (lhs.callableSpec == rhs.callableSpec);
}

bool operator!=(const EventSpec& lhs, const EventSpec& rhs) noexcept { return !(lhs == rhs); }

Event::Event(const EventSpec& spec)
  : Callable(spec.callableSpec)
  , hash_(impl::hash<EventSpec>()(spec))
  , id_(hashCombine(eventHashSeed, spec.callableSpec.name))
{
}

bool Event::operator==(const Event& other) const noexcept
{
  return &other == this || (getCallableSpec() == other.getCallableSpec());
}

bool Event::operator!=(const Event& other) const noexcept { return !(*this == other); }

std::shared_ptr<Event> Event::make(const EventSpec& spec)
{
  checkSpec(spec.callableSpec);
  return std::shared_ptr<Event>(new Event(spec));  // NOSONAR
}

MemberHash Event::getHash() const noexcept { return hash_; }

MemberHash Event::getId() const noexcept { return id_; }

}  // namespace sen
