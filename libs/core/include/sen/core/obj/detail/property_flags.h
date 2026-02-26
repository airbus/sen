// === property_flags.h ================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_CORE_OBJ_DETAIL_PROPERTY_FLAGS_H
#define SEN_CORE_OBJ_DETAIL_PROPERTY_FLAGS_H

// sen
#include "sen/core/base/compiler_macros.h"
#include "sen/core/base/numbers.h"

// std
#include <array>
#include <cassert>
#include <type_traits>

namespace sen::impl
{

constexpr uint8_t currMask = 0b10000000U;
constexpr uint8_t nextMask = 0b01000000U;
constexpr uint8_t dirtMask = 0b00100000U;
constexpr uint8_t syncMask = 0b00010000U;

template <typename T>
using PropBuffer = std::array<T, 2U>;

class PropertyFlags final
{
public:
  SEN_COPY_MOVE(PropertyFlags)

public:
  PropertyFlags() = default;
  ~PropertyFlags() = default;

public:
  [[nodiscard]] SEN_ALWAYS_INLINE uint8_t currentIndex() const noexcept { return (state_ & currMask) != 0U ? 1U : 0U; }
  [[nodiscard]] SEN_ALWAYS_INLINE uint8_t nextIndex() const noexcept { return (state_ & nextMask) != 0U ? 1U : 0U; }
  [[nodiscard]] SEN_ALWAYS_INLINE bool changedInLastCycle() const noexcept { return (state_ & dirtMask) != 0U; }
  [[nodiscard]] uint8_t advanceNext() noexcept;
  void advanceCurrent() noexcept;
  template <typename T>
  void setValue(std::array<T, 2>& buffer, const T& value) noexcept(std::is_nothrow_copy_constructible_v<T> &&
                                                                   std::is_nothrow_copy_assignable_v<T> &&
                                                                   noexcept(std::declval<T>() != std::declval<T>()));
  [[nodiscard]] bool getTypesInSync() const noexcept { return (state_ & syncMask) != 0U; }
  void setTypesInSync(bool value) noexcept
  {
    value ? state_ |= syncMask : state_ &= ~syncMask;  // NOLINT(hicpp-signed-bitwise)
  }

private:
  uint8_t state_ = 0;
};

static_assert(sizeof(PropertyFlags) == 1);

//----------------------------------------------------------------------------------------------------------------------
// Inline implementation
//----------------------------------------------------------------------------------------------------------------------

template <typename T>
void PropertyFlags::setValue(std::array<T, 2>& buffer,
                             const T& value) noexcept(std::is_nothrow_copy_constructible_v<T> &&
                                                      std::is_nothrow_copy_assignable_v<T> &&
                                                      noexcept(std::declval<T>() != std::declval<T>()))
{
  const uint8_t cur = currentIndex();
  const uint8_t nex = nextIndex();
  assert(cur < 2 && nex < 2);

  if (cur != nex)
  {
    buffer[nex] = value;  // NOLINT
  }
  else
  {
    if (buffer[cur] != value)  // NOLINT
    {
      buffer[advanceNext()] = value;  // NOLINT
    }
  }
}

inline void PropertyFlags::advanceCurrent() noexcept
{
  if ((state_ & nextMask) != 0U)  // next is 1
  {
    if ((state_ & currMask) != 0U)  // current is also 1
    {
      state_ &= ~dirtMask;  // set dirty to 0 (no changes detected) NOLINT(hicpp-signed-bitwise)
    }
    else  // current is 0
    {
      // cur = 1, next = 1, dirty = 1
      state_ = (state_ & syncMask) | currMask | nextMask | dirtMask;  // NOLINT(hicpp-signed-bitwise)
    }
  }
  else  // next is 0
  {
    if ((state_ & currMask) != 0U)  // current is 1
    {
      // next = 0, cur = 0, dirty = 1
      state_ = (state_ & syncMask) | dirtMask;  // NOLINT(hicpp-signed-bitwise)
    }
    else  // current is also 0
    {
      // dirty = 0 (no changes made)
      state_ &= ~dirtMask;  // NOLINT(hicpp-signed-bitwise)
    }
  }
}

inline uint8_t PropertyFlags::advanceNext() noexcept
{
  if ((state_ & nextMask) != 0U)  // next is 1
  {
    if ((state_ & currMask) != 0U)  // current is also 1
    {
      // next = 0
      state_ &= ~nextMask;  // NOLINT(hicpp-signed-bitwise)
      return 0U;
    }
    // current is 0, leave next as 1
    return 1U;
  }

  // next is 0
  if ((state_ & currMask) != 0U)  // current is 1
  {
    // leave next as 0
    return 0U;
  }

  // current is also 0, set next to 1
  state_ |= nextMask;
  return 1U;
}

}  // namespace sen::impl

#endif  // SEN_CORE_OBJ_DETAIL_PROPERTY_FLAGS_H
