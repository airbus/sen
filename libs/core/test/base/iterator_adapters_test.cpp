// === iterator_adapters_test.cpp ======================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

// sen
#include "sen/core//base/iterator_adapters.h"

// google test
#include <gtest/gtest.h>

// std
#include <array>
#include <cstddef>
#include <list>
#include <memory>
#include <mutex>
#include <numeric>
#include <shared_mutex>
#include <thread>
#include <tuple>
#include <type_traits>
#include <unordered_map>
#include <vector>

using sen::util::makeLockedRange;
using sen::util::makeRange;
using sen::util::SmartPtrIteratorAdapter;

//===--------------------------------------------------------------------------------------------------------------===//
// Base Definitions
//===--------------------------------------------------------------------------------------------------------------===//

TEST(IteratorRange, createRange)
{
  std::array<int, 2> data {21, 42};
  int* expectedBegin = &data[0];
  int* expectedEnd = &data[1];

  auto iteratorRange = makeRange(expectedBegin, expectedEnd);

  EXPECT_EQ(iteratorRange.begin(), expectedBegin);
  EXPECT_EQ(iteratorRange.end(), expectedEnd);
}

TEST(IteratorRange, iterateOverSeqMemory)
{
  std::array<int, 4> data {0, 1, 21, 42};
  int testSum {0};

  for (int elem: makeRange(data))
  {
    testSum += elem;
  }

  EXPECT_EQ(testSum, std::accumulate(data.begin(), data.end(), 0));
}

TEST(IteratorRange, iterateOverSeqDynMemory)
{
  std::vector<int> data {0, 1, 21, 42, 10, 420, 40, 82, 27};
  int testSum {0};

  for (int elem: makeRange(data))
  {
    testSum += elem;
  }

  EXPECT_EQ(testSum, std::accumulate(data.begin(), data.end(), 0));
}

TEST(IteratorRange, iterateOverNonSeqDynMemory)
{
  std::unordered_map<int, int> data {{0, 0}, {1, 1}, {2, 21}, {3, 42}, {4, 10}, {5, 420}, {6, 40}, {7, 82}, {8, 27}};
  int testSum {0};

  for (const auto& elem: makeRange(data))
  {
    testSum += elem.second;
  }

  EXPECT_EQ(
    testSum,
    std::accumulate(
      data.begin(), data.end(), 0, [](int currentSum, const auto& entry) { return currentSum + entry.second; }));
}

//===--------------------------------------------------------------------------------------------------------------===//
// Range Adapters
//===--------------------------------------------------------------------------------------------------------------===//

struct DoNothingMutex
{
  void lock() {}
  void unlock() {}
};

struct CountingMutex
{
  void lock() { lockCount_++; }
  void unlock() { unlockCount_++; }

  [[nodiscard]] int lockCount() { return lockCount_; }
  [[nodiscard]] int unlockCount() { return unlockCount_; }

private:
  int lockCount_ {0};
  int unlockCount_ {0};
};

TEST(LockedRangeAdapter, createAndIterateNonConstContainerVector)
{
  std::vector<int> cont {1, 2, 3, 4, 5, 6};
  DoNothingMutex m;

  int sum {0};
  for (auto i: makeLockedRange<std::lock_guard>(cont, m))
  {
    sum += i;
  }

  EXPECT_EQ(sum, std::accumulate(cont.begin(), cont.end(), 0));
}

TEST(LockedRangeAdapter, createAndIterateNonConstContainerList)
{
  std::list<int> cont {1, 2, 3, 4, 5, 6};
  DoNothingMutex m;

  int sum {0};
  for (auto i: makeLockedRange<std::lock_guard>(cont, m))
  {
    sum += i;
  }

  EXPECT_EQ(sum, std::accumulate(cont.begin(), cont.end(), 0));
}

TEST(LockedRangeAdapter, createAndIterateConstContainerVector)
{
  const std::vector<int> cont {1, 2, 3, 4, 5, 6};
  DoNothingMutex m;

  int sum {0};
  for (auto i: makeLockedRange<std::lock_guard>(cont, m))
  {
    sum += i;
  }

  EXPECT_EQ(sum, std::accumulate(cont.begin(), cont.end(), 0));
}

TEST(LockedRangeAdapter, correctLockCountNonConstContainerVector)
{
  std::vector<int> cont {1, 2, 3, 4, 5, 6};
  CountingMutex m;

  for (auto i: makeLockedRange<std::lock_guard>(cont, m))
  {
    std::ignore = i;
  }

  EXPECT_EQ(m.lockCount(), 1);
  EXPECT_EQ(m.lockCount(), m.unlockCount());
}

TEST(LockedRangeAdapter, stdMutexNonConstContainerVector)
{
  std::vector<int> cont {1, 2, 3, 4, 5, 6};
  // as every number gets incremented once by each thread, the sum should increase by 2x the size of the list
  const size_t expectedFinalSum = std::accumulate(cont.begin(), cont.end(), 0) + 2 * cont.size();
  std::mutex m;

  std::thread t1(
    [&cont, &m]()
    {
      for (auto& i: makeLockedRange<std::lock_guard>(cont, m))
      {
        i += 1;
      }
    });

  std::thread t2(
    [&cont, &m]()
    {
      for (auto& i: makeLockedRange<std::lock_guard>(cont, m))
      {
        i += 1;
      }
    });

  t1.join();
  t2.join();

  std::lock_guard lock(m);
  EXPECT_EQ(expectedFinalSum, std::accumulate(cont.begin(), cont.end(), 0));
}

TEST(LockedRangeAdapter, stdSharedMutexNonConstContainerVector)
{
  std::vector<int> cont {1, 2, 3, 4, 5, 6};
  std::shared_mutex m;

  int sum {0};
  int sumBeforeWriting = std::accumulate(cont.begin(), cont.end(), 0);
  std::thread writer(
    [&cont, &m]()
    {
      for (auto& i: makeLockedRange<std::lock_guard>(cont, m))
      {
        i += 1;
      }
    });

  std::thread reader(
    [&cont, &m, &sum]()
    {
      for (auto i: makeLockedRange<std::lock_guard>(cont, m))
      {
        sum += i;
      }
    });

  writer.join();
  reader.join();

  std::lock_guard lock(m);
  int sumAfterWriting = std::accumulate(cont.begin(), cont.end(), 0);

  // sum should either be what the sum was before the writer could write or the sum after all writes have been performed
  // BUT nothing in-between, as makeLockedRange should not allow interleaved accesses to the range.
  //
  // Furthermore, this test also helps to cache threading issues with tsan
  EXPECT_TRUE(sum == sumBeforeWriting || sum == sumAfterWriting);
}

//===--------------------------------------------------------------------------------------------------------------===//
// Iterator Adapters
//===--------------------------------------------------------------------------------------------------------------===//

TEST(SmartPtrIteratorAdapter, checkAdaptedIteratorInterface)
{
  using UnderlyingType = int;
  using ContainedType = std::unique_ptr<UnderlyingType>;
  using VectorType = std::vector<ContainedType>;

  VectorType data;

  auto adaptedBegin = SmartPtrIteratorAdapter(data.begin());
  auto adaptedEnd = SmartPtrIteratorAdapter(data.end());

  static_assert(std::is_same_v<decltype(*adaptedBegin), UnderlyingType*>,
                "Dereferencing the adapted iterator should yield a ptr to the underlying type.");

  static_assert(std::is_same_v<decltype(adaptedBegin.operator->()), UnderlyingType&>,
                "Member access operator of the adapted iterator should yield a ref to the underlying type.");

  // Test primarily that the adapted iterator is comparable
  EXPECT_TRUE(adaptedBegin == adaptedEnd);
  EXPECT_FALSE(adaptedBegin != adaptedEnd);
}

TEST(SmartPtrIteratorAdapter, runtimeUniquePtr)
{
  // given
  std::array<int, 9> initData {0, 1, 21, 42, 10, 420, 40, 82, 27};
  std::vector<std::unique_ptr<int>> data;
  data.reserve(initData.size());
  for (int v: initData)
  {
    data.push_back(std::make_unique<int>(v));
  }
  int testSum {0};

  for (int* elem: makeRange(SmartPtrIteratorAdapter(data.begin()), SmartPtrIteratorAdapter(data.end())))
  {
    testSum += *elem;
  }

  // then
  EXPECT_EQ(testSum, std::accumulate(initData.begin(), initData.end(), 0));
}

TEST(SmartPtrIteratorAdapter, runtimeSharedPtr)
{
  // given
  std::array<int, 9> initData {0, 1, 21, 42, 10, 420, 40, 82, 27};
  std::vector<std::shared_ptr<int>> data;
  data.reserve(initData.size());
  for (int v: initData)
  {
    data.push_back(std::make_shared<int>(v));
  }
  int testSum {0};

  // when
  for (int* elem: makeRange(SmartPtrIteratorAdapter(data.begin()), SmartPtrIteratorAdapter(data.end())))
  {
    testSum += *elem;
  }

  // then
  EXPECT_EQ(testSum, std::accumulate(initData.begin(), initData.end(), 0));
}
