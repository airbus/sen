// === subscription.h ==================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_CORE_OBJ_SUBSCRIPTION_H
#define SEN_CORE_OBJ_SUBSCRIPTION_H

// sen
#include "sen/core/obj/object_list.h"
#include "sen/core/obj/object_source.h"

namespace sen
{

/// \addtogroup obj
/// @{

/// A list of objects and a reference to its source (to keep it alive).
template <typename T>
struct Subscription
{
public:
  SEN_COPY_CONSTRUCT(Subscription) = delete;
  SEN_COPY_ASSIGN(Subscription) = delete;

public:  // special members
  Subscription(Subscription&& other) noexcept;
  Subscription& operator=(Subscription&& other) noexcept;
  Subscription() = default;
  ~Subscription();

public:
  ObjectList<T> list;                    // NOLINT(misc-non-private-member-variables-in-classes)
  std::shared_ptr<ObjectSource> source;  // NOLINT(misc-non-private-member-variables-in-classes)
};

//-------------------------------------------------------------------------------------------------------------------
// Inline implementation
//-------------------------------------------------------------------------------------------------------------------

template <typename T>
inline Subscription<T>::Subscription(Subscription&& other) noexcept
  : list(std::move(other.list)), source(std::move(other.source))
{
  other.source = {};
}

template <typename T>
inline Subscription<T>& Subscription<T>::operator=(Subscription&& other) noexcept
{
  list = std::move(other.list);
  source = other.source;
  other.source.reset();
}

template <typename T>
inline Subscription<T>::~Subscription()
{
  if (source)
  {
    source->removeSubscriber(&list, true);
  }
}

/// @}

}  // namespace sen

#endif  // SEN_CORE_OBJ_SUBSCRIPTION_H
