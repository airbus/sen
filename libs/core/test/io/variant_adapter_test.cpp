// === variant_adapter_test.cpp ========================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

// sen
#include "sen/core/base/numbers.h"
#include "sen/core/io/util.h"
#include "sen/core/meta/native_types.h"
#include "sen/core/meta/type.h"
#include "sen/core/meta/var.h"

// google test
#include <gtest/gtest.h>

// std
#include <algorithm>
#include <cctype>
#include <cstdint>
#include <limits>
#include <sstream>
#include <string>
#include <tuple>

using sen::BoolType;
using sen::Float32Type;
using sen::Float64Type;
using sen::Int16Type;
using sen::Int32Type;
using sen::Int64Type;
using sen::UInt16Type;
using sen::UInt32Type;
using sen::UInt64Type;
using sen::UInt8Type;
using sen::Var;

using std::make_tuple;
using std::numeric_limits;

std::string convertTypesToTestCaseName(sen::ConstTypeHandle<> originType, Var var, sen::ConstTypeHandle<> targetType)
{
  std::stringstream ss;
  ss << originType->getName();
  ss << "_" << var.getCopyAs<std::string>();
  ss << "_to_" << targetType->getName();
  std::string s = ss.str();
  std::replace(s.begin(), s.end(), '-', 'n');
  std::replace_if(s.begin(), s.end(), [](char c) { return !std::isalnum(c); }, '_');
  return s;
}

template <typename T>
struct VariantAdapterTestBase
  : public testing::TestWithParam<std::tuple<sen::ConstTypeHandle<>, Var, sen::ConstTypeHandle<>, T>>
{
  void runComparison()
  {
    Var var = std::get<1>(this->GetParam());

    std::ignore = sen::impl::adaptVariant(*std::get<2>(this->GetParam()), var, std::get<0>(this->GetParam()));

    EXPECT_TRUE(var.holds<T>());
    EXPECT_EQ(var.get<T>(), std::get<3>(this->GetParam()));
  }
};

//--------------------------------------------------------------------------------------------------------------
// ConversionTests: uint8
//--------------------------------------------------------------------------------------------------------------

struct ConvertToUint8: public VariantAdapterTestBase<uint8_t>
{
};

TEST_P(ConvertToUint8, ConversionTest) { runComparison(); };

INSTANTIATE_TEST_SUITE_P(
  VariantAdapterTest,
  ConvertToUint8,
  ::testing::Values(
    make_tuple(UInt8Type::get(), Var(uint8_t {42}), UInt8Type::get(), uint8_t {42}),
    make_tuple(UInt16Type::get(), Var(uint16_t {42}), UInt8Type::get(), uint8_t {42}),
    make_tuple(UInt32Type::get(), Var(uint32_t {42}), UInt8Type::get(), uint8_t {42}),
    make_tuple(UInt64Type::get(), Var(uint64_t {42}), UInt8Type::get(), uint8_t {42}),
    make_tuple(Int16Type::get(), Var(int16_t {42}), UInt8Type::get(), uint8_t {42}),
    make_tuple(Int32Type::get(), Var(int32_t {42}), UInt8Type::get(), uint8_t {42}),
    make_tuple(Int64Type::get(), Var(int64_t {42}), UInt8Type::get(), uint8_t {42}),
    make_tuple(Float32Type::get(), Var(float32_t {42}), UInt8Type::get(), uint8_t {42}),
    make_tuple(Float64Type::get(), Var(float64_t {42}), UInt8Type::get(), uint8_t {42}),
    /* overflow conversions: we expect capping to max target type */
    make_tuple(UInt16Type::get(), Var(uint16_t {35535}), UInt8Type::get(), numeric_limits<uint8_t>::max()),
    make_tuple(UInt32Type::get(), Var(uint32_t {65536}), UInt8Type::get(), numeric_limits<uint8_t>::max()),
    make_tuple(UInt64Type::get(), Var(uint64_t {65536}), UInt8Type::get(), numeric_limits<uint8_t>::max()),
    make_tuple(Int16Type::get(), Var(int32_t {31234}), UInt8Type::get(), numeric_limits<uint8_t>::max()),
    make_tuple(Int32Type::get(), Var(int32_t {65536}), UInt8Type::get(), numeric_limits<uint8_t>::max()),
    make_tuple(Int64Type::get(), Var(int64_t {65536}), UInt8Type::get(), numeric_limits<uint8_t>::max()),
    /* underflow conversions: we expect capping to lowest target type */
    make_tuple(Int16Type::get(), Var(int16_t {-42}), UInt8Type::get(), numeric_limits<uint8_t>::lowest()),
    make_tuple(Int32Type::get(), Var(int32_t {-42}), UInt8Type::get(), numeric_limits<uint8_t>::lowest()),
    make_tuple(Int64Type::get(), Var(int64_t {-42}), UInt8Type::get(), numeric_limits<uint8_t>::lowest()),
    /* underflow/rounding conversions: we expect capping to max target type */
    make_tuple(Float32Type::get(), Var(float32_t {-4.2}), UInt8Type::get(), numeric_limits<uint8_t>::lowest()),
    make_tuple(Float64Type::get(), Var(float64_t {-4.2}), UInt8Type::get(), numeric_limits<uint8_t>::lowest())),
  [](const testing::TestParamInfo<ConvertToUint8::ParamType>& info)
  { return convertTypesToTestCaseName(std::get<0>(info.param), std::get<1>(info.param), std::get<2>(info.param)); });

//--------------------------------------------------------------------------------------------------------------
// ConversionTests: uint16
//--------------------------------------------------------------------------------------------------------------

struct ConvertToUint16: public VariantAdapterTestBase<uint16_t>
{
};

TEST_P(ConvertToUint16, ConversionTest) { runComparison(); };

INSTANTIATE_TEST_SUITE_P(
  VariantAdapterTest,
  ConvertToUint16,
  ::testing::Values(
    make_tuple(UInt16Type::get(), Var(uint16_t {42}), UInt16Type::get(), uint16_t {42}),
    make_tuple(UInt8Type::get(), Var(uint8_t {42}), UInt16Type::get(), uint16_t {42}),
    make_tuple(UInt32Type::get(), Var(uint32_t {42}), UInt16Type::get(), uint16_t {42}),
    make_tuple(UInt64Type::get(), Var(uint64_t {42}), UInt16Type::get(), uint16_t {42}),
    make_tuple(Int16Type::get(), Var(int16_t {42}), UInt16Type::get(), uint16_t {42}),
    make_tuple(Int32Type::get(), Var(int32_t {42}), UInt16Type::get(), uint16_t {42}),
    make_tuple(Int64Type::get(), Var(int64_t {42}), UInt16Type::get(), uint16_t {42}),
    make_tuple(Float32Type::get(), Var(float32_t {42}), UInt16Type::get(), uint16_t {42}),
    make_tuple(Float64Type::get(), Var(float64_t {42}), UInt16Type::get(), uint16_t {42}),
    /* overflow conversions: we expect capping to max target type */
    make_tuple(UInt32Type::get(), Var(uint32_t {65536}), UInt16Type::get(), numeric_limits<uint16_t>::max()),
    make_tuple(UInt64Type::get(), Var(uint64_t {65536}), UInt16Type::get(), numeric_limits<uint16_t>::max()),
    make_tuple(Int32Type::get(), Var(int32_t {65536}), UInt16Type::get(), numeric_limits<uint16_t>::max()),
    make_tuple(Int64Type::get(), Var(int64_t {65536}), UInt16Type::get(), numeric_limits<uint16_t>::max()),
    /* underflow conversions: we expect capping to lowest target type */
    make_tuple(Int16Type::get(), Var(int16_t {-42}), UInt16Type::get(), numeric_limits<uint16_t>::lowest()),
    make_tuple(Int32Type::get(), Var(int32_t {-42}), UInt16Type::get(), numeric_limits<uint16_t>::lowest()),
    make_tuple(Int64Type::get(), Var(int64_t {-42}), UInt16Type::get(), numeric_limits<uint16_t>::lowest()),
    /* underflow/rounding conversions: we expect capping to max target type */
    make_tuple(Float32Type::get(), Var(float32_t {-4.2}), UInt16Type::get(), numeric_limits<uint16_t>::lowest()),
    make_tuple(Float64Type::get(), Var(float64_t {-4.2}), UInt16Type::get(), numeric_limits<uint16_t>::lowest())),
  [](const testing::TestParamInfo<ConvertToUint16::ParamType>& info)
  { return convertTypesToTestCaseName(std::get<0>(info.param), std::get<1>(info.param), std::get<2>(info.param)); });

//--------------------------------------------------------------------------------------------------------------
// ConversionTests: uint32
//--------------------------------------------------------------------------------------------------------------

struct ConvertToUint32: public VariantAdapterTestBase<uint32_t>
{
};

TEST_P(ConvertToUint32, ConversionTest) { runComparison(); };

INSTANTIATE_TEST_SUITE_P(
  VariantAdapterTest,
  ConvertToUint32,
  ::testing::Values(
    make_tuple(UInt32Type::get(), Var(uint32_t {42}), UInt32Type::get(), uint32_t {42}),
    make_tuple(UInt8Type::get(), Var(uint8_t {42}), UInt32Type::get(), uint32_t {42}),
    make_tuple(UInt16Type::get(), Var(uint16_t {42}), UInt32Type::get(), uint32_t {42}),
    make_tuple(UInt64Type::get(), Var(uint64_t {42}), UInt32Type::get(), uint32_t {42}),
    make_tuple(Int16Type::get(), Var(int16_t {42}), UInt32Type::get(), uint32_t {42}),
    make_tuple(Int32Type::get(), Var(int32_t {42}), UInt32Type::get(), uint32_t {42}),
    make_tuple(Int64Type::get(), Var(int64_t {42}), UInt32Type::get(), uint32_t {42}),
    make_tuple(Float32Type::get(), Var(float32_t {42}), UInt32Type::get(), uint32_t {42}),
    make_tuple(Float64Type::get(), Var(float64_t {42}), UInt32Type::get(), uint32_t {42}),
    /* overflow conversions: we expect capping to max target type */
    make_tuple(UInt64Type::get(), Var(uint64_t {4294967296ULL}), UInt32Type::get(), numeric_limits<uint32_t>::max()),
    make_tuple(Int64Type::get(), Var(int64_t {4294967296LL}), UInt32Type::get(), numeric_limits<uint32_t>::max()),
    /* underflow conversions: we expect capping to lowest target type */
    make_tuple(Int16Type::get(), Var(int16_t {-42}), UInt32Type::get(), numeric_limits<uint32_t>::lowest()),
    make_tuple(Int32Type::get(), Var(int32_t {-42}), UInt32Type::get(), numeric_limits<uint32_t>::lowest()),
    make_tuple(Int64Type::get(), Var(int64_t {-42}), UInt32Type::get(), numeric_limits<uint32_t>::lowest()),
    /* underflow/rounding conversions: we expect capping to max target type */
    make_tuple(Float32Type::get(), Var(float32_t {-4.2}), UInt32Type::get(), numeric_limits<uint32_t>::lowest()),
    make_tuple(Float64Type::get(), Var(float64_t {-4.2}), UInt32Type::get(), numeric_limits<uint32_t>::lowest())),
  [](const testing::TestParamInfo<ConvertToUint32::ParamType>& info)
  { return convertTypesToTestCaseName(std::get<0>(info.param), std::get<1>(info.param), std::get<2>(info.param)); });

//--------------------------------------------------------------------------------------------------------------
// ConversionTests: uint64
//--------------------------------------------------------------------------------------------------------------

struct ConvertToUint64: public VariantAdapterTestBase<uint64_t>
{
};

TEST_P(ConvertToUint64, ConversionTest) { runComparison(); };

INSTANTIATE_TEST_SUITE_P(
  VariantAdapterTest,
  ConvertToUint64,
  ::testing::Values(
    make_tuple(UInt64Type::get(), Var(uint64_t {42}), UInt64Type::get(), uint64_t {42}),
    make_tuple(UInt8Type::get(), Var(uint8_t {42}), UInt64Type::get(), uint64_t {42}),
    make_tuple(UInt16Type::get(), Var(uint16_t {42}), UInt64Type::get(), uint64_t {42}),
    make_tuple(UInt32Type::get(), Var(uint32_t {42}), UInt64Type::get(), uint64_t {42}),
    make_tuple(Int16Type::get(), Var(int16_t {42}), UInt64Type::get(), uint64_t {42}),
    make_tuple(Int32Type::get(), Var(int32_t {42}), UInt64Type::get(), uint64_t {42}),
    make_tuple(Int64Type::get(), Var(int64_t {42}), UInt64Type::get(), uint64_t {42}),
    make_tuple(Float32Type::get(), Var(float32_t {42}), UInt64Type::get(), uint64_t {42}),
    make_tuple(Float64Type::get(), Var(float64_t {42}), UInt64Type::get(), uint64_t {42}),
    /* overflow conversions: we expect capping to max target type */
    /* underflow conversions: we expect capping to lowest target type */
    make_tuple(Int16Type::get(), Var(int16_t {-42}), UInt64Type::get(), numeric_limits<uint64_t>::lowest()),
    make_tuple(Int32Type::get(), Var(int32_t {-42}), UInt64Type::get(), numeric_limits<uint64_t>::lowest()),
    make_tuple(Int64Type::get(), Var(int64_t {-42}), UInt64Type::get(), numeric_limits<uint64_t>::lowest()),
    /* underflow/rounding conversions: we expect capping to max target type */
    make_tuple(Float32Type::get(), Var(float32_t {-4.2}), UInt64Type::get(), numeric_limits<uint64_t>::lowest()),
    make_tuple(Float64Type::get(), Var(float64_t {-4.2}), UInt64Type::get(), numeric_limits<uint64_t>::lowest())),
  [](const testing::TestParamInfo<ConvertToUint64::ParamType>& info)
  { return convertTypesToTestCaseName(std::get<0>(info.param), std::get<1>(info.param), std::get<2>(info.param)); });

//--------------------------------------------------------------------------------------------------------------
// ConversionTests: int16
//--------------------------------------------------------------------------------------------------------------

struct ConvertToInt16: public VariantAdapterTestBase<int16_t>
{
};

TEST_P(ConvertToInt16, ConversionTest) { runComparison(); };

INSTANTIATE_TEST_SUITE_P(
  VariantAdapterTest,
  ConvertToInt16,
  ::testing::Values(
    make_tuple(Int16Type::get(), Var(int16_t {42}), Int16Type::get(), int16_t {42}),
    make_tuple(UInt8Type::get(), Var(uint8_t {42}), Int16Type::get(), int16_t {42}),
    make_tuple(UInt16Type::get(), Var(uint16_t {42}), Int16Type::get(), int16_t {42}),
    make_tuple(UInt32Type::get(), Var(uint32_t {42}), Int16Type::get(), int16_t {42}),
    make_tuple(UInt64Type::get(), Var(uint64_t {42}), Int16Type::get(), int16_t {42}),
    make_tuple(Int32Type::get(), Var(int32_t {42}), Int16Type::get(), int16_t {42}),
    make_tuple(Int64Type::get(), Var(int64_t {42}), Int16Type::get(), int16_t {42}),
    make_tuple(Float32Type::get(), Var(float32_t {42}), Int16Type::get(), int16_t {42}),
    make_tuple(Float64Type::get(), Var(float64_t {42}), Int16Type::get(), int16_t {42}),
    /* overflow conversions: we expect capping to max target type */
    make_tuple(UInt16Type::get(), Var(uint32_t {55535}), Int16Type::get(), numeric_limits<int16_t>::max()),
    make_tuple(UInt32Type::get(), Var(uint32_t {65536}), Int16Type::get(), numeric_limits<int16_t>::max()),
    make_tuple(UInt64Type::get(), Var(uint64_t {65536}), Int16Type::get(), numeric_limits<int16_t>::max()),
    make_tuple(Int32Type::get(), Var(int32_t {65536}), Int16Type::get(), numeric_limits<int16_t>::max()),
    make_tuple(Int64Type::get(), Var(int64_t {65536}), Int16Type::get(), numeric_limits<int16_t>::max()),
    /* underflow conversions: we expect capping to lowest target type */
    make_tuple(Int32Type::get(), Var(int32_t {-42768}), Int16Type::get(), numeric_limits<int16_t>::lowest()),
    make_tuple(Int64Type::get(), Var(int64_t {-3147483648LL}), Int16Type::get(), numeric_limits<int16_t>::lowest()),
    /* underflow/rounding conversions: we expect capping to max target type */
    make_tuple(Float32Type::get(), Var(float32_t {-42768}), Int16Type::get(), numeric_limits<int16_t>::lowest()),
    make_tuple(Float64Type::get(),
               Var(float64_t {-3147483648.0L}),
               Int16Type::get(),
               numeric_limits<int16_t>::lowest())),
  [](const testing::TestParamInfo<ConvertToInt16::ParamType>& info)
  { return convertTypesToTestCaseName(std::get<0>(info.param), std::get<1>(info.param), std::get<2>(info.param)); });

//--------------------------------------------------------------------------------------------------------------
// ConversionTests: int32
//--------------------------------------------------------------------------------------------------------------

struct ConvertToInt32: public VariantAdapterTestBase<int32_t>
{
};

TEST_P(ConvertToInt32, ConversionTest) { runComparison(); };

INSTANTIATE_TEST_SUITE_P(
  VariantAdapterTest,
  ConvertToInt32,
  ::testing::Values(
    make_tuple(Int32Type::get(), Var(int32_t {42}), Int32Type::get(), int32_t {42}),
    make_tuple(UInt8Type::get(), Var(uint8_t {42}), Int32Type::get(), int32_t {42}),
    make_tuple(UInt16Type::get(), Var(uint16_t {42}), Int32Type::get(), int32_t {42}),
    make_tuple(UInt32Type::get(), Var(uint32_t {42}), Int32Type::get(), int32_t {42}),
    make_tuple(UInt64Type::get(), Var(uint64_t {42}), Int32Type::get(), int32_t {42}),
    make_tuple(Int16Type::get(), Var(int16_t {42}), Int32Type::get(), int32_t {42}),
    make_tuple(Int64Type::get(), Var(int64_t {42}), Int32Type::get(), int32_t {42}),
    make_tuple(Float32Type::get(), Var(float32_t {42}), Int32Type::get(), int32_t {42}),
    make_tuple(Float64Type::get(), Var(float64_t {42.0L}), Int32Type::get(), int32_t {42}),
    /* overflow conversions: we expect capping to max target type */
    make_tuple(UInt32Type::get(), Var(uint32_t {3147483647}), Int32Type::get(), numeric_limits<int32_t>::max()),
    make_tuple(UInt64Type::get(), Var(uint64_t {3147483647}), Int32Type::get(), numeric_limits<int32_t>::max()),
    make_tuple(Int64Type::get(), Var(int64_t {3147483647LL}), Int32Type::get(), numeric_limits<int32_t>::max()),
    /* underflow conversions: we expect capping to lowest target type */
    make_tuple(Int64Type::get(), Var(int64_t {-3147483648LL}), Int32Type::get(), numeric_limits<int32_t>::lowest()),
    /* underflow/rounding conversions: we expect capping to max target type */
    make_tuple(Float32Type::get(),
               Var(float32_t {-3147483648.0F}),
               Int32Type::get(),
               numeric_limits<int32_t>::lowest()),
    make_tuple(Float64Type::get(),
               Var(float64_t {-3147483648.0L}),
               Int32Type::get(),
               numeric_limits<int32_t>::lowest())),
  [](const testing::TestParamInfo<ConvertToInt32::ParamType>& info)
  { return convertTypesToTestCaseName(std::get<0>(info.param), std::get<1>(info.param), std::get<2>(info.param)); });

//--------------------------------------------------------------------------------------------------------------
// ConversionTests: int64
//--------------------------------------------------------------------------------------------------------------

struct ConvertToInt64: public VariantAdapterTestBase<int64_t>
{
};

TEST_P(ConvertToInt64, ConversionTest) { runComparison(); };

INSTANTIATE_TEST_SUITE_P(
  VariantAdapterTest,
  ConvertToInt64,
  ::testing::Values(make_tuple(Int64Type::get(), Var(int64_t {42}), Int64Type::get(), int64_t {42}),
                    make_tuple(UInt8Type::get(), Var(uint8_t {42}), Int64Type::get(), int64_t {42}),
                    make_tuple(UInt16Type::get(), Var(uint16_t {42}), Int64Type::get(), int64_t {42}),
                    make_tuple(UInt32Type::get(), Var(uint32_t {42}), Int64Type::get(), int64_t {42}),
                    make_tuple(UInt64Type::get(), Var(uint64_t {42}), Int64Type::get(), int64_t {42}),
                    make_tuple(Int16Type::get(), Var(int16_t {42}), Int64Type::get(), int64_t {42}),
                    make_tuple(Int32Type::get(), Var(int32_t {42}), Int64Type::get(), int64_t {42}),
                    make_tuple(Float32Type::get(), Var(float32_t {42}), Int64Type::get(), int64_t {42}),
                    make_tuple(Float64Type::get(), Var(float64_t {42}), Int64Type::get(), int64_t {42}),
                    /* overflow conversions: we expect capping to max target type */
                    make_tuple(UInt64Type::get(),
                               Var(uint64_t {10223372036854775807UL}),
                               Int64Type::get(),
                               numeric_limits<int64_t>::max()),
                    /* underflow/rounding conversions: we expect capping to max target type */
                    make_tuple(Float32Type::get(),
                               Var(float32_t {-10223372036854775808.0F

                               }),
                               Int64Type::get(),
                               numeric_limits<int64_t>::lowest()),
                    make_tuple(Float64Type::get(),
                               Var(float64_t {-10223372036854775808.0L}),
                               Int64Type::get(),
                               numeric_limits<int64_t>::lowest())),
  [](const testing::TestParamInfo<ConvertToInt64::ParamType>& info)
  { return convertTypesToTestCaseName(std::get<0>(info.param), std::get<1>(info.param), std::get<2>(info.param)); });

//--------------------------------------------------------------------------------------------------------------
// ConversionTests: float32_t
//--------------------------------------------------------------------------------------------------------------

struct ConvertToFloat32: public VariantAdapterTestBase<float32_t>
{
};

TEST_P(ConvertToFloat32, ConversionTest) { runComparison(); };

INSTANTIATE_TEST_SUITE_P(
  VariantAdapterTest,
  ConvertToFloat32,
  ::testing::Values(make_tuple(Float32Type::get(), Var(float32_t {42.2}), Float32Type::get(), float32_t {42.2}),
                    make_tuple(UInt8Type::get(), Var(uint8_t {42}), Float32Type::get(), float32_t {42}),
                    make_tuple(UInt16Type::get(), Var(uint16_t {42}), Float32Type::get(), float32_t {42}),
                    make_tuple(UInt32Type::get(), Var(uint32_t {42}), Float32Type::get(), float32_t {42}),
                    make_tuple(UInt64Type::get(), Var(uint64_t {42}), Float32Type::get(), float32_t {42}),
                    make_tuple(Int16Type::get(), Var(int16_t {42}), Float32Type::get(), float32_t {42}),
                    make_tuple(Int32Type::get(), Var(int32_t {42}), Float32Type::get(), float32_t {42}),
                    make_tuple(Int64Type::get(), Var(int64_t {42}), Float32Type::get(), float32_t {42}),
                    make_tuple(Float64Type::get(), Var(float64_t {42.2}), Float32Type::get(), float32_t {42.2}),
                    /* overflow conversions: we expect capping to max target type */
                    make_tuple(Float64Type::get(),
                               Var(numeric_limits<float64_t>::max()),
                               Float32Type::get(),
                               numeric_limits<float32_t>::max()),
                    /* underflow conversions: we expect capping to max target type */
                    make_tuple(Float64Type::get(),
                               Var(numeric_limits<float64_t>::lowest()),
                               Float32Type::get(),
                               numeric_limits<float32_t>::lowest())),
  [](const testing::TestParamInfo<ConvertToFloat32::ParamType>& info)
  { return convertTypesToTestCaseName(std::get<0>(info.param), std::get<1>(info.param), std::get<2>(info.param)); });

//--------------------------------------------------------------------------------------------------------------
// ConversionTests: float64_t
//--------------------------------------------------------------------------------------------------------------

struct ConvertToFloat64: public VariantAdapterTestBase<float64_t>
{
};

TEST_P(ConvertToFloat64, ConversionTest) { runComparison(); };

INSTANTIATE_TEST_SUITE_P(
  VariantAdapterTest,
  ConvertToFloat64,
  ::testing::Values(make_tuple(Float64Type::get(), Var(float64_t {42.2}), Float64Type::get(), float64_t {42.2}),
                    make_tuple(UInt8Type::get(), Var(uint8_t {42}), Float64Type::get(), float64_t {42}),
                    make_tuple(UInt16Type::get(), Var(uint16_t {42}), Float64Type::get(), float64_t {42}),
                    make_tuple(UInt32Type::get(), Var(uint32_t {42}), Float64Type::get(), float64_t {42}),
                    make_tuple(UInt64Type::get(), Var(uint64_t {42}), Float64Type::get(), float64_t {42}),
                    make_tuple(Int16Type::get(), Var(int16_t {42}), Float64Type::get(), float64_t {42}),
                    make_tuple(Int32Type::get(), Var(int32_t {42}), Float64Type::get(), float64_t {42}),
                    make_tuple(Int64Type::get(), Var(int64_t {42}), Float64Type::get(), float64_t {42}),
                    make_tuple(Float32Type::get(), Var(float32_t {1.25}), Float64Type::get(), float64_t {1.25})),
  [](const testing::TestParamInfo<ConvertToFloat64::ParamType>& info)
  { return convertTypesToTestCaseName(std::get<0>(info.param), std::get<1>(info.param), std::get<2>(info.param)); });

//--------------------------------------------------------------------------------------------------------------
// ConversionTests: bool
//--------------------------------------------------------------------------------------------------------------

struct ConvertToBool: public VariantAdapterTestBase<bool>
{
};

TEST_P(ConvertToBool, ConversionTest) { runComparison(); };

INSTANTIATE_TEST_SUITE_P(VariantAdapterTest,
                         ConvertToBool,
                         ::testing::Values(
                           /* true conversions */
                           make_tuple(BoolType::get(), Var(true), BoolType::get(), bool {true}),
                           make_tuple(UInt8Type::get(), Var(uint8_t {42}), BoolType::get(), bool {true}),
                           make_tuple(UInt16Type::get(), Var(uint16_t {42}), BoolType::get(), bool {true}),
                           make_tuple(UInt32Type::get(), Var(uint32_t {42}), BoolType::get(), bool {true}),
                           make_tuple(UInt64Type::get(), Var(uint64_t {42}), BoolType::get(), bool {true}),
                           make_tuple(Int16Type::get(), Var(int16_t {42}), BoolType::get(), bool {true}),
                           make_tuple(Int32Type::get(), Var(int32_t {42}), BoolType::get(), bool {true}),
                           make_tuple(Int64Type::get(), Var(int64_t {42}), BoolType::get(), bool {true}),
                           make_tuple(Float32Type::get(), Var(float32_t {1.25}), BoolType::get(), bool {true}),
                           make_tuple(Float64Type::get(), Var(float64_t {42.2}), BoolType::get(), bool {true}),
                           /* false conversions */
                           make_tuple(BoolType::get(), Var(false), BoolType::get(), bool {false}),
                           make_tuple(UInt8Type::get(), Var(uint8_t {0}), BoolType::get(), bool {false}),
                           make_tuple(UInt16Type::get(), Var(uint16_t {0}), BoolType::get(), bool {false}),
                           make_tuple(UInt32Type::get(), Var(uint32_t {0}), BoolType::get(), bool {false}),
                           make_tuple(UInt64Type::get(), Var(uint64_t {0}), BoolType::get(), bool {false}),
                           make_tuple(Int16Type::get(), Var(int16_t {0}), BoolType::get(), bool {false}),
                           make_tuple(Int32Type::get(), Var(int32_t {0}), BoolType::get(), bool {false}),
                           make_tuple(Int64Type::get(), Var(int64_t {0}), BoolType::get(), bool {false}),
                           make_tuple(Float32Type::get(), Var(float32_t {0.0}), BoolType::get(), bool {false}),
                           make_tuple(Float64Type::get(), Var(float64_t {0.0}), BoolType::get(), bool {false})),
                         [](const testing::TestParamInfo<ConvertToBool::ParamType>& info)
                         {
                           return convertTypesToTestCaseName(
                             std::get<0>(info.param), std::get<1>(info.param), std::get<2>(info.param));
                         });
