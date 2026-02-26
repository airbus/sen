// === result_test.cpp =================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

// sen
#include "sen/core/base/numbers.h"
#include "sen/core/base/result.h"

// google test
#include <gtest/gtest.h>

// std
#include <utility>

using sen::BoolResult;
using sen::Err;
using sen::Ok;
using sen::Result;

enum class ErrorCode
{
  tooLarge,
  tooSmall,
  specialValue
};

Result<int, ErrorCode> add100(int a)
{
  if (a < 0)
  {
    return Err(ErrorCode::tooSmall);
  }

  if (a > 100)
  {
    return Err(ErrorCode::tooLarge);
  }

  if (a == 50)
  {
    return Err(ErrorCode::specialValue);
  }

  return Ok(a + 100);
}

Result<void, ErrorCode> doSomethingElse(int a)
{
  if (a < 0)
  {
    return Err(ErrorCode::tooSmall);
  }

  if (a > 100)
  {
    return Err(ErrorCode::tooLarge);
  }

  if (a == 50)
  {
    return Err(ErrorCode::specialValue);
  }

  return Ok();
}

Result<float32_t, ErrorCode> angleRanges(float a)
{
  if (a < -180.0)
  {
    return Err(ErrorCode::tooSmall);
  }
  if (a > 180.0)
  {
    return Err(ErrorCode::tooLarge);
  }
  if (a == -180.0 || a == 180.0)
  {
    return Err(ErrorCode::specialValue);
  }

  return Ok(a);
}

BoolResult doSomethingOther(int a)
{
  if (a < 0)
  {
    return Err();
  }

  return Ok();
}

struct MoveOnlyType
{
  int value;  // NOLINT(misc-non-private-member-variables-in-classes)
  explicit MoveOnlyType(int v): value(v) {}
  ~MoveOnlyType() = default;
  MoveOnlyType(MoveOnlyType&&) = default;
  MoveOnlyType& operator=(MoveOnlyType&&) = default;
  MoveOnlyType(const MoveOnlyType&) = delete;
  MoveOnlyType& operator=(const MoveOnlyType&) = delete;
};

Result<MoveOnlyType, ErrorCode> makeMoveOnlyResult(int a)
{
  if (a < 0)
  {
    return Err(ErrorCode::tooSmall);
  }

  return Ok(MoveOnlyType(a));
}

struct IntConstructable
{
  explicit IntConstructable(int value): value_(value) {}

  [[nodiscard]] int value() const { return value_; }

private:
  int value_;
};

struct ErrorCodeConstructible
{
  explicit ErrorCodeConstructible(ErrorCode e) { value_ = static_cast<int>(e); }
  [[nodiscard]] int value() const { return value_; }

private:
  int value_;
};

/// @test
/// Check result type with no errors
/// @requirements(SEN-1049)
TEST(Result, basics_no_error)
{
  auto result = add100(4);
  EXPECT_TRUE(result);
  EXPECT_TRUE(result.isOk());
  EXPECT_FALSE(result.isError());
  EXPECT_EQ(result.getValueOr(-1), 4 + 100);
  EXPECT_EQ(result.getValue(), 4 + 100);
}

/// @test
/// Check result type with error of type 1 (too small value specified)
/// @requirements(SEN-1049)
TEST(Result, basics_error_1)
{
  auto result = add100(-5);
  EXPECT_FALSE(result);
  EXPECT_FALSE(result.isOk());
  EXPECT_TRUE(result.isError());
  EXPECT_TRUE(result.getError() == ErrorCode::tooSmall);
  EXPECT_EQ(result.getValueOr(-1), -1);
}

/// @test
/// Check result type with error of type 2 (special value specified)
/// @requirements(SEN-1049)
TEST(Result, basics_error_2)
{
  auto result = add100(50);
  EXPECT_FALSE(result);
  EXPECT_FALSE(result.isOk());
  EXPECT_TRUE(result.isError());
  EXPECT_EQ(result.getError(), ErrorCode::specialValue);
  EXPECT_EQ(result.getValueOr(-1), -1);
}

/// @test
/// Check result type with error of type 3 (too large value specified)
/// @requirements(SEN-1049)
TEST(Result, basics_error_3)
{
  auto result = add100(120);
  EXPECT_FALSE(result);
  EXPECT_FALSE(result.isOk());
  EXPECT_TRUE(result.isError());
  EXPECT_EQ(result.getError(), ErrorCode::tooLarge);
  EXPECT_EQ(result.getValueOr(-1), -1);
}

/// @test
/// Checks that type compatible Results can be converted to each other (const reference).
/// @requirements(SEN-1049)
TEST(Result, basics_conversion_compatible_type_cref)
{
  {
    Result<int, ErrorCode> otherResult = Ok(21);

    Result<IntConstructable, ErrorCode> valueConstructable = otherResult;
    EXPECT_TRUE(valueConstructable.isOk());
    EXPECT_EQ(valueConstructable.getValue().value(), 21);
  }
  {
    Result<int, ErrorCode> otherResult = Err(ErrorCode::specialValue);

    Result<int, ErrorCodeConstructible> errorConstructable = otherResult;
    EXPECT_TRUE(errorConstructable.isError());
    EXPECT_EQ(errorConstructable.getError().value(), static_cast<int>(ErrorCode::specialValue));
  }
  {
    Result<int, ErrorCode> otherResult = Ok(42);

    Result<IntConstructable, ErrorCodeConstructible> bothConstructableContainsValue = otherResult;
    EXPECT_TRUE(bothConstructableContainsValue.isOk());
    EXPECT_EQ(bothConstructableContainsValue.getValue().value(), 42);
  }
  {
    Result<int, ErrorCode> otherResult = Err(ErrorCode::specialValue);

    Result<IntConstructable, ErrorCodeConstructible> bothConstructableContainsError = otherResult;
    EXPECT_TRUE(bothConstructableContainsError.isError());
    EXPECT_EQ(bothConstructableContainsError.getError().value(), static_cast<int>(ErrorCode::specialValue));
  }
}

/// @test
/// Checks that type compatible Results can be converted to each other (rvalue).
/// @requirements(SEN-1049)
TEST(Result, basics_conversion_compatible_type_rvalue)
{
  {
    Result<int, ErrorCode> otherResult = Ok(21);

    Result<IntConstructable, ErrorCode> valueConstructable = std::move(otherResult);
    EXPECT_TRUE(valueConstructable.isOk());
    EXPECT_EQ(valueConstructable.getValue().value(), 21);
  }
  {
    Result<int, ErrorCode> otherResult = Err(ErrorCode::specialValue);

    Result<int, ErrorCodeConstructible> errorConstructable = std::move(otherResult);
    EXPECT_TRUE(errorConstructable.isError());
    EXPECT_EQ(errorConstructable.getError().value(), static_cast<int>(ErrorCode::specialValue));
  }
  {
    Result<int, ErrorCode> otherResult = Ok(42);

    Result<IntConstructable, ErrorCodeConstructible> bothConstructableContainsValue = std::move(otherResult);
    EXPECT_TRUE(bothConstructableContainsValue.isOk());
    EXPECT_EQ(bothConstructableContainsValue.getValue().value(), 42);
  }
  {
    Result<int, ErrorCode> otherResult = Err(ErrorCode::specialValue);

    Result<IntConstructable, ErrorCodeConstructible> bothConstructableContainsError = std::move(otherResult);
    EXPECT_TRUE(bothConstructableContainsError.isError());
    EXPECT_EQ(bothConstructableContainsError.getError().value(), static_cast<int>(ErrorCode::specialValue));
  }
}

/// @test
/// Check basic comparisons (==, =!, <, <=, >, >=) between different results
/// @requirements(SEN-1049)
TEST(Result, basics_comparison)
{
  auto result1 = Result<float, ErrorCode>(Ok(24.4f));
  auto result2 = Result<float, ErrorCode>(Ok(24.4f));
  auto result3 = Result<float, ErrorCode>(Ok(25.0f));
  auto result4 = Result<float, ErrorCode>(Err(ErrorCode::tooLarge));

  EXPECT_EQ(result1, result2);
  EXPECT_EQ(result1, result1);
  EXPECT_NE(result1, result3);
  EXPECT_NE(result3, result4);
  EXPECT_NE(result1, result4);

  result1 = result4;
  EXPECT_EQ(result1, result4);

  result1 = result2;
  result3 = std::move(result2);
  EXPECT_EQ(result3, result1);
}

/// @test
/// Check valid value
/// @requirements(SEN-1049)
TEST(Result, void_payload_no_error)
{
  auto result = doSomethingElse(4);
  EXPECT_TRUE(result);
  EXPECT_TRUE(result.isOk());
  EXPECT_FALSE(result.isError());
}

/// @test
/// Check invalid value error 1
/// @requirements(SEN-1049)
TEST(Result, void_payload_error_1)
{
  auto result = doSomethingElse(-5);
  EXPECT_FALSE(result);
  EXPECT_FALSE(result.isOk());
  EXPECT_TRUE(result.isError());
  EXPECT_EQ(result.getError(), ErrorCode::tooSmall);
}

/// @test
/// Check invalid value error 2
/// @requirements(SEN-1049)
TEST(Result, void_payload_error_2)
{
  auto result = doSomethingElse(50);
  EXPECT_FALSE(result);
  EXPECT_FALSE(result.isOk());
  EXPECT_TRUE(result.isError());
  EXPECT_EQ(result.getError(), ErrorCode::specialValue);
}

/// @test
/// Check invalid value error 3
/// @requirements(SEN-1049)
TEST(Result, void_payload_error_3)
{
  auto result = doSomethingElse(120);
  EXPECT_FALSE(result);
  EXPECT_FALSE(result.isOk());
  EXPECT_TRUE(result.isError());
  EXPECT_EQ(result.getError(), ErrorCode::tooLarge);
}

/// @test
/// Check invalid value error 4
/// @requirements(SEN-1049)
TEST(Result, void_payload_error_4)
{
  auto errorCode = ErrorCode::tooSmall;
  Result<int, ErrorCode> result = Err(errorCode);
  EXPECT_FALSE(result);
  EXPECT_FALSE(result.isOk());
  EXPECT_TRUE(result.isError());
  EXPECT_EQ(result.getError(), errorCode);
}

/// @test
/// Check comparison between different void type results
/// @requirements(SEN-1049)
TEST(Result, void_payload_comparison)
{
  auto result1 = Result<void, ErrorCode>(Ok());
  auto result2 = Result<void, ErrorCode>(Ok());
  auto result3 = Result<void, ErrorCode>(Err(ErrorCode::tooLarge));
  auto result4 = Result<void, ErrorCode>(Err(ErrorCode::tooSmall));
  auto result5 = Result<void, ErrorCode>(Err(ErrorCode::tooSmall));

  EXPECT_EQ(result1, result2);
  EXPECT_EQ(result1, result1);
  EXPECT_NE(result1, result3);
  EXPECT_NE(result3, result4);
  EXPECT_EQ(result4, result5);
  EXPECT_NE(result1, result5);

  result1 = result4;
  EXPECT_EQ(result1, result4);

  result3 = std::move(result1);
  EXPECT_EQ(result3, result4);
}

/// @test
/// Check valid value 2
/// @requirements(SEN-1049)
TEST(Result, void_void_payload_no_error)
{
  auto result = doSomethingOther(4);
  EXPECT_TRUE(result);
  EXPECT_TRUE(result.isOk());
  EXPECT_FALSE(result.isError());
}

/// @test
/// Check invalid value 1
/// @requirements(SEN-1049)
TEST(Result, void_void_payload_error_1)
{
  auto result = doSomethingElse(-5);
  EXPECT_FALSE(result);
  EXPECT_FALSE(result.isOk());
  EXPECT_TRUE(result.isError());
}

/// @test
/// Check comparison between different void void type results
/// @requirements(SEN-1049)
TEST(Result, void_void_payload_comparison)
{
  BoolResult result1 = Ok();
  BoolResult result2 = Ok();
  BoolResult result3 = Err();

  EXPECT_EQ(result1, result2);
  EXPECT_EQ(result1, result1);
  EXPECT_NE(result1, result3);

  result1 = result3;
  EXPECT_EQ(result1, result3);

  result1 = result2;
  result3 = std::move(result1);
  EXPECT_EQ(result3, BoolResult(Ok()));
}

/// @test
/// Check move only results
/// @requirements(SEN-1049)
TEST(Result, move_only)
{
  auto result = makeMoveOnlyResult(-1);
  EXPECT_FALSE(result);

  result = makeMoveOnlyResult(1);
  EXPECT_TRUE(result);

  EXPECT_EQ(result.getValue().value, 1);
}

/// @test
/// Check floating values in result types
/// @requirements(SEN-1049)
TEST(Result, floating_values)
{
  auto result1 = angleRanges(180.0001);

  EXPECT_FALSE(result1);
  EXPECT_FALSE(result1.isOk());
  EXPECT_TRUE(result1.isError());
  EXPECT_EQ(result1.getError(), ErrorCode::tooLarge);

  auto result2 = angleRanges(-180.00);
  auto result3 = angleRanges(180.0000);
  EXPECT_EQ(result2.getError(), ErrorCode::specialValue);
  EXPECT_EQ(result3.getError(), ErrorCode::specialValue);
  EXPECT_EQ(result2.getError(), result3.getError());
}
