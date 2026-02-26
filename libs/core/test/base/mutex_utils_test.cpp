// === mutex_utils_test.cpp ============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

// sen
#include "sen/core/base/mutex_utils.h"

// gtest
#include <gtest/gtest.h>

// sen
#include <chrono>
#include <string>
#include <thread>

using sen::Guarded;

namespace
{

struct MyClass
{
  MyClass() = default;
  explicit MyClass(int a): a(a) {}

  int getAsInt() { return a; }
  void setIt(int i) { a = i; }

  friend inline bool operator==(const MyClass& lhs, const MyClass& rhs) { return lhs.a == rhs.a; }
  MyClass& operator+=(int i)
  {
    a += i;
    return *this;
  }

  int a;  // NOLINT(misc-non-private-member-variables-in-classes): test class
};

template <typename T>
class GuardedTest: public ::testing::Test
{
};

template <typename T>
struct TestTypeInfoTrait
{
};

template <>
struct TestTypeInfoTrait<int>
{
  static constexpr int initInput1 = 42;
  static constexpr int initInput2 = 21;
  static int exampleValue() { return initInput1; }
};

template <>
struct TestTypeInfoTrait<double>
{
  static constexpr double initInput1 = 42.2;
  static constexpr double initInput2 = 21.1;
  static double exampleValue() { return initInput1; }
};

template <>
struct TestTypeInfoTrait<std::string>
{
  static constexpr const char* initInput1 = "42";
  static constexpr const char* initInput2 = "21";
  static std::string exampleValue() { return {initInput1}; }
};

template <>
struct TestTypeInfoTrait<MyClass>
{
  static constexpr int initInput1 = 42;
  static constexpr int initInput2 = 21;
  static MyClass exampleValue() { return MyClass(initInput1); }
};

template <>
struct TestTypeInfoTrait<int*>
{
  inline static int i1 {42};
  inline static int i2 {21};
  static constexpr int* initInput1 = &i1;
  static constexpr int* initInput2 = &i2;
  static int* exampleValue() { return initInput1; }
};

using TestedTypes = ::testing::Types<int, double, std::string, MyClass, int*>;

TYPED_TEST_SUITE(GuardedTest, TestedTypes);

/// @test
/// Check default initialization of a guard object
/// @requirements(SEN-1048)
TYPED_TEST(GuardedTest, CreateWrapperDefaultInit) { Guarded<TypeParam> wrappedType; }

/// @test
/// Check initialization of a guard object with value
/// @requirements(SEN-1048)
TYPED_TEST(GuardedTest, CreateWrapperValueInit)
{
  Guarded<TypeParam> wrappedType {TestTypeInfoTrait<TypeParam>::initInput1};

  EXPECT_EQ(TestTypeInfoTrait<TypeParam>::exampleValue(), wrappedType.getValue());
}

/// @test
/// Check that a guard object can be assigned to a value
/// @requirements(SEN-1048)
TYPED_TEST(GuardedTest, Assign)
{
  Guarded<TypeParam> wrappedType;

  wrappedType = TestTypeInfoTrait<TypeParam>::exampleValue();

  EXPECT_EQ(TestTypeInfoTrait<TypeParam>::exampleValue(), wrappedType.getValue());
}

/// @test
/// Check that a guard object can be compared with a value
/// @requirements(SEN-1048)
TYPED_TEST(GuardedTest, Comparisions)
{
  Guarded<TypeParam> wrappedType {TestTypeInfoTrait<TypeParam>::initInput1};

  EXPECT_TRUE(wrappedType == TestTypeInfoTrait<TypeParam>::exampleValue());
  EXPECT_TRUE(TestTypeInfoTrait<TypeParam>::exampleValue() == wrappedType);
  EXPECT_TRUE(wrappedType == wrappedType);

  Guarded<TypeParam> wrappedType2 {TestTypeInfoTrait<TypeParam>::initInput2};
  EXPECT_TRUE(wrappedType2 != TestTypeInfoTrait<TypeParam>::exampleValue());
  EXPECT_TRUE(TestTypeInfoTrait<TypeParam>::exampleValue() != wrappedType2);
  EXPECT_TRUE(wrappedType != wrappedType2);
}

/// @test
/// Check correct underlying value in a guard object
/// @requirements(SEN-1048)
TEST(GuardedTest, AccessUnderlyingObject)
{
  int expectedValue = 42;
  Guarded<MyClass> wrappedType {42};

  EXPECT_EQ(expectedValue, wrappedType->getAsInt());
}

/// @test
/// Check increment from multiple threads of underlying value in a guard object
/// @requirements(SEN-1048)
TEST(GuardedTest, IncrementUnderlyingObjectFromMultipleThreads)
{
  const int reps = 10;
  const int numThreads = 3;
  Guarded<int> wrappedType {0};

  auto incWrappedValue = [&wrappedType, reps]()
  {
    for (int i = 0; i < reps; i++)
    {
      wrappedType += 1;
    }
  };

  std::thread t1(incWrappedValue);
  std::thread t2(incWrappedValue);
  std::thread t3(incWrappedValue);

  t1.join();
  t2.join();
  t3.join();

  EXPECT_EQ(reps * numThreads, wrappedType.getValue());
}

/// @test
/// Check that local token can set a value and protects from other threads in a guard object
/// @requirements(SEN-1048)
TEST(GuardedTest, LocalAccessTokenUsageWithThreads)
{
  const int reps = 10;
  const int numThreads = 3;
  const int additionalValue = 42;
  Guarded<MyClass> wrappedType {0};

  auto incWrappedValue = [&wrappedType, reps]()
  {
    for (int i = 0; i < reps; i++)
    {
      wrappedType += 1;
    }
  };

  std::thread t1;
  std::thread t2;
  std::thread t3;

  {
    auto localAccessToken = wrappedType.createAccessToken();
    ASSERT_EQ(localAccessToken->getAsInt(), 0);

    t1 = std::thread(incWrappedValue);
    t2 = std::thread(incWrappedValue);
    t3 = std::thread(incWrappedValue);

    ASSERT_EQ(localAccessToken->getAsInt(), 0);
    localAccessToken->setIt(additionalValue);

    std::this_thread::sleep_for(
      std::chrono::milliseconds(200));  // Enable the other threads to potentially "harm" out protected value

    EXPECT_EQ(localAccessToken->getAsInt(), additionalValue);
  }  // scope exit should releases the lock hold by the access token

  t1.join();
  t2.join();
  t3.join();

  EXPECT_EQ(reps * numThreads + additionalValue, wrappedType.getValue().getAsInt());
}

/// @test
/// Check that guard object can invoke a function and protect underlying value
/// @requirements(SEN-1048)
TEST(GuardedTest, CallableUsageWithThreads)
{
  const int reps = 10;
  const int numThreads = 3;
  const int initalValue = 0;
  Guarded<int> wrappedType {initalValue};

  auto incWrappedValue = [&wrappedType, reps]()
  {
    for (int i = 0; i < reps; i++)
    {
      wrappedType += 1;
    }
  };

  // invoke with lambda that copies
  wrappedType.invoke([initalValue](int copiedValue) { EXPECT_EQ(initalValue, copiedValue); });

  std::thread t1(incWrappedValue);
  std::thread t2(incWrappedValue);
  std::thread t3(incWrappedValue);

  // invoke with lambda that takes a ref
  int addedValue1 {2};
  wrappedType.invoke(
    [](int& refToValue)
    {
      refToValue += 2;  // <- same as adding addedValue1 but without a lambda capture
    });

  // invoke with lambda that takes a ref and has a capture
  int addedValue2 {3};
  wrappedType.invoke([addedValue2](int& refToValue) { refToValue += addedValue2; });

  t1.join();
  t2.join();
  t3.join();

  EXPECT_EQ(reps * numThreads + addedValue1 + addedValue2, wrappedType.getValue());
}

}  // namespace
