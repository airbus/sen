// === span_test.cpp ===================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

// sen
#include "sen/core/base/span.h"

// google test
#include <gtest/gtest.h>

// std
#include <array>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <type_traits>
#include <vector>

using sen::makeConstSpan;
using sen::makeSpan;
using sen::Span;

namespace
{

class SpanTest: public ::testing::Test
{
protected:
  std::array<int32_t, 2> value {3, 4};
  static constexpr std::array<int32_t, 2> cvalue {5, 6};
};

class SpanVectorTest: public ::testing::Test
{
protected:
  using Vec = std::vector<int>;
};

}  // namespace

/// @test
/// Check default constructor
/// @requirements(SEN-355)
TEST_F(SpanTest, default_construct)
{
  constexpr Span<int32_t> span;
  static_assert(span.empty(), "container size emtpy");
  static_assert(span.data() == nullptr, "data is null");
}

/// @test
/// Check pointer size constructor
/// @requirements(SEN-355)
TEST_F(SpanTest, pointer_size_construct)
{
  Span<int32_t const> span {cvalue.data(), cvalue.size()};
  EXPECT_EQ(span.size(), cvalue.size());
  EXPECT_EQ(span.data(), cvalue.data());
  EXPECT_EQ(span[0U], cvalue[0U]);
}

/// @test
/// Check array constructor
/// @requirements(SEN-355)
TEST_F(SpanTest, array_construct)
{
  Span<int32_t const> cspan {cvalue};
  EXPECT_EQ(cspan.size(), cvalue.size());
  EXPECT_EQ(cspan.data(), cvalue.data());
}

/// @test
/// Check vector constructor
/// @requirements(SEN-355)
TEST_F(SpanTest, vector_construct)
{
  using VectorT = std::vector<int32_t>;

  VectorT vector = {3, 4};

  Span<int32_t> span(vector);
  EXPECT_EQ(span.size(), vector.size());
  EXPECT_EQ(span.data(), vector.data());

  const VectorT cvector = {3, 4};
  Span<const int32_t> constSpan(cvector);
  EXPECT_EQ(constSpan.size(), cvector.size());
  EXPECT_EQ(constSpan.data(), cvector.data());
}

/// @test
/// Check first method
/// @requirements(SEN-355)
TEST_F(SpanTest, first)
{
  Span<int32_t> span {value};

  auto const first0 = span.first(0U);
  EXPECT_EQ(first0.data(), span.data());
  EXPECT_EQ(first0.size(), 0U);

  auto const first1 = span.first(1U);
  EXPECT_EQ(first1.data(), span.data());
  EXPECT_EQ(first1.size(), 1U);

  auto const first2 = span.first(2U);
  EXPECT_EQ(first2.data(), span.data());
  EXPECT_EQ(first2.size(), 2U);
}

/// @test
/// Check last method
/// @requirements(SEN-355)
TEST_F(SpanTest, last)
{
  Span<int32_t> span {value};

  auto const last0 = span.last(0U);
  EXPECT_EQ(last0.data(), (span.data() + 2U));  // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
  EXPECT_EQ(last0.size(), 0U);

  auto const last1 = span.last(1U);
  EXPECT_EQ(last1.data(), (span.data() + 1U));  // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
  EXPECT_EQ(last1.size(), 1U);

  auto const last2 = span.last(2U);
  EXPECT_EQ(last2.data(), span.data());
  EXPECT_EQ(last2.size(), 2U);
}

/// @test
/// Check subspan method
/// @requirements(SEN-355)
TEST_F(SpanTest, subspan_offset)
{
  Span<int32_t> span {value};

  auto const sub0 = span.subspan();
  EXPECT_EQ(sub0.data(), span.data());
  EXPECT_EQ(sub0.size(), 2U);

  auto const sub1 = span.subspan(1U);
  EXPECT_EQ(sub1.data(), (span.data() + 1U));  // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
  EXPECT_EQ(sub1.size(), 1U);

  auto const sub2 = span.subspan(2U);
  EXPECT_EQ(sub2.data(), (span.data() + 2U));  // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
  EXPECT_EQ(sub2.size(), 0U);
}

/// @test
/// Check subspan method using offset and count
/// @requirements(SEN-355)
TEST_F(SpanTest, subspan_offset_and_count)
{
  Span<int32_t> span {value};

  auto const sub0n0 = span.subspan(0U, 0U);
  EXPECT_EQ(sub0n0.data(), span.data());
  EXPECT_EQ(sub0n0.size(), 0U);

  auto const sub0n1 = span.subspan(0U, 1U);
  EXPECT_EQ(sub0n1.data(), span.data());
  EXPECT_EQ(sub0n1.size(), 1);

  auto const sub0n2 = span.subspan(0U, 2U);
  EXPECT_EQ(sub0n2.data(), span.data());
  EXPECT_EQ(sub0n2.size(), 2);

  auto const sub1n0 = span.subspan(1, 0U);
  EXPECT_EQ(sub1n0.data(), (span.data() + 1));  // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
  EXPECT_EQ(sub1n0.size(), 0U);

  auto const sub1n1 = span.subspan(1U, 1U);
  EXPECT_EQ(sub1n1.data(), (span.data() + 1U));  // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
  EXPECT_EQ(sub1n1.size(), 1);

  auto const sub2n0 = span.subspan(2U, 0U);
  EXPECT_EQ(sub2n0.data(), (span.data() + 2U));  // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
  EXPECT_EQ(sub2n0.size(), 0U);
}

/// @test
/// Check access operator in span
/// @requirements(SEN-355)
TEST_F(SpanTest, access)
{
  Span<int32_t> span {value};
  const auto& constSpan = span;
  Span<const int32_t> spanOfConst {value};
  const auto& constSpanOfConst = spanOfConst;

  // begin(), cbegin()
  EXPECT_EQ(&span[0U], span.begin());
  EXPECT_EQ(&constSpan[0U], constSpan.begin());
  EXPECT_EQ(&spanOfConst[0U], spanOfConst.cbegin());
  EXPECT_EQ(&constSpanOfConst[0U], constSpanOfConst.cbegin());

  // end(), cend()
  const std::size_t endPos = span.size() - 1;
  EXPECT_EQ((std::next(&span[endPos])), span.end());
  EXPECT_EQ((std::next(&constSpan[endPos])), constSpan.end());
  EXPECT_EQ((std::next(&spanOfConst[endPos])), spanOfConst.cend());
  EXPECT_EQ((std::next(&constSpanOfConst[endPos])), constSpanOfConst.cend());

  // data()
  EXPECT_EQ(&span[0U], span.data());
  EXPECT_EQ(&constSpan[0U], constSpan.data());
  EXPECT_EQ(&spanOfConst[0U], spanOfConst.data());
  EXPECT_EQ(&constSpanOfConst[0U], constSpanOfConst.data());

  // operator []
  for (std::size_t i = 0U; i < value.size(); ++i)
  {
    EXPECT_EQ(span[i], value.at(i));
    EXPECT_EQ(constSpan[i], value.at(i));
    EXPECT_EQ(spanOfConst[i], value.at(i));
    EXPECT_EQ(constSpanOfConst[i], value.at(i));
  }
}

/// @test
/// Check empty method
/// @requirements(SEN-355)
TEST_F(SpanTest, empty)
{
  std::array<int32_t, 0U> array {};

  Span<int32_t> nonEmptySpan {value};
  Span<int32_t> emptySpan {array};
  Span<int32_t> emptySpan2 {nullptr, 0U};

  EXPECT_TRUE(emptySpan.empty());
  EXPECT_TRUE(emptySpan2.empty());
  EXPECT_FALSE(nonEmptySpan.empty());
}

template <typename T, std::size_t n>
using CArray = T[n];

template <typename T>
using MakeSpanScalarType = decltype(makeSpan(std::declval<T&>()));

template <typename T>
using MakeSpanPointerType = decltype(makeSpan(std::declval<T*&>(), 2UL));

template <typename T>
using MakeSpanCArrayType = decltype(makeSpan(std::declval<CArray<T, 3UL>&>()));

template <typename T>
using MakeSpanIteratorType = decltype(makeSpan(std::declval<T*&>(), std::declval<T*&>()));

template <typename T>
using MakeSpanVectorType = decltype(makeSpan(std::declval<std::vector<T>&>()));

template <typename T>
using MakeSpanArrayType = decltype(makeSpan(std::declval<std::array<T, 1UL>&>()));

/// @test
/// Check span type compatibilitity
/// @requirements(SEN-355)
TEST_F(SpanVectorTest, static_asserts)
{
  static_assert(std::is_same<MakeSpanScalarType<int32_t>, Span<int32_t>>::value, "type compatibility");
  static_assert(std::is_same<MakeSpanPointerType<int32_t>, Span<int32_t>>::value, "type compatibility");
  static_assert(std::is_same<MakeSpanCArrayType<int32_t>, Span<int32_t>>::value, "type compatibility");
  static_assert(std::is_same<MakeSpanIteratorType<int32_t>, Span<int32_t>>::value, "type compatibility");
  static_assert(std::is_same<MakeSpanVectorType<int32_t>, Span<int32_t>>::value, "type compatibility");
  static_assert(std::is_same<MakeSpanArrayType<int32_t>, Span<int32_t>>::value, "type compatibility");
}

/// @test
/// Check span constructor using scalar
/// @requirements(SEN-355)
TEST_F(SpanVectorTest, scalar)
{
  auto scalar = 1;
  auto scalarSpan = makeSpan(scalar);
  EXPECT_EQ(&scalar, scalarSpan.data());
  EXPECT_EQ(1, scalarSpan.size());
}

/// @test
/// Test span pointer
/// @requirements(SEN-355)
TEST_F(SpanVectorTest, pointer)
{
  const std::size_t n = 3UL;
  int buffer[n] = {};
  auto pointerSpan = makeSpan(buffer, n);
  EXPECT_EQ(buffer, pointerSpan.data());
  EXPECT_EQ(n, pointerSpan.size());
}

/// @test
/// Test span carray
/// @requirements(SEN-355)
TEST_F(SpanVectorTest, carray)
{
  const std::size_t n = 3UL;
  int buffer[n] = {};
  auto carraySpan = makeSpan(buffer);
  EXPECT_EQ(buffer, carraySpan.data());
  EXPECT_EQ(n, carraySpan.size());
}

/// @test
/// Test span iterator
/// @requirements(SEN-355)
TEST_F(SpanVectorTest, iterator)
{
  auto array = std::array<int, 3UL> {1, 2, 3};
  auto iteratorSpan = makeSpan(array.begin(), array.end());
  EXPECT_EQ(array.data(), iteratorSpan.data());
  EXPECT_EQ(array.size(), iteratorSpan.size());
}

/// @test
/// Test span array
/// @requirements(SEN-355)
TEST_F(SpanVectorTest, array)
{
  auto array = std::array<int, 3UL> {1, 2, 3};
  auto arraySpan = makeSpan(array);
  EXPECT_EQ(array.data(), arraySpan.data());
  EXPECT_EQ(array.size(), arraySpan.size());
}

/// @test
/// Test span vector
/// @requirements(SEN-355)
TEST_F(SpanVectorTest, vector)
{
  Vec vector = {1, 2, 3};
  auto vectorSpan = makeSpan(vector);
  EXPECT_EQ(vector.data(), vectorSpan.data());
  EXPECT_EQ(vector.size(), vectorSpan.size());
}

template <typename T>
using MakeConstSpanPointerType = decltype(makeConstSpan(std::declval<T*>(), 2UL));

template <typename T>
using MakeConstSpanCArrayType = decltype(makeConstSpan(std::declval<CArray<T, 3UL>>()));

template <typename T>
using MakeConstSpanIteratorType = decltype(makeConstSpan(std::declval<T*>(), std::declval<T*>()));

template <typename T>
using MakeConstSpanVectorType = decltype(makeConstSpan(std::declval<std::vector<T>>()));

template <typename T>
using MakeConstSpanArrayType = decltype(makeConstSpan(std::declval<std::array<T, 1UL>>()));

/// @test
/// Test span type compatibility
/// @requirements(SEN-355)
TEST_F(SpanVectorTest, const_static_asserts)
{
  static_assert(std::is_same<MakeConstSpanPointerType<int32_t>, Span<const int32_t>>::value, "type compatibility");
  static_assert(std::is_same<MakeConstSpanCArrayType<int32_t>, Span<const int32_t>>::value, "type compatibility");
  static_assert(std::is_same<MakeConstSpanIteratorType<int32_t>, Span<const int32_t>>::value, "type compatibility");
  static_assert(std::is_same<MakeConstSpanVectorType<int32_t>, Span<const int32_t>>::value, "type compatibility");
  static_assert(std::is_same<MakeConstSpanArrayType<int32_t>, Span<const int32_t>>::value, "type compatibility");
}

/// @test
/// Test span const pointer
/// @requirements(SEN-355)
TEST_F(SpanVectorTest, const_pointer)
{
  const std::size_t n = 3UL;
  const int buffer[n] = {};
  auto const pointerSpan = makeConstSpan(buffer, n);
  EXPECT_EQ(buffer, pointerSpan.data());
  EXPECT_EQ(n, pointerSpan.size());
}

/// @test
/// Test span const carray
/// @requirements(SEN-355)
TEST_F(SpanVectorTest, const_carray)
{
  const std::size_t n = 3UL;
  const int buffer[n] = {};
  auto const carraySpan = makeConstSpan(buffer);
  EXPECT_EQ(buffer, carraySpan.data());
  EXPECT_EQ(n, carraySpan.size());
}

/// @test
/// Test span const iterator
/// @requirements(SEN-355)
TEST_F(SpanVectorTest, const_iterator)
{
  auto const array = std::array<int, 3UL> {1, 2, 3};
  auto const iteratorSpan = makeConstSpan(array.cbegin(), array.cend());
  EXPECT_EQ(array.data(), iteratorSpan.data());
  EXPECT_EQ(array.size(), iteratorSpan.size());
}

/// @test
/// Test span const array
/// @requirements(SEN-355)
TEST_F(SpanVectorTest, const_array)
{
  auto const array = std::array<int, 3UL> {1, 2, 3};
  auto const arraySpan = makeConstSpan(array);
  EXPECT_EQ(array.data(), arraySpan.data());
  EXPECT_EQ(array.size(), arraySpan.size());
}

/// @test
/// Test span const vector
/// @requirements(SEN-355)
TEST_F(SpanVectorTest, const_vector)
{
  Vec const vector = {1, 2, 3};
  auto const vectorSpan = makeConstSpan(vector);
  EXPECT_EQ(vector.data(), vectorSpan.data());
  EXPECT_EQ(vector.size(), vectorSpan.size());
}
