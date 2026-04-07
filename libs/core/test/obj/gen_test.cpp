// === gen_test.cpp ====================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "sen/core/obj/detail/gen.h"

// sen
#include "sen/core/base/result.h"
#include "sen/core/meta/var.h"
#include "sen/core/obj/detail/remote_object.h"

// generated code
#include "stl/example_class.stl.h"

// google test
#include <gtest/gtest.h>

// std
#include <memory>
#include <optional>
#include <string>
#include <type_traits>
#include <utility>

#ifndef SEN_MAYBE_EXPORT
#  define SEN_MAYBE_EXPORT(x)
#endif

namespace
{

sen::impl::RemoteObjectInfo createTestProxyInfo()
{
  return sen::impl::RemoteObjectInfo {example_class::ExampleClassInterface::meta(),
                                      "TestProxy",
                                      sen::ObjectId {88U},
                                      nullptr,
                                      [](auto&&...) -> sen::Result<sen::impl::CallId, std::string>
                                      { return sen::Ok(sen::impl::CallId {1U}); },
                                      "localTestProxy",
                                      nullptr,
                                      sen::ObjectOwnerId {1U},
                                      {},
                                      std::nullopt};
}

SEN_IMPL_GEN_UNBOUNDED_SEQUENCE(TestUnboundedSeq, int)
SEN_IMPL_GEN_BOUNDED_SEQUENCE(TestBoundedSeq, int, 5)
SEN_IMPL_GEN_FIXED_SEQUENCE(TestFixedSeq, int, 3)
SEN_IMPL_GEN_OPTIONAL(TestOptionalInt, int)

}  // namespace

/// @test
/// Verifies that the generated override destructor correctly cleans up the object when deleted via a base class pointer
/// @requirements(SEN-583)
TEST(GenMacroTest, RemoteProxyPolymorphicDestruction)
{
  auto info = createTestProxyInfo();

  const std::unique_ptr<sen::impl::RemoteObject> proxyObj =
    std::make_unique<example_class::ExampleClassRemoteProxy>(std::move(info));

  ASSERT_NE(proxyObj, nullptr);
  EXPECT_TRUE(proxyObj->isRemote());
}

/// @test
/// Verifies that the base class generated correctly links to its metadata and initializes properly
/// @requirements(SEN-583, SEN-573)
TEST(GenMacroTest, BaseClassLifecycleAndMetaWiring)
{
  const sen::VarMap emptyArgs;
  const auto baseObj = std::make_shared<example_class::ExampleClassBase>("MyBaseObj", emptyArgs);

  EXPECT_EQ(baseObj->getName(), "MyBaseObj");
  EXPECT_EQ(baseObj->getClass()->getName(), "ExampleClass");
  EXPECT_EQ(example_class::ExampleClassBase::meta()->getName(), "ExampleClass");
}

/// @test
/// Verifies that local proxy objects correctly identify themselves as local components, distinct from remote ones
/// @requirements(SEN-583, SEN-351)
TEST(GenMacroTest, LocalProxyIdentifiesAsLocal)
{
  const sen::VarMap emptyArgs;
  const auto owner = std::make_shared<example_class::ExampleClassBase>("OwnerObj", emptyArgs);

  const example_class::ExampleClassLocalProxy localProxy(owner.get(), "Prefix_");
  EXPECT_FALSE(localProxy.isRemote());
}

/// @test
/// Checks that generated unbounded lists function exactly like standard dynamic arrays
/// @requirements(SEN-577)
TEST(GenMacroTest, UnboundedSequenceBehaviors)
{
  TestUnboundedSeq seq;
  EXPECT_TRUE(seq.empty());

  seq.push_back(10);
  seq.push_back(20);

  ASSERT_EQ(seq.size(), 2U);
  EXPECT_EQ(seq.at(0), 10);
  EXPECT_EQ(seq.at(1), 20);
}

/// @test
/// Ensures that generated bounded lists correctly initialize and respect their hard-coded capacity logic
/// @requirements(SEN-577, SEN-908)
TEST(GenMacroTest, BoundedSequenceBehaviors)
{
  TestBoundedSeq seq;
  EXPECT_EQ(seq.size(), 0U);
  EXPECT_EQ(seq.capacity(), 5U);

  seq.push_back(42);
  ASSERT_EQ(seq.size(), 1U);
  EXPECT_EQ(seq.front(), 42);
}

/// @test
/// Verifies that fixed size arrays enforce limits during initialization list construction and operator usage
/// @requirements(SEN-577)
TEST(GenMacroTest, FixedSequenceBehaviors)
{
  const TestFixedSeq seq = {100, 200, 300};
  ASSERT_EQ(seq.size(), 3U);
  EXPECT_EQ(seq.at(0), 100);
  EXPECT_EQ(seq.at(2), 300);

  const TestFixedSeq partialSeq = {99};
  EXPECT_EQ(partialSeq.at(0), 99);
  EXPECT_EQ(partialSeq.at(1), 0);

  const TestFixedSeq overflowSeq = {1, 2, 3, 4, 5, 6};
  ASSERT_EQ(overflowSeq.size(), 3U);
  EXPECT_EQ(overflowSeq.at(2), 3);
}

/// @test
/// Verifies the boolean states, containment, and comparisons of generated optional types
/// @requirements(SEN-583)
TEST(GenMacroTest, OptionalDataHandling)
{
  TestOptionalInt opt1;
  TestOptionalInt opt2;

  EXPECT_TRUE(opt1 == opt2);
  EXPECT_FALSE(opt1 != opt2);
  EXPECT_FALSE(static_cast<bool>(opt1));

  opt1.emplace(77);
  EXPECT_TRUE(opt1.has_value());
  EXPECT_EQ(*opt1, 77);
  EXPECT_TRUE(opt1 != opt2);

  opt2.emplace(77);
  EXPECT_TRUE(opt1 == opt2);

  opt1.reset();
  EXPECT_FALSE(opt1.has_value());
}

/// @test
/// Verifies that the generated structures strictly and safely map a class to its respective base, local proxy, and
/// remote proxy types
/// @requirements(SEN-1056, SEN-583)
TEST(GenMacroTest, ClassTraitsResolutionMapping)
{
  using Traits = sen::SenClassRelation<example_class::ExampleClassBase>;

  constexpr bool isBaseSame = std::is_same_v<Traits::BaseType, example_class::ExampleClassBase>;
  constexpr bool isRemoteSame = std::is_same_v<Traits::RemoteProxyType, example_class::ExampleClassRemoteProxy>;
  constexpr bool isLocalSame = std::is_same_v<Traits::LocalProxyType, example_class::ExampleClassLocalProxy>;

  EXPECT_TRUE(isBaseSame);
  EXPECT_TRUE(isRemoteSame);
  EXPECT_TRUE(isLocalSame);
  EXPECT_FALSE(Traits::isBaseTypeTemplate);
}
