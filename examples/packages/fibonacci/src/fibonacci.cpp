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
#include <cstdint>
#include <future>
#include <iostream>
#include <iterator>
#include <memory>
#include <string>
#include <thread>
#include <utility>

namespace fibonacci
{

namespace
{

[[nodiscard]] uint64_t fibonacci(uint32_t n)
{
  std::this_thread::sleep_for(std::chrono::seconds(5));  // sleep some time to simulate some heavy workload

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

}  // namespace

/// Computes the Fibonacci number.
class FibonacciWorker: public WorkerBase
{
public:
  SEN_NOCOPY_NOMOVE(FibonacciWorker)

public:
  using WorkerBase::WorkerBase;
  ~FibonacciWorker() override = default;

public:
  void computeFibonacciImpl(uint32_t n, std::promise<uint64_t>&& promise) override { promise.set_value(fibonacci(n)); }
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
    workers_->source = api.getSource(getWorkersBus());
    workers_->source->addSubscriber(sen::Interest::make(query, api.getTypes()), &workers_->list, true);
  }

public:
  /// Forwards the call to a random worker, or does the work if none is found.
  void computeFibonacciImpl(uint32_t n, std::promise<uint64_t>&& promise) override
  {
    if (workers_->list.getObjects().empty())
    {
      std::cout << "No workers available, manager is performing the task" << std::endl;
      promise.set_value(fibonacci(n));
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
  [[nodiscard]] WorkerInterface& selectRandomWorker() const
  {
    const auto& objects = workers_->list.getObjects();

    // NOLINTNEXTLINE
    return objects.size() == 1 ? *objects.front() : *(*std::next(objects.begin(), rand() % objects.size()));
  }

private:
  std::shared_ptr<sen::Subscription<WorkerInterface>> workers_;
};

SEN_EXPORT_CLASS(FibonacciWorker)
SEN_EXPORT_CLASS(FibonacciManager)

}  // namespace fibonacci
