// === subscription.h ==================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_CORE_OBJ_SUBSCRIPTION_H
#define SEN_CORE_OBJ_SUBSCRIPTION_H

// sen
#include "sen/core/obj/interest.h"
#include "sen/core/obj/object_list.h"
#include "sen/core/obj/object_source.h"

// std
#include <memory>
#include <utility>

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
  /// Attaches this subscription to a source and registers the given interest.
  /// Objects matching the interest will start populating the list.
  void attachTo(std::shared_ptr<ObjectSource> src, std::shared_ptr<Interest> interest, bool notifyAboutExisting);

  /// Detaches from the source, unregistering the list as a subscriber.
  /// Called automatically on destruction. Pass notifyAboutExisting=false
  /// to suppress removal callbacks during explicit teardown.
  void release(bool notifyAboutExisting = true);

  /// Returns the source currently attached to this subscription, or nullptr if none.
  [[nodiscard]] std::shared_ptr<ObjectSource> getSource() const { return source_; }

public:
  ObjectList<T> list;  // NOLINT(misc-non-private-member-variables-in-classes)

private:
  std::shared_ptr<ObjectSource> source_;
};

//-------------------------------------------------------------------------------------------------------------------
// Inline implementation
//-------------------------------------------------------------------------------------------------------------------

template <typename T>
inline Subscription<T>::Subscription(Subscription&& other) noexcept
  : list(std::move(other.list)), source_(std::move(other.source_))
{
}

template <typename T>
inline Subscription<T>& Subscription<T>::operator=(Subscription<T>&& other) noexcept
{
  if (this != &other)
  {
    release(true);
    list = std::move(other.list);
    source_ = std::move(other.source_);
  }

  return *this;
}

template <typename T>
inline Subscription<T>::~Subscription()
{
  release(true);
}

template <typename T>
inline void Subscription<T>::attachTo(std::shared_ptr<ObjectSource> src,
                                      std::shared_ptr<Interest> interest,
                                      bool notifyAboutExisting)
{
  release();

  source_ = std::move(src);
  source_->addSubscriber(std::move(interest), &list, notifyAboutExisting);
}

template <typename T>
inline void Subscription<T>::release(bool notifyAboutExisting)
{
  if (source_)
  {
    source_->removeSubscriber(&list, notifyAboutExisting);
    source_.reset();
  }
}

/// @}

}  // namespace sen

#endif  // SEN_CORE_OBJ_SUBSCRIPTION_H
