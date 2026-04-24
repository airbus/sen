// === fibonacci.cpp ===================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

// generated code
#include "stl/fibonacci.stl.h"

// sen
#include "sen/core/base/compiler_macros.h"
#include "sen/core/meta/class_type.h"
#include "sen/core/obj/interest.h"
#include "sen/core/obj/subscription.h"
#include "sen/kernel/component_api.h"

// std
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <future>
#include <iostream>
#include <iterator>
#include <memory>
#include <random>
#include <string>
#include <thread>
#include <utility>

namespace fibonacci
{

// Pure Fibonacci computation (no side effects, suitable for unit testing).
[[nodiscard]] uint64_t computeFibonacci(uint32_t n)
{
  if (n == 0 || n == 1)
  {
    return n;
  }

  uint64_t fib1 = 0;
  uint64_t fib2 = 1;
  for (uint64_t i = 2; i <= n; ++i)
  {
    const auto next = fib1 + fib2;
    fib1 = fib2;
    fib2 = next;
  }
  return fib2;
}

/// Computes the Fibonacci number.
class FibonacciWorker: public WorkerBase
{
public:
  SEN_NOCOPY_NOMOVE(FibonacciWorker)

public:
  using WorkerBase::WorkerBase;
  ~FibonacciWorker() override = default;

public:
  void computeFibonacciImpl(uint32_t n, std::promise<uint64_t>&& promise) override
  {
    std::this_thread::sleep_for(std::chrono::seconds(5));  // simulate heavy workload
    promise.set_value(::fibonacci::computeFibonacci(n));
  }
};

/// Forwards work to the workers found in "workersBus".
class FibonacciManager: public ManagerBase<>
{
public:
  SEN_NOCOPY_NOMOVE(FibonacciManager)

public:
  using ManagerBase::ManagerBase;
  ~FibonacciManager() override = default;

protected:
  void registered(sen::kernel::RegistrationApi& api) override
  {
    // select all the workers in "workersBus"
    std::string query;
    query.append("SELECT fibonacci.Worker FROM ")
      .append(getWorkersBus())
      .append(" WHERE name != \"")
      .append(getName())
      .append("\"");

    // install the interest
    workers_ = std::make_shared<sen::Subscription<WorkerInterface>>();
    workers_->attachTo(api.getSource(getWorkersBus()), sen::Interest::make(query, api.getTypes()), true);
  }

public:
  /// Forwards the call to a random worker, or does the work if none is found.
  void computeFibonacciImpl(uint32_t n, std::promise<uint64_t>&& promise) override
  {
    if (workers_->list.getObjects().empty())
    {
      std::cout << "No workers available, manager is performing the task" << std::endl;
      std::this_thread::sleep_for(std::chrono::seconds(5));  // simulate heavy workload
      promise.set_value(::fibonacci::computeFibonacci(n));
    }
    else
    {
      auto& selectedWorker = selectRandomWorker();
      std::cout << "Sending task to " << selectedWorker.asObject().getName() << std::endl;
      selectedWorker.computeFibonacci(
        n, {this, [p = std::move(promise)](const auto& result) mutable { p.set_value(result.getValue()); }});
    }
  }

private:
  [[nodiscard]] WorkerInterface& selectRandomWorker()
  {
    const auto& objects = workers_->list.getObjects();
    if (objects.size() == 1)
    {
      return *objects.front();
    }
    std::uniform_int_distribution<std::size_t> dist {0, objects.size() - 1};
    return *(*std::next(objects.begin(), static_cast<std::ptrdiff_t>(dist(rng_))));
  }

private:
  std::mt19937 rng_ {std::random_device {}()};
  std::shared_ptr<sen::Subscription<WorkerInterface>> workers_;
};

SEN_EXPORT_CLASS(FibonacciWorker)
SEN_EXPORT_CLASS(FibonacciManager)

}  // namespace fibonacci
