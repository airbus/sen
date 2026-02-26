// === helpers.h =======================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_LIBS_KERNEL_TEST_INTEGRATION_TRANSPORT_HELPERS_H
#define SEN_LIBS_KERNEL_TEST_INTEGRATION_TRANSPORT_HELPERS_H

// sen
#include "sen/core/base/class_helpers.h"
#include "sen/core/base/timestamp.h"

// generated code
#include "test_helpers/tester.stl.h"

// std
#include <cmath>
#include <future>
#include <string>
#include <type_traits>

namespace sen::test
{

//--------------------------------------------------------------------------------------------------------------
// Helpers
//--------------------------------------------------------------------------------------------------------------

template <typename T>
[[nodiscard]] inline bool eq(T a, T b)
{
  // handle possible quantities of floating point types
  if constexpr (!std::is_same_v<T, sen::TimeStamp> && sen::HasValueType<T>::value)
  {
    return eq(a.get(), b.get());
  }

  if constexpr (std::is_floating_point_v<T>)
  {
    constexpr T eps = 1e-3;
    return std::fabs(a - b) < eps;
  }

  return a == b;
}

template <typename PropType>
[[nodiscard]] inline bool checkProp(std::promise<TestResult>& promise,
                                    const std::string& propName,
                                    sen::MaybeRef<PropType> value,
                                    sen::MaybeRef<PropType> expectation)
{
  if (!eq(value, expectation))
  {
    std::string err;
    err.append("Incorrect value of ");
    err.append(propName);
    promise.set_value(TestResult(err));
    return false;
  }

  return true;
}

[[nodiscard]] inline bool checkMember(std::promise<TestResult>& promise, const std::string& memberName, bool isOk)
{
  if (isOk)
  {
    return true;
  }

  std::string err = "Member ";
  err.append(memberName);
  err.append(" adaptation failed.");
  promise.set_value(TestResult(err));
  return false;
}

}  // namespace sen::test

#endif  // SEN_LIBS_KERNEL_TEST_INTEGRATION_TRANSPORT_HELPERS_H
