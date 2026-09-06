// === every_kind_model.h ==============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_LIBS_GEN_TEST_EVERY_KIND_MODEL_H
#define SEN_LIBS_GEN_TEST_EVERY_KIND_MODEL_H

// sen
#include "sen/core/lang/stl_parser.h"
#include "sen/core/lang/stl_resolver.h"
#include "sen/core/lang/stl_scanner.h"
#include "sen/core/lang/stl_statement.h"

// std
#include <string>
#include <vector>

namespace sen::gen::test
{

/// One of everything the type system has, plus a class carrying a method and an event. No FOM
/// in the corpus declares either of those, so STL is the only way to reach that code.
///
/// Every declared type is referenced by something, as a real model's would be. The UML
/// generator draws a sequence as an association and so leaves out one nothing refers to.
constexpr auto everyKindStl = R"(package k.deep;

// How far a thing is, in metres.
quantity<f32, m> Metres;

// The colours a thing may be painted.
enum Colour : u8 { red, green, blue }

// A place, given as two distances.
struct Point { x : Metres, y : Metres }

// A round thing, which is also a place.
struct Circle: Point { centre : Point, radius : Metres }

// Either a place or a round thing.
variant Figure { Point, Circle }

// Where something has been.
sequence<Point, 8> Track;

// Three distances, always three.
array<Metres, 3> Triple;

// A colour, or nothing.
optional<Colour> MaybeColour;

// Another name for a distance.
alias Distance Metres;

// The root of the little hierarchy.
class Base
{
  // How many of it there are.
  var one : Metres;

  // Asked to settle down.
  fn settle();
}

// One step down.
class Middle: extends Base
{
  // Whether it is switched on.
  var live : bool [writable];

  // Emitted when it stops.
  event stopped(why : Colour);
}

// Two steps down, and where most of the members are.
class Derived: extends Middle
{
  // What shape it takes.
  var shape : Figure;

  // Everywhere it has been.
  var trail : Track;

  // Its colour, if it has one.
  var tint  : MaybeColour;

  // How far away it is.
  var span  : Distance [confirmed];

  // Three fixed distances.
  var fixed : Triple;

  // Works out how far, from a place.
  fn measure(from : Point) -> Metres;

  // Emitted when it moves.
  event moved(to : Point);
}
)";

/// A model resolved from STL source. Owns the statements as well as the context, because the
/// context refers to them.
class ResolvedModel
{
public:
  explicit ResolvedModel(const std::string& stl)
  {
    sen::lang::StlScanner scanner {stl};
    sen::lang::StlParser parser {scanner.scanTokens()};
    statements_ = parser.parse();

    sen::lang::StlResolver resolver {statements_, resolverContext_, context_};
    set_ = resolver.resolve({});
  }

  [[nodiscard]] const sen::lang::TypeSetContext& context() const { return context_; }

  [[nodiscard]] const sen::lang::TypeSet& set() const { return *set_; }

private:
  std::vector<sen::lang::StlStatement> statements_;
  sen::lang::ResolverContext resolverContext_;
  sen::lang::TypeSetContext context_;
  const sen::lang::TypeSet* set_ = nullptr;
};

}  // namespace sen::gen::test

#endif  // SEN_LIBS_GEN_TEST_EVERY_KIND_MODEL_H
