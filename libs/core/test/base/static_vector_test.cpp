// === static_vector_test.cpp ==========================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

// sen
#include "sen/core/base/detail/assert_impl.h"
#include "sen/core/base/numbers.h"
#include "sen/core/base/static_vector.h"

// google test
#include <gtest/gtest.h>

// std
#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <initializer_list>
#include <limits>
#include <random>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

using sen::StaticVector;
using sen::StaticVectorBase;
using sen::StaticVectorError;

// NOLINTBEGIN(cppcoreguidelines-pro-bounds-pointer-arithmetic)

namespace
{

std::size_t callCount = 0U;

void testAssertHandler(const sen::impl::CheckInfo& checkInfo) noexcept
{
  std::ignore = checkInfo;
  callCount++;
}

void checkTermination(std::function<void()> func)
{
  auto oldHandler = sen::impl::setFailedCheckHandler(testAssertHandler);
  ASSERT_NE(oldHandler, nullptr);
  auto prevCallCount = callCount;

  func();

  EXPECT_EQ(callCount, prevCallCount + 1);

  std::ignore = sen::impl::setFailedCheckHandler(oldHandler);
}

template <typename V, std::size_t size>
struct VectorTestTypes
{
  using ValueType = V;
  using Vec = StaticVector<ValueType, size>;
  static constexpr std::size_t s = size;
};

template <typename Types>
class VectorTestTemplate: public ::testing::Test
{
public:
  using T = typename Types::ValueType;
  using Vec = typename Types::Vec;
  static constexpr std::size_t s = Types::s;

  void SetUp() override
  {
    for (auto& elem: sampleData)
    {
      getTestData(elem);
    }

    for (auto& elem: insertData)
    {
      getTestData(elem);
    }

    for (auto& elem: differentData)
    {
      getTestData(elem);
    }

    getTestData(testValue);
  }

protected:
  void populate(Vec& vector) const
  {
    for (const auto& elem: sampleData)
    {
      EXPECT_TRUE(vector.push_back(elem));
    }
  }

  void populateDifferent(Vec& vector) const
  {
    for (const auto& elem: differentData)
    {
      EXPECT_TRUE(vector.push_back(elem));
    }
  }

  static void populateShorter(Vec& vector) { EXPECT_TRUE(vector.resize(vector.capacity() / 2, T {4})); }

  static void checkDefaultConstructor(Vec& vector)  // NOLINT(readability-function-size)
  {
    EXPECT_EQ(vector.size(), 0);  // NOLINT(readability-container-size-empty)
    EXPECT_TRUE(vector.empty());
    EXPECT_FALSE(vector.full());
    EXPECT_EQ(vector.capacity(), s);
    EXPECT_EQ(vector.maxSize(), s);

    // begin/end
    EXPECT_EQ(vector.begin(), vector.end());
    EXPECT_EQ(vector.cbegin(), vector.cend());
    EXPECT_EQ(vector.rbegin(), vector.rend());

    // begin/end const
    const auto& constVector = vector;
    EXPECT_EQ(constVector.begin(), constVector.end());
    EXPECT_EQ(constVector.cbegin(), constVector.cend());
    EXPECT_EQ(constVector.rbegin(), constVector.rend());
  }

  static void checkEmpty(Vec& vector)
  {
    checkDefaultConstructor(vector);

    // pop_back
    checkResultError(vector.pop_back(), StaticVectorError::empty);

    {
      auto anotherVector = vector;  // NOLINT(performance-unnecessary-copy-initialization)
      EXPECT_TRUE(anotherVector.empty());
      EXPECT_FALSE(anotherVector.full());
    }

    {
      auto anotherVector = std::move(vector);
      EXPECT_TRUE(anotherVector.empty());
      EXPECT_FALSE(anotherVector.full());
    }
  }

  static void checkFilledWith(const T& value, const Vec& vector)
  {
    for (const auto& elem: vector)
    {
      EXPECT_EQ(elem, value);
    }

    std::array<T, s> data {};
    data.fill(value);
    EXPECT_TRUE(std::equal(vector.begin(), vector.end(), data.begin()));
  }

  template <typename R>
  static void checkResultError(const R& result, StaticVectorError error)
  {
    EXPECT_TRUE(result.isError());
    EXPECT_EQ(result.getError(), error);
  }

  static void getTestData(T& value)
  {
    static_assert(std::is_integral_v<T> || std::is_floating_point_v<T>,
                  "Only supported for integer and floating point types");
    static std::mt19937 rng(0);
    if constexpr (sizeof(T) == 1)
    {
      static std::uniform_int_distribution<uint32_t> dist(std::numeric_limits<uint32_t>::min(),
                                                          std::numeric_limits<uint32_t>::max());
      value = static_cast<T>(dist(rng));  // NOLINT
    }
    else if constexpr (std::is_integral_v<T>)
    {
      static std::uniform_int_distribution<T> dist(std::numeric_limits<T>::min(), std::numeric_limits<T>::max());
      value = dist(rng);
    }
    else if constexpr (std::is_floating_point_v<T>)
    {
      static std::uniform_real_distribution<T> dist(std::numeric_limits<T>::min(), std::numeric_limits<T>::max());
      value = dist(rng);
    }
  }

protected:
  T testValue = {};                    // NOLINT(misc-non-private-member-variables-in-classes)
  std::array<T, s> sampleData {};      // NOLINT(misc-non-private-member-variables-in-classes)
  std::array<T, s> differentData {};   // NOLINT(misc-non-private-member-variables-in-classes)
  std::array<T, s / 4> insertData {};  // NOLINT(misc-non-private-member-variables-in-classes)
};

class BasicVectorTest: public ::testing::Test
{
public:
  using Vec = StaticVector<char, 10>;

  void SetUp() override { ASSERT_TRUE(comparisonData.assign({'a', 'b', 'c', 'd', 'e'})); }

protected:
  Vec vector;          // NOLINT(misc-non-private-member-variables-in-classes)
  Vec comparisonData;  // NOLINT(misc-non-private-member-variables-in-classes)
};

}  // namespace

// The list of types we want to test.
using TestTypes = testing::Types<VectorTestTypes<int8_t, 10>,
                                 VectorTestTypes<int16_t, 10>,
                                 VectorTestTypes<int32_t, 10>,
                                 VectorTestTypes<float32_t, 10>>;

TYPED_TEST_SUITE(VectorTestTemplate, TestTypes);

/// @test
/// Resizes to zero, full, and half capacity, default-constructing new elements and updating size,
/// empty, and full at each step, and a resize with a value fills every element with that value.
/// @requirements(SEN-355)
TYPED_TEST(VectorTestTemplate, resize)
{
  using Vec = typename TypeParam::Vec;
  using T = typename TypeParam::ValueType;
  static constexpr auto s = TypeParam::s;

  Vec vector;
  this->checkEmpty(vector);

  // resize 0
  EXPECT_TRUE(vector.resize(0));
  this->checkEmpty(vector);

  // fill it with the default value
  {
    EXPECT_TRUE(vector.resize(s));
    EXPECT_EQ(vector.size(), s);
    EXPECT_FALSE(vector.empty());
    EXPECT_TRUE(vector.full());
  }

  // clear
  vector.clear();
  this->checkEmpty(vector);

  // fill it half of capacity
  {
    constexpr std::size_t halfSize = s / 2;
    EXPECT_TRUE(vector.resize(halfSize));
    EXPECT_EQ(vector.size(), halfSize);
    EXPECT_FALSE(vector.empty());
    EXPECT_FALSE(vector.full());
    this->checkFilledWith(T {}, vector);
  }

  // clear
  vector.clear();
  this->checkEmpty(vector);

  // fill it with a value
  {
    EXPECT_TRUE(vector.resize(s, this->testValue));
    EXPECT_EQ(vector.size(), s);
    EXPECT_FALSE(vector.empty());
    EXPECT_TRUE(vector.full());
    this->checkFilledWith(this->testValue, vector);
  }
}

/// @test
/// Default-constructs empty with capacity and maxSize equal to the static capacity, and the
/// StaticVectorBase view reports the same limits.
/// @requirements(SEN-355)
TYPED_TEST(VectorTestTemplate, defaultConstruction)
{
  using Vec = typename TypeParam::Vec;
  static constexpr auto s = TypeParam::s;

  {
    Vec vector;

    // defaults
    EXPECT_EQ(vector.capacity(), s);
    EXPECT_EQ(vector.maxSize(), s);
    this->checkEmpty(vector);
  }

  {
    Vec vector;

    // check the base
    auto& base = vector.base();
    const auto& constBase = vector.base();

    EXPECT_EQ(base.capacity(), s);
    EXPECT_EQ(constBase.maxSize(), s);
  }
}

/// @test
/// Copy-constructs an equal vector whose storage is independent, modifying the copy breaks the equality.
/// @requirements(SEN-355)
TYPED_TEST(VectorTestTemplate, copyConstructor)
{
  using Vec = typename TypeParam::Vec;
  using T = typename TypeParam::ValueType;

  Vec vector1;
  this->populate(vector1);

  Vec vector2(vector1);

  EXPECT_EQ(vector2, vector1);

  vector2[2] = T {44};
  EXPECT_NE(vector2, vector1);
}

/// @test
/// Move-constructs a vector that takes over the source elements and leaves the moved-from vector empty.
/// @requirements(SEN-355)
TYPED_TEST(VectorTestTemplate, moveConstructor)
{
  using Vec = typename TypeParam::Vec;

  Vec vector1;
  this->populate(vector1);

  Vec vector2(std::move(vector1));

  EXPECT_TRUE(std::equal(vector2.begin(), vector2.end(), this->sampleData.begin()));
  EXPECT_TRUE(vector1.empty());
}

/// @test
/// Copy-assigns all elements from another vector, leaving the two vectors equal.
/// @requirements(SEN-355)
TYPED_TEST(VectorTestTemplate, assign)
{
  using Vec = typename TypeParam::Vec;

  Vec vector1;
  this->populate(vector1);

  Vec vector2;
  vector2 = vector1;

  EXPECT_TRUE(std::equal(vector2.begin(), vector2.end(), this->sampleData.begin()));
  EXPECT_EQ(vector1, vector2);
}

/// @test
/// Rejects assign from a range longer than the capacity, reporting the full error.
/// @requirements(SEN-355)
TYPED_TEST(VectorTestTemplate, assign2)
{
  using Vec = typename TypeParam::Vec;
  using T = typename TypeParam::ValueType;
  static constexpr auto s = TypeParam::s;

  Vec vector1;

  StaticVector<T, s + 1> larger;
  EXPECT_TRUE(larger.resize(larger.maxSize(), T {4}));

  this->checkResultError(vector1.assign(larger.begin(), larger.end()), StaticVectorError::full);
}

/// @test
/// Rejects assign with a reversed iterator range, reporting the badRange error.
/// @requirements(SEN-355)
TYPED_TEST(VectorTestTemplate, assign3)
{
  using Vec = typename TypeParam::Vec;
  using T = typename TypeParam::ValueType;
  static constexpr auto s = TypeParam::s;

  Vec vector1;

  StaticVector<T, s + 1> larger;
  EXPECT_TRUE(larger.resize(larger.maxSize(), T {4}));
  this->checkResultError(vector1.assign(larger.end(), larger.begin()), StaticVectorError::badRange);
}

/// @test
/// Move-initializes a new vector that receives every element and leaves the moved-from vector empty.
/// @requirements(SEN-355)
TYPED_TEST(VectorTestTemplate, moveAssign)
{
  using Vec = typename TypeParam::Vec;

  Vec vector1;
  this->populate(vector1);

  Vec vector2 = std::move(vector1);

  EXPECT_TRUE(std::equal(vector2.begin(), vector2.end(), this->sampleData.begin()));
  EXPECT_TRUE(vector1.empty());
}

/// @test
/// Assigns an empty range into a populated vector through the StaticVectorBase reference interface, the
/// assign reporting success.
/// @requirements(SEN-355)
TYPED_TEST(VectorTestTemplate, assignmentBase)
{
  using Vec = typename TypeParam::Vec;
  using T = typename TypeParam::ValueType;

  Vec vector1;
  Vec vector2;

  this->populate(vector1);

  StaticVectorBase<T>& base1 = vector1;
  StaticVectorBase<T>& base2 = vector2;

  EXPECT_TRUE(base1.assign(base2.begin(), base2.end()));
  EXPECT_TRUE(std::equal(base1.begin(), base1.end(), base2.begin()));
}

/// @test
/// Keeps the contents unchanged under copy self-assignment.
/// @requirements(SEN-355)
TYPED_TEST(VectorTestTemplate, selfAssignment)
{
  using Vec = typename TypeParam::Vec;

  Vec vector1;
  this->populate(vector1);

  Vec vector2 = vector1;

  // we are doing this on purpose
  vector2.operator=(vector2);  // NOLINT(misc-redundant-expression)

  EXPECT_TRUE(std::equal(vector1.begin(), vector1.end(), vector2.begin()));
}

/// @test
/// Returns from begin the address of the first element, on mutable and const vectors alike.
/// @requirements(SEN-355)
TYPED_TEST(VectorTestTemplate, begin)
{
  using Vec = typename TypeParam::Vec;

  Vec vector1;
  this->populate(vector1);

  Vec vector2;
  this->populate(vector2);

  const Vec& constVector = vector2;

  EXPECT_EQ(&vector1[0], vector1.begin());
  EXPECT_EQ(&constVector[0], constVector.begin());
}

/// @test
/// Returns from end the address one past the last element, on mutable and const vectors alike.
/// @requirements(SEN-355)
TYPED_TEST(VectorTestTemplate, end)
{
  using Vec = typename TypeParam::Vec;

  Vec vector1;
  this->populate(vector1);

  Vec vector2;
  this->populate(vector2);

  const Vec& constVector = vector2;

  EXPECT_EQ(std::next(&vector1[vector1.size() - 1]), vector1.end());
  EXPECT_EQ(std::next(&constVector[constVector.size() - 1]), constVector.end());
}

/// @test
/// Grows from empty to a size below capacity and reports the new size.
/// @requirements(SEN-355)
TYPED_TEST(VectorTestTemplate, resizeUp)
{
  using Vec = typename TypeParam::Vec;

  constexpr std::size_t newSize = 5;
  static_assert(Vec::staticCapacity >= newSize, "capacity check");

  Vec vector1;
  EXPECT_TRUE(vector1.resize(newSize));
  EXPECT_EQ(vector1.size(), newSize);
}

/// @test
/// Grows with a fill value so every element equals that value, accepts a same-size resize as a no-op,
/// and reports the full error for a size beyond maxSize.
/// @requirements(SEN-355)
TYPED_TEST(VectorTestTemplate, resizeUpValue)
{
  using Vec = typename TypeParam::Vec;
  using T = typename TypeParam::ValueType;
  static constexpr auto s = TypeParam::s;

  {
    Vec vector;

    constexpr std::size_t halfSize = s / 2;
    EXPECT_TRUE(vector.resize(halfSize, this->testValue));
    EXPECT_EQ(vector.size(), halfSize);
    EXPECT_FALSE(vector.empty());
    EXPECT_FALSE(vector.full());
    this->checkFilledWith(this->testValue, vector);
  }

  {
    constexpr std::size_t newSize = 5;

    Vec vector1;
    EXPECT_TRUE(vector1.resize(newSize, T {}));
    EXPECT_EQ(vector1.size(), newSize);
    EXPECT_TRUE(vector1.resize(newSize, T {}));
    EXPECT_EQ(vector1.size(), newSize);
  }

  {
    Vec vector1;
    this->checkResultError(vector1.resize(vector1.maxSize() + 1, T {}), StaticVectorError::full);
  }
}

/// @test
/// Refuses to resize beyond capacity, keeping the previous size and the empty and full flags.
/// @requirements(SEN-355)
TYPED_TEST(VectorTestTemplate, resizeExcess)
{
  using Vec = typename TypeParam::Vec;
  static constexpr auto s = TypeParam::s;

  Vec vector;
  constexpr std::size_t halfSize = s / 2;
  EXPECT_TRUE(vector.resize(halfSize));

  constexpr std::size_t newSize = s + 1;
  EXPECT_FALSE(vector.resize(newSize));
  EXPECT_EQ(vector.size(), halfSize);
  EXPECT_FALSE(vector.empty());
  EXPECT_FALSE(vector.full());
}

/// @test
/// Shrinks a full vector to half capacity, reporting the reduced size and no longer full.
/// @requirements(SEN-355)
TYPED_TEST(VectorTestTemplate, resizeDown)
{
  using Vec = typename TypeParam::Vec;
  static constexpr auto s = TypeParam::s;

  Vec vector;
  EXPECT_TRUE(vector.resize(s));
  EXPECT_EQ(vector.size(), vector.maxSize());
  EXPECT_FALSE(vector.empty());
  EXPECT_TRUE(vector.full());

  constexpr std::size_t halfSize = s / 2;
  EXPECT_TRUE(vector.resize(halfSize));
  EXPECT_EQ(vector.size(), halfSize);
  EXPECT_FALSE(vector.empty());
  EXPECT_FALSE(vector.full());
}

/// @test
/// Shrinks a vector filled with a value to half capacity, the remaining elements still equal to that
/// value.
/// @requirements(SEN-355)
TYPED_TEST(VectorTestTemplate, resizeDownValue)
{
  using Vec = typename TypeParam::Vec;
  static constexpr auto s = TypeParam::s;

  Vec vector;
  EXPECT_TRUE(vector.resize(s, this->testValue));
  EXPECT_EQ(vector.size(), vector.maxSize());
  EXPECT_FALSE(vector.empty());
  EXPECT_TRUE(vector.full());
  this->checkFilledWith(this->testValue, vector);

  constexpr std::size_t halfSize = s / 2;
  EXPECT_TRUE(vector.resize(halfSize, this->testValue));
  EXPECT_EQ(vector.size(), halfSize);
  EXPECT_FALSE(vector.empty());
  EXPECT_FALSE(vector.full());
  this->checkFilledWith(this->testValue, vector);
}

/// @test
/// Reports not empty and full after a resize to the whole static capacity.
/// @requirements(SEN-355)
TYPED_TEST(VectorTestTemplate, empty)
{
  using Vec = typename TypeParam::Vec;
  static constexpr auto s = TypeParam::s;

  Vec vector;
  EXPECT_TRUE(vector.resize(s));
  EXPECT_EQ(vector.size(), vector.maxSize());
  EXPECT_FALSE(vector.empty());
  EXPECT_TRUE(vector.full());
}

/// @test
/// Reports size zero, empty, and not full on a default-constructed vector.
/// @requirements(SEN-355)
TYPED_TEST(VectorTestTemplate, full)
{
  using Vec = typename TypeParam::Vec;

  Vec vector;
  EXPECT_EQ(vector.size(), 0U);  // NOLINT(readability-container-size-empty)
  EXPECT_TRUE(vector.empty());
  EXPECT_FALSE(vector.full());
}

/// @test
/// Returns each populated element by position through operator[].
/// @requirements(SEN-355)
TYPED_TEST(VectorTestTemplate, index)
{
  using Vec = typename TypeParam::Vec;

  Vec vector1;

  this->populate(vector1);

  for (std::size_t i = 0; i < this->sampleData.size(); ++i)
  {
    EXPECT_EQ(vector1[i], this->sampleData.at(i));
  }
}

/// @test
/// Returns each populated element by position through operator[] on a const vector.
/// @requirements(SEN-355)
TYPED_TEST(VectorTestTemplate, indexConst)
{
  using Vec = typename TypeParam::Vec;

  Vec vector1;
  this->populate(vector1);

  const Vec& constVector1 = vector1;

  for (std::size_t i = 0; i < this->sampleData.size(); ++i)
  {
    EXPECT_EQ(constVector1[i], this->sampleData.at(i));
  }
}

/// @test
/// Returns the first populated element from front.
/// @requirements(SEN-355)
TYPED_TEST(VectorTestTemplate, front)
{
  using Vec = typename TypeParam::Vec;

  Vec vector1;
  this->populate(vector1);

  EXPECT_EQ(vector1.front(), this->sampleData.front());
}

/// @test
/// Returns the first populated element from front on a const vector.
/// @requirements(SEN-355)
TYPED_TEST(VectorTestTemplate, frontConst)
{
  using Vec = typename TypeParam::Vec;

  Vec vector1;
  this->populate(vector1);
  const Vec& constVector = vector1;

  EXPECT_EQ(constVector.front(), this->sampleData.front());
}

/// @test
/// Returns the last populated element from back.
/// @requirements(SEN-355)
TYPED_TEST(VectorTestTemplate, back)
{
  using Vec = typename TypeParam::Vec;

  Vec vector1;
  this->populate(vector1);

  EXPECT_EQ(vector1.back(), this->sampleData.back());
}

/// @test
/// Returns the last populated element from back on a const vector.
/// @requirements(SEN-355)
TYPED_TEST(VectorTestTemplate, backConst)
{
  using Vec = typename TypeParam::Vec;

  Vec vector1;
  this->populate(vector1);
  const Vec& constVector = vector1;

  EXPECT_EQ(constVector.back(), this->sampleData.back());
}

/// @test
/// Exposes through data a contiguous block holding exactly the populated elements.
/// @requirements(SEN-355)
TYPED_TEST(VectorTestTemplate, data)
{
  using Vec = typename TypeParam::Vec;

  Vec vector1;
  this->populate(vector1);

  EXPECT_TRUE(std::equal(vector1.data(), vector1.data() + vector1.size(), this->sampleData.begin()));
}

/// @test
/// Exposes through data on a const vector a contiguous block holding exactly the populated elements.
/// @requirements(SEN-355)
TYPED_TEST(VectorTestTemplate, dataConst)
{
  using Vec = typename TypeParam::Vec;

  Vec vector1;
  this->populate(vector1);

  const Vec& constVector = vector1;

  EXPECT_TRUE(std::equal(constVector.data(), constVector.data() + constVector.size(), this->sampleData.begin()));
}

/// @test
/// Copies an iterator range through assign, matching the source size and contents.
/// @requirements(SEN-355)
TYPED_TEST(VectorTestTemplate, assignRange)
{
  using Vec = typename TypeParam::Vec;

  Vec vector1;
  this->populate(vector1);

  Vec vector2;

  EXPECT_TRUE(vector2.assign(vector1.begin(), vector1.end()));
  EXPECT_EQ(vector2.size(), vector1.size());
  EXPECT_TRUE(std::equal(vector2.begin(), vector2.end(), vector1.begin()));
}

/// @test
/// Fills through assign with a count and a value, matching an array of that count holding the same
/// value.
/// @requirements(SEN-355)
TYPED_TEST(VectorTestTemplate, assignSizeValue)
{
  using Vec = typename TypeParam::Vec;
  using T = typename TypeParam::ValueType;
  static constexpr auto s = TypeParam::s;

  std::array<T, s / 2> array {};
  array.fill(this->testValue);

  Vec vector1;
  EXPECT_TRUE(vector1.assign(s / 2, this->testValue));
  EXPECT_EQ(array.size(), vector1.size());
  EXPECT_TRUE(std::equal(vector1.begin(), vector1.end(), array.begin()));
}

/// @test
/// Rejects assign with a count above capacity, reporting the full error.
/// @requirements(SEN-355)
TYPED_TEST(VectorTestTemplate, assignSizeValueExcess)
{
  using Vec = typename TypeParam::Vec;
  static constexpr auto s = TypeParam::s;

  constexpr std::size_t arraySize = s + 1;

  Vec vector1;
  this->checkResultError(vector1.assign(arraySize, this->testValue), StaticVectorError::full);
}

/// @test
/// Assigns an initializer list below and at exact capacity, and reports the full error when the list is
/// longer than the capacity.
/// @requirements(SEN-355)
TYPED_TEST(VectorTestTemplate, assignInitList)
{
  using Vec = typename TypeParam::Vec;
  using T = typename TypeParam::ValueType;

  // valid list
  {
    Vec vector;
    std::initializer_list<T> list = {T {}, T {}, T {}};

    EXPECT_TRUE(vector.assign(list));
    EXPECT_EQ(vector.size(), list.size());
  }

  // valid same capacity
  {
    StaticVector<T, 3> vector;
    std::initializer_list<T> list = {T {}, T {}, T {}};

    EXPECT_TRUE(vector.assign(list));
    EXPECT_EQ(vector.size(), list.size());
    EXPECT_EQ(vector.capacity(), list.size());
  }

  // not enough capacity
  {
    StaticVector<T, 2> vector;
    std::initializer_list<T> list = {T {}, T {}, T {}};
    this->checkResultError(vector.assign(list), StaticVectorError::full);
  }
}

/// @test
/// Assigns a temporary initializer list below and at exact capacity, and reports the full error when
/// the list is longer than the capacity.
/// @requirements(SEN-355)
TYPED_TEST(VectorTestTemplate, moveInitList)
{
  using Vec = typename TypeParam::Vec;
  using T = typename TypeParam::ValueType;

  // valid list
  {
    Vec vector;
    EXPECT_TRUE(vector.assign({T {}, T {}, T {}}));
    EXPECT_EQ(vector.size(), 3U);
  }

  // valid same capacity
  {
    StaticVector<T, 3> vector;
    EXPECT_TRUE(vector.assign({T {}, T {}, T {}}));
    EXPECT_EQ(vector.size(), 3U);
  }

  // not enough capacity
  {
    StaticVector<T, 2> vector;
    this->checkResultError(vector.assign({T {}, T {}, T {}}), StaticVectorError::full);
  }
}

/// @test
/// Appends each element through push_back, after which size and contents match the pushed sequence.
/// @requirements(SEN-355)
TYPED_TEST(VectorTestTemplate, pushBack)
{
  using Vec = typename TypeParam::Vec;

  Vec vector;
  for (std::size_t i = 0; i < this->sampleData.size(); ++i)
  {
    EXPECT_TRUE(vector.push_back(this->sampleData.at(i)));
  }

  EXPECT_EQ(this->sampleData.size(), vector.size());
  EXPECT_TRUE(std::equal(vector.begin(), vector.end(), this->sampleData.begin()));
}

/// @test
/// Appends default-constructed elements through the argumentless push_back, matching a default-resized
/// vector, and reports the full error once at capacity.
/// @requirements(SEN-355)
TYPED_TEST(VectorTestTemplate, pushBackEmpty)
{
  using Vec = typename TypeParam::Vec;
  static constexpr auto s = TypeParam::s;

  Vec vector1;
  EXPECT_TRUE(vector1.resize(s));

  Vec vector2;
  for (std::size_t i = 0; i < vector1.size(); ++i)
  {
    EXPECT_TRUE(vector2.push_back());
  }

  EXPECT_EQ(vector1.size(), vector2.size());
  EXPECT_TRUE(std::equal(vector1.begin(), vector1.end(), vector2.begin()));
  this->checkResultError(vector2.push_back(), StaticVectorError::full);
}

/// @test
/// Appends temporary values through push_back, matching a std::vector built with the same calls.
/// @requirements(SEN-355)
TYPED_TEST(VectorTestTemplate, pushBackLiteral)
{
  using Vec = typename TypeParam::Vec;
  using T = typename TypeParam::ValueType;
  static constexpr auto s = TypeParam::s;

  static_assert(s >= 4, "insufficient capacity for this test");

  Vec vector1;
  std::vector<T> vector2;

  EXPECT_TRUE(vector1.push_back(T {}));
  EXPECT_TRUE(vector1.push_back(T {}));
  EXPECT_TRUE(vector1.push_back(T {}));
  EXPECT_TRUE(vector1.push_back(T {}));

  vector2.push_back(T {});
  vector2.push_back(T {});
  vector2.push_back(T {});
  vector2.push_back(T {});

  EXPECT_EQ(vector1.size(), vector2.size());
  EXPECT_TRUE(std::equal(vector1.begin(), vector1.end(), vector2.begin()));
}

/// @test
/// Reports the full error when push_back of a temporary value hits a full vector.
/// @requirements(SEN-355)
TYPED_TEST(VectorTestTemplate, pushBackExcess)
{
  using Vec = typename TypeParam::Vec;
  using T = typename TypeParam::ValueType;
  static constexpr auto s = TypeParam::s;

  Vec vector1;

  for (std::size_t i = 0; i < s; ++i)
  {
    EXPECT_TRUE(vector1.push_back(T {}));
  }

  this->checkResultError(vector1.push_back(T {}), StaticVectorError::full);
}

/// @test
/// Reports the full error when push_back of a const reference hits a full vector.
/// @requirements(SEN-355)
TYPED_TEST(VectorTestTemplate, pushBackExcess2)
{
  using Vec = typename TypeParam::Vec;
  using T = typename TypeParam::ValueType;
  static constexpr auto s = TypeParam::s;

  Vec vector1;
  const T val {};

  for (std::size_t i = 0; i < s; ++i)
  {
    EXPECT_TRUE(vector1.push_back(val));
  }

  this->checkResultError(vector1.push_back(val), StaticVectorError::full);
}

/// @test
/// Appends each element through emplace_back, after which size and contents match the source sequence.
/// @requirements(SEN-355)
TYPED_TEST(VectorTestTemplate, emplaceBack)
{
  using Vec = typename TypeParam::Vec;

  Vec vector;
  for (std::size_t i = 0; i < this->sampleData.size(); ++i)
  {
    EXPECT_TRUE(vector.emplace_back(this->sampleData.at(i)));
  }

  EXPECT_EQ(this->sampleData.size(), vector.size());
  EXPECT_TRUE(std::equal(vector.begin(), vector.end(), this->sampleData.begin()));
}

/// @test
/// Reports the full error when emplace_back hits a full vector.
/// @requirements(SEN-355)
TYPED_TEST(VectorTestTemplate, emplaceBackExcess)
{
  using Vec = typename TypeParam::Vec;
  static constexpr auto s = TypeParam::s;

  Vec vector1;

  for (std::size_t i = 0; i < s; ++i)
  {
    EXPECT_TRUE(vector1.emplace_back());
  }

  this->checkResultError(vector1.emplace_back(), StaticVectorError::full);
}

/// @test
/// Inserts through emplace at the end and mid-vector, growing the size and placing the value at the
/// given position, and reports full on a full vector and badRange for positions outside the vector.
/// @requirements(SEN-355)
TYPED_TEST(VectorTestTemplate, emplace)
{
  using Vec = typename TypeParam::Vec;
  using T = typename TypeParam::ValueType;

  // on a full vector
  {
    Vec vector1;
    this->populate(vector1);
    this->checkResultError(vector1.emplace(vector1.end(), T {}), StaticVectorError::full);
  }

  // on an invalid position
  {
    Vec vector1;

    this->checkResultError(vector1.emplace(vector1.end() + 1, T {}), StaticVectorError::badRange);
  }

  // on an invalid position 2
  {
    Vec vector1;

    this->checkResultError(vector1.emplace(vector1.begin() - 1, T {}), StaticVectorError::badRange);
  }

  // valid
  {
    auto data = this->sampleData;

    Vec vector;
    for (std::size_t i = 0; i < this->sampleData.size(); ++i)
    {
      EXPECT_TRUE(vector.emplace(vector.end(), data.at(i)));
    }

    EXPECT_EQ(this->sampleData.size(), vector.size());
    EXPECT_TRUE(std::equal(vector.begin(), vector.end(), this->sampleData.begin()));
  }

  // at some location
  {
    T v {};
    this->getTestData(v);

    Vec vector;
    EXPECT_TRUE(vector.resize(5));
    EXPECT_TRUE(vector.emplace(vector.begin() + 2, v));
    EXPECT_EQ(vector.size(), 6U);
    EXPECT_EQ(*(vector.begin() + 2), v);
  }
}

/// @test
/// Removes the last element on each pop_back, matching a std::vector after the same removals.
/// @requirements(SEN-355)
TYPED_TEST(VectorTestTemplate, popBack)
{
  using Vec = typename TypeParam::Vec;
  using T = typename TypeParam::ValueType;

  Vec vector1;
  this->populate(vector1);

  std::vector<T> vector2(vector1.begin(), vector1.end());

  EXPECT_TRUE(vector1.pop_back());
  EXPECT_TRUE(vector1.pop_back());

  vector2.pop_back();
  vector2.pop_back();

  EXPECT_EQ(vector1.size(), vector2.size());
  EXPECT_TRUE(std::equal(vector1.begin(), vector1.end(), vector2.begin()));
}

/// @test
/// Pops every element and then reports the empty error on one more pop_back.
/// @requirements(SEN-355)
TYPED_TEST(VectorTestTemplate, popBackError)
{
  using Vec = typename TypeParam::Vec;

  Vec vector;
  EXPECT_TRUE(vector.resize(2));
  EXPECT_TRUE(vector.pop_back());
  EXPECT_TRUE(vector.pop_back());
  this->checkResultError(vector.pop_back(), StaticVectorError::empty);
}

/// @test
/// Inserts a value at every position offset, matching std::vector insertion, and reports badRange for
/// positions before begin or past end.
/// @requirements(SEN-355)
TYPED_TEST(VectorTestTemplate, insertPositionValue)
{
  using Vec = typename TypeParam::Vec;
  using T = typename TypeParam::ValueType;

  const std::size_t startingPoint = 5;

  for (std::size_t offset = 0; offset <= 5; ++offset)
  {
    Vec data;

    // assign elements to the 2 vectors

    EXPECT_TRUE(data.assign(this->sampleData.begin(), this->sampleData.begin() + startingPoint));

    std::vector<T> compareData(startingPoint);
    for (std::size_t i = 0; i < startingPoint; ++i)
    {
      compareData.at(i) = this->sampleData.at(i);
    }

    // insert at the offset
    EXPECT_TRUE(data.insert(data.begin() + offset, this->testValue));

    compareData.insert(compareData.begin() + offset, this->testValue);

    // check that are the same
    EXPECT_EQ(compareData.size(), data.size());
    EXPECT_TRUE(std::equal(data.begin(), data.end(), compareData.begin()));

    // check that we cannot insert out of range (moved value)
    this->checkResultError(data.insert(data.begin() - 1, T {}), StaticVectorError::badRange);

    // check that we cannot insert out of range (moved value)
    this->checkResultError(data.insert(data.end() + 1, T {}), StaticVectorError::badRange);

    // check that we cannot insert out of range
    this->checkResultError(data.insert(data.begin() - 1, this->testValue), StaticVectorError::badRange);

    // check that we cannot insert out of range
    this->checkResultError(data.insert(data.end() + 1, this->testValue), StaticVectorError::badRange);
  }
}

/// @test
/// Inserts a moved temporary at every position offset, matching std::vector insertion at the same
/// offset.
/// @requirements(SEN-355)
TYPED_TEST(VectorTestTemplate, insertPositionValueMoved)
{
  using Vec = typename TypeParam::Vec;
  using T = typename TypeParam::ValueType;

  const std::size_t startingPoint = 5;

  for (std::size_t offset = 0; offset <= 5; ++offset)
  {
    Vec data;

    // assign elements to the 2 vectors
    EXPECT_TRUE(data.assign(this->sampleData.begin(), this->sampleData.begin() + startingPoint));
    std::vector<T> compareData(startingPoint);
    for (std::size_t i = 0; i < startingPoint; ++i)
    {
      compareData.at(i) = this->sampleData.at(i);
    }

    // insert at the offset
    EXPECT_TRUE(data.insert(data.begin() + offset, T {}));
    compareData.insert(compareData.begin() + offset, T {});

    // check that are the same
    EXPECT_EQ(compareData.size(), data.size());
    EXPECT_TRUE(std::equal(data.begin(), data.end(), compareData.begin()));
  }
}

/// @test
/// Reports the full error for insert of a value at any position of a full vector.
/// @requirements(SEN-355)
TYPED_TEST(VectorTestTemplate, insertPositionValueExcess)
{
  using Vec = typename TypeParam::Vec;
  static constexpr auto s = TypeParam::s;

  Vec vector1;
  EXPECT_TRUE(vector1.resize(s, this->testValue));

  this->checkResultError(vector1.insert(vector1.begin() + 2, this->testValue), StaticVectorError::full);
  this->checkResultError(vector1.insert(vector1.begin(), this->testValue), StaticVectorError::full);
  this->checkResultError(vector1.insert(vector1.begin() + vector1.size(), this->testValue), StaticVectorError::full);
}

/// @test
/// Reports the full error for insert of a moved temporary at any position of a full vector.
/// @requirements(SEN-355)
TYPED_TEST(VectorTestTemplate, insertPositionValueMovedExcess)
{
  using Vec = typename TypeParam::Vec;
  using T = typename TypeParam::ValueType;
  static constexpr auto s = TypeParam::s;

  Vec vector1;
  EXPECT_TRUE(vector1.resize(s));

  this->checkResultError(vector1.insert(vector1.begin() + 2, T {}), StaticVectorError::full);
  this->checkResultError(vector1.insert(vector1.begin(), T {}), StaticVectorError::full);
  this->checkResultError(vector1.insert(vector1.begin() + vector1.size(), T {}), StaticVectorError::full);
}

/// @test
/// Inserts a counted run of one value at every position offset, matching std::vector, and reports
/// badRange for positions outside the vector.
/// @requirements(SEN-355)
TYPED_TEST(VectorTestTemplate, insertPositionNValue)
{
  using Vec = typename TypeParam::Vec;
  using T = typename TypeParam::ValueType;

  const std::size_t startingPoint = 5;
  const std::size_t insertSize = 3;

  for (std::size_t offset = 0; offset <= startingPoint; ++offset)
  {
    Vec data;

    // assign elements to the 2 vectors
    EXPECT_TRUE(data.assign(this->sampleData.begin(), this->sampleData.begin() + startingPoint));
    std::vector<T> compareData(startingPoint);
    for (std::size_t i = 0; i < startingPoint; ++i)
    {
      compareData.at(i) = this->sampleData.at(i);
    }

    // insert some elements
    EXPECT_TRUE(data.insert(data.begin() + offset, insertSize, this->testValue));
    compareData.insert(compareData.begin() + offset, insertSize, this->testValue);

    EXPECT_EQ(compareData.size(), data.size());
    EXPECT_TRUE(std::equal(data.begin(), data.end(), compareData.begin()));
  }

  {
    Vec data;
    this->populateShorter(data);

    // check that we cannot insert out of range
    this->checkResultError(data.insert(data.begin() - 1, insertSize, this->testValue), StaticVectorError::badRange);
    this->checkResultError(data.insert(data.end() + 1, insertSize, this->testValue), StaticVectorError::badRange);
  }
}

/// @test
/// Reports the full error for a counted insert at any position of a full vector.
/// @requirements(SEN-355)
TYPED_TEST(VectorTestTemplate, insertPositionNValueExcess)
{
  using Vec = typename TypeParam::Vec;
  static constexpr auto s = TypeParam::s;

  Vec vector1;
  EXPECT_TRUE(vector1.resize(s, this->testValue));

  const std::size_t insertSize = 3;

  this->checkResultError(vector1.insert(vector1.begin(), insertSize, this->testValue), StaticVectorError::full);
  this->checkResultError(vector1.insert(vector1.begin() + 2, insertSize, this->testValue), StaticVectorError::full);
  this->checkResultError(vector1.insert(vector1.begin() + 4, insertSize, this->testValue), StaticVectorError::full);
  this->checkResultError(vector1.insert(vector1.begin() + vector1.size(), insertSize, this->testValue),
                         StaticVectorError::full);
}

/// @test
/// Inserts an iterator range at every position offset, matching std::vector insertion at the same
/// offset.
/// @requirements(SEN-355)
TYPED_TEST(VectorTestTemplate, insertPositionRange)
{
  using Vec = typename TypeParam::Vec;
  using T = typename TypeParam::ValueType;

  const std::size_t startingPoint = 5;

  for (std::size_t offset = 0; offset <= startingPoint; ++offset)
  {
    Vec data;

    EXPECT_TRUE(data.resize(data.maxSize(), T {}));

    // assign elements to the 2 vectors
    EXPECT_TRUE(data.assign(this->sampleData.begin(), this->sampleData.begin() + startingPoint));
    std::vector<T> compareData(startingPoint);
    for (std::size_t i = 0; i < startingPoint; ++i)
    {
      compareData.at(i) = this->sampleData.at(i);
    }

    // insert some elements
    EXPECT_TRUE(data.insert(data.begin() + offset, this->insertData.begin(), this->insertData.end()));
    compareData.insert(compareData.begin() + offset, this->insertData.begin(), this->insertData.end());

    EXPECT_EQ(compareData.size(), data.size());
    EXPECT_TRUE(std::equal(data.begin(), data.end(), compareData.begin()));
  }
}

/// @test
/// Reports the full error for a range insert at any position of a full vector.
/// @requirements(SEN-355)
TYPED_TEST(VectorTestTemplate, insertPositionRangeExcess)
{
  using Vec = typename TypeParam::Vec;
  static constexpr auto s = TypeParam::s;

  Vec vector1;
  EXPECT_TRUE(vector1.resize(s));

  this->checkResultError(vector1.insert(vector1.begin(), this->sampleData.begin(), this->sampleData.end()),
                         StaticVectorError::full);
  this->checkResultError(vector1.insert(vector1.begin() + 2, this->sampleData.begin(), this->sampleData.end()),
                         StaticVectorError::full);
  this->checkResultError(vector1.insert(vector1.begin() + 4, this->sampleData.begin(), this->sampleData.end()),
                         StaticVectorError::full);
  this->checkResultError(vector1.insert(vector1.begin() + 4, this->sampleData.begin(), this->sampleData.end()),
                         StaticVectorError::full);
}

/// @test
/// Inserts an initializer list at begin, and refuses one at a position outside the vector or when the
/// elements do not fit.
/// @requirements(SEN-355)
TYPED_TEST(VectorTestTemplate, insertInitList)
{
  using Vec = typename TypeParam::Vec;
  using T = typename TypeParam::ValueType;

  // valid
  {
    Vec vector1;
    EXPECT_TRUE(vector1.insert(vector1.begin(), {T {}, T {}, T {}}));
    EXPECT_EQ(vector1.size(), 3U);
  }

  // bad position 1
  {
    Vec vector1;
    EXPECT_FALSE(vector1.insert(vector1.begin() - 1, {T {}, T {}, T {}}));
  }

  // bad position 2
  {
    Vec vector1;
    EXPECT_FALSE(vector1.insert(vector1.end() + 1, {T {}, T {}, T {}}));
  }

  // full
  {
    Vec vector1;
    this->populate(vector1);
    EXPECT_FALSE(vector1.insert(vector1.end(), {T {}, T {}, T {}}));
  }
}

/// @test
/// Reports badRange for move_insert at a position outside the vector or with a reversed source range,
/// and full when the source does not fit.
/// @requirements(SEN-355)
TYPED_TEST(VectorTestTemplate, moveInsertError)
{
  using Vec = typename TypeParam::Vec;

  Vec vector1;

  {
    Vec vector2;
    this->populateShorter(vector2);

    this->checkResultError(vector1.move_insert(vector1.begin() - 1, vector2.begin(), vector2.end()),
                           StaticVectorError::badRange);
  }

  {
    Vec vector2;
    this->populateShorter(vector2);

    this->checkResultError(vector1.move_insert(vector1.end() + 1, vector2.begin(), vector2.end()),
                           StaticVectorError::badRange);
  }

  {
    Vec vector2;
    this->populateShorter(vector2);

    this->checkResultError(vector1.move_insert(vector1.end(), vector2.end(), vector2.begin()),
                           StaticVectorError::badRange);
  }

  {
    Vec source;
    this->populate(source);

    Vec target;
    this->populate(target);

    this->checkResultError(target.move_insert(target.end(), source.begin(), source.end()), StaticVectorError::full);
  }
}

/// @test
/// Erases a single element, matching std::vector, and reports badRange for a position outside the
/// vector.
/// @requirements(SEN-355)
TYPED_TEST(VectorTestTemplate, eraseSingle)
{
  using Vec = typename TypeParam::Vec;
  using T = typename TypeParam::ValueType;

  {
    std::vector<T> compareData(this->sampleData.begin(), this->sampleData.end());
    Vec data;
    EXPECT_TRUE(data.assign(this->sampleData.begin(), this->sampleData.end()));

    compareData.erase(compareData.begin() + 2);
    EXPECT_TRUE(data.erase(data.begin() + 2));
    EXPECT_EQ(compareData.size(), data.size());
    EXPECT_TRUE(std::equal(data.begin(), data.end(), compareData.begin()));
  }

  {
    Vec data;
    this->populate(data);

    this->checkResultError(data.erase(data.begin() - 1), StaticVectorError::badRange);
  }

  {
    Vec data;
    this->populate(data);

    this->checkResultError(data.erase(data.end() + 1), StaticVectorError::badRange);
  }
}

/// @test
/// Erases an iterator range mid-vector, matching a std::vector after the same erase.
/// @requirements(SEN-355)
TYPED_TEST(VectorTestTemplate, eraseRange)
{
  using Vec = typename TypeParam::Vec;
  using T = typename TypeParam::ValueType;
  static constexpr auto s = TypeParam::s;

  static_assert(s >= 6, "this test requires a minimum of elements");

  std::vector<T> compareData(this->sampleData.begin(), this->sampleData.end());
  Vec data;
  EXPECT_TRUE(data.assign(this->sampleData.begin(), this->sampleData.end()));

  compareData.erase(compareData.begin() + 2, compareData.begin() + 4);
  EXPECT_TRUE(data.erase(data.begin() + 2, data.begin() + 4));
  EXPECT_EQ(compareData.size(), data.size());
  EXPECT_TRUE(std::equal(data.begin(), data.end(), compareData.begin()));
}

/// @test
/// Reports badRange for a reversed erase range, and erasing an empty range changes nothing.
/// @requirements(SEN-355)
TYPED_TEST(VectorTestTemplate, eraseRange2)
{
  using Vec = typename TypeParam::Vec;

  {
    Vec data;
    this->populate(data);

    this->checkResultError(data.erase(data.begin() + 4, data.begin() + 2), StaticVectorError::badRange);
  }

  {
    Vec data1;
    this->populate(data1);

    Vec data2;
    this->populate(data2);

    EXPECT_TRUE(std::equal(data1.begin(), data1.end(), data2.begin()));
    EXPECT_TRUE(data1.erase(data1.begin() + 4, data1.begin() + 4));
    EXPECT_TRUE(std::equal(data1.begin(), data1.end(), data2.begin()));
  }
}

/// @test
/// Empties a populated vector through clear.
/// @requirements(SEN-355)
TYPED_TEST(VectorTestTemplate, clear)
{
  using Vec = typename TypeParam::Vec;
  using T = typename TypeParam::ValueType;

  std::vector<T> compareData(this->sampleData.begin(), this->sampleData.end());
  Vec data;
  EXPECT_TRUE(data.assign(this->sampleData.begin(), this->sampleData.end()));

  EXPECT_FALSE(data.empty());
  EXPECT_FALSE(compareData.empty());

  data.clear();
  compareData.clear();

  EXPECT_TRUE(data.empty());
  EXPECT_TRUE(compareData.empty());
}

/// @test
/// Visits the assigned elements in order from begin to end.
/// @requirements(SEN-355)
TYPED_TEST(VectorTestTemplate, iterator)
{
  using Vec = typename TypeParam::Vec;
  using T = typename TypeParam::ValueType;

  std::vector<T> compareData(this->sampleData.begin(), this->sampleData.end());
  Vec data;
  EXPECT_TRUE(data.assign(this->sampleData.begin(), this->sampleData.end()));
  EXPECT_TRUE(std::equal(data.begin(), data.end(), compareData.begin()));
}

/// @test
/// Visits the assigned elements in order from cbegin to cend.
/// @requirements(SEN-355)
TYPED_TEST(VectorTestTemplate, constIterator)
{
  using Vec = typename TypeParam::Vec;
  using T = typename TypeParam::ValueType;

  std::vector<T> compareData(this->sampleData.begin(), this->sampleData.end());
  Vec data;
  EXPECT_TRUE(data.assign(this->sampleData.begin(), this->sampleData.end()));
  EXPECT_TRUE(std::equal(data.cbegin(), data.cend(), compareData.cbegin()));
}

/// @test
/// Visits the assigned elements in reverse order from rbegin to rend.
/// @requirements(SEN-355)
TYPED_TEST(VectorTestTemplate, reverseIterator)
{
  using Vec = typename TypeParam::Vec;
  using T = typename TypeParam::ValueType;

  std::vector<T> compareData(this->sampleData.begin(), this->sampleData.end());
  Vec data;
  EXPECT_TRUE(data.assign(this->sampleData.begin(), this->sampleData.end()));
  EXPECT_TRUE(std::equal(data.rbegin(), data.rend(), compareData.rbegin()));
}

/// @test
/// Visits the assigned elements in reverse order from crbegin to crend.
/// @requirements(SEN-355)
TYPED_TEST(VectorTestTemplate, constReverseIterator)
{
  using Vec = typename TypeParam::Vec;
  using T = typename TypeParam::ValueType;

  std::vector<T> compareData(this->sampleData.begin(), this->sampleData.end());
  Vec data;
  EXPECT_TRUE(data.assign(this->sampleData.begin(), this->sampleData.end()));

  EXPECT_TRUE(std::equal(std::crbegin(data), std::crend(data), std::crbegin(compareData)));
}

/// @test
/// Compares equal for identical contents, and unequal for different contents or a shorter vector.
/// @requirements(SEN-355)
TYPED_TEST(VectorTestTemplate, equal)
{
  using Vec = typename TypeParam::Vec;

  Vec vector1;
  Vec vector2;

  this->populate(vector1);
  this->populate(vector2);

  EXPECT_EQ(vector1, vector2);

  Vec vectorDifferent;
  this->populateDifferent(vectorDifferent);

  EXPECT_NE(vector1, vectorDifferent);

  Vec vectorShorter;
  this->populateShorter(vectorShorter);

  EXPECT_NE(vector1, vectorShorter);
}

/// @test
/// Yields false from operator!= for identical contents, and true for different contents or a shorter
/// vector.
/// @requirements(SEN-355)
TYPED_TEST(VectorTestTemplate, notEqual)
{
  using Vec = typename TypeParam::Vec;

  Vec vector1;
  Vec vector2;

  this->populate(vector1);
  this->populate(vector2);

  EXPECT_FALSE(vector1 != vector2);

  Vec vectorDifferent;
  this->populateDifferent(vectorDifferent);

  EXPECT_NE(vector1, vectorDifferent);

  Vec vectorShorter;
  this->populateShorter(vectorShorter);

  EXPECT_NE(vector1, vectorShorter);
}

/// @test
/// Copy-assigns empty and populated vectors into equality, move-assigns the full size into empty or
/// shorter targets, and survives move self-assignment with contents intact.
/// @requirements(SEN-355)
TYPED_TEST(VectorTestTemplate, assignOperator)
{
  using Vec = typename TypeParam::Vec;

  {
    Vec vector1;
    Vec vector2;

    EXPECT_TRUE(vector1 == vector2);
    vector2 = vector1;
    EXPECT_TRUE(vector1 == vector2);
  }

  {
    Vec vector1;
    Vec vector2;

    this->populate(vector1);
    this->populateDifferent(vector2);

    EXPECT_NE(vector1, vector2);
    vector2 = vector1;
    EXPECT_EQ(vector1, vector2);
  }

  {
    Vec vector1;
    this->populate(vector1);

    const auto vector1Size = vector1.size();

    Vec vector2;
    EXPECT_TRUE(vector2.empty());

    vector2 = std::move(vector1);
    EXPECT_FALSE(vector2.empty());
    EXPECT_EQ(vector2.size(), vector1Size);
  }

  {
    Vec vector1;
    Vec vector2;

    this->populate(vector1);
    this->populateShorter(vector2);

    auto normalSize = vector1.size();
    auto shortSize = vector2.size();
    EXPECT_GT(normalSize, shortSize);

    EXPECT_NE(vector1, vector2);

    vector2 = std::move(vector1);
    EXPECT_EQ(vector2.size(), normalSize);
  }

  {
    Vec vector1;
    Vec vector2;
    this->populate(vector1);
    this->populate(vector2);

    EXPECT_EQ(vector1, vector2);

    vector1 = std::move(vector1);

    EXPECT_EQ(vector1, vector2);
  }
}

/// @test
/// Exchanges the contents of two vectors through swap, each afterwards equal to the other's original.
/// @requirements(SEN-355)
TYPED_TEST(VectorTestTemplate, swap)
{
  using Vec = typename TypeParam::Vec;

  Vec vector1;
  this->populate(vector1);

  Vec vector1Untouched;
  this->populate(vector1Untouched);

  Vec vector2;
  this->populateDifferent(vector2);

  Vec vector2Untouched;
  this->populateDifferent(vector2Untouched);

  EXPECT_EQ(vector1, vector1Untouched);
  EXPECT_EQ(vector2, vector2Untouched);

  vector1.swap(vector2);

  EXPECT_EQ(vector2, vector1Untouched);
  EXPECT_EQ(vector1, vector2Untouched);
}

/// @test
/// Move-constructs from a full vector, the new vector full and the source left empty.
/// @requirements(SEN-355)
TYPED_TEST(VectorTestTemplate, copyMoveConstructor)
{
  using Vec = typename TypeParam::Vec;

  Vec vector;
  this->populate(vector);

  EXPECT_TRUE(vector.full());

  auto anotherVector(std::move(vector));
  EXPECT_TRUE(vector.empty());
  EXPECT_TRUE(anotherVector.full());
}

/// @test
/// Constructs with an element count, default-filling that many elements, and a count above capacity
/// trips the failed-check handler.
/// @requirements(SEN-355)
TYPED_TEST(VectorTestTemplate, makeN)
{
  using Vec = typename TypeParam::Vec;
  using T = typename TypeParam::ValueType;
  static constexpr auto s = TypeParam::s;

  {
    constexpr std::size_t size = s / 2;
    auto vector = Vec(size);
    EXPECT_EQ(vector.size(), size);

    for (const auto& elem: vector)
    {
      EXPECT_EQ(elem, T {});
    }
  }

  {
    checkTermination([]() { std::ignore = Vec(TypeParam::s + 1); });
  }
}

/// @test
/// Constructs with a count and a value, filling that many elements with the value, and a count above
/// capacity trips the failed-check handler.
/// @requirements(SEN-355)
TYPED_TEST(VectorTestTemplate, makeNValue)
{
  using Vec = typename TypeParam::Vec;
  static constexpr auto s = TypeParam::s;

  {
    constexpr std::size_t size = s / 2;
    auto vector = Vec(size, this->testValue);
    EXPECT_EQ(vector.size(), size);

    for (const auto& elem: vector)
    {
      EXPECT_EQ(elem, this->testValue);
    }
  }

  {
    checkTermination(
      [&]()
      {
        constexpr std::size_t size = s + 1;
        std::ignore = Vec(size, this->testValue);
      });
  }
}

/// @test
/// Constructs from an initializer list below and at exact capacity, and a list longer than the capacity
/// trips the failed-check handler.
/// @requirements(SEN-355)
TYPED_TEST(VectorTestTemplate, makeList)
{
  using Vec = typename TypeParam::Vec;
  using T = typename TypeParam::ValueType;

  // valid list
  {
    auto vec = Vec({T {}, T {}, T {}});
    EXPECT_EQ(vec.size(), 3U);
  }

  // valid same capacity
  {
    auto vec = StaticVector<T, 3>({T {}, T {}, T {}});
    EXPECT_EQ(vec.size(), 3U);
    EXPECT_EQ(vec.capacity(), 3U);
  }

  // not enough capacity
  {
    checkTermination([]() { std::ignore = StaticVector<T, 2>({T {}, T {}, T {}}); });
  }
}

/// @test
/// Constructs from an iterator pair, copying the range, and a reversed pair trips the failed-check
/// handler.
/// @requirements(SEN-355)
TYPED_TEST(VectorTestTemplate, makeIterators)
{
  using Vec = typename TypeParam::Vec;

  {
    auto vec = Vec(this->sampleData.begin(), this->sampleData.end());
    EXPECT_TRUE(std::equal(vec.begin(), vec.end(), this->sampleData.begin()));
  }

  {
    checkTermination([&]() { std::ignore = Vec(this->sampleData.end(), this->sampleData.begin()); });
  }
}

/// @test
/// Inserts a five-element initializer list into an empty vector, producing exactly those elements.
/// @requirements(SEN-355)
TEST_F(BasicVectorTest, initializer_list)
{
  EXPECT_TRUE(vector.insert(vector.begin(), {'a', 'b', 'c', 'd', 'e'}));
  EXPECT_EQ(vector.size(), 5U);
  EXPECT_EQ(vector, comparisonData);
}

/// @test
/// Inserts an initializer list mid-vector, returning an iterator to the first inserted element and
/// producing the merged sequence.
/// @requirements(SEN-355)
TEST_F(BasicVectorTest, initializer_list_2)
{
  EXPECT_TRUE(vector.insert(vector.begin(), {'a', 'd', 'e'}));
  EXPECT_EQ(vector.size(), 3U);
  auto result = vector.insert(vector.begin() + 1, {'b', 'c'});
  EXPECT_TRUE(result);
  EXPECT_EQ(*result.getValue(), 'b');
  EXPECT_EQ(vector, comparisonData);
}

/// @test
/// Inserts an initializer list at begin ahead of existing elements, returning an iterator to the first
/// inserted element and producing the merged sequence.
/// @requirements(SEN-355)
TEST_F(BasicVectorTest, initializer_list_3)
{
  EXPECT_TRUE(vector.insert(vector.begin(), {'d', 'e'}));
  EXPECT_EQ(vector.size(), 2U);
  auto result = vector.insert(vector.begin(), {'a', 'b', 'c'});
  EXPECT_TRUE(result);
  EXPECT_EQ(*result.getValue(), 'a');
  EXPECT_EQ(vector, comparisonData);
}

/// @test
/// Inserts single elements by const reference at begin, end, and interior positions, building the
/// expected sequence step by step.
/// @requirements(SEN-355)
TEST_F(BasicVectorTest, position_and_const_ref)
{
  char val = 'a';
  EXPECT_TRUE(vector.insert(vector.begin(), val));
  EXPECT_EQ(vector, Vec({'a'}));

  val = 'd';
  EXPECT_TRUE(vector.insert(vector.end(), val));
  EXPECT_EQ(vector, Vec({'a', 'd'}));

  val = 'b';
  EXPECT_TRUE(vector.insert(vector.begin() + 1, val));
  EXPECT_EQ(vector, Vec({'a', 'b', 'd'}));

  val = 'c';
  EXPECT_TRUE(vector.insert(vector.end() - 1, val));
  EXPECT_EQ(vector, Vec({'a', 'b', 'c', 'd'}));

  val = 'e';
  EXPECT_TRUE(vector.insert(vector.begin() + 4, val));  // ['a', 'b', 'c', 'd', 'e']
  EXPECT_EQ(vector, Vec({'a', 'b', 'c', 'd', 'e'}));
  EXPECT_EQ(vector, comparisonData);
}

/// @test
/// Inserts counted runs of one value at chosen positions, returns the given position unchanged for a
/// count of zero, and refuses the insert once the vector is full.
/// @requirements(SEN-355)
TEST_F(BasicVectorTest, position_and_n_values)
{
  EXPECT_TRUE(vector.insert(vector.begin(), 5, 'a'));
  EXPECT_EQ(vector.size(), 5);
  EXPECT_EQ(vector, Vec({'a', 'a', 'a', 'a', 'a'}));

  EXPECT_TRUE(vector.insert(vector.begin() + 1, 1, 'b'));
  EXPECT_EQ(vector, Vec({'a', 'b', 'a', 'a', 'a', 'a'}));

  EXPECT_TRUE(vector.insert(vector.begin() + 2, 1, 'c'));
  EXPECT_EQ(vector, Vec({'a', 'b', 'c', 'a', 'a', 'a', 'a'}));

  EXPECT_TRUE(vector.insert(vector.begin() + 3, 1, 'd'));
  EXPECT_EQ(vector, Vec({'a', 'b', 'c', 'd', 'a', 'a', 'a', 'a'}));

  EXPECT_TRUE(vector.insert(vector.begin() + 4, 1, 'e'));
  EXPECT_EQ(vector, Vec({'a', 'b', 'c', 'd', 'e', 'a', 'a', 'a', 'a'}));

  EXPECT_TRUE(vector.pop_back());
  EXPECT_EQ(vector, Vec({'a', 'b', 'c', 'd', 'e', 'a', 'a', 'a'}));

  EXPECT_TRUE(vector.pop_back());
  EXPECT_EQ(vector, Vec({'a', 'b', 'c', 'd', 'e', 'a', 'a'}));

  EXPECT_TRUE(vector.pop_back());
  EXPECT_EQ(vector, Vec({'a', 'b', 'c', 'd', 'e', 'a'}));

  EXPECT_TRUE(vector.pop_back());  // ['a', 'b', 'c', 'd', 'e']
  EXPECT_EQ(vector, comparisonData);

  // if n is 0, no changes and pos is returned
  {
    auto pos = vector.begin() + 4;
    auto result = vector.insert(pos, 0, 'a');
    EXPECT_TRUE(result);
    EXPECT_EQ(result.getValue(), pos);
  }

  EXPECT_TRUE(vector.insert(vector.begin() + 2, 5, 'X'));
  EXPECT_EQ(vector, Vec({'a', 'b', 'X', 'X', 'X', 'X', 'X', 'c', 'd', 'e'}));
  EXPECT_TRUE(vector.full());

  EXPECT_FALSE(vector.insert(vector.begin(), 1, 'N'));
}

/// @test
/// Inserts initializer lists at begin, end, and interior positions, accumulating the expected
/// sequence, and refuses the insert that would exceed capacity.
/// @requirements(SEN-355)
TEST_F(BasicVectorTest, initializer_list_make)
{
  EXPECT_TRUE(vector.insert(vector.begin(), {'a', 'b', 'c'}));
  EXPECT_EQ(vector, Vec({'a', 'b', 'c'}));

  EXPECT_TRUE(vector.insert(vector.begin() + 1, {'a', 'b'}));
  EXPECT_EQ(vector, Vec({'a', 'a', 'b', 'b', 'c'}));

  EXPECT_TRUE(vector.insert(vector.end(), {'c'}));
  EXPECT_EQ(vector, Vec({'a', 'a', 'b', 'b', 'c', 'c'}));

  EXPECT_TRUE(vector.insert(vector.begin(), {'a'}));
  EXPECT_EQ(vector, Vec({'a', 'a', 'a', 'b', 'b', 'c', 'c'}));

  EXPECT_TRUE(vector.insert(vector.end(), {'c'}));
  EXPECT_EQ(vector, Vec({'a', 'a', 'a', 'b', 'b', 'c', 'c', 'c'}));

  EXPECT_TRUE(vector.insert(vector.begin() + 3, {'a', 'b'}));
  EXPECT_EQ(vector, Vec({'a', 'a', 'a', 'a', 'b', 'b', 'b', 'c', 'c', 'c'}));

  EXPECT_FALSE(vector.insert(vector.begin() + 3, {'a', 'b'}));
}

/// @test
/// Inserts iterator ranges from another vector at begin, end, and interior positions, and refuses the
/// insert that would exceed capacity.
/// @requirements(SEN-355)
TEST_F(BasicVectorTest, range)
{
  const Vec reference = Vec({'a', 'b', 'c'});
  EXPECT_TRUE(vector.insert(vector.begin(), reference.begin(), reference.end()));
  EXPECT_EQ(vector, reference);

  EXPECT_TRUE(vector.insert(vector.begin() + 1, reference.begin(), reference.begin() + 2));
  EXPECT_EQ(vector, Vec({'a', 'a', 'b', 'b', 'c'}));

  EXPECT_TRUE(vector.insert(vector.end(), reference.end() - 1, reference.end()));
  EXPECT_EQ(vector, Vec({'a', 'a', 'b', 'b', 'c', 'c'}));

  EXPECT_TRUE(vector.insert(vector.begin(), reference.begin(), reference.begin() + 1));
  EXPECT_EQ(vector, Vec({'a', 'a', 'a', 'b', 'b', 'c', 'c'}));

  EXPECT_TRUE(vector.insert(vector.end(), reference.end() - 1, reference.end()));
  EXPECT_EQ(vector, Vec({'a', 'a', 'a', 'b', 'b', 'c', 'c', 'c'}));

  EXPECT_TRUE(vector.insert(vector.begin() + 3, reference.begin(), reference.begin() + 2));
  EXPECT_EQ(vector, Vec({'a', 'a', 'a', 'a', 'b', 'b', 'b', 'c', 'c', 'c'}));

  EXPECT_FALSE(vector.insert(vector.begin() + 3, reference.begin(), reference.begin() + 2));
}

/// @test
/// Move-inserts iterator ranges at begin, end, and interior positions, accumulating the expected
/// sequence, and refuses the insert that would exceed capacity.
/// @requirements(SEN-355)
TEST_F(BasicVectorTest, move_insert)
{
  {
    const Vec reference = Vec({'a', 'b', 'c'});
    EXPECT_TRUE(vector.move_insert(vector.begin(), reference.begin(), reference.end()));
    EXPECT_EQ(vector, reference);
  }

  {
    const Vec reference = Vec({'a', 'b', 'c'});
    EXPECT_TRUE(vector.move_insert(vector.begin() + 1, reference.begin(), reference.begin() + 2));
    EXPECT_EQ(vector, Vec({'a', 'a', 'b', 'b', 'c'}));
  }

  {
    const Vec reference = Vec({'a', 'b', 'c'});
    EXPECT_TRUE(vector.move_insert(vector.end(), reference.end() - 1, reference.end()));
    EXPECT_EQ(vector, Vec({'a', 'a', 'b', 'b', 'c', 'c'}));
  }

  {
    const Vec reference = Vec({'a', 'b', 'c'});
    EXPECT_TRUE(vector.move_insert(vector.begin(), reference.begin(), reference.begin() + 1));
    EXPECT_EQ(vector, Vec({'a', 'a', 'a', 'b', 'b', 'c', 'c'}));
  }

  {
    const Vec reference = Vec({'a', 'b', 'c'});
    EXPECT_TRUE(vector.move_insert(vector.end(), reference.end() - 1, reference.end()));
    EXPECT_EQ(vector, Vec({'a', 'a', 'a', 'b', 'b', 'c', 'c', 'c'}));
  }

  {
    const Vec reference = Vec({'a', 'b', 'c'});
    EXPECT_TRUE(vector.move_insert(vector.begin() + 3, reference.begin(), reference.begin() + 2));
    EXPECT_EQ(vector, Vec({'a', 'a', 'a', 'a', 'b', 'b', 'b', 'c', 'c', 'c'}));
  }

  {
    const Vec reference = Vec({'a', 'b', 'c'});
    EXPECT_FALSE(vector.move_insert(vector.begin() + 3, reference.begin(), reference.begin() + 2));
  }
}

/// @test
/// Erases the full range from begin to end, leaving the vector empty.
/// @requirements(SEN-355)
TEST_F(BasicVectorTest, erase_all)
{
  auto vector = Vec({'a', 'b', 'c', 'd', 'e'});
  EXPECT_FALSE(vector.empty());
  EXPECT_TRUE(vector.erase(vector.begin(), vector.end()));
  EXPECT_TRUE(vector.empty());
}

/// @test
/// Erases the two leading elements, keeping the remaining tail in order.
/// @requirements(SEN-355)
TEST_F(BasicVectorTest, erase_start_of_range)
{
  auto vector = Vec({'a', 'b', 'c', 'd', 'e'});
  EXPECT_TRUE(vector.erase(vector.begin(), vector.begin() + 2));
  EXPECT_EQ(vector, Vec({'c', 'd', 'e'}));
}

/// @test
/// Erases the two trailing elements, keeping the leading elements in order.
/// @requirements(SEN-355)
TEST_F(BasicVectorTest, erase_end_of_range)
{
  auto vector = Vec({'a', 'b', 'c', 'd', 'e'});
  EXPECT_TRUE(vector.erase(vector.end() - 2, vector.end()));
  EXPECT_EQ(vector, Vec({'a', 'b', 'c'}));
}

/// @test
/// Erases the interior elements, keeping only the first and the last.
/// @requirements(SEN-355)
TEST_F(BasicVectorTest, erase_mid_of_range)
{
  auto vector = Vec({'a', 'b', 'c', 'd', 'e'});
  EXPECT_TRUE(vector.erase(vector.begin() + 1, vector.end() - 1));
  EXPECT_EQ(vector, Vec({'a', 'e'}));
}

/// @test
/// Refuses emplace at positions outside the vector, inserts at begin, end, and interior positions
/// building the expected sequence, and refuses one more emplace once full.
/// @requirements(SEN-355)
TEST_F(BasicVectorTest, emplace_position)
{
  EXPECT_FALSE(vector.emplace(vector.begin() - 1, 'a'));
  EXPECT_FALSE(vector.emplace(vector.end() + 1, 'a'));

  EXPECT_TRUE(vector.empty());

  EXPECT_TRUE(vector.emplace(vector.begin(), 'a'));
  EXPECT_EQ(vector, Vec({'a'}));

  EXPECT_TRUE(vector.emplace(vector.end(), 'd'));
  EXPECT_EQ(vector, Vec({'a', 'd'}));

  EXPECT_TRUE(vector.emplace(vector.begin() + 1, 'b'));
  EXPECT_EQ(vector, Vec({'a', 'b', 'd'}));

  EXPECT_TRUE(vector.emplace(vector.end() - 1, 'c'));
  EXPECT_EQ(vector, Vec({'a', 'b', 'c', 'd'}));

  EXPECT_TRUE(vector.emplace(vector.begin() + 4, 'e'));
  EXPECT_EQ(vector, Vec({'a', 'b', 'c', 'd', 'e'}));

  while (!vector.full())
  {
    EXPECT_TRUE(vector.emplace(vector.end(), 'X'));
  }

  EXPECT_FALSE(vector.emplace(vector.end(), 'Y'));
}

/// @test
/// Emplaces mid-vector, shifting the trailing elements one slot to the right.
/// @requirements(SEN-355)
TEST_F(BasicVectorTest, emplace_corner_case)
{
  Vec v = Vec({1, 2, 3});
  EXPECT_TRUE(v.emplace(v.begin() + 1, 100));
  EXPECT_EQ(v, Vec({1, 100, 2, 3}));
}

/// @test
/// Appends through emplace_back up to capacity, producing the appended sequence, and refuses one more
/// append once full.
/// @requirements(SEN-355)
TEST_F(BasicVectorTest, emplace_back)
{
  for (size_t i = 0; i < vector.capacity(); ++i)
  {
    EXPECT_TRUE(vector.emplace_back('a' + char(i)));
  }

  EXPECT_TRUE(vector.full());
  EXPECT_EQ(vector, Vec({'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j'}));
  EXPECT_FALSE(vector.emplace_back('k'));
}

/// @test
/// Resizes default-filling new elements, accepts same-size and shrinking resizes, and reports the full
/// error for a size beyond capacity.
/// @requirements(SEN-355)
TYPED_TEST(VectorTestTemplate, resizeDefault)
{
  using Vec = typename TypeParam::Vec;
  using T = typename TypeParam::ValueType;
  static constexpr auto s = TypeParam::s;

  Vec vector;
  EXPECT_TRUE(vector.resize(s / 2));
  EXPECT_EQ(vector.size(), s / 2);
  for (const auto& item: vector)
  {
    EXPECT_EQ(item, T {});
  }

  EXPECT_TRUE(vector.resize(s / 2));
  EXPECT_TRUE(vector.resize(s / 4));
  EXPECT_EQ(vector.size(), s / 4);
  EXPECT_TRUE(vector.resize(vector.size()));
  this->checkResultError(vector.resize(s + 1), StaticVectorError::full);
}

/// @test
/// Reports badRange for a reversed erase range, empties the vector when erasing the full range, keeps
/// it intact for an empty range, and shrinks by one for a single-element range.
/// @requirements(SEN-355)
TYPED_TEST(VectorTestTemplate, eraseRangeComprehensive)
{
  using Vec = typename TypeParam::Vec;
  Vec vector;
  this->populate(vector);

  this->checkResultError(vector.erase(vector.end(), vector.begin()), StaticVectorError::badRange);

  auto it = vector.erase(vector.begin(), vector.end());
  EXPECT_TRUE(it);
  EXPECT_TRUE(vector.empty());

  this->populate(vector);
  EXPECT_TRUE(vector.erase(vector.begin() + 1, vector.begin() + 1));
  EXPECT_EQ(vector.size(), TypeParam::s);

  EXPECT_TRUE(vector.erase(vector.begin(), vector.begin() + 1));
  EXPECT_EQ(vector.size(), TypeParam::s - 1);
}

/// @test
/// Propagates badRange out of insert for a position before begin and for an invalid source iterator
/// range.
/// @requirements(SEN-355)
TYPED_TEST(VectorTestTemplate, internalErrorPropagation)
{
  using Vec = typename TypeParam::Vec;
  using T = typename TypeParam::ValueType;
  Vec vector;

  this->checkResultError(vector.insert(vector.begin() - 1, T {}), StaticVectorError::badRange);
  this->checkResultError(vector.insert(vector.begin(), vector.end() + 1, vector.begin()), StaticVectorError::badRange);
}

/// @test
/// Reports the full error on a full vector from every append overload: push_back by moved value, by
/// lvalue reference, and with no argument, and emplace_back.
/// @requirements(SEN-355)
TYPED_TEST(VectorTestTemplate, fullCapacityErrors)
{
  using Vec = typename TypeParam::Vec;
  using T = typename TypeParam::ValueType;
  Vec vector;
  this->populate(vector);

  this->checkResultError(vector.push_back(T {}), StaticVectorError::full);
  T val {};
  this->checkResultError(vector.push_back(val), StaticVectorError::full);
  this->checkResultError(vector.emplace_back(), StaticVectorError::full);
  this->checkResultError(vector.push_back(), StaticVectorError::full);
}

/// @test
/// Emplaces at end on a non-full vector, succeeding and placing the value as the new back.
/// @requirements(SEN-355)
TEST(StaticVectorLogicTest, EmplaceAtEnd)
{
  StaticVector<int, 5> v;
  v.push_back(1);
  auto res = v.emplace(v.end(), 42);

  EXPECT_TRUE(res.isOk());
  EXPECT_EQ(v.back(), 42);
}

/// @test
/// Erases the empty range at end, succeeding and leaving the size unchanged.
/// @requirements(SEN-355)
TEST(StaticVectorLogicTest, EraseEmptyRangeAtEnd)
{
  StaticVector<int, 5> v = {1, 2, 3};
  auto res = v.erase(v.end(), v.end());

  EXPECT_TRUE(res.isOk());
  EXPECT_EQ(v.size(), 3);
}

/// @test
/// Emplaces at begin a reference to the vector's own first element, both slots holding the original
/// value afterwards.
/// @requirements(SEN-355)
TEST(StaticVectorLogicTest, EmplaceSelfReference)
{
  StaticVector<int, 5> v;
  v.push_back(10);
  std::ignore = v.emplace(v.begin(), v[0]);

  EXPECT_EQ(v[0], 10);
  EXPECT_EQ(v[1], 10);
}

/// @test
/// Walks the error paths on StaticVector of std::string and of std::variant elements: appends and
/// inserts on a full vector fail, positions outside the vector and reversed ranges report errors,
/// pop_back on an empty vector fails, and resize beyond capacity fails while valid resizes succeed.
/// @requirements(SEN-355)
TEST(StaticVectorCoverageExtra, ComplexInstantiation)
{
  // std::string instantiation check
  {
    StaticVector<std::string, 2> v;
    EXPECT_TRUE(v.push_back("A"));
    EXPECT_TRUE(v.emplace_back("B"));
    EXPECT_TRUE(v.full());

    std::string s = "C";
    EXPECT_TRUE(v.push_back(s).isError());
    EXPECT_TRUE(v.push_back(std::move(s)).isError());
    EXPECT_TRUE(v.emplace_back("C").isError());

    std::vector<std::string> arr = {"X", "Y"};
    EXPECT_TRUE(v.move_insert(v.begin(), arr.begin(), arr.end()).isError());

    EXPECT_TRUE(v.emplace(v.end() + 1, "Z").isError());
    EXPECT_TRUE(v.erase(v.end(), v.begin()).isError());
    EXPECT_TRUE(v.erase(v.end() + 1).isError());

    v.clear();
    EXPECT_TRUE(v.empty());
    EXPECT_TRUE(v.pop_back().isError());

    EXPECT_TRUE(v.resize(0).isOk());
    EXPECT_TRUE(v.resize(3).isError());
    EXPECT_TRUE(v.resize(2).isOk());
    EXPECT_TRUE(v.resize(1).isOk());
    EXPECT_TRUE(v.resize(1).isOk());
  }

  // std::variant instantiation check
  {
    using TestVariant = std::variant<int, double, std::string>;
    StaticVector<TestVariant, 2> v;

    TestVariant val1 = 10;
    TestVariant val2 = 20.5;
    EXPECT_TRUE(v.push_back(val1));
    EXPECT_TRUE(v.push_back(std::move(val2)));
    EXPECT_TRUE(v.full());

    TestVariant val3 = "full";
    EXPECT_TRUE(v.push_back(val3).isError());
    EXPECT_TRUE(v.push_back(TestVariant {30}).isError());
    EXPECT_TRUE(v.emplace_back(40).isError());

    std::vector<TestVariant> arr = {1, 2};
    EXPECT_TRUE(v.move_insert(v.begin(), arr.begin(), arr.end()).isError());

    EXPECT_TRUE(v.emplace(v.end() + 1, 50).isError());
    EXPECT_TRUE(v.erase(v.end(), v.begin()).isError());
    EXPECT_TRUE(v.erase(v.end() + 1).isError());

    v.clear();
    EXPECT_TRUE(v.empty());
    EXPECT_TRUE(v.pop_back().isError());

    EXPECT_TRUE(v.resize(0).isOk());
    EXPECT_TRUE(v.resize(3).isError());
    EXPECT_TRUE(v.resize(2).isOk());
    EXPECT_TRUE(v.resize(1).isOk());
    EXPECT_TRUE(v.resize(1).isOk());
  }
}

/// @test
/// Two equal vectors are neither less than nor greater than one another, and the ordering
/// operators agree with the contents.
/// @requirements(SEN-355)
TYPED_TEST(VectorTestTemplate, orderingOperators)
{
  using Vec = typename TypeParam::Vec;

  // Equal vectors are neither smaller nor greater than one another.
  {
    Vec vector1;
    Vec vector2;
    this->populate(vector1);
    this->populate(vector2);

    EXPECT_FALSE(vector1 < vector2);
    EXPECT_FALSE(vector1 > vector2);
    EXPECT_FALSE(vector2 < vector1);
    EXPECT_FALSE(vector2 > vector1);
  }

  // The first element that differs decides, and the two operators agree on it.
  {
    Vec sample;
    Vec different;
    this->populate(sample);
    this->populateDifferent(different);

    // Parenthesised: unparenthesised < and > inside a macro argument list read as template
    // brackets, and clang-format rewrites them into something that does not compile.
    EXPECT_EQ((sample < different), (different > sample));
    EXPECT_EQ((different < sample), (sample > different));
    EXPECT_NE((sample < different), (different < sample));
  }

  // A prefix is smaller than what it is a prefix of, whatever the elements are.
  {
    Vec longer;
    Vec shorter;
    this->populate(longer);
    this->populate(shorter);
    shorter.pop_back();

    EXPECT_TRUE(shorter < longer);
    EXPECT_TRUE(longer > shorter);
    EXPECT_FALSE(longer < shorter);
    EXPECT_FALSE(shorter > longer);
  }

  // An empty vector is smaller than any non-empty one.
  {
    Vec empty;
    Vec populated;
    this->populate(populated);

    EXPECT_TRUE(empty < populated);
    EXPECT_TRUE(populated > empty);
    EXPECT_FALSE(empty > populated);
  }
}

// NOLINTEND(cppcoreguidelines-pro-bounds-pointer-arithmetic)
