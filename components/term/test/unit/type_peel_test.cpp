// === type_peel_test.cpp ==============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "type_peel.h"

// sen
#include "sen/core/meta/alias_type.h"
#include "sen/core/meta/native_types.h"
#include "sen/core/meta/optional_type.h"
#include "sen/core/meta/sequence_type.h"
#include "sen/core/meta/struct_type.h"

// test stl
#include "stl/term.stl.h"
#include "stl/test_object.stl.h"

// google test
#include <gtest/gtest.h>

namespace sen::components::term
{
namespace
{

/// @test
/// Every kind of type term can be shown has a word for it, and the words differ. `types` puts this
/// in a column and the completer puts it in an annotation, so a kind that came back as the generic
/// "type" would read as a type term does not understand.
///
/// Variant and quantity are not built here: both need a spec shape this test would have to guess
/// at, and a guess that compiles is not evidence. Their branches stay uncovered and named.
TEST(TypePeel, EachKindOfTypeHasItsOwnWord)
{
  const auto sequence = SequenceType::make(SequenceSpec {"Seq", "ns.Seq", "", Int32Type::get()});
  const auto structure = StructType::make(StructSpec {"S", "ns.S", "", {{"x", "", Int32Type::get()}}});
  const auto optional = OptionalType::make(OptionalSpec {"O", "ns.O", "", Int32Type::get()});
  const auto alias = AliasType::make(AliasSpec {"A", "ns.A", "", Int32Type::get()});

  EXPECT_EQ(typeKindName(*::term::test::TestObjectInterface::meta()), "class");
  EXPECT_EQ(typeKindName(*MetaTypeTrait<ThemeStyle>::meta()), "enum");
  EXPECT_EQ(typeKindName(*sequence), "sequence");
  EXPECT_EQ(typeKindName(*structure), "struct");
  EXPECT_EQ(typeKindName(*optional), "optional");
  EXPECT_EQ(typeKindName(*alias), "alias");

  // A primitive is none of the above, and falls back to the generic word rather than to nothing.
  EXPECT_EQ(typeKindName(*Int32Type::get()), "type");
}

/// @test
/// Peeling follows a chain of aliases to its end rather than one layer of it. An alias of an alias
/// is what a generated STL produces when one named type is defined in terms of another, and the
/// caller that peeled once would still be holding an alias where it expected a number.
TEST(TypePeel, PeelingFollowsTheWholeAliasChain)
{
  const auto inner = AliasType::make(AliasSpec {"Inner", "ns.Inner", "", Int32Type::get()});
  const auto outer = AliasType::make(AliasSpec {"Outer", "ns.Outer", "", inner});

  EXPECT_EQ(typeKindName(*outer), "alias") << "the unpeeled type is still an alias";
  EXPECT_EQ(peelAliases(ConstTypeHandle<>(outer))->getName(), Int32Type::get()->getName());

  // The raw-pointer overload exists because a caller holding a `const Type&` cannot make a handle.
  const Type* peeled = peelAliases(outer.type());
  EXPECT_EQ(peeled->getName(), Int32Type::get()->getName());
}

/// @test
/// Peeling to a value goes through aliases and optionals together, which is what a form field
/// needs: whether a value is optional decides whether it may be left empty, not how it is edited.
TEST(TypePeel, PeelingToAValueGoesThroughOptionalsAndAliases)
{
  const auto optional = OptionalType::make(OptionalSpec {"O", "ns.O", "", Int32Type::get()});
  const auto alias = AliasType::make(AliasSpec {"A", "ns.A", "", optional});

  EXPECT_EQ(peelToValue(ConstTypeHandle<>(alias))->getName(), Int32Type::get()->getName());

  // peelAliases stops at the optional, which is the difference between the two.
  EXPECT_EQ(typeKindName(*peelAliases(ConstTypeHandle<>(alias))), "optional");
}

}  // namespace
}  // namespace sen::components::term
