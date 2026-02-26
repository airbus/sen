// === numbers.h =======================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_CORE_BASE_NUMBERS_H
#define SEN_CORE_BASE_NUMBERS_H

#include <cstdint>

/// \addtogroup util
/// @{

using float32_t = float;        // NOLINT
using float64_t = double;       // NOLINT
using uchar_t = unsigned char;  // NOLINT

using i8 = int8_t;      // NOLINT
using u8 = uint8_t;     // NOLINT
using i16 = int16_t;    // NOLINT
using u16 = uint16_t;   // NOLINT
using i32 = int32_t;    // NOLINT
using u32 = uint32_t;   // NOLINT
using i64 = int64_t;    // NOLINT
using u64 = uint64_t;   // NOLINT
using f32 = float32_t;  // NOLINT
using f64 = float64_t;  // NOLINT

static_assert(sizeof(float32_t) == sizeof(int32_t));
static_assert(sizeof(float64_t) == sizeof(int64_t));

/// @}

#endif  // SEN_CORE_BASE_NUMBERS_H
