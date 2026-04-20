// === printer_test.cpp ================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

// component
#include "mock_terminal.h"
#include "printer.h"

// sen
#include "sen/core/base/checked_conversions.h"
#include "sen/core/base/compiler_macros.h"
#include "sen/core/base/duration.h"
#include "sen/core/base/result.h"
#include "sen/core/base/span.h"
#include "sen/core/base/timestamp.h"
#include "sen/core/io/input_stream.h"
#include "sen/core/io/output_stream.h"
#include "sen/core/meta/class_type.h"
#include "sen/core/meta/enum_type.h"
#include "sen/core/meta/method.h"
#include "sen/core/meta/native_types.h"
#include "sen/core/meta/optional_type.h"
#include "sen/core/meta/property.h"
#include "sen/core/meta/quantity_type.h"
#include "sen/core/meta/sequence_type.h"
#include "sen/core/meta/struct_type.h"
#include "sen/core/meta/time_types.h"
#include "sen/core/meta/type.h"
#include "sen/core/meta/var.h"
#include "sen/core/meta/variant_type.h"
#include "sen/core/obj/callback.h"
#include "sen/core/obj/detail/native_object_impl.h"
#include "sen/core/obj/native_object.h"
#include "sen/core/obj/object.h"

// generated code
#include "stl/shell.stl.h"

// google test
#include <gmock/gmock.h>
#include <gtest/gtest.h>

// std
#include "sen/core/meta/alias_type.h"

#include <cstdint>
#include <exception>
#include <list>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

using sen::Var;
using sen::components::shell::Printer;
using sen::components::shell::test::MockTerminal;

namespace
{

class PrinterTest: public testing::Test
{
protected:
  void SetUp() override
  {
    mockTerminal = std::make_unique<testing::NiceMock<MockTerminal>>();

    ON_CALL(*mockTerminal, getSize(testing::_, testing::_))
      .WillByDefault(testing::Invoke(
        [](uint32_t& rows, uint32_t& cols)
        {
          rows = 24U;
          cols = 80U;
        }));

    printer = std::make_unique<Printer>(mockTerminal.get(),
                                        sen::components::shell::BufferStyle::hexdump,
                                        sen::components::shell::TimeStyle::timestampUTC);
  }

  std::unique_ptr<testing::NiceMock<MockTerminal>>
    mockTerminal;                    // NOLINT(misc-non-private-member-variables-in-classes)
  std::unique_ptr<Printer> printer;  // NOLINT(misc-non-private-member-variables-in-classes)
};

class DummyObject final: public sen::NativeObject
{
  SEN_NOCOPY_NOMOVE(DummyObject)

public:
  explicit DummyObject(std::string name): NativeObject(std::move(name)) {}
  ~DummyObject() override = default;

  [[nodiscard]] sen::ConstTypeHandle<sen::ClassType> getClass() const noexcept override
  {
    static auto type = []() noexcept -> sen::ConstTypeHandle<sen::ClassType>
    {
      try
      {
        auto u32Spec = sen::PropertySpec("propU32", "Desc", sen::UInt32Type::get());
        const std::vector<sen::StructField> fields = {{"fieldA", "desc", sen::UInt32Type::get()}};
        const auto structType = sen::StructType::make(sen::StructSpec("StructT", "s.StructT", "D", fields));
        auto structSpec = sen::PropertySpec("propStruct", "Desc", structType);

        return sen::ClassType::make(
          sen::ClassSpec {"Dummy", "sen.Dummy", "A dummy class", {u32Spec, structSpec}, {}, {}, std::nullopt, {}});
      }
      catch (...)
      {
        std::terminate();
      }
    }();
    return type;
  }

  [[nodiscard]] sen::TimeStamp getLastCommitTime() const noexcept override
  {
    return sen::TimeStamp(1609459200000000000ULL);
  }

  void invokeAllPropertyCallbacks() override {}

protected:
  Var senImplGetPropertyImpl(const sen::MemberHash propertyId) const override
  {
    if (const auto property = getClass()->searchPropertyByName("propU32");
        property != nullptr && property->getId() == propertyId)
    {
      return {sen::std_util::checkedConversion<uint32_t>(12345)};
    }
    if (const auto property = getClass()->searchPropertyByName("propStruct");
        property != nullptr && property->getId() == propertyId)
    {
      sen::VarMap varMap;
      varMap["fieldA"] = Var(sen::std_util::checkedConversion<uint32_t>(42));
      return {varMap};
    }
    return {};
  }

  void senImplRemoveTypedConnection(sen::ConnId /*id*/) override {}
  void senImplRemoveUntypedConnection(sen::ConnId /*id*/, sen::MemberHash /*memberHash*/) override {}
  void senImplSetNextPropertyUntyped(sen::MemberHash /*propertyId*/, const Var& /*value*/) override {}
  Var senImplGetNextPropertyUntyped(sen::MemberHash /*propertyId*/) const override { return {}; }
  void senImplCommitImpl(sen::TimeStamp /*time*/) override {}
  void senImplWriteChangedPropertiesToStream(sen::OutputStream& /*confirmed*/,
                                             sen::impl::BufferProvider /*uni*/,
                                             sen::impl::BufferProvider /*multi*/) override
  {
  }
  void senImplStreamCall(sen::MemberHash /*methodId*/,
                         sen::InputStream& /*in*/,
                         sen::StreamCallForwarder&& /*fwd*/) override
  {
  }
  void senImplVariantCall(sen::MemberHash /*methodId*/,
                          const sen::VarList& /*args*/,
                          sen::VariantCallForwarder&& /*fwd*/) override
  {
  }
  sen::impl::FieldValueGetter senImplGetFieldValueGetter(sen::MemberHash /*propertyId*/,
                                                         sen::Span<uint16_t> /*fields*/) const override
  {
    return {};
  }
  uint32_t senImplComputeMaxReliableSerializedPropertySizeImpl() const override { return 0U; }
  void senImplWriteAllPropertiesToStream(sen::OutputStream& /*out*/) const override {}
  void senImplWriteStaticPropertiesToStream(sen::OutputStream& /*out*/) const override {}
  void senImplWriteDynamicPropertiesToStream(sen::OutputStream& /*out*/) const override {}
};

}  // namespace

/// @test
/// Verifies that text exceeding the width limit is wrapped correctly, and extremely long words avoid creating a blank
/// leading line
/// @requirements(SEN-369)
TEST_F(PrinterTest, FormatTextForWidthExactness)
{
  const std::string input = "12345 67890 12345 67890";
  const std::string result = Printer::formatTextForWidth(input, 11U, 2U);
  EXPECT_EQ(result, "12345 67890\n  12345 67890");

  const std::string longWord = "ThisWordIsWayTooLongForThisWidth";
  const std::string resultLong = Printer::formatTextForWidth(longWord, 10U, 2U);
  EXPECT_EQ(resultLong, "ThisWordIsWayTooLongForThisWidth");

  const std::string combined = "ThisWordIsWayTooLongForThisWidth Normal";
  const std::string resultCombined = Printer::formatTextForWidth(combined, 10U, 2U);
  EXPECT_EQ(resultCombined, "ThisWordIsWayTooLongForThisWidth\n  Normal");
}

/// @test
/// Verifies the correct output string generation when an error is printed
/// @requirements(SEN-369, SEN-1049)
TEST_F(PrinterTest, PrintErrorExactOutput)
{
  // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg, hicpp-vararg)
  printer->printError("Connection %s failed code %d", "timeout", 404);
  EXPECT_EQ(mockTerminal->getOutputBuffer(), "  Error: Connection timeout failed code 404\n");
}

/// @test
/// Verifies method call results handling for void, valid primitive values, and exceptions
/// @requirements(SEN-369, SEN-1049)
TEST_F(PrinterTest, PrintMethodCallResultAllPaths)
{
  const sen::MethodSpec mSpecVoid {{"mVoid", "desc"}, sen::VoidType::get()};
  const auto methodVoid = sen::Method::make(mSpecVoid);

  const sen::MethodResult<Var> resVoid {sen::impl::Ok {Var()}};
  printer->printMethodCallResult(resVoid, methodVoid.get(), "cmd");
  EXPECT_TRUE(mockTerminal->getOutputBuffer().empty());

  const sen::MethodSpec mSpecU32 {{"mU32", "desc"}, sen::UInt32Type::get()};
  const auto methodU32 = sen::Method::make(mSpecU32);

  const sen::MethodResult<Var> resValid {sen::impl::Ok {Var(sen::std_util::checkedConversion<uint32_t>(42))}};
  printer->printMethodCallResult(resValid, methodU32.get(), "cmd");
  EXPECT_EQ(mockTerminal->getOutputBuffer(), "\n42\n");

  mockTerminal->clearOutputBuffer();

  const sen::MethodResult<Var> resError {
    sen::impl::Err {std::make_exception_ptr(std::runtime_error("Test Exception"))}};
  printer->printMethodCallResult(resError, methodU32.get(), "cmd");
  EXPECT_EQ(mockTerminal->getOutputBuffer(), "  Error: call error: Test Exception\n");
}

/// @test
/// Evaluates text representation of struct hierarchies inside an object
/// @requirements(SEN-369, SEN-573)
TEST_F(PrinterTest, PrintPropertiesExactFormatting)
{
  DummyObject dummy("testDummy");
  printer->printProperties(&dummy);

  const std::string out = mockTerminal->getOutputBuffer();
  EXPECT_THAT(out, testing::HasSubstr("state at 2021-01-01"));
  EXPECT_THAT(out, testing::HasSubstr("- propU32: 12345"));
  EXPECT_THAT(out, testing::HasSubstr("- propStruct: "));
  EXPECT_THAT(out, testing::HasSubstr("  - fieldA: 42"));
}

/// @test
/// Verifies variant error handling for bad content and out-of-bounds indices
/// @requirements(SEN-369, SEN-1053)
TEST_F(PrinterTest, PrintValueVariantErrorPaths)
{
  const std::vector<sen::VariantField> fields = {{0, "desc", sen::UInt32Type::get()}};
  const auto spec = sen::VariantSpec {"MyVar", "sen.MyVar", "Desc", fields};
  auto type = sen::VariantType::make(spec);

  EXPECT_ANY_THROW(printer->printValue(Var(sen::std_util::checkedConversion<uint32_t>(10)), 0, type.type()));

  const sen::KeyedVar keyedVarIncorrect {99, std::make_shared<Var>(sen::std_util::checkedConversion<uint32_t>(55))};
  EXPECT_ANY_THROW(printer->printValue(Var(keyedVarIncorrect), 0, type.type()));
}

/// @test
/// Verifies sequence hexdump formatting wrapping exactly at 8 bytes
/// @requirements(SEN-369, SEN-577)
TEST_F(PrinterTest, PrintValueSequenceHexdumpWrapping)
{
  const auto spec = sen::SequenceSpec {"Seq", "sen.Seq", "Desc", sen::UInt8Type::get()};
  auto type = sen::SequenceType::make(spec);

  sen::VarList varListLong;
  for (uint8_t i = 0; i < 9; ++i)
  {
    varListLong.emplace_back(i);
  }

  printer->printValue(Var(varListLong), 1, type.type());
  const std::string out = mockTerminal->getOutputBuffer();
  EXPECT_THAT(out, testing::HasSubstr("00 01 02 03 04 05 06 07 \n    08 "));
}

/// @test
/// Verifies enumeration description truncations when text exceeds width bounds
/// @requirements(SEN-369, SEN-902)
TEST_F(PrinterTest, PrintValueEnumTruncation)
{
  const std::vector<sen::Enumerator> enums = {{"E1", 1, std::string(100, 'A')}};
  const auto spec = sen::EnumSpec("MyEnum", "sen.MyEnum", "Desc", enums, sen::UInt32Type::get());
  auto type = sen::EnumType::make(spec);

  printer->printValue(Var(sen::std_util::checkedConversion<uint32_t>(1)), 0, type.type());
  const std::string out = mockTerminal->getOutputBuffer();
  EXPECT_THAT(out, testing::HasSubstr("E1 ("));
  EXPECT_THAT(out, testing::HasSubstr("..)"));
}

/// @test
/// Verifies that empty variables output exactly '<empty>' string
/// @requirements(SEN-369)
TEST_F(PrinterTest, PrintEmptyValue)
{
  printer->printValue(Var(), 1, sen::StringType::get().type());
  EXPECT_EQ(mockTerminal->getOutputBuffer(), "<empty>");
}

/// @test
/// Validates duration types output seconds properly formatted
/// @requirements(SEN-369, SEN-575)
TEST_F(PrinterTest, PrintDurationValue)
{
  printer->printValue(Var(sen::Duration::fromHertz(1.0)), 0, sen::DurationType::get().type());
  EXPECT_THAT(mockTerminal->getOutputBuffer(), testing::HasSubstr("1.000000 s"));
}

/// @test
/// Validates correct visual generation of tables enforcing truncation limits on the last column
/// @requirements(SEN-369)
TEST_F(PrinterTest, PrintTableTruncation)
{
  const std::vector<std::string> header = {"H1", "H2"};
  std::string longText =
    "This text is very long and should be truncated to fit the terminal width of 80 characters without going "
    "completely out of bounds or crashing.";
  const std::vector<std::vector<std::string>> table = {{"V1", longText}};

  Printer::printTable(header, table, 0, false, mockTerminal.get());
  EXPECT_THAT(mockTerminal->getOutputBuffer(), testing::HasSubstr(".."));
  EXPECT_TRUE(mockTerminal->getOutputBuffer().length() <= 85);
}

/// @test
/// Evaluates the printWelcome logic
/// @requirements(SEN-369)
TEST_F(PrinterTest, PrintWelcomeFormatGeneration)
{
  Printer::printWelcome(mockTerminal.get());

  const std::string out = mockTerminal->getOutputBuffer();
  EXPECT_THAT(out, testing::HasSubstr("compiler "));
  EXPECT_THAT(out, testing::HasSubstr("revision "));
  EXPECT_THAT(out, testing::HasSubstr("branch "));
}

/// @test
/// Evaluates text representation of instances in a list
/// @requirements(SEN-369)
TEST_F(PrinterTest, PrintInstancesLogic)
{
  DummyObject dummy("testDummy1");
  const std::list<sen::Object*> objectsList = {&dummy};

  printer->printInstances(objectsList);

  const std::string out = mockTerminal->getOutputBuffer();
  EXPECT_THAT(out, testing::HasSubstr("testDummy1"));
  EXPECT_THAT(out, testing::HasSubstr("[Dummy]"));
  EXPECT_THAT(out, testing::HasSubstr("[Local]"));
}

/// @test
/// Checks that object description generates all sections: ID, properties and methods
/// @requirements(SEN-369)
TEST_F(PrinterTest, PrintDescriptionObject)
{
  const DummyObject dummy("testDummy");
  printer->printDescription(dummy);

  const std::string out = mockTerminal->getOutputBuffer();
  EXPECT_THAT(out, testing::HasSubstr("OBJECT testDummy"));
  EXPECT_THAT(out, testing::HasSubstr("CLASS sen.Dummy"));
  EXPECT_THAT(out, testing::HasSubstr("PROPERTIES"));
  EXPECT_THAT(out, testing::HasSubstr("propU32"));
  EXPECT_THAT(out, testing::HasSubstr("propStruct"));
}

/// @test
/// Verifies that alias and optional types are correctly unwrapped and printed by TypeWriter
/// @requirements(SEN-369)
TEST_F(PrinterTest, PrintValueAliasAndOptional)
{
  const auto aliasSpec = sen::AliasSpec {"MyAlias", "sen.MyAlias", "Desc", sen::Int32Type::get()};
  const auto aliasType = sen::AliasType::make(aliasSpec);

  printer->printValue(Var(sen::std_util::checkedConversion<int32_t>(-99)), 0, aliasType.type());
  EXPECT_THAT(mockTerminal->getOutputBuffer(), testing::HasSubstr("-99"));

  mockTerminal->clearOutputBuffer();

  const auto optSpec = sen::OptionalSpec {"MyOpt", "sen.MyOpt", "Desc", sen::Int32Type::get()};
  const auto optType = sen::OptionalType::make(optSpec);

  printer->printValue(Var(sen::std_util::checkedConversion<int32_t>(100)), 0, optType.type());
  EXPECT_THAT(mockTerminal->getOutputBuffer(), testing::HasSubstr("100"));
}

/// @test
/// Ensures that Native, Integral and Real types output correct descriptive structures
/// @requirements(SEN-369)
TEST_F(PrinterTest, PrintDescriptionNumericTypes)
{
  printer->printDescription(*sen::UInt32Type::get());
  std::string out = mockTerminal->getOutputBuffer();
  EXPECT_THAT(out, testing::HasSubstr("INTEGRAL TYPE"));
  EXPECT_THAT(out, testing::HasSubstr("size in bytes: 4"));
  EXPECT_THAT(out, testing::HasSubstr("is signed:     no"));

  mockTerminal->clearOutputBuffer();

  printer->printDescription(*sen::Float64Type::get());
  out = mockTerminal->getOutputBuffer();
  EXPECT_THAT(out, testing::HasSubstr("REAL TYPE"));
  EXPECT_THAT(out, testing::HasSubstr("size in bytes: 8"));
  EXPECT_THAT(out, testing::HasSubstr("has infinity:  yes"));
}

/// @test
/// Ensures that sequence type properties are properly described
/// @requirements(SEN-369)
TEST_F(PrinterTest, PrintDescriptionSequenceType)
{
  const auto spec = sen::SequenceSpec {"BoundedSeq", "sen.BoundedSeq", "Desc", sen::UInt8Type::get(), 10U, false};
  const auto type = sen::SequenceType::make(spec);

  printer->printDescription(*type);
  const std::string out = mockTerminal->getOutputBuffer();
  EXPECT_THAT(out, testing::HasSubstr("SEQUENCE TYPE"));
  EXPECT_THAT(out, testing::HasSubstr("element type:  u8"));
  EXPECT_THAT(out, testing::HasSubstr("is bounded:    yes"));
  EXPECT_THAT(out, testing::HasSubstr("max size:      10"));
}

/// @test
/// Verifies that printing an enum error correctly displays the allowed values in a table format.
/// @requirements(SEN-369)
TEST_F(PrinterTest, PrintEnumErrorFormatting)
{
  const std::vector<sen::Enumerator> enums = {{"StateA", 1, "First state"}, {"StateB", 2, "Second state"}};
  const auto spec = sen::EnumSpec("MyEnum", "sen.MyEnum", "Desc", enums, sen::UInt32Type::get());
  const auto type = sen::EnumType::make(spec);

  printer->printEnumError("myArg", type->asEnumType());
  std::string out = mockTerminal->getOutputBuffer();
  EXPECT_THAT(out, testing::HasSubstr("Error: Invalid enumeration for argument 'myArg'."));
  EXPECT_THAT(out, testing::HasSubstr("StateA"));
  EXPECT_THAT(out, testing::HasSubstr("[1]"));
  EXPECT_THAT(out, testing::HasSubstr("Second state"));

  mockTerminal->clearOutputBuffer();

  printer->printEnumError("", type->asEnumType());
  out = mockTerminal->getOutputBuffer();
  EXPECT_THAT(out, testing::HasSubstr("Error: Invalid enumeration value. Allowed values are:"));
  EXPECT_THAT(out, testing::HasSubstr("StateB"));
}

/// @test
/// Verifies the string conversion fallback inside TypeWriter for generic native types
/// @requirements(SEN-369)
TEST_F(PrinterTest, PrintValueBooleanType)
{
  printer->printValue(Var(true), 0, sen::BoolType::get().type());
  EXPECT_THAT(mockTerminal->getOutputBuffer(), testing::HasSubstr("true"));

  mockTerminal->clearOutputBuffer();

  printer->printValue(Var(false), 0, sen::BoolType::get().type());
  EXPECT_THAT(mockTerminal->getOutputBuffer(), testing::HasSubstr("false"));
}

/// @test
/// Verifies the correct output string generation for quantities with and without units
/// @requirements(SEN-369)
TEST_F(PrinterTest, PrintValueQuantityType)
{
  const auto qtySpec = sen::QuantitySpec {"MyQty", "sen.MyQty", "Desc", sen::Float32Type::get()};
  const auto qtyType = sen::QuantityType::make(qtySpec);

  printer->printValue(Var("9.81"), 0, qtyType.type());
  EXPECT_THAT(mockTerminal->getOutputBuffer(), testing::HasSubstr("9.81"));
}

/// @test
/// Verifies that a valid variant type is printed correctly, formatting its inner type
/// @requirements(SEN-369)
TEST_F(PrinterTest, PrintValueVariantSuccessPath)
{
  const std::vector<sen::VariantField> fields = {{0, "desc", sen::UInt32Type::get()}};
  const auto spec = sen::VariantSpec {"MyVar", "sen.MyVar", "Desc", fields};
  const auto type = sen::VariantType::make(spec);

  const sen::KeyedVar keyedVar {0, std::make_shared<Var>(sen::std_util::checkedConversion<uint32_t>(55))};
  printer->printValue(Var(keyedVar), 0, type.type());

  const std::string out = mockTerminal->getOutputBuffer();
  EXPECT_THAT(out, testing::HasSubstr("- type:  u32"));
  EXPECT_THAT(out, testing::HasSubstr("- value: 55"));
}

/// @test
/// Verifies that normal sequence types are printed using the standard list format
/// @requirements(SEN-369)
TEST_F(PrinterTest, PrintValueSequenceStandardFormatting)
{
  const auto spec = sen::SequenceSpec {"SeqU32", "sen.SeqU32", "Desc", sen::UInt32Type::get()};
  const auto type = sen::SequenceType::make(spec);

  const sen::VarList varList = {Var(sen::std_util::checkedConversion<uint32_t>(10)),
                                Var(sen::std_util::checkedConversion<uint32_t>(20))};

  printer->printValue(Var(varList), 0, type.type());
  const std::string out = mockTerminal->getOutputBuffer();
  EXPECT_THAT(out, testing::HasSubstr("[0] 10"));
  EXPECT_THAT(out, testing::HasSubstr("[1] 20"));
}

/// @test
/// Verifies the output when an empty struct is passed to the printer
/// @requirements(SEN-369)
TEST_F(PrinterTest, PrintValueStructEmpty)
{
  const std::vector<sen::StructField> fields;
  const auto structType = sen::StructType::make(sen::StructSpec("EmptyStruct", "s.EmptyStruct", "D", fields));

  printer->printValue(Var(sen::VarMap()), 0, structType.type());
  EXPECT_EQ(mockTerminal->getOutputBuffer(), "<empty>");
}

/// @test
/// Verifies timestamp formatting when local time style is configured
/// @requirements(SEN-369)
TEST_F(PrinterTest, PrintTimestampLocalStyle)
{
  const auto printerLocal = std::make_unique<Printer>(mockTerminal.get(),
                                                      sen::components::shell::BufferStyle::hexdump,
                                                      sen::components::shell::TimeStyle::timestampLocal);

  printerLocal->printValue(Var(sen::TimeStamp(1609459200000000000ULL)), 0, sen::TimestampType::get().type());
  EXPECT_FALSE(mockTerminal->getOutputBuffer().empty());
}

/// @test
/// Verifies TypeDescriptionPrinter handles AliasType correctly
/// @requirements(SEN-369)
TEST_F(PrinterTest, PrintDescriptionAliasType)
{
  const auto aliasSpec = sen::AliasSpec {"MyAlias", "sen.MyAlias", "Desc", sen::Int32Type::get()};
  const auto aliasType = sen::AliasType::make(aliasSpec);

  printer->printDescription(*aliasType);
  const std::string out = mockTerminal->getOutputBuffer();
  EXPECT_THAT(out, testing::HasSubstr("ALIAS TYPE"));
  EXPECT_THAT(out, testing::HasSubstr("aliased type:  i32"));
}
