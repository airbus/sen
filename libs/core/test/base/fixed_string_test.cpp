// === fixed_string_test.cpp ===========================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

// sen
#include "sen/core/base/fixed_string.h"

// google test
#include <gtest/gtest.h>

#include <string>
#include <tuple>

using sen::FixedString;

namespace
{

/// @test
/// Check default constructor
TEST(FixedString, stringInitialized)
{
  const char* data = "Foobar";
  FixedString<6> str(std::string {data});

  EXPECT_EQ(str.size(), 6);
  EXPECT_EQ(str.capacity(), 6);

  EXPECT_EQ(str, data);
}

TEST(FixedString, stringViewInitialized)
{
  const char* data = "Foobar";
  FixedString<6> str(std::string_view {data});

  EXPECT_EQ(str.size(), 6);
  EXPECT_EQ(str.capacity(), 6);

  EXPECT_EQ(str, data);
}

TEST(FixedString, stringInitializedWithMoreBuffer)
{
  const char* data = "Foobar";
  FixedString<10> str(std::string {data});

  EXPECT_EQ(str.size(), 6);
  EXPECT_EQ(str.capacity(), 10);

  EXPECT_EQ(str, data);
}

TEST(FixedString, assignedSmaller)
{
  const char* data = "Bar";
  FixedString<6> str(std::string("Foobar"));
  FixedString<3> str2(std::string {data});

  str = str2;

  EXPECT_EQ(str.size(), 3);
  EXPECT_EQ(str2, data);
}

TEST(FixedString, assignedLarger)
{
  FixedString<6> str(std::string("Foobar"));
  FixedString<13> str2(std::string("BuzzerDropped"));

  str = str2;

  EXPECT_EQ(str.size(), 6);

  EXPECT_EQ(str, "Buzzer");
}

TEST(FixedString, compareAgainstStringView)
{
  const char* data = "Foobar";
  FixedString<6> str(std::string {data});

  std::string_view strView = str;

  EXPECT_EQ(strView, std::string_view {"Foobar"});
}

//===----------------------------------------------------------------------===//
// Operators

TEST(FixedString, assignCStringRef)
{
  FixedString<6> str;
  std::string newContent = "Foobar";

  str.assign(newContent);

  EXPECT_EQ(str, newContent);
}

TEST(FixedString, assignCStringView)
{
  FixedString<6> str;
  std::string_view newContent = "Foobar";

  str.assign(newContent);

  EXPECT_EQ(str, newContent);
}

TEST(FixedString, assignIterators)
{
  FixedString<6> str;
  std::string_view newContent = "Foobar";

  str.assign(newContent.begin(), newContent.end());

  EXPECT_EQ(str, newContent);
}

TEST(FixedString, assignInitializerList)
{
  FixedString<6> str;

  str.assign({'F', 'o', 'o', 'b', 'a', 'r'});

  std::string_view newContent = "Foobar";
  EXPECT_EQ(str, newContent);
}

//--------------------------------------------------------------------------------------------------------------------//
// Element access

TEST(FixedString, accessAt)
{
  FixedString<6> str(std::string("Foobar"));
  const FixedString<6>& cStr = str;

  EXPECT_EQ(str.at(0), 'F');
  EXPECT_EQ(str.at(1), 'o');
  EXPECT_EQ(str.at(2), 'o');
  EXPECT_EQ(str.at(3), 'b');
  EXPECT_EQ(str.at(4), 'a');
  EXPECT_EQ(str.at(5), 'r');

  EXPECT_EQ(cStr.at(0), 'F');
  EXPECT_EQ(cStr.at(1), 'o');
  EXPECT_EQ(cStr.at(2), 'o');
  EXPECT_EQ(cStr.at(3), 'b');
  EXPECT_EQ(cStr.at(4), 'a');
  EXPECT_EQ(cStr.at(5), 'r');

  EXPECT_ANY_THROW(std::ignore = str.at(6));
  EXPECT_ANY_THROW(std::ignore = cStr.at(6));
}

TEST(FixedString, accessOperatorBracket)
{
  FixedString<6> str(std::string("Foobar"));
  const FixedString<6>& cStr = str;

  EXPECT_EQ(str[0], 'F');
  EXPECT_EQ(str[1], 'o');
  EXPECT_EQ(str[2], 'o');
  EXPECT_EQ(str[3], 'b');
  EXPECT_EQ(str[4], 'a');
  EXPECT_EQ(str[5], 'r');

  EXPECT_EQ(cStr[0], 'F');
  EXPECT_EQ(cStr[1], 'o');
  EXPECT_EQ(cStr[2], 'o');
  EXPECT_EQ(cStr[3], 'b');
  EXPECT_EQ(cStr[4], 'a');
  EXPECT_EQ(cStr[5], 'r');

#if defined(DEBUG)
  EXPECT_DEATH(std::ignore = str[6]);
  EXPECT_DEATH(std::ignore = cStr[6]);
#endif
}

TEST(FixedString, accessFront)
{
  FixedString<6> str(std::string("Foobar"));
  const FixedString<6>& cStr = str;

  EXPECT_EQ(str.front(), 'F');
  EXPECT_EQ(cStr.front(), 'F');
}

TEST(FixedString, accessBack)
{
  FixedString<6> str(std::string("Foobar"));
  const FixedString<6>& cStr = str;

  EXPECT_EQ(str.back(), 'r');
  EXPECT_EQ(cStr.back(), 'r');
}

TEST(FixedString, accessDataReads)
{
  FixedString<6> str(std::string("Foobar"));
  const FixedString<6>& cStr = str;

  std::char_traits<char>::compare(str.data(), "Foobar", 6);
  std::char_traits<char>::compare(cStr.data(), "Foobar", 6);
  std::char_traits<char>::compare(str.c_str(), "Foobar", 6);
}

//--------------------------------------------------------------------------------------------------------------------//
// Iterators

TEST(FixedString, iterators)
{
  FixedString<6> str(std::string("Foobar"));
  const FixedString<6>& cStr = str;

  std::array<char, 6> rawData {'F', 'o', 'o', 'b', 'a', 'r'};

  int idx {0};
  for (char c: str)
  {
    ASSERT_EQ(rawData[idx], c);  // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)
    idx++;
  }

  idx = 0;
  for (char c: cStr)
  {
    ASSERT_EQ(rawData[idx], c);  // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)
    idx++;
  }

  idx = 0;
  // TODO: fix pointer arithmetic
  for (auto itr = str.cbegin(); itr != str.cend(); ++itr)
  {                                 // NOLINT
    ASSERT_EQ(rawData[idx], *itr);  // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)
    idx++;
  }
}

TEST(FixedString, iteratorsReverse)
{
  FixedString<6> str(std::string("Foobar"));
  const FixedString<6>& cStr = str;

  std::array<char, 6> rawData {'F', 'o', 'o', 'b', 'a', 'r'};

  int idx {5};
  for (auto itr = str.rbegin(); itr != str.rend(); ++itr)  // NOLINT
  {
    ASSERT_EQ(rawData[idx], *itr);  // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)
    idx--;
  }

  idx = 5;
  for (auto itr = cStr.rbegin(); itr != cStr.rend(); ++itr)  // NOLINT
  {
    ASSERT_EQ(rawData[idx], *itr);  // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)
    idx--;
  }

  idx = 5;
  // TODO: fix pointer arithmetic
  for (auto itr = str.crbegin(); itr != str.crend(); ++itr)  // NOLINT
  {
    ASSERT_EQ(rawData[idx], *itr);  // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)
    idx--;
  }
}

//--------------------------------------------------------------------------------------------------------------------//
// Capacity

TEST(FixedString, capacity)
{
  FixedString<10> str(std::string("Foobar"));

  EXPECT_FALSE(str.empty());
  EXPECT_EQ(str.size(), 6);
  EXPECT_EQ(str.length(), 6);
  EXPECT_EQ(str.capacity(), 10);
}

TEST(FixedString, empty)
{
  FixedString<10> str;

  EXPECT_TRUE(str.empty());
}

//--------------------------------------------------------------------------------------------------------------------//
// Modifiers

TEST(FixedString, clear)
{
  FixedString<10> str(std::string("Foobar"));
  EXPECT_FALSE(str.empty());

  str.clear();

  EXPECT_TRUE(str.empty());
}

//--------------------------------------------------------------------------------------------------------------------//
// Search

TEST(FixedString, findFound)
{
  FixedString<6> str(std::string("Foobar"));

  FixedString<2> other("ob");
  EXPECT_EQ(str.find(other), 2);
  EXPECT_EQ(str.find(other, 1), 2);

  EXPECT_EQ(str.find("ob"), 2);
  EXPECT_EQ(str.find("ob", 1), 2);
  EXPECT_EQ(str.find("ob", 0, 1), 1);

  EXPECT_EQ(str.find('b'), 3);
  EXPECT_EQ(str.find('b', 1), 3);
}

TEST(FixedString, findNotFound)
{
  FixedString<6> str(std::string("Foobar"));

  FixedString<2> other("bo");
  EXPECT_EQ(str.find(other), FixedString<0>::npos);
  EXPECT_EQ(str.find(other, 1), FixedString<0>::npos);

  EXPECT_EQ(str.find("bo"), FixedString<0>::npos);
  EXPECT_EQ(str.find("bo", 1), FixedString<0>::npos);
  EXPECT_EQ(str.find("xo", 0, 1), FixedString<0>::npos);

  EXPECT_EQ(str.find('x'), FixedString<0>::npos);
  EXPECT_EQ(str.find('x', 1), FixedString<0>::npos);
}

TEST(FixedString, rfindFound)
{
  FixedString<6> str(std::string("Foobar"));

  FixedString<2> other("ob");
  EXPECT_EQ(str.rfind(other), 2);
  EXPECT_EQ(str.rfind(other, 4), 2);

  EXPECT_EQ(str.rfind("ob"), 2);
  EXPECT_EQ(str.rfind("ob", 4), 2);
  EXPECT_EQ(str.rfind("ob", 5, 1), 2);

  EXPECT_EQ(str.rfind('b'), 3);
  EXPECT_EQ(str.rfind('b', 4), 3);
}

TEST(FixedString, rfindNotFound)
{
  FixedString<6> str(std::string("Foobar"));

  FixedString<2> other("bo");
  EXPECT_EQ(str.rfind(other), FixedString<0>::npos);
  EXPECT_EQ(str.rfind(other, 5), FixedString<0>::npos);

  EXPECT_EQ(str.rfind("bo"), FixedString<0>::npos);
  EXPECT_EQ(str.rfind("bo", 5), FixedString<0>::npos);
  EXPECT_EQ(str.rfind("xo", FixedString<0>::npos, 1), FixedString<0>::npos);

  EXPECT_EQ(str.rfind('x'), FixedString<0>::npos);
  EXPECT_EQ(str.rfind('x', 5), FixedString<0>::npos);
}

TEST(FixedString, findFirstOfFound)
{
  FixedString<12> str(std::string("FoobarbarooF"));

  FixedString<3> other("bar");
  EXPECT_EQ(str.find_first_of(other), 3);
  EXPECT_EQ(str.find_first_of(other, 1), 3);

  EXPECT_EQ(str.find_first_of("abr"), 3);
  EXPECT_EQ(str.find_first_of("abr", 4), 4);
  EXPECT_EQ(str.find_first_of("arb", 4, 2), 4);

  EXPECT_EQ(str.find_first_of('b'), 3);
  EXPECT_EQ(str.find_first_of('b', 5), 6);
}

TEST(FixedString, findFirstOfNotFound)
{
  FixedString<12> str(std::string("FoobarbarooF"));

  FixedString<3> other("BAR");
  EXPECT_EQ(str.find_first_of(other), FixedString<0>::npos);
  EXPECT_EQ(str.find_first_of(other, 1), FixedString<0>::npos);

  EXPECT_EQ(str.find_first_of("BAR"), FixedString<0>::npos);
  EXPECT_EQ(str.find_first_of("BAR", 4), FixedString<0>::npos);
  EXPECT_EQ(str.find_first_of("BAR", 4, 2), FixedString<0>::npos);

  EXPECT_EQ(str.find_first_of('x'), FixedString<6>::npos);
  EXPECT_EQ(str.find_first_of('x', 5), FixedString<0>::npos);
}

TEST(FixedString, findFirstNotOfFound)
{
  FixedString<12> str(std::string("FoobarbarooF"));

  FixedString<3> other("Fob");
  EXPECT_EQ(str.find_first_not_of(other), 4);
  EXPECT_EQ(str.find_first_not_of(other, 5), 5);

  EXPECT_EQ(str.find_first_not_of("Fob"), 4);
  EXPECT_EQ(str.find_first_not_of("Fob", 5), 5);
  EXPECT_EQ(str.find_first_not_of("For", 5, 2), 5);
  EXPECT_EQ(str.find_first_not_of("Fobar", 4, 4), 5);

  EXPECT_EQ(str.find_first_not_of('F'), 1);
  EXPECT_EQ(str.find_first_not_of('b', 6), 7);
}

TEST(FixedString, findFirstNotOfNotFound)
{
  FixedString<12> str(std::string("FoobarbarooF"));

  FixedString<5> other("Fobar");
  EXPECT_EQ(str.find_first_not_of(other), FixedString<0>::npos);
  EXPECT_EQ(str.find_first_not_of(other, 1), FixedString<0>::npos);

  EXPECT_EQ(str.find_first_not_of("Fobar"), FixedString<0>::npos);
  EXPECT_EQ(str.find_first_not_of("Fobar", 4), FixedString<0>::npos);
  EXPECT_EQ(str.find_first_not_of("barFou", 10, 5), FixedString<0>::npos);

  FixedString<3> str2(std::string("XXX"));
  EXPECT_EQ(str2.find_first_not_of('X'), FixedString<0>::npos);
  EXPECT_EQ(str2.find_first_not_of('X', 1), FixedString<0>::npos);
}

TEST(FixedString, findLastOfFound)
{
  FixedString<12> str(std::string("FoobarbarooF"));

  FixedString<3> other("bar");
  EXPECT_EQ(str.find_last_of(other), 8);
  EXPECT_EQ(str.find_last_of(other, 5), 5);

  EXPECT_EQ(str.find_last_of("abr"), 8);
  EXPECT_EQ(str.find_last_of("abr", 4), 4);
  EXPECT_EQ(str.find_last_of("arb", 4, 2), 4);

  EXPECT_EQ(str.find_last_of('b'), 6);
  EXPECT_EQ(str.find_last_of('b', 5), 3);
}

TEST(FixedString, findLastOfNotFound)
{
  FixedString<12> str(std::string("FoobarbarooF"));

  FixedString<3> other("XUR");
  EXPECT_EQ(str.find_last_of(other), FixedString<0>::npos);
  EXPECT_EQ(str.find_last_of(other, 1), FixedString<0>::npos);

  EXPECT_EQ(str.find_last_of("XUR"), FixedString<0>::npos);
  EXPECT_EQ(str.find_last_of("XUR", 4), FixedString<0>::npos);
  EXPECT_EQ(str.find_last_of("XUR", 4, 2), FixedString<0>::npos);

  EXPECT_EQ(str.find_last_of('x'), FixedString<6>::npos);
  EXPECT_EQ(str.find_last_of('x', 5), FixedString<0>::npos);
}

TEST(FixedString, findLastNotOfFound)
{
  FixedString<12> str(std::string("FoobarbarooF"));

  FixedString<3> other("Fob");
  EXPECT_EQ(str.find_last_not_of(other), 8);
  EXPECT_EQ(str.find_last_not_of(other, 5), 5);

  EXPECT_EQ(str.find_last_not_of("Fob"), 8);
  EXPECT_EQ(str.find_last_not_of("Fob", 5), 5);
  EXPECT_EQ(str.find_last_not_of("For", 5, 2), 5);
  EXPECT_EQ(str.find_last_not_of("obaFr", 4, 3), 0);

  EXPECT_EQ(str.find_last_not_of('F'), 10);
  EXPECT_EQ(str.find_last_not_of('b', 6), 5);
}

TEST(FixedString, findLastNotOfNotFound)
{
  FixedString<12> str(std::string("FoobarbarooF"));

  FixedString<5> other("Fobar");
  EXPECT_EQ(str.find_last_not_of(other), FixedString<0>::npos);
  EXPECT_EQ(str.find_last_not_of(other, 1), FixedString<0>::npos);

  EXPECT_EQ(str.find_last_not_of("Fobar"), FixedString<0>::npos);
  EXPECT_EQ(str.find_last_not_of("Fobar", 4), FixedString<0>::npos);
  EXPECT_EQ(str.find_last_not_of("barFou", 10, 5), FixedString<0>::npos);

  FixedString<3> str2(std::string("XXX"));
  EXPECT_EQ(str2.find_last_not_of('X'), FixedString<0>::npos);
  EXPECT_EQ(str2.find_last_not_of('X', 1), FixedString<0>::npos);
}

//--------------------------------------------------------------------------------------------------------------------//
// Operations

TEST(FixedString, comparisions)
{
  FixedString<3> baseString {"Foo"};

  FixedString<3> otherFixedString {"Bar"};
  FixedString<3> sameFixedString {"Foo"};
  EXPECT_FALSE(baseString == otherFixedString);
  EXPECT_TRUE(baseString == sameFixedString);
  EXPECT_TRUE(baseString != otherFixedString);
  EXPECT_FALSE(baseString != sameFixedString);

  std::string otherString {"Bar"};
  std::string sameString {"Foo"};
  EXPECT_FALSE(baseString == otherString);
  EXPECT_TRUE(baseString == sameString);
  EXPECT_TRUE(baseString != otherString);
  EXPECT_FALSE(baseString != sameString);
  EXPECT_FALSE(otherString == baseString);
  EXPECT_TRUE(sameString == baseString);
  EXPECT_TRUE(otherString != baseString);
  EXPECT_FALSE(sameString != baseString);

  std::string_view otherStringView {"Bar"};
  std::string_view sameStringView {"Foo"};
  EXPECT_FALSE(baseString == otherStringView);
  EXPECT_TRUE(baseString == sameStringView);
  EXPECT_TRUE(baseString != otherStringView);
  EXPECT_FALSE(baseString != sameStringView);
  EXPECT_FALSE(otherStringView == baseString);
  EXPECT_TRUE(sameStringView == baseString);
  EXPECT_TRUE(otherStringView != baseString);
  EXPECT_FALSE(sameStringView != baseString);

  const char* otherCharPtr {"Bar"};
  const char* sameCharPtr {"Foo"};
  EXPECT_FALSE(baseString == otherCharPtr);
  EXPECT_TRUE(baseString == sameCharPtr);
  EXPECT_TRUE(baseString != otherCharPtr);
  EXPECT_FALSE(baseString != sameCharPtr);
  EXPECT_FALSE(otherCharPtr == baseString);
  EXPECT_TRUE(sameCharPtr == baseString);
  EXPECT_TRUE(otherCharPtr != baseString);
  EXPECT_FALSE(sameCharPtr != baseString);
}

}  // namespace
