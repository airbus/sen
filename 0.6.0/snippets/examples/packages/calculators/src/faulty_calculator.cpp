// === faulty_calculator.cpp ===========================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

// generated code
#include "stl/calculator.stl.h"

// sen
#include "sen/core/base/compiler_macros.h"
#include "sen/core/base/numbers.h"
#include "sen/core/meta/class_type.h"

// std
#include <cstdint>

namespace calculators
{

/// A calculator implementation that sometimes provides wrong results.
class FaultyCalculator: public CalculatorBase
{
public:
  SEN_NOCOPY_NOMOVE(FaultyCalculator)

public:
  using CalculatorBase::CalculatorBase;
  ~FaultyCalculator() override = default;

protected:
  /// Implementation of the add method.
  /// Might return a wrong answer.
  float32_t addImpl(float32_t a, float32_t b) override
  {
    callCount_++;
    const auto result = a + b + (shouldFail() ? 1.0f : 0.0f);
    setNextCurrent(result);
    return result;
  }

  /// Implementation of the addWithCurrent method.
  /// Might return a wrong answer.
  float32_t addWithCurrentImpl(float32_t a) override
  {
    // Same as add() but use the value of our "current" property.
    return addImpl(a, getCurrent());
  }

  /// Implementation of the divide method.
  /// Might return a wrong answer.
  float32_t divideImpl(float32_t a, float32_t b) override
  {
    callCount_++;

    float32_t result = 0.0f;
    if (b == 0.0f)
    {
      divisionByZero();
    }
    else
    {
      result = (a / b) + (shouldFail() ? 1.0f : 0.0f);
    }

    setNextCurrent(result);
    return result;
  }

  /// Implementation of the divideWithCurrent method.
  /// Might return a wrong answer.
  float32_t divideByCurrentImpl(float32_t a) override { return divideImpl(a, getCurrent()); }

private:
  [[nodiscard]] bool shouldFail() const noexcept
  {
    // Fail every other call.
    return callCount_ % 2 == 0;
  }

private:
  uint32_t callCount_ = 0U;
};

SEN_EXPORT_CLASS(FaultyCalculator)

}  // namespace calculators
