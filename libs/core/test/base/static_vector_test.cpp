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
/// Checks resizing with 0, half and full capacity.
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
/// Checks default construction of a static vector
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
/// Checks copy constructor of a static vector
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
/// Checks move constructor of a static vector
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
/// Checks assignation of a static vector
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
/// Checks failure when trying to assign the content of a bigger static vector
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
/// Checks failure when trying to assign an invalid range form another static vector
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
/// Checks equality of two static vectors, where the second one was created with the content of the first using the
/// move constructor
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
/// Checks equality of two static vectors, where the second one is created using the base assignment of the first one
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
/// Checks equality of self-assignment static vector
/// @requirements(SEN-355)
TYPED_TEST(VectorTestTemplate, selfAssignment)
{
  using Vec = typename TypeParam::Vec;

  Vec vector1;
  this->populate(vector1);

  Vec vector2 = vector1;

  // we are doing this on purpose
  vector2 = vector2;  // NOLINT(misc-redundant-expression)

  EXPECT_TRUE(std::equal(vector1.begin(), vector1.end(), vector2.begin()));
}

/// @test
/// Checks usage of beginning memory pointer
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
/// Checks usage of ending memory pointer
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
/// Check resize operator when the new size is less than the max capacity
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
/// Check resize operator with value when the new size is less than the max capacity
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
/// Checks failure on resize with a new size greater than the max capacity
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
/// Check resize operator when resizing to a smaller size
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
/// Check resize operator with value when resizing to a smaller size
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
/// Check empty static vector resize operator
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
/// Check basic operators on empty static vector
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
/// Check static vector index operator
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
/// Check index operator on const static vector
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
/// Check front method on static vector
/// @requirements(SEN-355)
TYPED_TEST(VectorTestTemplate, front)
{
  using Vec = typename TypeParam::Vec;

  Vec vector1;
  this->populate(vector1);

  EXPECT_EQ(vector1.front(), this->sampleData.front());
}

/// @test
/// Check front method on const static vector
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
/// Check back method on static vector
/// @requirements(SEN-355)
TYPED_TEST(VectorTestTemplate, back)
{
  using Vec = typename TypeParam::Vec;

  Vec vector1;
  this->populate(vector1);

  EXPECT_EQ(vector1.back(), this->sampleData.back());
}

/// @test
/// Check back method on const static vector
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
/// Check data method on static vector
/// @requirements(SEN-355)
TYPED_TEST(VectorTestTemplate, data)
{
  using Vec = typename TypeParam::Vec;

  Vec vector1;
  this->populate(vector1);

  EXPECT_TRUE(std::equal(vector1.data(), vector1.data() + vector1.size(), this->sampleData.begin()));
}

/// @test
/// Check data method on const static vector
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
/// Check assign method on static vector
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
/// Check assign method on const static vector
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
/// Check failure on assign method with excess of data
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
/// Check assign method with an initializer list
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
/// Check assign method with move init list
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
/// Check push back method
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
/// Check empty push back method
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
/// Check push back method with literal values
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
/// Check push back method of a full static vector with a literal value
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
/// Check push back method of a full static vector
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
/// Check emplace back method of a static vector
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
/// Check emplace back method of a full static vector
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
/// Check emplace method of a static vector
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
/// Check pop back method of static vector
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
/// Check failure of pop back method
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
/// Check insert method of static vector
/// @requirements(SEN-355)
TYPED_TEST(VectorTestTemplate, insertPositionValue)
{
  using Vec = typename TypeParam::Vec;
  using T = typename TypeParam::ValueType;

  const std::size_t startingPoint = 5;

  for (std::size_t offset = 0; offset <= 5; ++offset)
  {
    std::vector<T> compareData;
    Vec data;

    // assign elements to the 2 vectors

    EXPECT_TRUE(data.assign(this->sampleData.begin(), this->sampleData.begin() + startingPoint));

    compareData.assign(this->sampleData.begin(), this->sampleData.begin() + startingPoint);

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
/// Check insert method with moved value
/// @requirements(SEN-355)
TYPED_TEST(VectorTestTemplate, insertPositionValueMoved)
{
  using Vec = typename TypeParam::Vec;
  using T = typename TypeParam::ValueType;

  const std::size_t startingPoint = 5;

  for (std::size_t offset = 0; offset <= 5; ++offset)
  {
    std::vector<T> compareData;
    Vec data;

    // assign elements to the 2 vectors
    EXPECT_TRUE(data.assign(this->sampleData.begin(), this->sampleData.begin() + startingPoint));
    compareData.assign(this->sampleData.begin(), this->sampleData.begin() + startingPoint);

    // insert at the offset
    EXPECT_TRUE(data.insert(data.begin() + offset, T {}));
    compareData.insert(compareData.begin() + offset, T {});

    // check that are the same
    EXPECT_EQ(compareData.size(), data.size());
    EXPECT_TRUE(std::equal(data.begin(), data.end(), compareData.begin()));
  }
}

/// @test
/// Check insert method of a full static vector
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
/// Check insert method of a full static vector with moved values
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
/// Check insert method with N values
/// @requirements(SEN-355)
TYPED_TEST(VectorTestTemplate, insertPositionNValue)
{
  using Vec = typename TypeParam::Vec;
  using T = typename TypeParam::ValueType;

  const std::size_t startingPoint = 5;
  const std::size_t insertSize = 3;

  for (std::size_t offset = 0; offset <= startingPoint; ++offset)
  {
    std::vector<T> compareData;
    Vec data;

    // assign elements to the 2 vectors
    EXPECT_TRUE(data.assign(this->sampleData.begin(), this->sampleData.begin() + startingPoint));
    compareData.assign(this->sampleData.begin(), this->sampleData.begin() + startingPoint);

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
/// Check insert method with N values that exceed vector size
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
/// Check insert method in certain position range
/// @requirements(SEN-355)
TYPED_TEST(VectorTestTemplate, insertPositionRange)
{
  using Vec = typename TypeParam::Vec;
  using T = typename TypeParam::ValueType;

  const std::size_t startingPoint = 5;

  for (std::size_t offset = 0; offset <= startingPoint; ++offset)
  {
    std::vector<T> compareData;
    Vec data;

    EXPECT_TRUE(data.resize(data.maxSize(), T {}));

    // assign elements to the 2 vectors
    EXPECT_TRUE(data.assign(this->sampleData.begin(), this->sampleData.begin() + startingPoint));
    compareData.assign(this->sampleData.begin(), this->sampleData.begin() + startingPoint);

    // insert some elements
    EXPECT_TRUE(data.insert(data.begin() + offset, this->insertData.begin(), this->insertData.end()));
    compareData.insert(compareData.begin() + offset, this->insertData.begin(), this->insertData.end());

    EXPECT_EQ(compareData.size(), data.size());
    EXPECT_TRUE(std::equal(data.begin(), data.end(), compareData.begin()));
  }
}

/// @test
/// Check insert method in certain range that exceed vector size
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
/// Check insert method with initializer list
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
/// Check failure cases of insert method
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
/// Check erase method
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
/// Check ranged-erase method
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
/// Check ranged-erase method
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
/// Check clear method
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
/// Check usage of iterator over static vector
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
/// Check usage of const iterator over static vector
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
/// Check usage of reverse iterator over static vector
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
/// Check usage of const reverse iterator over static vector
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
/// Check equality of two populated static vectors
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
/// Check inequality of two populated static vectors
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
/// Check assign operator
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
/// Check swap method
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
/// Check copy move constructor of a static vector
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
/// Check creation of static vector with specific size
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
/// Check creation of static vector with specific size and values
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
/// Check creation of static vector with initializer list
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
/// Check creation of static vector with iterators
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
/// Check insert method with initializer list (1)
/// @requirements(SEN-355)
TEST_F(BasicVectorTest, initializer_list)
{
  EXPECT_TRUE(vector.insert(vector.begin(), {'a', 'b', 'c', 'd', 'e'}));
  EXPECT_EQ(vector.size(), 5U);
  EXPECT_EQ(vector, comparisonData);
}

/// @test
/// Check insert method with initializer list (2)
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
/// Check insert method with initializer list (3)
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
/// Check insert method indicating position and const reference
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
/// Check insert method indicating position and values
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
/// Check insert method with initialized lists
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
/// Check insert method with ranges
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
/// Check move insert method
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
/// Check erasing of all values in static vector
/// @requirements(SEN-355)
TEST_F(BasicVectorTest, erase_all)
{
  auto vector = Vec({'a', 'b', 'c', 'd', 'e'});
  EXPECT_FALSE(vector.empty());
  EXPECT_TRUE(vector.erase(vector.begin(), vector.end()));
  EXPECT_TRUE(vector.empty());
}

/// @test
/// CHeck first values erasing in static vector
/// @requirements(SEN-355)
TEST_F(BasicVectorTest, erase_start_of_range)
{
  auto vector = Vec({'a', 'b', 'c', 'd', 'e'});
  EXPECT_TRUE(vector.erase(vector.begin(), vector.begin() + 2));
  EXPECT_EQ(vector, Vec({'c', 'd', 'e'}));
}

/// @test
/// Check last values erasing in static vector
/// @requirements(SEN-355)
TEST_F(BasicVectorTest, erase_end_of_range)
{
  auto vector = Vec({'a', 'b', 'c', 'd', 'e'});
  EXPECT_TRUE(vector.erase(vector.end() - 2, vector.end()));
  EXPECT_EQ(vector, Vec({'a', 'b', 'c'}));
}

/// @test
/// Check mid values erasing in static vector
/// @requirements(SEN-355)
TEST_F(BasicVectorTest, erase_mid_of_range)
{
  auto vector = Vec({'a', 'b', 'c', 'd', 'e'});
  EXPECT_TRUE(vector.erase(vector.begin() + 1, vector.end() - 1));
  EXPECT_EQ(vector, Vec({'a', 'e'}));
}

/// @test
/// Check corner case in emplace method of basic static vector
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

  for (size_t i = 0; !vector.full(); ++i)
  {
    EXPECT_TRUE(vector.emplace(vector.end(), 'X'));
  }

  EXPECT_FALSE(vector.emplace(vector.end(), 'Y'));
}

/// @test
/// Check corner case in emplace method of basic static vector
/// @requirements(SEN-355)
TEST_F(BasicVectorTest, emplace_corner_case)
{
  Vec v = Vec({1, 2, 3});
  EXPECT_TRUE(v.emplace(v.begin() + 1, 100));
  EXPECT_EQ(v, Vec({1, 100, 2, 3}));
}

/// @test
/// Check emplace back method of basic static vector
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
/// Checks the default resize method
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
/// Checks range erase branches
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
/// Checks internal error propagation for SEN_VECTOR_TRY
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
/// Checks push_back and emplace_back when full for specific coverage
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
/// Checks that emplace at the end of a non-full vector works correctly
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
/// Checks that erasing an empty range at the end is a valid operation
/// @requirements(SEN-355)
TEST(StaticVectorLogicTest, EraseEmptyRangeAtEnd)
{
  StaticVector<int, 5> v = {1, 2, 3};
  auto res = v.erase(v.end(), v.end());

  EXPECT_TRUE(res.isOk());
  EXPECT_EQ(v.size(), 3);
}

/// @test
/// Checks emplace when the argument is a reference to an element inside the vector
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
/// Cheks error paths and macro expansions for complex instantiations
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

// NOLINTEND(cppcoreguidelines-pro-bounds-pointer-arithmetic)
