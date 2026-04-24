// === parser_test.cpp =================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

// sen
#include "sen/core/lang/stl_parser.h"
#include "sen/core/lang/stl_scanner.h"
#include "sen/core/lang/stl_statement.h"

// google test
#include <gtest/gtest.h>

// std
#include <cstddef>
#include <cstdint>
#include <string>
#include <variant>
#include <vector>

using sen::lang::StlParser;
using sen::lang::StlScanner;

inline void encode(const std::string& str, std::vector<uint64_t>& buffer)
{
  buffer.reserve(str.size() / sizeof(uint64_t));

  const auto strSize = str.size();
  std::size_t index = 0U;

  while (index < str.size())
  {
    uint64_t v = 0U;

    for (std::size_t i = 0U; i < 8U; ++i)
    {
      const auto strIndex = index + i;
      if (strIndex < strSize)
      {
        v |= static_cast<uint64_t>(str[strIndex]) << (8U * i);
      }
      else
      {
        break;
      }
    }

    buffer.push_back(v);
    index += sizeof(v);
  }
}

inline void decode(const std::vector<uint64_t>& buffer, std::string& str)
{
  str.reserve(buffer.size() * sizeof(uint64_t));
  constexpr std::size_t byteSize = 8U;

  for (auto v: buffer)
  {
    for (std::size_t i = 0U; i < sizeof(v); ++i)
    {
      unsigned char c = v & (0xFFU);

      if (c == 0U)
      {
        return;
      }

      str.push_back(static_cast<char>(c));
      v = v >> byteSize;
    }
  }
}

/// @test
/// Checks correct parsing with a string containing a stl file with an import package, a large package name and
/// struct, enum, variant, sequence and quantity types
/// @requirements(SEN-903)
TEST(Parser, codeDencode)
{
  std::string program = R"(import "cucu"

       package my.custom_package.that.is.very.large;

       struct MyStruct: MyParentStruct
       {
         field1: i32,
         field2: i32
       }

       enum MyEnum
       {
         theEnumeratorA,
         theEnumeratorB,
       }

       variant MyVariant
       {
         MyType,
         MyOtherType
       }

       sequence<MyReal, 100> StringList;
       quantity<float64, rad> Lat;
         )";

  std::vector<uint64_t> buffer;
  encode(program, buffer);

  std::string decodedProgram;
  decode(buffer, decodedProgram);

  EXPECT_EQ(program, decodedProgram);
}

/// @test
/// Checks correct parsing with a string containing a stl file with an import package, a large package name and
/// complex enum and structs types, optional and array types and different class types
/// @requirements(SEN-903)
TEST(Parser, basic)
{
  // import, package, enum, struct...
  {
    std::string program =
      R"(
import "cucu"

package my.custom_package.that.is.very.large;

// Run modes for the kernel
enum RunMode: u8
{
  realTime,       // components execute using system time
  virtualTime,    // components execute using discrete steps (set by the user)
  startAndStop,   // starts and stops (useful for smoke tests)
}

// Parameters to configure kernel execution
struct KernelParams
{
  runMode     : RunMode,  // how to run the kernel
  appName     : string,   // optional name of the application
  bus         : string,   // where to publish the kernel objects (defaults to local.kernel)
  clockBus    : string,   // where to publish the virtual clock (if any), defaults to bus
  clockName   : string,   // the name of the virtual clock (if any), defaults to 'clock'
  clockMaster : bool,     // if time is virtualized, publish a master clock to clockBus
  logConfig   : sen.kernel.log.Config // logging configuration
}

// Type of operating system
enum OsKind : u8
{
   windowsOs, // Microsoft Windows
   linuxOs,   // Linux
   androidOs, // Android Linux
   appleOs,   // Apple OS (iOS, tvOS, etc..)
   unixOs,    // all unices not caught above
   posixOs,   // Posix
   otherOs    // other, unknown
}

  )";

    StlScanner scanner(program);
    auto tokens = scanner.scanTokens();
    EXPECT_FALSE(tokens.empty());

    StlParser parser(tokens);
    auto statements = parser.parse();

    EXPECT_EQ(5U, statements.size());
  }

  // class, var, fn, event, extends...
  {
    std::string program =
      R"(
package school;

class Person
{
  var firstName     : string [static];
  var surName       : string [static];
  var brainActivity : f32;

  // method that takes a string and returns another
  fn ask(question : string) -> string;

  // an event that requires confirmed transport
  event saidSomething(words : string) [confirmed];
}

class MyClass
{
}

class MySubClass : extends MyClass
{
}

    )";

    StlScanner scanner(program);
    auto tokens = scanner.scanTokens();
    EXPECT_FALSE(tokens.empty());

    StlParser parser(tokens);
    auto statements = parser.parse();

    EXPECT_EQ(4U, statements.size());
  }

  // variant, optional, array, sequence, quantity...
  {
    std::string program =
      R"(
struct HideCursor;
struct ShowCursor;
struct SaveCursorPosition;
struct RestoreCursorPosition;
struct MoveCursorLeft { cells: u32 }
struct MoveCursorRight { cells: u32 }
struct MoveCursorUp { cells: u32 }
struct MoveCursorDown { cells: u32 }
struct Print { text : string }

struct CPrint
{
  flags : u32,
  color : u32,
  text  : string
}

variant TerminalCommand
{
  HideCursor,
  ShowCursor,
  SaveCursorPosition,
  RestoreCursorPosition,
  MoveCursorLeft,
  MoveCursorRight,
  MoveCursorUp,
  MoveCursorDown,
  Print,
  CPrint
}

optional<f64> MaybeFloat64;
quantity<f64, deg> Lat [min: -90.0,  max: 90.0];
sequence<f32, 100> MyBoundedSequence;
array<u32, 3> MyArray;

    )";

    StlScanner scanner(program);
    auto tokens = scanner.scanTokens();
    EXPECT_FALSE(tokens.empty());

    StlParser parser(tokens);
    auto statements = parser.parse();

    EXPECT_EQ(15U, statements.size());
  }
}

/// @test
/// Checks the correct tokenization of classes that may look as structures
/// @requirements(SEN-363)
TEST(Parser, classesAsStructures)
{
  // Detect mixup of class and struct
  {
    std::string program =
      R"(
class CPrint
{
  flags : u32,
  color : u32,
  text  : string
}

    )";

    StlScanner scanner(program);
    auto tokens = scanner.scanTokens();
    EXPECT_FALSE(tokens.empty());

    StlParser parser(tokens);
    EXPECT_ANY_THROW(auto statements = parser.parse());
  }
}

/// @test
/// Checks correct tokenization of a basic sen query with no condition
/// @requirements(SEN-363)
TEST(Parser, basicQuery)
{
  std::string program = "SELECT rpr.PhysicalEntity FROM se.env";
  StlScanner scanner(program);
  auto tokens = scanner.scanTokens();

  EXPECT_EQ(9U, tokens.size());
  StlParser parser(tokens);
  auto query = parser.parseQuery();

  EXPECT_TRUE(query.type.has_value());
  EXPECT_EQ(query.type.value().qualifiedName, "rpr.PhysicalEntity");
  EXPECT_EQ(query.bus.session.lexeme(), "se");
  EXPECT_EQ(query.bus.bus.lexeme(), "env");
}

/// @test
/// Check correct tokenization of sen queries with different conditions and operators
/// @requirements(SEN-363)
TEST(Parser, queryCondition)
{
  std::vector<std::string> programs = {
    R"(SELECT rpr.PhysicalEntity FROM se.env WHERE name == "ownship")",
    R"(SELECT rpr.PhysicalEntity FROM se.env WHERE lat == 8 AND count == 4)",
    R"(SELECT rpr.PhysicalEntity FROM se.env WHERE lat < 13)",
    R"(SELECT rpr.PhysicalEntity FROM se.env WHERE lat >= 13)",
    R"(SELECT rpr.PhysicalEntity FROM se.env WHERE lat >= 13 AND lon > 14.4)",
    R"(SELECT rpr.PhysicalEntity FROM se.env WHERE lat >= 13 AND (lon > 14.4 OR lon < 16))"};

  for (const auto& program: programs)
  {
    StlScanner scanner(program);
    auto tokens = scanner.scanTokens();

    StlParser parser(tokens);
    auto query = parser.parseQuery();

    EXPECT_TRUE(query.type.has_value());
    EXPECT_EQ(query.type.value().qualifiedName, "rpr.PhysicalEntity");
    EXPECT_EQ(query.bus.session.lexeme(), "se");
    EXPECT_EQ(query.bus.bus.lexeme(), "env");
    EXPECT_TRUE(query.condition);
  }
}

/// @test
/// Checks the correct parsing of comments in an STL file
/// @requirements(SEN-903)
TEST(Parser, commentValidation)
{
  // All comments correct. Comments before definition, inline comments. Correct commented parameters, just once
  {
    std::string program = R"(
class Person
{
  // This variable stores the first name of a person
  var firstName     : string [static];
  var surName       : string [static]; // This variable stores the surname of a person
  // This variable stores the brain activity of a person
  var brainActivity : f32; // It is a f32

  // method that takes a string and returns another
  // @param question1 String question to perform
  // @param question2: String question to perform
  fn ask(question1 : string, question2 : string) -> string;

  // an event that requires confirmed transport
  event saidSomething(words : string) [confirmed]; // This is an event
}

    )";

    StlScanner scanner(program);
    auto tokens = scanner.scanTokens();
    EXPECT_FALSE(tokens.empty());

    StlParser parser(tokens);
    auto statements = parser.parse();
    EXPECT_EQ(1U, statements.size());

    for (const auto& statement: statements)
    {
      if (std::holds_alternative<sen::lang::StlClassStatement>(statement))
      {
        const auto& classDecl = std::get<sen::lang::StlClassStatement>(statement);
        const auto& properties = classDecl.members.properties;
        const auto& functions = classDecl.members.functions;
        const auto& events = classDecl.members.events;

        ASSERT_EQ("This variable stores the first name of a person", properties[0].description[0].lexeme());
        ASSERT_EQ("This variable stores the surname of a person", properties[1].description[0].lexeme());
        ASSERT_EQ("This variable stores the brain activity of a person", properties[2].description[0].lexeme());
        ASSERT_EQ("It is a f32", properties[2].description[1].lexeme());

        ASSERT_EQ("method that takes a string and returns another", functions[0].description[0].lexeme());

        ASSERT_EQ("an event that requires confirmed transport", events[0].description[0].lexeme());
        ASSERT_EQ("This is an event", events[0].description[1].lexeme());
      }
    }
    EXPECT_NO_THROW(std::ignore = statements);
  }

  // Parameter documented more than once
  {
    std::string program = R"(
class Person
{
  // @param question String question to perform
  // @param question String question to perform
  fn ask(question : string) -> string;
}

    )";

    StlScanner scanner(program);
    auto tokens = scanner.scanTokens();
    EXPECT_FALSE(tokens.empty());

    StlParser parser(tokens);
    EXPECT_ANY_THROW(auto statements = parser.parse());
  }

  // Parameter documented and not defined
  {
    std::string program = R"(
class Person
{
  // @param name String question to perform
  fn ask(question : string) -> string;
}

    )";

    StlScanner scanner(program);
    auto tokens = scanner.scanTokens();
    EXPECT_FALSE(tokens.empty());

    StlParser parser(tokens);
    EXPECT_ANY_THROW(auto statements = parser.parse());
  }
}
