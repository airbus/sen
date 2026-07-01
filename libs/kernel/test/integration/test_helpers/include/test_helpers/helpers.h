// === helpers.h =======================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_LIBS_KERNEL_TEST_INTEGRATION_TRANSPORT_HELPERS_H
#define SEN_LIBS_KERNEL_TEST_INTEGRATION_TRANSPORT_HELPERS_H

// sen
#include "sen/core/base/class_helpers.h"
#include "sen/core/base/timestamp.h"

// generated code
#include "test_helpers/test_helpers.stl.h"

// sen
#include "sen/core/base/compiler_macros.h"
#include "sen/core/meta/var.h"
#include "sen/core/obj/connection_guard.h"
#include "sen/core/obj/object_source.h"
#include "sen/core/obj/subscription.h"
#include "sen/kernel/component_api.h"

// spdlog
#include <spdlog/logger.h>

// std
#include <algorithm>
#include <array>
#include <cmath>
#include <future>
#include <list>
#include <memory>
#include <string>
#include <type_traits>
#include <vector>

namespace sen::test
{

//--------------------------------------------------------------------------------------------------------------
// Helpers
//--------------------------------------------------------------------------------------------------------------

template <typename T>
[[nodiscard]] inline bool eq(T a, T b)
{
  // handle possible quantities of floating point types
  if constexpr (!std::is_same_v<T, sen::TimeStamp> && sen::HasValueType<T>::value)
  {
    return eq(a.get(), b.get());
  }

  if constexpr (std::is_floating_point_v<T>)
  {
    constexpr T eps = 1e-3;
    return std::fabs(a - b) < eps;
  }

  return a == b;
}

template <typename PropType>
[[nodiscard]] inline bool checkProp(std::promise<TestResult>& promise,
                                    const std::string& propName,
                                    sen::MaybeRef<PropType> value,
                                    sen::MaybeRef<PropType> expectation)
{
  if (!eq(value, expectation))
  {
    std::string err;
    err.append("Incorrect value of ");
    err.append(propName);
    promise.set_value(TestResult(err));
    return false;
  }

  return true;
}

[[nodiscard]] inline bool checkMember(std::promise<TestResult>& promise, const std::string& memberName, bool isOk)
{
  if (isOk)
  {
    return true;
  }

  std::string err = "Member ";
  err.append(memberName);
  err.append(" adaptation failed.");
  promise.set_value(TestResult(err));
  return false;
}

template <typename T>
[[nodiscard]] inline bool allObjectsWithState(const std::list<T*>& objects, const ConnectionState state)
{
  return std::all_of(objects.begin(), objects.end(), [state](const T* obj) { return obj->getState() == state; });
}

/// Terminates each process where it is included (via the kernel API) when it detects that all StatefulObjects in the
/// process are 'finished'
class SEN_EXPORT ProcessTerminatorImpl final: public ProcessTerminatorBase
{
public:
  ProcessTerminatorImpl(std::string name, const sen::VarMap& args);

public:
  void registered(sen::kernel::RegistrationApi& api) override;

private:
  std::shared_ptr<sen::Subscription<StatefulObjectInterface>> objectsSub_;
  std::vector<sen::ConnectionGuard> guards_;
  std::shared_ptr<spdlog::logger> logger_;
};

/// Holds a connection state
class SEN_EXPORT StateFulObjectImpl: public StatefulObjectBase
{
public:
  StateFulObjectImpl(std::string name, const sen::VarMap& args);

public:
  void registered(sen::kernel::RegistrationApi& api) override;

protected:
  [[nodiscard]] sen::kernel::KernelApi* getApi() const;
  [[nodiscard]] spdlog::logger* getLogger() const;

private:
  sen::kernel::KernelApi* api_ = nullptr;
  std::shared_ptr<spdlog::logger> logger_;
};

/// Performs actions in the integration test. These normally include creating, erasing and modifying objects
class SEN_EXPORT PublisherImpl: public PublisherBase<StateFulObjectImpl>
{
public:
  PublisherImpl(std::string name, const sen::VarMap& args);

public:
  void registered(sen::kernel::RegistrationApi& api) override;
  void update(sen::kernel::RunApi& runApi) override;

protected:
  virtual void action1();
  virtual void action2();
  virtual void action3();
  virtual void action4();

private:
  std::shared_ptr<sen::ObjectSource> bus_;
  std::shared_ptr<sen::Subscription<ListenerInterface>> listenersSub_;
  std::vector<sen::ConnectionGuard> cbGuards_;
  std::array<bool, 5U> actionFlags_ = {false, false, false, false, false};
};

/// Reacts to action performs by the publisher and compares the result to the test expectation. These reactions normally
/// involve creating subscriptions for objects, installing callbacks for property changes, etc.
class SEN_EXPORT ListenerImpl: public ListenerBase<StateFulObjectImpl>
{
public:
  ListenerImpl(std::string name, const sen::VarMap& args);

public:
  void registered(sen::kernel::RegistrationApi& api) override;
  void update(sen::kernel::RunApi& runApi) override;

protected:
  virtual void check1();
  virtual void check2();
  virtual void check3();
  virtual void check4();

private:
  std::shared_ptr<sen::ObjectSource> bus_;
  std::shared_ptr<sen::Subscription<PublisherInterface>> publisherSub_;
  std::vector<sen::ConnectionGuard> cbGuards_;
  std::array<bool, 5U> checkFlags_ = {false, false, false, false, false};
};

}  // namespace sen::test

#endif  // SEN_LIBS_KERNEL_TEST_INTEGRATION_TRANSPORT_HELPERS_H
