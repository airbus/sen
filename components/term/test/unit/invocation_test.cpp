// === invocation_test.cpp =============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "test_object_impl.h"

// sen
#include "sen/core/io/util.h"
#include "sen/core/meta/var.h"
#include "sen/core/obj/callback.h"
#include "sen/kernel/test_kernel.h"

// google test
#include <gtest/gtest.h>

// std
#include <memory>
#include <string>
#include <variant>

namespace sen::components::term
{
namespace
{

//--------------------------------------------------------------------------------------------------------------
// Test fixture
//--------------------------------------------------------------------------------------------------------------

class InvocationTest: public ::testing::Test
{
protected:
  void SetUp() override
  {
    testObj_ = std::make_shared<test::TestObjectImpl>("testObj", VarMap {});
    testObj_->setNextCounter(42);

    component_.onInit(
      [this](kernel::InitApi&& api) -> kernel::PassResult
      {
        auto source = api.getSource(kernel::BusAddress {"local", "test"});
        source->add(testObj_);
        return kernel::done();
      });

    component_.onRun([](kernel::RunApi& api) -> kernel::FuncResult
                     { return api.execLoop(Duration(std::chrono::milliseconds(10)), []() {}); });

    kernel_ = std::make_unique<kernel::TestKernel>(&component_);
    kernel_->step(3);
  }

  static const Method* findMethod(std::string_view name) { return test::findTestMethod(name); }

  std::shared_ptr<test::TestObjectImpl> testObj_;
  kernel::TestComponent component_;
  std::unique_ptr<kernel::TestKernel> kernel_;
};

//--------------------------------------------------------------------------------------------------------------
// Argument parsing tests
//--------------------------------------------------------------------------------------------------------------

TEST_F(InvocationTest, ParseNoArgs)
{
  auto parsed = fromJson(R"({ "args": [] })").get<VarMap>();
  auto argList = findElement(parsed, "args", "").get<VarList>();
  EXPECT_EQ(argList.size(), 0U);
}

TEST_F(InvocationTest, ParseIntArgs)
{
  auto* method = findMethod("add");
  ASSERT_NE(method, nullptr);

  auto parsed = fromJson(R"({ "args": [3, 7] })").get<VarMap>();
  auto argList = findElement(parsed, "args", "").get<VarList>();
  ASSERT_EQ(argList.size(), 2U);

  const auto& methodArgs = method->getArgs();
  for (size_t i = 0; i < methodArgs.size(); ++i)
  {
    auto result = impl::adaptVariant(*methodArgs[i].type, argList[i]);
    EXPECT_TRUE(result.isOk()) << "Arg " << i << ": " << result.getError();
  }
}

TEST_F(InvocationTest, ParseStringArg)
{
  auto parsed = fromJson(R"({ "args": ["hello world"] })").get<VarMap>();
  auto argList = findElement(parsed, "args", "").get<VarList>();
  ASSERT_EQ(argList.size(), 1U);
  EXPECT_EQ(argList[0].get<std::string>(), "hello world");
}

//--------------------------------------------------------------------------------------------------------------
// Argument validation tests
//--------------------------------------------------------------------------------------------------------------

TEST_F(InvocationTest, TooManyArgsDetected)
{
  auto* method = findMethod("ping");
  ASSERT_NE(method, nullptr);
  EXPECT_TRUE(method->getArgs().empty());
}

TEST_F(InvocationTest, TooFewArgsDetected)
{
  auto* method = findMethod("add");
  ASSERT_NE(method, nullptr);
  EXPECT_EQ(method->getArgs().size(), 2U);
}

//--------------------------------------------------------------------------------------------------------------
// PropertyGetter shortcut tests
//--------------------------------------------------------------------------------------------------------------

TEST_F(InvocationTest, PropertyGetterDetected)
{
  auto* method = findMethod("getCounter");
  ASSERT_NE(method, nullptr);

  auto* isGetter = std::get_if<PropertyGetter>(&method->getPropertyRelation());
  ASSERT_NE(isGetter, nullptr);
}

TEST_F(InvocationTest, PropertyGetterReturnsValue)
{
  auto* method = findMethod("getCounter");
  ASSERT_NE(method, nullptr);

  auto* isGetter = std::get_if<PropertyGetter>(&method->getPropertyRelation());
  ASSERT_NE(isGetter, nullptr);

  auto value = testObj_->getPropertyUntyped(isGetter->property);
  EXPECT_EQ(value.get<int32_t>(), 42);
}

TEST_F(InvocationTest, PropertySetterDetected)
{
  auto* method = findMethod("setNextCounter");
  ASSERT_NE(method, nullptr);
  EXPECT_TRUE(std::holds_alternative<PropertySetter>(method->getPropertyRelation()));
}

TEST_F(InvocationTest, RegularMethodIsNonPropertyRelated)
{
  auto* method = findMethod("ping");
  ASSERT_NE(method, nullptr);
  EXPECT_TRUE(std::holds_alternative<NonPropertyRelated>(method->getPropertyRelation()));
}

//--------------------------------------------------------------------------------------------------------------
// Method invocation tests
//--------------------------------------------------------------------------------------------------------------

TEST_F(InvocationTest, InvokePing)
{
  auto* method = findMethod("ping");
  ASSERT_NE(method, nullptr);

  bool callbackFired = false;
  std::string result;

  testObj_->invokeUntyped(method,
                          {},
                          MethodCallback<Var> {testObj_.get(),
                                               [&](const MethodResult<Var>& r)
                                               {
                                                 callbackFired = true;
                                                 ASSERT_TRUE(r.isOk());
                                                 result = r.getValue().get<std::string>();
                                               }});

  kernel_->step(2);
  EXPECT_TRUE(callbackFired);
  EXPECT_EQ(result, "pong");
}

TEST_F(InvocationTest, InvokeAdd)
{
  auto* method = findMethod("add");
  ASSERT_NE(method, nullptr);

  VarList args = {Var(int32_t {10}), Var(int32_t {32})};
  for (size_t i = 0; i < method->getArgs().size(); ++i)
  {
    ASSERT_TRUE(impl::adaptVariant(*method->getArgs()[i].type, args[i]).isOk());
  }

  bool callbackFired = false;
  int32_t result = 0;

  testObj_->invokeUntyped(method,
                          args,
                          MethodCallback<Var> {testObj_.get(),
                                               [&](const MethodResult<Var>& r)
                                               {
                                                 callbackFired = true;
                                                 ASSERT_TRUE(r.isOk());
                                                 result = r.getValue().get<int32_t>();
                                               }});

  kernel_->step(2);
  EXPECT_TRUE(callbackFired);
  EXPECT_EQ(result, 42);
}

TEST_F(InvocationTest, InvokeEcho)
{
  auto* method = findMethod("echo");
  ASSERT_NE(method, nullptr);

  VarList args = {Var(std::string("hello"))};
  ASSERT_TRUE(impl::adaptVariant(*method->getArgs()[0].type, args[0]).isOk());

  bool callbackFired = false;
  std::string result;

  testObj_->invokeUntyped(method,
                          args,
                          MethodCallback<Var> {testObj_.get(),
                                               [&](const MethodResult<Var>& r)
                                               {
                                                 callbackFired = true;
                                                 ASSERT_TRUE(r.isOk());
                                                 result = r.getValue().get<std::string>();
                                               }});

  kernel_->step(2);
  EXPECT_TRUE(callbackFired);
  EXPECT_EQ(result, "hello");
}

TEST_F(InvocationTest, InvokeReset)
{
  auto* method = findMethod("reset");
  ASSERT_NE(method, nullptr);

  bool callbackFired = false;

  testObj_->invokeUntyped(method,
                          {},
                          MethodCallback<Var> {testObj_.get(),
                                               [&](const MethodResult<Var>& r)
                                               {
                                                 callbackFired = true;
                                                 EXPECT_TRUE(r.isOk());
                                               }});

  kernel_->step(2);
  EXPECT_TRUE(callbackFired);
  kernel_->step(2);
  EXPECT_EQ(testObj_->getCounter(), 0);
}

//--------------------------------------------------------------------------------------------------------------
// Method metadata tests
//--------------------------------------------------------------------------------------------------------------

TEST_F(InvocationTest, MethodArgCounts)
{
  EXPECT_EQ(findMethod("ping")->getArgs().size(), 0U);
  EXPECT_EQ(findMethod("add")->getArgs().size(), 2U);
  EXPECT_EQ(findMethod("echo")->getArgs().size(), 1U);
  EXPECT_EQ(findMethod("reset")->getArgs().size(), 0U);
}

//--------------------------------------------------------------------------------------------------------------
// Result serialization tests
//--------------------------------------------------------------------------------------------------------------

TEST_F(InvocationTest, ToJsonInt) { EXPECT_EQ(toJson(Var(int32_t {42})), "42"); }

TEST_F(InvocationTest, ToJsonString) { EXPECT_EQ(toJson(Var(std::string("hello"))), "\"hello\""); }

}  // namespace
}  // namespace sen::components::term
