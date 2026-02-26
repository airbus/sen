// === type_traits_test.cpp ============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

// sen
#include "sen/core/meta/type_traits.h"

// generated code
#include "stl/test_type_traits.stl.h"

// google test
#include <gtest/gtest.h>

// std
#include <type_traits>

using sen::SenBaseTypeT;
using sen::SenInterfaceTypeT;
using sen::SenLocalProxyTypeT;
using sen::SenRemoteProxyTypeT;

using test_type_traits::ClassWithoutTemplatedBaseBase;
using test_type_traits::ClassWithoutTemplatedBaseInterface;
using test_type_traits::ClassWithoutTemplatedBaseLocalProxy;
using test_type_traits::ClassWithoutTemplatedBaseRemoteProxy;

using test_type_traits::BaseBase;
using test_type_traits::ClassWithTemplatedBaseBase;
using test_type_traits::ClassWithTemplatedBaseInterface;
using test_type_traits::ClassWithTemplatedBaseLocalProxy;
using test_type_traits::ClassWithTemplatedBaseRemoteProxy;

namespace
{

template <typename T>
class TypeRelationTestWithoutTemplatedBase: public ::testing::Test
{
};

using GeneratedSenClasses = ::testing::Types<ClassWithoutTemplatedBaseInterface,
                                             ClassWithoutTemplatedBaseBase,
                                             ClassWithoutTemplatedBaseLocalProxy,
                                             ClassWithoutTemplatedBaseRemoteProxy>;
TYPED_TEST_SUITE(TypeRelationTestWithoutTemplatedBase, GeneratedSenClasses);

/// @test
/// Check types without template
/// @requirements(SEN-1056)
TYPED_TEST(TypeRelationTestWithoutTemplatedBase, TestAccessorDeclarations)
{
  ASSERT_TRUE((std::is_same_v<ClassWithoutTemplatedBaseInterface, SenInterfaceTypeT<TypeParam>>));
  ASSERT_TRUE((std::is_same_v<ClassWithoutTemplatedBaseBase, SenBaseTypeT<TypeParam>>));
  ASSERT_TRUE((std::is_same_v<ClassWithoutTemplatedBaseLocalProxy, SenLocalProxyTypeT<TypeParam>>));
  ASSERT_TRUE((std::is_same_v<ClassWithoutTemplatedBaseRemoteProxy, SenRemoteProxyTypeT<TypeParam>>));
  ASSERT_FALSE(sen::SenClassRelation<TypeParam>::isBaseTypeTemplate);
}

template <typename T>
class TypeRelationTestWithTemplatedBase: public ::testing::Test
{
};

using GeneratedSenClassesWithTemplatedBase = ::testing::Types<ClassWithTemplatedBaseInterface,
                                                              ClassWithTemplatedBaseBase<BaseBase>,
                                                              ClassWithTemplatedBaseLocalProxy,
                                                              ClassWithTemplatedBaseRemoteProxy>;
TYPED_TEST_SUITE(TypeRelationTestWithTemplatedBase, GeneratedSenClassesWithTemplatedBase);

class SomeOtherBase
{
};

/// @test
/// Check types with template
/// @requirements(SEN-1056)
TYPED_TEST(TypeRelationTestWithTemplatedBase, TestAccessorDeclarations)
{
  ASSERT_TRUE((std::is_same_v<ClassWithTemplatedBaseInterface, SenInterfaceTypeT<TypeParam>>));
  ASSERT_TRUE((std::is_same_v<ClassWithTemplatedBaseBase<BaseBase>, SenBaseTypeT<TypeParam, BaseBase>>));
  ASSERT_TRUE((std::is_same_v<ClassWithTemplatedBaseBase<SomeOtherBase>, SenBaseTypeT<TypeParam, SomeOtherBase>>));
  ASSERT_TRUE((std::is_same_v<ClassWithTemplatedBaseLocalProxy, SenLocalProxyTypeT<TypeParam>>));
  ASSERT_TRUE((std::is_same_v<ClassWithTemplatedBaseRemoteProxy, SenRemoteProxyTypeT<TypeParam>>));
  ASSERT_TRUE(sen::SenClassRelation<TypeParam>::isBaseTypeTemplate);
}

}  // namespace
