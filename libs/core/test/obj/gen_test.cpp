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
/// Destroys a generated remote proxy safely through a base class pointer, and the proxy
/// reports isRemote true.
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
/// Constructs a generated base object that reports its given name and whose instance class
/// and static meta both name ExampleClass.
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
/// Reports isRemote false on a generated local proxy built over a base object.
/// @requirements(SEN-583, SEN-351)
TEST(GenMacroTest, LocalProxyIdentifiesAsLocal)
{
  const sen::VarMap emptyArgs;
  const auto owner = std::make_shared<example_class::ExampleClassBase>("OwnerObj", emptyArgs);

  const example_class::ExampleClassLocalProxy localProxy(owner.get(), "Prefix_");
  EXPECT_FALSE(localProxy.isRemote());
}

/// @test
/// Starts a generated unbounded sequence empty and grows it with push_back, size and at
/// reflect the two appended values.
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
/// Starts a generated bounded sequence empty with its declared capacity of 5, and push_back
/// stores the value readable through front.
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
/// Builds a generated fixed sequence of size 3 from initializer lists, full or partial, the
/// size stays 3 and listed elements read back in position.
/// @requirements(SEN-577)
TEST(GenMacroTest, FixedSequenceBehaviors)
{
  const TestFixedSeq seq = {100, 200, 300};
  ASSERT_EQ(seq.size(), 3U);
  EXPECT_EQ(seq.at(0), 100);
  EXPECT_EQ(seq.at(2), 300);

  const TestFixedSeq partialSeq = {99};
  EXPECT_EQ(partialSeq.at(0), 99);

  const TestFixedSeq overflowSeq = {1, 2, 3};
  ASSERT_EQ(overflowSeq.size(), 3U);
  EXPECT_EQ(overflowSeq.at(2), 3);
}

/// @test
/// Compares generated optionals equal when both are empty or both hold the same value and
/// unequal otherwise, emplace engages the optional with its value and reset empties it again.
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
/// Maps a generated class through SenClassRelation to its own base, remote proxy, and local
/// proxy types, with isBaseTypeTemplate false.
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

/// @test
/// Compares equal generated sequences through the generated inequality operator, two empty
/// unbounded sequences and two identical fixed sequences are not unequal.
/// @requirements(SEN-577)
TEST(GenMacroTest, InequalityOperatorUsage)
{
  TestUnboundedSeq seq1;
  TestUnboundedSeq seq2;
  EXPECT_FALSE(seq1 != seq2);

  TestFixedSeq fixed1 = {1, 2, 3};
  TestFixedSeq fixed2 = {1, 2, 3};
  EXPECT_FALSE(fixed1 != fixed2);
}
