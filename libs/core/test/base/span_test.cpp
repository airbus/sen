// === span_test.cpp ===================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

// sen
#include "sen/core/base/span.h"
#include "sen/core/base/static_vector.h"

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
using sen::StaticVector;

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
/// Defaults to an empty span with null data, both asserted at compile time.
/// @requirements(SEN-355)
TEST_F(SpanTest, default_construct)
{
  constexpr Span<int32_t> span;
  static_assert(span.empty(), "container size emtpy");
  static_assert(span.data() == nullptr, "data is null");
}

/// @test
/// Views the given pointer and size, reporting the array's size, data address, and first element.
/// @requirements(SEN-355)
TEST_F(SpanTest, pointer_size_construct)
{
  Span<int32_t const> span {cvalue.data(), cvalue.size()};
  EXPECT_EQ(span.size(), cvalue.size());
  EXPECT_EQ(span.data(), cvalue.data());
  EXPECT_EQ(span[0U], cvalue[0U]);
}

/// @test
/// Views a std::array, the span's size and data matching the array's.
/// @requirements(SEN-355)
TEST_F(SpanTest, array_construct)
{
  Span<int32_t const> cspan {cvalue};
  EXPECT_EQ(cspan.size(), cvalue.size());
  EXPECT_EQ(cspan.data(), cvalue.data());
}

/// @test
/// Views a std::vector, mutable and const spans both reporting the vector's size and data.
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
/// Returns prefixes through first, sizes 0, 1, and 2 all sharing the span's data pointer.
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
/// Returns suffixes through last, size 0 pointing one past the end, size 1 at the final element, and size 2 at
/// the span's start.
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
/// Drops a prefix through subspan, the default keeping everything while offsets 1 and 2 advance the data
/// pointer and shrink the size to 1 and 0.
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
/// Selects windows through subspan with offset and count, the data pointer advanced by the offset and the size
/// equal to the count, over every valid pair on a two element span.
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
/// Agrees across begin, end, cbegin, cend, data, and operator[] for mutable, const, and span-of-const views,
/// every element reading the underlying array's value.
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
/// Reports empty for a span over a zero length array and for a null pointer with size 0, and non-empty for a
/// span over elements.
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
/// Deduces Span<int32_t> from makeSpan for scalar, pointer and size, C array, iterator pair, vector, and
/// std::array arguments, asserted at compile time.
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
/// Spans a single scalar through makeSpan, the data pointing at the variable with size 1.
/// @requirements(SEN-355)
TEST_F(SpanVectorTest, scalar)
{
  auto scalar = 1;
  auto scalarSpan = makeSpan(scalar);
  EXPECT_EQ(&scalar, scalarSpan.data());
  EXPECT_EQ(1, scalarSpan.size());
}

/// @test
/// Spans a pointer and length through makeSpan, data and size matching the buffer.
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
/// Spans a C array through makeSpan, deducing its data and size 3.
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
/// Spans an iterator pair through makeSpan, covering the array's data and full size.
/// @requirements(SEN-355)
TEST_F(SpanVectorTest, iterator)
{
  auto array = std::array<int, 3UL> {1, 2, 3};
  auto iteratorSpan = makeSpan(array.begin(), array.end());
  EXPECT_EQ(array.data(), iteratorSpan.data());
  EXPECT_EQ(array.size(), iteratorSpan.size());
}

/// @test
/// Spans a std::array through makeSpan, data and size matching the array.
/// @requirements(SEN-355)
TEST_F(SpanVectorTest, array)
{
  auto array = std::array<int, 3UL> {1, 2, 3};
  auto arraySpan = makeSpan(array);
  EXPECT_EQ(array.data(), arraySpan.data());
  EXPECT_EQ(array.size(), arraySpan.size());
}

/// @test
/// Spans a std::vector through makeSpan, data and size matching the vector.
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

template <typename T>
using MakeConstSpanStaticVectorType = decltype(makeConstSpan(std::declval<StaticVector<T, 4UL>>()));

/// @test
/// Deduces Span<const int32_t> from makeConstSpan for pointer and size, C array, iterator pair, vector,
/// std::array, and StaticVector arguments, asserted at compile time.
/// @requirements(SEN-355)
TEST_F(SpanVectorTest, const_static_asserts)
{
  static_assert(std::is_same<MakeConstSpanPointerType<int32_t>, Span<const int32_t>>::value, "type compatibility");
  static_assert(std::is_same<MakeConstSpanCArrayType<int32_t>, Span<const int32_t>>::value, "type compatibility");
  static_assert(std::is_same<MakeConstSpanIteratorType<int32_t>, Span<const int32_t>>::value, "type compatibility");
  static_assert(std::is_same<MakeConstSpanVectorType<int32_t>, Span<const int32_t>>::value, "type compatibility");
  static_assert(std::is_same<MakeConstSpanArrayType<int32_t>, Span<const int32_t>>::value, "type compatibility");
  static_assert(std::is_same<MakeConstSpanStaticVectorType<int32_t>, Span<const int32_t>>::value, "type compatibility");
}

/// @test
/// Spans a StaticVector through makeConstSpan, covering the three pushed elements rather than the capacity of
/// four and reading them back in order.
/// @requirements(SEN-355)
TEST_F(SpanVectorTest, const_static_vector)
{
  StaticVector<int32_t, 4UL> vector;
  EXPECT_TRUE(vector.push_back(1));
  EXPECT_TRUE(vector.push_back(2));
  EXPECT_TRUE(vector.push_back(3));

  const auto vectorSpan = makeConstSpan(vector);

  EXPECT_EQ(vector.data(), vectorSpan.data());
  ASSERT_EQ(vector.size(), vectorSpan.size());
  EXPECT_EQ(vectorSpan[0], 1);
  EXPECT_EQ(vectorSpan[2], 3);
}

/// @test
/// Produces an empty span from makeConstSpan over a StaticVector holding no elements.
/// @requirements(SEN-355)
TEST_F(SpanVectorTest, const_static_vector_empty)
{
  const StaticVector<int32_t, 4UL> vector;

  const auto vectorSpan = makeConstSpan(vector);

  EXPECT_EQ(vectorSpan.size(), 0UL);
  EXPECT_TRUE(vectorSpan.empty());
}

/// @test
/// Spans a const buffer and length through makeConstSpan, data and size matching the buffer.
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
/// Spans a const C array through makeConstSpan, deducing its data and size 3.
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
/// Spans a const iterator pair through makeConstSpan, covering the array's data and full size.
/// @requirements(SEN-355)
TEST_F(SpanVectorTest, const_iterator)
{
  auto const array = std::array<int, 3UL> {1, 2, 3};
  auto const iteratorSpan = makeConstSpan(array.cbegin(), array.cend());
  EXPECT_EQ(array.data(), iteratorSpan.data());
  EXPECT_EQ(array.size(), iteratorSpan.size());
}

/// @test
/// Spans a const std::array through makeConstSpan, data and size matching the array.
/// @requirements(SEN-355)
TEST_F(SpanVectorTest, const_array)
{
  auto const array = std::array<int, 3UL> {1, 2, 3};
  auto const arraySpan = makeConstSpan(array);
  EXPECT_EQ(array.data(), arraySpan.data());
  EXPECT_EQ(array.size(), arraySpan.size());
}

/// @test
/// Spans a const std::vector through makeConstSpan, data and size matching the vector.
/// @requirements(SEN-355)
TEST_F(SpanVectorTest, const_vector)
{
  Vec const vector = {1, 2, 3};
  auto const vectorSpan = makeConstSpan(vector);
  EXPECT_EQ(vector.data(), vectorSpan.data());
  EXPECT_EQ(vector.size(), vectorSpan.size());
}
