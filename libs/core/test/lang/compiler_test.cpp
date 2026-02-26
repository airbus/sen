// === compiler_test.cpp ===============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

// sen
#include "sen/core/base/numbers.h"
#include "sen/core/lang/vm.h"

// google test
#include <gtest/gtest.h>

// std
#include <csignal>
#include <string>
#include <tuple>
#include <utility>
#include <variant>
#include <vector>

using sen::lang::VM;

namespace
{

bool checkQuery(std::string& program)
{
  VM vm;

  // compile and check program compilation went ok
  auto result = vm.compile(vm.parse(program));
  EXPECT_TRUE(result.isOk());

  // disassemble chunk of code
  auto chunk = std::move(result).getValue();
  chunk.disassemble("test");

  // interpret chunk of code and check each byte of the chunk was interpreted correctly as instruction
  auto codeResult = vm.interpret(chunk);
  EXPECT_TRUE(codeResult.isOk());

  EXPECT_TRUE(std::holds_alternative<bool>(codeResult.getValue()));
  return std::get<bool>(codeResult.getValue());
}

void checkCompilationFailure(std::string& program)
{
  VM vm;

  EXPECT_ANY_THROW(std::ignore = checkQuery(program));
}

}  // namespace

/// @test
/// Checks vm compiler with basic condition
/// @requirements(SEN-363)
TEST(Compiler, basic)
{
  // condition is true
  {
    std::string program = R"(SELECT rpr.PhysicalEntity FROM se.env WHERE "cucu" = "ownship" OR (9 > 5 OR 1 <
    7))";

    EXPECT_TRUE(checkQuery(program));
  }

  // condition is false
  {
    std::string program = R"(SELECT rpr.PhysicalEntity FROM se.env WHERE "cucu" = "ownship" AND (9 > 5 OR 1 <
    7))";

    EXPECT_FALSE(checkQuery(program));
  }

  // any class
  {
    std::string program = R"(SELECT some.obj FROM some.bus WHERE -10 < 0)";

    EXPECT_TRUE(checkQuery(program));
  }

  // condition is true 2
  {
    std::string program = R"(SELECT MY_CLASS_NAME.OTHER.THIS_THAT FROM SOME_SESSION._____BUS WHERE 1 > 0)";

    EXPECT_TRUE(checkQuery(program));
  }

  // comma use
  {
    std::string program = R"(SELECT class1 FROM any.bus WHERE 1 = 1, false)";

    EXPECT_TRUE(checkQuery(program));
  }

  // queries cascade
  {
    std::string program = R"(SELECT rpr.Aircraft FROM se.env WHERE velocity >= 1.0
                             SELECT rpr.PhysicalEntity FROM se.env WHERE latitude < 225)";

    VM vm;
    auto result = vm.compile(vm.parse(program));

    EXPECT_TRUE(result.isOk());
    auto chunk = std::move(result).getValue();
    chunk.disassemble("test");

    float64_t velocity = 40.0;
    float64_t latitude = 35.0;

    std::vector<sen::lang::ValueGetter> environment = {
      [&]() { return velocity; },
      [&]() { return latitude; },
    };

    auto codeResult = vm.interpret(chunk, environment);
    EXPECT_TRUE(codeResult.isOk());

    EXPECT_TRUE(std::holds_alternative<bool>(codeResult.getValue()));
    EXPECT_TRUE(std::get<bool>(codeResult.getValue()));

    velocity = -4.5;
    codeResult = vm.interpret(chunk, environment);
    EXPECT_TRUE(codeResult.isOk());

    EXPECT_TRUE(std::holds_alternative<bool>(codeResult.getValue()));
    EXPECT_FALSE(std::get<bool>(codeResult.getValue()));
  }

  // queries cascade
  {
    std::string program = R"(SELECT rpr.Aircraft       FROM se.env WHERE 6 >= 1.0
                             SELECT rpr.PhysicalEntity FROM se.env WHERE 3 < 225)";

    EXPECT_TRUE(checkQuery(program));
  }
}

/// @test
/// Checks use of true and false keywords in queries
/// @requirements(SEN-363)
TEST(Compiler, trueFalse)
{
  // basic true
  {
    std::string program = R"(SELECT someDummyClass FROM dummy.bus WHERE true)";

    EXPECT_TRUE(checkQuery(program));
  }

  // basic false
  {
    std::string program = R"(SELECT someDummyClass FROM dummy.bus WHERE false)";

    EXPECT_FALSE(checkQuery(program));
  }

  {
    std::string program = R"(SELECT someDummyClass FROM dummy.bus WHERE true OR false)";

    EXPECT_TRUE(checkQuery(program));
  }

  {
    std::string program = R"(SELECT someDummyClass FROM dummy.bus WHERE true AND false)";

    EXPECT_FALSE(checkQuery(program));
  }

  {
    std::string program = R"(SELECT someDummyClass FROM dummy.bus WHERE NOT true)";

    EXPECT_FALSE(checkQuery(program));
  }

  {
    std::string program = R"(SELECT someDummyClass FROM dummy.bus WHERE true AND NOT false)";

    EXPECT_TRUE(checkQuery(program));
  }

  {
    std::string program = R"(SELECT someDummyClass FROM dummy.bus WHERE true AND !false)";

    EXPECT_TRUE(checkQuery(program));
  }
}

/// @test
/// Checks vm compiler with variable as condition
/// @requirements(SEN-363)
TEST(Compiler, variables)
{
  std::string program = R"(SELECT rpr.PhysicalEntity FROM se.env WHERE situation.lat > 33.0 AND lon < 44.53)";

  VM vm;
  auto result = vm.compile(vm.parse(program));

  EXPECT_TRUE(result.isOk());
  auto chunk = std::move(result).getValue();
  chunk.disassemble("test");

  float64_t lat = 40.0;
  float64_t lon = 35.0;

  std::vector<sen::lang::ValueGetter> environment = {
    [&]() { return lat; },
    [&]() { return lon; },
  };

  auto codeResult = vm.interpret(chunk, environment);
  EXPECT_TRUE(codeResult.isOk());

  EXPECT_TRUE(std::holds_alternative<bool>(codeResult.getValue()));
  EXPECT_TRUE(std::get<bool>(codeResult.getValue()));

  lon = 44.54;
  codeResult = vm.interpret(chunk, environment);
  EXPECT_TRUE(codeResult.isOk());

  EXPECT_TRUE(std::holds_alternative<bool>(codeResult.getValue()));
  EXPECT_FALSE(std::get<bool>(codeResult.getValue()));
}

/// @test
/// Checks vm compiler with two variables as condition
/// @requirements(SEN-363)
TEST(Compiler, variables2)
{
  std::string program = R"(SELECT rpr.PhysicalEntity FROM se.env WHERE name = "myWeapon")";

  std::vector<sen::lang::ValueGetter> environment = {
    []() { return std::string("myWeapon"); },
  };

  {
    VM vm;
    auto result = vm.compile(vm.parse(program));

    EXPECT_TRUE(result.isOk());
    auto chunk = std::move(result).getValue();
    chunk.disassemble("test");

    std::ignore = chunk.getOrRegisterVariable("name");

    auto codeResult = vm.interpret(chunk, environment);
    EXPECT_TRUE(codeResult.isOk());

    EXPECT_TRUE(std::holds_alternative<bool>(codeResult.getValue()));
    EXPECT_TRUE(std::get<bool>(codeResult.getValue()));
  }

  {
    VM vm;
    auto result = vm.compile(vm.parse(program));
    EXPECT_TRUE(result.isOk());
    auto chunk = std::move(result).getValue();
    chunk.disassemble("test");

    environment.front() = []() { return std::string("xyz"); };

    auto codeResult = vm.interpret(chunk, environment);
    EXPECT_TRUE(codeResult.isOk());
    EXPECT_TRUE(std::holds_alternative<bool>(codeResult.getValue()));
    EXPECT_FALSE(std::get<bool>(codeResult.getValue()));
  }
}

/// @test
/// Checks incorrect program definition in vm compiler
/// @requirements(SEN-363)
TEST(Compiler, failure)
{  // no condition
  {
    std::string program = R"(SELECT rpr.PhysicalEntity FROM se.env WHERE)";

    checkCompilationFailure(program);
  }

  // no session name
  {
    std::string program = R"(SELECT myClassName FROM WHERE 1 > 2)";

    checkCompilationFailure(program);
  }

  // no bus name
  {
    std::string program = R"(SELECT myClassName FROM badsession WHERE 1 > 2)";

    checkCompilationFailure(program);
  }

  // no bus name 2
  {
    std::string program = R"(SELECT myClassName FROM badsession. WHERE 1 > 2)";

    checkCompilationFailure(program);
  }

  // bad bus name
  {
    std::string program = R"(SELECT myClassName FROM badsession.* WHERE 1 > 2)";

    checkCompilationFailure(program);
  }

  // no class name
  {
    std::string program = R"(SELECT FROM asce.c WHERE 1 > 2)";

    checkCompilationFailure(program);
  }

  // only query keywords
  {
    std::string program = R"(SELECT FROM WHERE)";

    checkCompilationFailure(program);
  }

  // empty
  {
    std::string program;

    checkCompilationFailure(program);
  }

  // binary operator 1
  {
    std::string program = R"(SELECT rpr.object FROM bla.bla WHERE +)";

    checkCompilationFailure(program);
  }

  // binary operator 2
  {
    std::string program = R"(SELECT rpr.aircraft FROM bla.bla WHERE =)";

    checkCompilationFailure(program);
  }

  // binary operator 3
  {
    std::string program = R"(SELECT rpr.aircraft FROM bla.bla WHERE < 3)";

    checkCompilationFailure(program);
  }

  // binary operator 4
  {
    std::string program = R"(SELECT rpr.aircraft FROM bla.bla WHERE  2<)";

    checkCompilationFailure(program);
  }

#ifndef WIN32
  // TODO Fix in windows
  // bad chars
  {
    std::string program = R"(SELECT &%$ FROM bla.bla WHERE 1)";

    checkCompilationFailure(program);
  }

  // runtime error (bad between use)
  {
    std::string program = R"(SELECT rpr.PhysicalEntity FROM se.env WHERE 5834.43 BETWEEN _ AND _)";

    EXPECT_EXIT(checkQuery(program), ::testing::KilledBySignal(SIGABRT), ".*");
  }

  // runtime error (undefined var)
  {
    std::string program = R"(SELECT rpr.Platform FROM se.env WHERE coordinate.x > 125.35)";

    EXPECT_EXIT(checkQuery(program), ::testing::KilledBySignal(SIGABRT), ".*");
  }

#endif
}

/// @test
/// Check use of floating numbers in queries
/// @requirements(SEN-363)
TEST(Compiler, floatingNumbers)
{
  // condition is true
  {
    std::string program =
      R"(SELECT rpr.PhysicalEntity FROM se.env WHERE "myvar" = "true" OR (9 != 9.1 AND 3.141516 >= 3.14))";

    EXPECT_TRUE(checkQuery(program));
  }

  // condition is false
  {
    std::string program = R"(SELECT myClassName FROM se.env WHERE "cucu" = "ownship" OR 9.100 = 9.101 OR 2.674 > 3.14)";

    EXPECT_FALSE(checkQuery(program));
  }

  // equal
  {
    std::string program =
      R"(SELECT rpr.PhysicalEntity FROM se.env WHERE (9.0001 = 9.0001 AND -1.1923 = -1.1923 AND 0.0 = 0))";

    EXPECT_TRUE(checkQuery(program));
  }

  // not equal
  {
    std::string program =
      R"(SELECT myClassName FROM se.env WHERE -3.14159 != 3.14159 AND (23.0000000001 != 23.0) AND 76 != 75.9999999)";

    EXPECT_TRUE(checkQuery(program));
  }

  // NOT equal
  {
    std::string program =
      R"(SELECT myClassName FROM se.env WHERE NOT (-3.14159 = 3.14158) AND NOT (23.0000000001 = 23.0) AND NOT (76 = 75.9999999))";

    EXPECT_TRUE(checkQuery(program));
  }
}

/// @test
/// Checks use of strings in queries
/// @requirements(SEN-363)
TEST(Compiler, strings)
{
  // basic
  {
    std::string program = R"(SELECT myClassName FROM se.env WHERE "hello world" = "hello world")";

    EXPECT_TRUE(checkQuery(program));
  }

  // case sensitivity
  {
    std::string program = R"(SELECT myClassName FROM se.env WHERE "Hello world" = "hello world")";

    EXPECT_FALSE(checkQuery(program));
  }

  // basic
  {
    std::string program = R"(SELECT myClassName FROM se.env WHERE ("$&%·ñ/%·") = "$&%·ñ/%·")";

    EXPECT_TRUE(checkQuery(program));
  }

  // empty
  {
    std::string program = R"(SELECT myClassName FROM se.env WHERE ("" = ""))";

    EXPECT_TRUE(checkQuery(program));
  }

  // empty 2
  {
    std::string program = R"(SELECT myClassName FROM se.env WHERE "" = "")";

    EXPECT_TRUE(checkQuery(program));
  }
}

/// @test
/// Check use of parenthesis in queries
/// @requirements(SEN-363)
TEST(Compiler, parenthesis)
{
  // multiple
  {
    std::string program = R"(SELECT rpr.PhysicalEntity FROM se.env WHERE (((((("myvar1" != "blabla")))))))";

    EXPECT_TRUE(checkQuery(program));
  }

  // different bracket group levels
  {
    std::string program =
      R"(SELECT rpr.PhysicalEntity FROM se.env WHERE ((( ("myvar1" != "blabla") AND (1 < 2 ) )) OR (4 > 5) ))";

    EXPECT_TRUE(checkQuery(program));
  }

  // missing parenthesis
  {
    std::string program =
      R"(SELECT rpr.PhysicalEntity FROM se.env WHERE ((( ("myvar1" != "blabla") AND (1 < 2 ) )) OR (4 > 5)  )";

    checkCompilationFailure(program);
  }
}

/// @test
/// Checks use of bang operator in queries
/// @requirements(SEN-363)
TEST(Compiler, bang)
{
  // basic
  {
    std::string program = R"(SELECT myClass FROM some.bus WHERE !(5 = 4))";

    EXPECT_TRUE(checkQuery(program));
  }

  // concatenated
  {
    std::string program = R"(SELECT myClass FROM some.bus WHERE !(!(5 = 4)))";

    EXPECT_FALSE(checkQuery(program));
  }

  // multiple
  {
    std::string program = R"(SELECT myClass FROM some.bus WHERE !( 5 = 4 OR (3 != 4 AND !(!(20.3 = 20.3)) ) ))";

    EXPECT_FALSE(checkQuery(program));
  }

  // NOT + bang
  {
    std::string program = R"(SELECT myClass FROM some.bus WHERE NOT !(5 = 4))";

    EXPECT_FALSE(checkQuery(program));
  }
}

/// @test
/// Check use of keyword "between" in queries
/// @requirements(SEN-363)
TEST(Compiler, between)
{
  // basic
  {
    std::string program = R"(SELECT rpr.aircraft FROM ascse.env WHERE 5 BETWEEN 4 AND 902492756)";

    EXPECT_TRUE(checkQuery(program));
  }

  // not between
  {
    std::string program = R"(SELECT rpr.aircraftPhysical FROM se.env WHERE 5 NOT BETWEEN 3 AND 4)";

    EXPECT_TRUE(checkQuery(program));
  }

  // no bounds
  {
    std::string program = R"(SELECT rpr.aircraft FROM ascse.env WHERE 20 BETWEEN 20 AND 20)";

    EXPECT_TRUE(checkQuery(program));
  }

  // multiple between keywords
  {
    std::string program = R"(SELECT rpr.aircraft FROM ascse.env WHERE (5 BETWEEN 4 AND 8) OR (-5 BETWEEN 1 AND 10))";

    EXPECT_TRUE(checkQuery(program));
  }

  // floats
  {
    std::string program =
      R"(SELECT rpr.aircraft FROM ascse.env WHERE (534.5 BETWEEN 4 AND 5.5) AND (5 BETWEEN 1 AND 10))";

    EXPECT_FALSE(checkQuery(program));
  }

  // floats 2
  {
    std::string program =
      R"(SELECT rpr.aircraft FROM ascse.env WHERE (534.5 BETWEEN 4 AND 5.5) AND (5 BETWEEN 1 AND 10))";

    EXPECT_FALSE(checkQuery(program));
  }

  // variable
  {
    std::string program =
      R"(SELECT rpr.aircraft FROM ascse.env WHERE (val1 BETWEEN 4 AND 5.5) AND (val2 BETWEEN 1 AND 10))";

    VM vm;
    auto result = vm.compile(vm.parse(program));

    EXPECT_TRUE(result.isOk());
    auto chunk = std::move(result).getValue();
    chunk.disassemble("test");

    float64_t val1 = 5.23;
    float64_t val2 = 7;

    std::vector<sen::lang::ValueGetter> environment = {
      [&]() { return val1; },
      [&]() { return val2; },
    };

    auto codeResult = vm.interpret(chunk, environment);
    EXPECT_TRUE(codeResult.isOk());

    EXPECT_TRUE(std::holds_alternative<bool>(codeResult.getValue()));
    EXPECT_TRUE(std::get<bool>(codeResult.getValue()));

    val2 = 10.0001;
    codeResult = vm.interpret(chunk, environment);
    EXPECT_TRUE(codeResult.isOk());

    EXPECT_TRUE(std::holds_alternative<bool>(codeResult.getValue()));
    EXPECT_FALSE(std::get<bool>(codeResult.getValue()));
  }

  // incorrect
  {
    std::string program = R"(SELECT rpr.PhysicalEntity FROM se.env WHERE 5834.43 BETWEEN )";

    checkCompilationFailure(program);
  }

  // incorrect 2
  {
    std::string program = R"(SELECT rpr.PhysicalEntity FROM se.env WHERE 5834.43 BETWEEN *)";

    checkCompilationFailure(program);
  }

  // incorrect 3
  {
    std::string program = R"(SELECT rpr.PhysicalEntity FROM se.env WHERE 5834.43 BETWEEN 56)";

    checkCompilationFailure(program);
  }

  // incorrect 4
  {
    std::string program = R"(SELECT rpr.PhysicalEntity FROM se.env WHERE 5 BETWEEN 10 AND 3)";

    checkCompilationFailure(program);
  }

  // incorrect 5
  {
    std::string program = R"(SELECT rpr.PhysicalEntity FROM se.env WHERE 5 BETWEEN 10 AND -11)";

    checkCompilationFailure(program);
  }
}

/// @test
/// Check use of keyword "in" in queries
/// @requirements(SEN-363)
TEST(Compiler, in)
{
  // basic
  {
    std::string program = R"(SELECT rpr.aircraft FROM ascse.env WHERE 1 IN (1, 2, 3, 4, 5, 6, 7, 8, 9, 10))";

    EXPECT_TRUE(checkQuery(program));
  }

  // NOT keyword
  {
    std::string program = R"(SELECT rpr.aircraft FROM ascse.env WHERE 1 NOT IN (1, 2, 3, 4, 5, 6, 7, 8, 9, 10))";

    EXPECT_FALSE(checkQuery(program));
  }

  // variables
  {
    std::string program =
      R"(SELECT rpr.physicalAircraft FROM se.env WHERE city IN (("Madrid"), ("Barcelona"), ("Pamplona"), ("Vitoria")))";

    VM vm;
    auto result = vm.compile(vm.parse(program));

    EXPECT_TRUE(result.isOk());
    auto chunk = std::move(result).getValue();
    chunk.disassemble("test");

    std::string city = "vitoria";

    std::vector<sen::lang::ValueGetter> environment = {[&]() { return city; }};

    auto codeResult = vm.interpret(chunk, environment);
    EXPECT_TRUE(codeResult.isOk());

    EXPECT_TRUE(std::holds_alternative<bool>(codeResult.getValue()));
    EXPECT_FALSE(std::get<bool>(codeResult.getValue()));

    city = "Vitoria";
    codeResult = vm.interpret(chunk, environment);
    EXPECT_TRUE(codeResult.isOk());

    EXPECT_TRUE(std::holds_alternative<bool>(codeResult.getValue()));
    EXPECT_TRUE(std::get<bool>(codeResult.getValue()));
  }

  // match twice and no ""
  {
    std::string program =
      R"(SELECT rpr.physicalAircraft FROM se.env WHERE valueType IN ("On", "Off", "Standby", "Switch", "Toggle", Switch, "Off"))";

    VM vm;
    auto result = vm.compile(vm.parse(program));

    EXPECT_TRUE(result.isOk());
    auto chunk = std::move(result).getValue();
    chunk.disassemble("test");

    std::string city = "Off";

    std::vector<sen::lang::ValueGetter> environment = {[&]() { return city; }};

    auto codeResult = vm.interpret(chunk, environment);
    EXPECT_TRUE(codeResult.isOk());

    EXPECT_TRUE(std::holds_alternative<bool>(codeResult.getValue()));
    EXPECT_TRUE(std::get<bool>(codeResult.getValue()));

    city = "Switch";
    codeResult = vm.interpret(chunk, environment);
    EXPECT_TRUE(codeResult.isOk());

    EXPECT_TRUE(std::holds_alternative<bool>(codeResult.getValue()));
    EXPECT_TRUE(std::get<bool>(codeResult.getValue()));
  }

  // incorrect
  {
    std::string program =
      R"(SELECT rpr.physicalAircraft FROM se.env WHERE valueType IN "On", "Off", "Standby", "Switch", "Toggle")";

    checkCompilationFailure(program);
  }

  // incorrect 2
  {
    std::string program = R"(SELECT rpr.physicalAircraft FROM se.env WHERE valueType IN "On", "Off", "Standby",)";

    checkCompilationFailure(program);
  }

  // incorrect 3
  {
    std::string program = R"(SELECT rpr.aircraft FROM se.env WHERE IN)";

    checkCompilationFailure(program);
  }

  // empty
  {
    std::string program = R"(SELECT rpr.aircraft FROM se.env WHERE 1 IN ())";

    checkCompilationFailure(program);
  }
}

/// @test
/// Check use of operators (+, -, *, /) in queries
/// @requirements(SEN-363)
TEST(Compiler, operations)
{
  // add and substract
  {
    std::string program = R"(SELECT se.Aircraft FROM se.env WHERE 20.40 + 5.60 = 26.00 AND 20.40 - 5.60 = 14.80)";

    checkQuery(program);
  }

  // allowed double substract
  {
    std::string program = R"(SELECT se.Aircraft FROM se.env WHERE 20.40 -- 10 = 30.40)";

    checkQuery(program);
  }

  // multiplication
  {
    std::string program = R"(SELECT se.Aircraft FROM se.env WHERE 3 * 0.01 >= 0.03 AND 3 * 0.01 <= 0.03)";

    checkQuery(program);
  }

  // division
  {
    std::string program = R"(SELECT se.Aircraft FROM se.env WHERE 3 / 0.01 >= 300 AND 3 / 0.01 <= 300)";

    checkQuery(program);
  }

  // operations priority
  {
    std::string program = R"(SELECT se.PhysicalEntity FROM se.env WHERE 3 + 5 * 2 - 5 = 8)";

    checkQuery(program);
  }

  // grouped operations
  {
    std::string program = R"(SELECT se.PhysicalEntity FROM se.env WHERE (3 + 5) * 2 - 5 = 11)";

    checkQuery(program);
  }

  // variables
  {
    std::string program =
      R"(SELECT rpr.EnvironmentalProcess FROM synthetic.environment WHERE arealObjectMass + linearObjectMass > 500.50
                                                                                        AND   arealObjectMass - linearObjectMass >= 0)";

    VM vm;
    auto result = vm.compile(vm.parse(program));

    EXPECT_TRUE(result.isOk());
    auto chunk = std::move(result).getValue();
    chunk.disassemble("test");

    float32_t arealObjectMass = 375.60;
    float32_t linearObjectMass = 425.90;

    std::vector<sen::lang::ValueGetter> environment = {
      [&]() { return arealObjectMass; },
      [&]() { return linearObjectMass; },
    };

    auto codeResult = vm.interpret(chunk, environment);
    EXPECT_TRUE(codeResult.isOk());

    EXPECT_TRUE(std::holds_alternative<bool>(codeResult.getValue()));
    EXPECT_FALSE(std::get<bool>(codeResult.getValue()));

    arealObjectMass = 425.91;
    codeResult = vm.interpret(chunk, environment);
    EXPECT_TRUE(codeResult.isOk());

    EXPECT_TRUE(std::holds_alternative<bool>(codeResult.getValue()));
    EXPECT_TRUE(std::get<bool>(codeResult.getValue()));
  }

  // between operations
  {
    std::string program =
      R"(SELECT se.Aircraft FROM se.env WHERE currentSpeed BETWEEN commandedSpeed - 10.0 AND commandedSpeed + 10.0)";

    VM vm;
    auto result = vm.compile(vm.parse(program));

    EXPECT_TRUE(result.isOk());
    auto chunk = std::move(result).getValue();
    chunk.disassemble("test");

    float32_t currentSpeed = 20.854;
    float32_t commandedSpeed = 25.5;

    std::vector<sen::lang::ValueGetter> environment = {
      [&]() { return currentSpeed; },
      [&]() { return commandedSpeed; },
    };

    auto codeResult = vm.interpret(chunk, environment);
    EXPECT_TRUE(codeResult.isOk());

    EXPECT_TRUE(std::holds_alternative<bool>(codeResult.getValue()));
    EXPECT_TRUE(std::get<bool>(codeResult.getValue()));

    currentSpeed = 0;
    codeResult = vm.interpret(chunk, environment);
    EXPECT_TRUE(codeResult.isOk());

    EXPECT_TRUE(std::holds_alternative<bool>(codeResult.getValue()));
    EXPECT_FALSE(std::get<bool>(codeResult.getValue()));
  }

  // between operations 2
  {
    std::string program =
      R"(SELECT rpr.aircraft FROM se.env WHERE currentLocation BETWEEN (-0.5 + waypoint) / 2.0 AND waypoint * 2.0 - 1.0)";

    VM vm;
    auto result = vm.compile(vm.parse(program));

    EXPECT_TRUE(result.isOk());
    auto chunk = std::move(result).getValue();
    chunk.disassemble("test");

    float32_t currentLocation = 1.50;
    float32_t waypoint = 2.75;

    std::vector<sen::lang::ValueGetter> environment = {
      [&]() { return currentLocation; },
      [&]() { return waypoint; },
    };

    auto codeResult = vm.interpret(chunk, environment);
    EXPECT_TRUE(codeResult.isOk());

    EXPECT_TRUE(std::holds_alternative<bool>(codeResult.getValue()));
    EXPECT_TRUE(std::get<bool>(codeResult.getValue()));

    currentLocation = -1.50;
    codeResult = vm.interpret(chunk, environment);
    EXPECT_TRUE(codeResult.isOk());

    EXPECT_TRUE(std::holds_alternative<bool>(codeResult.getValue()));
    EXPECT_FALSE(std::get<bool>(codeResult.getValue()));
  }

  // incorrect
  {
    std::string program = R"(SELECT se.Aircraft FROM se.env WHERE 20.40 +)";

    checkCompilationFailure(program);
  }

  // incorrect 2
  {
    std::string program = R"(SELECT se.Aircraft FROM se.env WHERE 34.4 -)";

    checkCompilationFailure(program);
  }

  // incorrect 3
  {
    std::string program = R"(SELECT se.Aircraft FROM se.env WHERE 11.0 *)";

    checkCompilationFailure(program);
  }

  // incorrect 4
  {
    std::string program = R"(SELECT se.Aircraft FROM se.env WHERE 0.4873 /)";

    checkCompilationFailure(program);
  }

  // incorrect 5
  {
    std::string program = R"(SELECT se.Aircraft FROM se.env WHERE 20.40 +* 43.3)";

    checkCompilationFailure(program);
  }
}
