// === callback_test.cpp ===============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

// sen

// sen
#include "sen/core/base/class_helpers.h"
#include "sen/core/base/compiler_macros.h"
#include "sen/core/obj/callback.h"

// generated code
#include "stl/example_class.stl.h"

// google test
#include <gtest/gtest.h>

// std
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <tuple>
#include <utility>

namespace example_class
{

class ExampleClassImpl final: public ExampleClassBase
{
public:
  SEN_NOCOPY_NOMOVE(ExampleClassImpl)

public:
  using ExampleClassBase::ExampleClassBase;
  ~ExampleClassImpl() override = default;

public:
  void myFunc(int value) { std::ignore = value; }
};

}  // namespace example_class

using sen::Callback;
using sen::impl::WorkQueue;

template <typename... Args>
using Function = std::function<void(sen::MaybeRef<Args>...)>;

template <typename InfoClass, typename... Args>
using FunctionWithCallInfo = std::function<void(const InfoClass&, sen::MaybeRef<Args>...)>;

namespace
{

uint64_t counter;

// example plain function that updates counter value
void setCounter(const uint64_t value) { counter = value; }

// example function with meta information that updates counter value
void setCustomQualifiedName(const std::string& prefix, std::string& name) { name.insert(0, prefix + "."); }

/// @test
/// Builds a callback from an explicit work queue and a free function, the callback reports
/// valid and the same queue, and invoke runs the function with the passed value.
/// @requirements(SEN-355)
TEST(Callback, queuePlainFunctionConstructor)
{
  counter = 0U;

  auto validQueue = WorkQueue(50, false);
  const auto cb = Callback<uint64_t>(&validQueue, setCounter);

  EXPECT_TRUE(cb.lock().isValid());
  EXPECT_TRUE(cb.lock().isSameQueue(&validQueue));

  EXPECT_EQ(counter, 0);
  cb.invoke(30U);
  EXPECT_EQ(counter, 30U);
}

/// @test
/// Builds a callback on an explicit work queue around a two-argument function with a reference
/// parameter, invoke forwards both arguments and the function's change to the referenced
/// string is visible to the caller.
/// @requirements(SEN-355)
TEST(Callback, queueMetaInfoFuncConstructor)
{
  auto validQueue = WorkQueue(20, true);
  const auto cb = Callback<std::string, std::string&>(&validQueue, setCustomQualifiedName);

  EXPECT_TRUE(cb.lock().isValid());

  const std::string prefix = "bus";
  std::string name = "aircraft1";

  cb.invoke(prefix, name);
  EXPECT_EQ(name, "bus.aircraft1");
}

/// @test
/// Builds a callback taking its work queue from a Sen object and wrapping a free function,
/// the callback reports valid and invoke runs the function with the passed value.
/// @requirements(SEN-355)
TEST(Callback, objPlainFunctionConstructor)
{
  counter = 0U;

  auto obj = std::make_shared<example_class::ExampleClassImpl>("obj1");
  auto validQueue = WorkQueue(5, true);

  const auto cb = Callback<uint64_t>(obj.get(), setCounter);
  EXPECT_TRUE(cb.lock().isValid());

  EXPECT_EQ(counter, 0U);
  EXPECT_NO_THROW(cb.invoke(64U));
  EXPECT_EQ(counter, 64U);
}

/// @test
/// Builds a callback taking its work queue from a Sen object around a two-argument function
/// with a reference parameter, invoke forwards the arguments and the referenced string shows
/// the change.
/// @requirements(SEN-355)
TEST(Callback, objMetaInfoFuncConstructor)
{
  auto obj = std::make_shared<example_class::ExampleClassImpl>("obj1");
  auto validQueue = WorkQueue(10, false);

  const auto cb = Callback<std::string, std::string&>(obj.get(), setCustomQualifiedName);
  EXPECT_TRUE(cb.lock().isValid());

  const std::string prefix = "rpr";
  std::string name = "Platform";

  cb.invoke(prefix, name);
  EXPECT_EQ(name, "rpr.Platform");
}

/// @test
/// Invokes the wrapped function while the callback is valid and stops after
/// lock().invalidate(), the callback then reports invalid and invoke becomes a no-op.
/// @requirements(SEN-355)
TEST(Callback, cancelCheck)
{
  auto obj = std::make_shared<example_class::ExampleClassImpl>("obj1");
  EXPECT_NE(nullptr, sen::impl::getWorkQueue(obj.get()));

  bool called = false;
  const auto cb = Callback<sen::MethodCallInfo>(obj.get(), [&]() { called = true; });
  EXPECT_TRUE(cb.lock().isValid());

  cb.invoke({});
  EXPECT_TRUE(called);

  called = false;
  cb.lock().invalidate();

  EXPECT_FALSE(cb.lock().isValid());

  cb.invoke({});
  EXPECT_FALSE(called);
}

/// @test
/// Invalidates the callback when the receiver object is destroyed, isValid turns false and
/// invoke no longer runs the wrapped function.
/// @requirements(SEN-355)
TEST(Callback, deletedReceiverCheck)
{
  auto obj = std::make_shared<example_class::ExampleClassImpl>("obj1");
  EXPECT_NE(nullptr, sen::impl::getWorkQueue(obj.get()));

  bool called = false;
  const auto cb = Callback<sen::MethodCallInfo>(obj.get(), [&]() { called = true; });
  EXPECT_TRUE(cb.lock().isValid());

  cb.invoke({});
  EXPECT_TRUE(called);

  called = false;
  obj.reset();

  EXPECT_FALSE(cb.lock().isValid());

  cb.invoke({});
  EXPECT_FALSE(called);
}

}  // namespace
