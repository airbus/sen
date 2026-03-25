// === utils_io_test.cpp ===============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "../support/reader_writer.h"

// sen
#include "sen/core/base/duration.h"
#include "sen/core/base/numbers.h"
#include "sen/core/base/quantity.h"
#include "sen/core/base/span.h"
#include "sen/core/io/input_stream.h"
#include "sen/core/io/output_stream.h"
#include "sen/core/io/util.h"
#include "sen/core/meta/alias_type.h"
#include "sen/core/meta/class_type.h"
#include "sen/core/meta/enum_type.h"
#include "sen/core/meta/native_types.h"
#include "sen/core/meta/optional_type.h"
#include "sen/core/meta/quantity_type.h"
#include "sen/core/meta/sequence_type.h"
#include "sen/core/meta/struct_type.h"
#include "sen/core/meta/time_types.h"
#include "sen/core/meta/type.h"
#include "sen/core/meta/type_visitor.h"
#include "sen/core/meta/var.h"
#include "sen/core/meta/variant_type.h"

// google test
#include <gtest/gtest.h>

// std
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <variant>
#include <vector>

SEN_RANGED_QUANTITY(MyQuantity, float32_t, -15.0f, 15.0f)

struct TrivialType
{
  int32_t x;
  int32_t y;
};

inline bool operator==(const TrivialType& lhs, const TrivialType& rhs) { return lhs.x == rhs.x && lhs.y == rhs.y; }

enum Level : uint8_t
{
  low = 0,
  mid = 1,
  high = 2
};

enum PhonePrefixes : int64_t
{
  usa = 1,
  france = 33,
  spain = 34,
  germany = 49
};

namespace
{
class MockNativeType: public sen::NativeType
{
public:
  MockNativeType(): NativeType(sen::MemberHash(9001)) {}
  [[nodiscard]] std::string_view getName() const noexcept override { return "MockNative"; }
  [[nodiscard]] std::string_view getDescription() const noexcept override { return ""; }
  [[nodiscard]] bool equals(const Type& /*other*/) const noexcept override { return false; }
  void accept(sen::FullTypeVisitor& tv) const override { tv.apply(*this); }
  [[nodiscard]] bool isBounded() const noexcept override { return true; }
};

class MockNumericType: public sen::NumericType
{
public:
  MockNumericType(): NumericType(sen::MemberHash(9002)) {}
  [[nodiscard]] std::string_view getName() const noexcept override { return "MockNumeric"; }
  [[nodiscard]] std::string_view getDescription() const noexcept override { return ""; }
  [[nodiscard]] bool equals(const Type& /*other*/) const noexcept override { return false; }
  void accept(sen::FullTypeVisitor& tv) const override { tv.apply(*this); }
  [[nodiscard]] bool isSigned() const noexcept override { return false; }
  [[nodiscard]] bool hasInfinity() const noexcept override { return false; }
  [[nodiscard]] bool isIEC559() const noexcept override { return false; }
  [[nodiscard]] std::size_t getByteSize() const noexcept override { return 4; }
};

class MockIntegralType: public sen::IntegralType
{
public:
  MockIntegralType(): IntegralType(sen::MemberHash(9003)) {}
  [[nodiscard]] std::string_view getName() const noexcept override { return "MockIntegral"; }
  [[nodiscard]] std::string_view getDescription() const noexcept override { return ""; }
  [[nodiscard]] bool equals(const Type& /*other*/) const noexcept override { return false; }
  void accept(sen::FullTypeVisitor& tv) const override { tv.apply(*this); }
  [[nodiscard]] bool isSigned() const noexcept override { return false; }
  [[nodiscard]] bool hasInfinity() const noexcept override { return false; }
  [[nodiscard]] bool isIEC559() const noexcept override { return false; }
  [[nodiscard]] std::size_t getByteSize() const noexcept override { return 4; }
};

class MockRealType: public sen::RealType
{
public:
  MockRealType(): RealType(sen::MemberHash(9004)) {}
  [[nodiscard]] std::string_view getName() const noexcept override { return "MockReal"; }
  [[nodiscard]] std::string_view getDescription() const noexcept override { return ""; }
  [[nodiscard]] bool equals(const Type& /*other*/) const noexcept override { return false; }
  void accept(sen::FullTypeVisitor& tv) const override { tv.apply(*this); }
  [[nodiscard]] bool isSigned() const noexcept override { return true; }
  [[nodiscard]] bool hasInfinity() const noexcept override { return true; }
  [[nodiscard]] bool isIEC559() const noexcept override { return true; }
  [[nodiscard]] std::size_t getByteSize() const noexcept override { return 4; }
  [[nodiscard]] double getMaxValue() const noexcept override { return 1.0; }
  [[nodiscard]] double getMinValue() const noexcept override { return 0.0; }
  [[nodiscard]] double getEpsilon() const noexcept override { return 0.1; }
};

class MockType: public sen::Type
{
public:
  MockType(): Type(sen::MemberHash(9006)) {}
  [[nodiscard]] std::string_view getName() const noexcept override { return "MockType"; }
  [[nodiscard]] std::string_view getDescription() const noexcept override { return ""; }
  [[nodiscard]] bool equals(const Type& /*other*/) const noexcept override { return false; }
  void accept(sen::FullTypeVisitor& tv) const override { tv.apply(*this); }
  [[nodiscard]] bool isBounded() const noexcept override { return true; }
};

}  // namespace

/// @test
/// Check struct fields insertion and extraction to/from sen var maps
/// @requirements(SEN-1053)
TEST(IOUtils, Structs)
{
  constexpr TrivialType st {132, 332};
  TrivialType st2 {};
  sen::VarMap map;

  EXPECT_TRUE(map.empty());
  sen::impl::structFieldToVarMap("xAxis", st.x, map);
  EXPECT_FALSE(map.empty());

  // check map values;
  for (const auto& [key, var]: map)
  {
    EXPECT_EQ(key, "xAxis");
    EXPECT_EQ(var, st.x);
  }

  sen::impl::extractStructFieldFromMap("xAxis", st2.x, &map);
  EXPECT_EQ(st.x, st2.x);
  EXPECT_NE(st.y, st2.y);

  sen::impl::structFieldToVarMap("yAxis", st.y, map);
  sen::impl::extractStructFieldFromMap("yAxis", st2.y, &map);
  EXPECT_EQ(st.y, st2.y);
}

/// @test
/// Check string split method works ok
/// @requirements(SEN-576)
TEST(IOUtils, splitString)
{
  {
    const std::string string = "hello world this is me";
    std::vector<std::string> expectedVec = {"hello", "world", "this", "is", "me"};

    const auto vec = sen::impl::split(string, ' ');
    EXPECT_EQ(vec.size(), 5);
    EXPECT_EQ(vec, expectedVec);
  }

  // case no delimiter found
  {
    const std::string string = "this is an example string test";
    std::vector<std::string> expectedVec = {"this is an example string test"};

    const auto vec = sen::impl::split(string, 'Y');
    EXPECT_EQ(vec.size(), 1);
    EXPECT_EQ(vec, expectedVec);
  }

  // case-sensitive
  {
    const std::string string = "ZzeezzZzezZ";
    std::vector<std::string> expectedVec = {"", "zeezz", "zez", ""};

    const auto vec = sen::impl::split(string, 'Z');
    EXPECT_EQ(vec.size(), 4);
    EXPECT_EQ(vec, expectedVec);
  }

  // case null char
  {
    const std::string string = "hello world";
    std::vector<std::string> expectedVec = {"hello world\0\0"};  // NOLINT(bugprone-string-literal-with-embedded-nul)

    const auto vec = sen::impl::split(string, '\0');
    EXPECT_EQ(vec.size(), 1);
    EXPECT_EQ(vec, expectedVec);
  }
}

/// @test
/// Check sequence conversion to/from sen variant
/// @requirements(SEN-1053)
TEST(IOUtils, SequenceToFromVariant)
{
  const std::vector vec {10.2f, 11.1f};
  sen::Var myVar;

  sen::impl::sequenceToVariant(vec, myVar);
  std::vector<float32_t> vec2;
  sen::impl::variantToSequence(myVar, vec2);

  EXPECT_EQ(vec[0], vec2[0]);
  EXPECT_EQ(vec[1], vec2[1]);
}

/// @test
/// Check array conversion to/from sen variant
/// @requirements(SEN-1053)
TEST(IOUtils, Array)
{
  const std::vector<float32_t> vec1 {1.2, 3.4, 5.6, 7.8};
  sen::test::TestWriter writer;
  sen::OutputStream out(writer);

  // write data to out buffer
  sen::impl::writeArray(out, vec1);
  EXPECT_FALSE(writer.getBuffer().empty());

  const auto buff = writer.getBuffer();
  sen::test::TestReader reader(buff);
  sen::InputStream in(sen::makeConstSpan<uint8_t>(reader.getBuffer()));

  // read vector from in buffer
  std::vector<float32_t> vec2;
  vec2.resize(4);

  sen::impl::readArray(in, vec2);
  EXPECT_EQ(vec1, vec2);
}

/// @test
/// Check reading/writing of sequences
/// @requirements(SEN-579)
TEST(IOUtils, Sequence)
{
  const std::vector<uint32_t> vec1 {1, 2, 3};
  sen::test::TestWriter writer;
  sen::OutputStream out(writer);

  // write data to out buffer
  sen::impl::writeSequence(out, vec1);
  EXPECT_FALSE(writer.getBuffer().empty());

  const auto buff = writer.getBuffer();
  sen::test::TestReader reader(buff);
  sen::InputStream in(sen::makeConstSpan<uint8_t>(reader.getBuffer()));

  // read vector from in buffer
  std::vector<uint32_t> vec2;
  sen::impl::readSequence(in, vec2);
  EXPECT_EQ(vec1, vec2);
}

/// @test
/// Check reading/writing of multiple sequences
/// @requirements(SEN-579)
TEST(IOUtils, MultipleSequences)
{
  const std::vector<float32_t> vec1 {5.5, 6.6, 7.7, 8.8, 9.9};
  const std::vector<float32_t> vec2 {10.5, 11.4, 12.3, 13.2, 14.1};
  const std::vector<float32_t> vec3 {-0.324f, -0.842, 0.99};
  const std::vector<float32_t> vec4 {3.3f, -3.14};
  const std::vector vecs {vec1, vec2, vec3, vec4};
  sen::test::TestWriter writer;
  sen::OutputStream out(writer);

  // write data to out buffer
  for (const auto& vec: vecs)
  {
    sen::impl::writeSequence(out, vec);
  }

  const auto buff = writer.getBuffer();
  sen::test::TestReader reader(buff);
  sen::InputStream in(sen::makeConstSpan<uint8_t>(reader.getBuffer()));

  // read vector from in buffer
  for (const auto& vec: vecs)
  {
    std::vector<float32_t> expectedVec;
    sen::impl::readSequence(in, expectedVec);
    EXPECT_EQ(vec, expectedVec);
  }
}

/// @test
/// Check reading/writing of quantities
/// @requirements(SEN-1054)
TEST(IOUtils, Quantities)
{
  const MyQuantity quantity = 4.0f;
  sen::test::TestWriter writer;
  sen::OutputStream out(writer);

  // write data to out buffer
  sen::impl::writeQuantity(out, quantity);
  EXPECT_FALSE(writer.getBuffer().empty());

  const auto buff = writer.getBuffer();
  sen::test::TestReader reader(buff);
  sen::InputStream in(sen::makeConstSpan<uint8_t>(reader.getBuffer()));

  // read vector from in buffer
  MyQuantity expectedQuantity;
  sen::impl::readQuantity(in, expectedQuantity);
  EXPECT_EQ(quantity, expectedQuantity);
}

/// @test
/// Check reading/writing of enums
/// @requirements(SEN-902)
TEST(IOUtils, Enums)
{
  {
    constexpr Level difficulty = mid;
    sen::test::TestWriter writer;
    sen::OutputStream out(writer);

    // write data to out buffer
    sen::impl::writeEnum(out, difficulty);
    EXPECT_FALSE(writer.getBuffer().empty());

    const auto buff = writer.getBuffer();
    sen::test::TestReader reader(buff);
    sen::InputStream in(sen::makeConstSpan<uint8_t>(reader.getBuffer()));

    // read vector from in buffer
    Level expectedDifficulty;
    sen::impl::readEnum(in, expectedDifficulty);
    EXPECT_EQ(difficulty, expectedDifficulty);
  }

  {
    constexpr PhonePrefixes prefix = spain;
    sen::test::TestWriter writer;
    sen::OutputStream out(writer);

    // write data to out buffer
    sen::impl::writeEnum(out, prefix);
    EXPECT_FALSE(writer.getBuffer().empty());

    const auto buff = writer.getBuffer();
    sen::test::TestReader reader(buff);
    sen::InputStream in(sen::makeConstSpan<uint8_t>(reader.getBuffer()));

    // read vector from in buffer
    PhonePrefixes expectedPhonePrefix;
    sen::impl::readEnum(in, expectedPhonePrefix);
    EXPECT_EQ(prefix, expectedPhonePrefix);
  }
}

/// @test
/// Check reading/writing of variants
/// @requirements(SEN-1053)
TEST(IOUtils, Variants)
{
  uint32_t variantValue = 60U;
  std::variant<std::string, uint32_t, float64_t> variant = variantValue;
  sen::test::TestWriter writer;
  sen::OutputStream out(writer);

  sen::SerializationTraits<uint32_t>::write(out, variantValue);
  EXPECT_FALSE(writer.getBuffer().empty());

  const auto buff = writer.getBuffer();
  sen::test::TestReader reader(buff);
  sen::InputStream in(sen::makeConstSpan<uint8_t>(reader.getBuffer()));

  // read vector from in buffer
  uint32_t expectedVariantValue;
  sen::impl::readVariantField<uint32_t>(in, expectedVariantValue);
  EXPECT_EQ(variantValue, expectedVariantValue);
}

/// @test
/// Check adaptVariant handles enum errors and useStrings flag
/// @requirements(SEN-1053)
TEST(IOUtils, AdaptVariantEnumErrors)
{
  std::optional<sen::ConstTypeHandle<sen::IntegralType>> intTypeHandleOpt;
  for (const auto& nt: sen::getNativeTypes())
  {
    if (auto casted = sen::dynamicTypeHandleCast<const sen::IntegralType>(nt))
    {
      intTypeHandleOpt = casted;
      break;
    }
  }
  ASSERT_TRUE(intTypeHandleOpt.has_value());

  sen::EnumSpec spec("TestEnum", "TestEnum", "", {{"A", 0, ""}, {"B", 1, ""}}, intTypeHandleOpt.value());
  auto enumType = sen::EnumType::make(spec);

  sen::Var varStringA = std::string("A");
  auto res0 = sen::impl::adaptVariant(*enumType, varStringA, std::nullopt, true);
  EXPECT_TRUE(res0.isOk());

  sen::Var varStringC = std::string("C");
  auto res1 = sen::impl::adaptVariant(*enumType, varStringC);
  EXPECT_TRUE(res1.isError());

  sen::Var varKey99 = 99U;
  auto res2 = sen::impl::adaptVariant(*enumType, varKey99);
  EXPECT_TRUE(res2.isError());

  sen::Var varValidKey = 0U;
  auto res3 = sen::impl::adaptVariant(*enumType, varValidKey, std::nullopt, true);
  EXPECT_TRUE(res3.isOk());
  EXPECT_TRUE(varValidKey.holds<std::string>());
  EXPECT_EQ(varValidKey.get<std::string>(), "A");
}

/// @test
/// Check adaptVariant error handling for StructType formatting
/// @requirements(SEN-1053)
TEST(IOUtils, AdaptVariantStructErrors)
{
  sen::StructSpec spec("TestStruct", "TestStruct", "", {{"field1", "", sen::getNativeTypes()[0]}});
  auto structType = sen::StructType::make(spec);

  sen::Var varNotMap = 42U;
  auto res1 = sen::impl::adaptVariant(*structType, varNotMap);
  EXPECT_TRUE(res1.isError());

  sen::VarMap badMap;
  badMap.try_emplace("wrong_field", 42U);
  sen::Var varBadMap = badMap;
  auto res2 = sen::impl::adaptVariant(*structType, varBadMap);
  EXPECT_TRUE(res2.isError());
}

/// @test
/// Check adaptVariant error handling for VariantType formatting
/// @requirements(SEN-1053)
TEST(IOUtils, AdaptVariantVariantTypeErrors)
{
  sen::VariantSpec spec;
  spec.name = "TestVariant";
  spec.qualifiedName = "TestVariant";
  spec.fields.emplace_back(1, "", sen::getNativeTypes()[0]);
  auto variantType = sen::VariantType::make(spec);

  sen::Var varKeyedInvalid = sen::KeyedVar(99U, std::make_shared<sen::Var>(42U));
  auto resInvalid = sen::impl::adaptVariant(*variantType, varKeyedInvalid);
  EXPECT_TRUE(resInvalid.isError());

  sen::Var varNotMap = 42U;
  auto res1 = sen::impl::adaptVariant(*variantType, varNotMap);
  EXPECT_TRUE(res1.isError());

  sen::VarMap missingTypeMap;
  missingTypeMap.try_emplace("value", 42U);
  sen::Var varMissingType = missingTypeMap;
  auto res2 = sen::impl::adaptVariant(*variantType, varMissingType);
  EXPECT_TRUE(res2.isError());

  sen::VarMap missingValueMap;
  missingValueMap.try_emplace("type", 1U);
  sen::Var varMissingValue = missingValueMap;
  auto res3 = sen::impl::adaptVariant(*variantType, varMissingValue);
  EXPECT_TRUE(res3.isError());

  sen::VarMap invalidTypeStringMap;
  invalidTypeStringMap.try_emplace("type", std::string("InvalidType"));
  invalidTypeStringMap.try_emplace("value", 42U);
  sen::Var varInvalidTypeString = invalidTypeStringMap;
  auto res4 = sen::impl::adaptVariant(*variantType, varInvalidTypeString);
  EXPECT_TRUE(res4.isError());

  sen::VarMap invalidTypeKeyMap;
  invalidTypeKeyMap.try_emplace("type", 99U);
  invalidTypeKeyMap.try_emplace("value", 42U);
  sen::Var varInvalidTypeKey = invalidTypeKeyMap;
  auto res5 = sen::impl::adaptVariant(*variantType, varInvalidTypeKey);
  EXPECT_TRUE(res5.isError());

  sen::VarMap validTypeStringMap;
  validTypeStringMap.try_emplace("type", std::string(sen::getNativeTypes()[0]->getName()));
  validTypeStringMap.try_emplace("value", 42U);
  sen::Var varValidTypeString = validTypeStringMap;
  auto res6 = sen::impl::adaptVariant(*variantType, varValidTypeString);
  EXPECT_TRUE(res6.isOk());
}

/// @test
/// Check adaptVariant error handling for SequenceType formatting
/// @requirements(SEN-1053)
TEST(IOUtils, AdaptVariantSequenceErrors)
{
  sen::SequenceSpec spec("TestSeq", "TestSeq", "", sen::getNativeTypes()[0]);
  auto seqType = sen::SequenceType::make(spec);

  sen::Var varNotList = 42U;
  auto res1 = sen::impl::adaptVariant(*seqType, varNotList);
  EXPECT_TRUE(res1.isError());

  sen::VarList list;
  list.emplace_back(10U);
  sen::Var vList = list;
  auto res2 = sen::impl::adaptVariant(*seqType, vList);
  EXPECT_TRUE(res2.isOk());
}

/// @test
/// Check adaptVariant saturates quantities out of bounds
/// @requirements(SEN-1053)
TEST(IOUtils, AdaptVariantQuantitySaturation)
{
  std::optional<sen::ConstTypeHandle<sen::RealType>> f64TypeHandleOpt;
  for (const auto& nt: sen::getNativeTypes())
  {
    if (nt->isFloat64Type())
    {
      f64TypeHandleOpt = sen::dynamicTypeHandleCast<const sen::RealType>(nt);
      break;
    }
  }
  ASSERT_TRUE(f64TypeHandleOpt.has_value());

  sen::QuantitySpec spec("TestQ", "TestQ", "", f64TypeHandleOpt.value(), std::nullopt, -10.0, 10.0);
  auto quantityType = sen::QuantityType::make(spec);

  sen::Var varLarge = 20.0;
  auto res1 = sen::impl::adaptVariant(*quantityType, varLarge);
  EXPECT_TRUE(res1.isError());
  EXPECT_DOUBLE_EQ(varLarge.getCopyAs<double>(), 10.0);

  sen::Var varSmall = -20.0;
  auto res2 = sen::impl::adaptVariant(*quantityType, varSmall);
  EXPECT_TRUE(res2.isError());
  EXPECT_DOUBLE_EQ(varSmall.getCopyAs<double>(), -10.0);

  sen::Var varEmpty;
  auto res3 = sen::impl::adaptVariant(*quantityType, varEmpty);
  EXPECT_TRUE(res3.isOk());
}

/// @test
/// Check getSerializedSize handles native types and VoidType edge cases
/// @requirements(SEN-1053)
TEST(IOUtils, SerializedSizeNativeAndEdgeCases)
{
  for (const auto& nt: sen::getNativeTypes())
  {
    sen::Var varVal;
    if (nt->isStringType())
    {
      varVal = std::string("test");
    }
    else if (nt->isBoolType())
    {
      varVal = false;
    }
    else
    {
      varVal = 0U;
      sen::impl::adaptVariant(*nt, varVal);
    }

    uint32_t size = sen::impl::getSerializedSize(varVal, &*nt);
    EXPECT_GE(size, 0U);
  }
}

/// @test
/// Check streams handling of VoidType
/// @requirements(SEN-1053)
TEST(IOUtils, VoidTypeStreamsAndSizes)
{
  auto voidType = sen::VoidType::get();
  sen::test::TestWriter writer;
  sen::OutputStream out(writer);
  sen::Var voidVar;

  sen::impl::writeToStream(voidVar, out, *voidType);
  EXPECT_TRUE(writer.getBuffer().empty());

  sen::test::TestReader reader(writer.getBuffer());
  sen::InputStream in(sen::makeConstSpan<uint8_t>(reader.getBuffer()));
  sen::Var readVar;
  sen::impl::readFromStream(readVar, in, *voidType);
  EXPECT_TRUE(readVar.isEmpty());

  uint32_t voidSize = sen::impl::getSerializedSize(voidVar, &*voidType);
  EXPECT_EQ(voidSize, 0U);
}

/// @test
/// Check Variant matching via Qualified Name and handling of KeyedVar errors
/// @requirements(SEN-1053)
TEST(IOUtils, VariantAdvancedLogic)
{
  sen::StructSpec fieldStructSpec("MyCustomStruct", "org.sen.MyCustomStruct", "", {});
  auto customFieldType = sen::StructType::make(fieldStructSpec);

  sen::VariantSpec spec;
  spec.name = "MyVar";
  spec.qualifiedName = "org.sen.MyVar";
  spec.fields.emplace_back(42, "desc", customFieldType);
  auto vType = sen::VariantType::make(spec);

  sen::VarMap map;
  map["type"] = std::string("org.sen.MyCustomStruct");
  map["value"] = sen::VarMap {};
  sen::Var v = map;
  EXPECT_TRUE(sen::impl::adaptVariant(*vType, v).isOk());

  sen::Var kv = sen::KeyedVar(999, std::make_shared<sen::Var>(sen::VarMap {}));
  auto res = sen::impl::adaptVariant(*vType, kv);
  EXPECT_TRUE(res.isError());
}

/// @test
/// Check Recursive SerializedSize calculation for Structs, Sequences, and Duration
/// @requirements(SEN-1053)
TEST(IOUtils, RecursiveSizeCalculation)
{
  sen::StructSpec sSpec("S", "S", "", {{"x", "", sen::UInt32Type::get()}});
  auto sType = sen::StructType::make(sSpec);

  sen::VarMap m;
  m["x"] = 100U;
  sen::Var vStruct = m;

  EXPECT_EQ(sen::impl::getSerializedSize(vStruct, &*sType), 4U);

  sen::SequenceSpec seqSpec("Seq", "Seq", "", sType);
  auto seqType = sen::SequenceType::make(seqSpec);
  sen::VarList l = {vStruct, vStruct};
  sen::Var vList = l;
  EXPECT_EQ(sen::impl::getSerializedSize(vList, &*seqType), 12U);

  sen::Var vDur = sen::Duration(150);
  EXPECT_GT(sen::impl::getSerializedSize(vDur, &*sen::DurationType::get()), 0U);
}

/// @test
/// Check adaptVariant behavior for VoidType and AliasType
/// @requirements(SEN-1053)
TEST(IOUtils, MetadataTypeEdgeCases)
{
  auto voidType = sen::VoidType::get();
  sen::Var vVoid;
  EXPECT_TRUE(sen::impl::adaptVariant(*voidType, vVoid).isOk());

  auto aliasType = sen::AliasType::make({"Alias", "Alias", "", sen::Float32Type::get()});
  sen::Var vAlias = 5.5;
  EXPECT_TRUE(sen::impl::adaptVariant(*aliasType, vAlias).isOk());
  EXPECT_TRUE(vAlias.holds<float32_t>());
}

/// @test
/// Check StreamWriter handles monostate by writing default values
/// @requirements(SEN-1053)
TEST(IOUtils, StreamWriterDefaults)
{
  sen::test::TestWriter writer;
  sen::OutputStream out(writer);
  const sen::Var empty;

  sen::impl::writeToStream(empty, out, *sen::Int32Type::get());

  const auto buffer = writer.getBuffer();
  EXPECT_EQ(buffer.size(), 4U);
  EXPECT_EQ(buffer[0], 0);
}

/// @test
/// Check SerializedSize for Optional and Quantity types
/// @requirements(SEN-1053)
TEST(IOUtils, SizeOptionalAndQuantity)
{
  auto optType = sen::OptionalType::make({"O", "O", "", sen::UInt8Type::get()});
  sen::Var vPresent = 5U;
  EXPECT_EQ(sen::impl::getSerializedSize(vPresent, &*optType), 2U);

  sen::QuantitySpec qSpec("Q", "Q", "", sen::Float32Type::get());
  auto qType = sen::QuantityType::make(qSpec);
  sen::Var vQ = 1.0f;
  EXPECT_EQ(sen::impl::getSerializedSize(vQ, &*qType), 4U);
}

/// @test
/// Check ClassType rejections and unhandled visitors
/// @requirements(SEN-1053)
TEST(IOUtils, ClassTypeUnhandled)
{
  sen::ClassSpec cSpec;
  cSpec.name = "TestClass";
  cSpec.qualifiedName = "org.test.TestClass";
  auto cType = sen::ClassType::make(cSpec);

  sen::Var v;
  sen::test::TestWriter writer;
  sen::OutputStream out(writer);

  EXPECT_ANY_THROW(sen::impl::writeToStream(v, out, *cType));

  sen::test::TestReader reader(writer.getBuffer());
  sen::InputStream in(sen::makeConstSpan<uint8_t>(reader.getBuffer()));
  EXPECT_ANY_THROW(sen::impl::readFromStream(v, in, *cType));

  EXPECT_ANY_THROW(std::ignore = sen::impl::getSerializedSize(v, &*cType));
}

/// @test
/// Check missing keys for Variant types in streams
/// @requirements(SEN-1053)
TEST(IOUtils, VariantStreamBadKey)
{
  sen::VariantSpec vSpec;
  vSpec.name = "V";
  vSpec.qualifiedName = "org.test.V";
  vSpec.fields.emplace_back(1, "", sen::BoolType::get());
  auto vType = sen::VariantType::make(vSpec);

  sen::test::TestWriter writer;
  sen::OutputStream out(writer);
  out.writeUInt32(99);
  out.writeBool(true);

  sen::test::TestReader reader(writer.getBuffer());
  sen::InputStream in(sen::makeConstSpan<uint8_t>(reader.getBuffer()));
  sen::Var v;

  EXPECT_ANY_THROW(sen::impl::readFromStream(v, in, *vType));
}

/// @test
/// Check writing VarMap representation of variants into streams
/// @requirements(SEN-1053)
TEST(IOUtils, VariantStreamMapWrite)
{
  sen::VariantSpec vSpec;
  vSpec.name = "V";
  vSpec.qualifiedName = "org.test.V";
  vSpec.fields.emplace_back(1, "", sen::BoolType::get());
  auto vType = sen::VariantType::make(vSpec);

  sen::VarMap map;
  map["type"] = 1U;
  map["value"] = true;
  sen::Var v = map;

  sen::test::TestWriter writer;
  sen::OutputStream out(writer);
  EXPECT_NO_THROW(sen::impl::writeToStream(v, out, *vType));

  EXPECT_EQ(writer.getBuffer().size(), 5U);
}

/// @test
/// Check streams ignoring length prefix on fixed-size Sequences
/// @requirements(SEN-1053)
TEST(IOUtils, SequenceFixedSize)
{
  sen::SequenceSpec sSpec("Seq", "Seq", "", sen::UInt8Type::get(), 2, true);
  auto seqType = sen::SequenceType::make(sSpec);

  sen::VarList lst = {sen::Var(10U), sen::Var(20U)};
  sen::Var v = lst;

  sen::test::TestWriter writer;
  sen::OutputStream out(writer);
  sen::impl::writeToStream(v, out, *seqType);

  EXPECT_EQ(writer.getBuffer().size(), 2U);

  sen::test::TestReader reader(writer.getBuffer());
  sen::InputStream in(sen::makeConstSpan<uint8_t>(reader.getBuffer()));
  sen::Var vRead;
  sen::impl::readFromStream(vRead, in, *seqType);
  EXPECT_EQ(vRead.get<sen::VarList>().size(), 2U);
}

/// @test
/// Check sequence and struct adaptations with multiple errors to test error preservation
/// @requirements(SEN-1053)
TEST(IOUtils, AdaptVariantMultipleErrors)
{
  sen::QuantitySpec qSpec("BoundQ", "BoundQ", "", sen::Float32Type::get(), std::nullopt, 0.0f, 10.0f);
  auto qType = sen::QuantityType::make(qSpec);

  sen::StructSpec structSpec("MultiErrStruct", "MultiErrStruct", "", {{"f1", "", qType}, {"f2", "", qType}});
  auto sType = sen::StructType::make(structSpec);

  sen::VarMap m;
  m["f1"] = 20.0f;
  m["f2"] = 30.0f;
  sen::Var vStruct = m;

  auto resStruct = sen::impl::adaptVariant(*sType, vStruct);
  EXPECT_TRUE(resStruct.isError());

  sen::SequenceSpec seqSpec("MultiErrSeq", "MultiErrSeq", "", qType);
  auto seqType = sen::SequenceType::make(seqSpec);

  sen::VarList l;
  l.emplace_back(20.0f);
  l.emplace_back(30.0f);
  sen::Var vSeq = l;

  auto resSeq = sen::impl::adaptVariant(*seqType, vSeq);
  EXPECT_TRUE(resSeq.isError());
}

/// @test
/// Check StreamWriter correctly processes DurationType
/// @requirements(SEN-1053)
TEST(IOUtils, StreamWriterDuration)
{
  const sen::Duration dur(5000);
  const sen::Var vDur = dur;
  sen::test::TestWriter writer;
  sen::OutputStream out(writer);

  EXPECT_NO_THROW(sen::impl::writeToStream(vDur, out, *sen::DurationType::get()));
  EXPECT_FALSE(writer.getBuffer().empty());
}

/// @test
/// Check SerializedSizeGetter processes Enum, Alias, and Variant with KeyedVar
/// @requirements(SEN-1053)
TEST(IOUtils, SerializedSizeAdvancedTypes)
{
  sen::EnumSpec eSpec("SizeEnum", "SizeEnum", "", {{"A", 1, ""}}, sen::UInt8Type::get());
  auto eType = sen::EnumType::make(eSpec);
  sen::Var vEnum = 1U;
  EXPECT_EQ(sen::impl::getSerializedSize(vEnum, &*eType), 1U);

  auto aType = sen::AliasType::make({"SizeAlias", "SizeAlias", "", sen::UInt32Type::get()});
  sen::Var vAlias = 42U;
  EXPECT_EQ(sen::impl::getSerializedSize(vAlias, &*aType), 4U);

  sen::VariantSpec vSpec;
  vSpec.name = "SizeVariant";
  vSpec.qualifiedName = "SizeVariant";
  vSpec.fields.emplace_back(1, "", sen::BoolType::get());
  auto vType = sen::VariantType::make(vSpec);

  sen::Var kv = sen::KeyedVar(1U, std::make_shared<sen::Var>(true));
  EXPECT_GT(sen::impl::getSerializedSize(kv, &*vType), 0U);
}

/// @test
/// Check adaptVariant handles multiple errors in a bounded sequence
/// @requirements(SEN-1053)
TEST(IOUtils, AdaptVariantBoundedSequenceErrors)
{
  std::optional<sen::ConstTypeHandle<sen::RealType>> f64TypeHandleOpt;
  for (const auto& nt: sen::getNativeTypes())
  {
    if (nt->isFloat64Type())
    {
      f64TypeHandleOpt = sen::dynamicTypeHandleCast<const sen::RealType>(nt);
      break;
    }
  }
  ASSERT_TRUE(f64TypeHandleOpt.has_value());

  sen::QuantitySpec qSpec("BoundQ", "BoundQ", "", f64TypeHandleOpt.value(), std::nullopt, 0.0, 10.0);
  auto qType = sen::QuantityType::make(qSpec);

  sen::SequenceSpec seqSpec("MultiErrSeqBounded", "MultiErrSeqBounded", "", qType, 2, true);
  auto seqType = sen::SequenceType::make(seqSpec);

  sen::VarList l;
  l.emplace_back(20.0);
  l.emplace_back(30.0);
  sen::Var vSeq = l;

  auto resSeq = sen::impl::adaptVariant(*seqType, vSeq);
  EXPECT_TRUE(resSeq.isError());
}

/// @test
/// Check unhandled paths asserting in adaptVariant for base types
/// @requirements(SEN-1053)
TEST(IOUtils, BaseTypeUnhandledPaths_Death)
{
  const MockNumericType mockNumeric;
  const MockIntegralType mockIntegral;
  const MockRealType mockReal;
  sen::Var var = 0U;

  EXPECT_DEATH(std::ignore = sen::impl::adaptVariant(mockNumeric, var), "");
  EXPECT_DEATH(std::ignore = sen::impl::adaptVariant(mockIntegral, var), "");
  EXPECT_DEATH(std::ignore = sen::impl::adaptVariant(mockReal, var), "");
}

/// @test
/// Check unhandled paths throwing runtime error in visitors for base types
/// @requirements(SEN-1053)
TEST(IOUtils, BaseTypeUnhandledPaths_Throws)
{
  MockType mockType;
  MockNativeType mockNative;
  MockNumericType mockNumeric;
  MockIntegralType mockIntegral;
  MockRealType mockReal;

  sen::test::TestWriter writer;
  sen::OutputStream out(writer);
  sen::Var var;

  EXPECT_ANY_THROW(sen::impl::writeToStream(var, out, mockType));
  EXPECT_ANY_THROW(sen::impl::writeToStream(var, out, mockNative));

  const auto buff = writer.getBuffer();
  sen::test::TestReader reader(buff);
  sen::InputStream in(sen::makeConstSpan<uint8_t>(reader.getBuffer()));

  EXPECT_ANY_THROW(sen::impl::readFromStream(var, in, mockType));
  EXPECT_ANY_THROW(sen::impl::readFromStream(var, in, mockNative));
  EXPECT_ANY_THROW(sen::impl::readFromStream(var, in, mockNumeric));
  EXPECT_ANY_THROW(sen::impl::readFromStream(var, in, mockIntegral));
  EXPECT_ANY_THROW(sen::impl::readFromStream(var, in, mockReal));

  EXPECT_ANY_THROW(std::ignore = sen::impl::getSerializedSize(var, &mockType));
  EXPECT_ANY_THROW(std::ignore = sen::impl::getSerializedSize(var, &mockNative));
  EXPECT_ANY_THROW(std::ignore = sen::impl::getSerializedSize(var, &mockNumeric));
  EXPECT_ANY_THROW(std::ignore = sen::impl::getSerializedSize(var, &mockIntegral));
  EXPECT_ANY_THROW(std::ignore = sen::impl::getSerializedSize(var, &mockReal));
}

/// @test
/// Check StreamWriter handles monostate for all native and time types
/// @requirements(SEN-1053)
TEST(IOUtils, WriteNativeMonostateAllTypes)
{
  sen::test::TestWriter writer;
  sen::OutputStream out(writer);

  for (const auto& nt: sen::getNativeTypes())
  {
    sen::Var emptyVar;
    EXPECT_NO_THROW(sen::impl::writeToStream(emptyVar, out, *nt));
  }

  sen::Var emptyDur;
  EXPECT_NO_THROW(sen::impl::writeToStream(emptyDur, out, *sen::DurationType::get()));

  sen::Var emptyTs;
  EXPECT_NO_THROW(sen::impl::writeToStream(emptyTs, out, *sen::TimestampType::get()));
}

/// @test
/// Check writeSequence explicitly handles size 0
/// @requirements(SEN-1053)
TEST(IOUtils, EmptySequenceWrite)
{
  sen::test::TestWriter writer;
  sen::OutputStream out(writer);

  const std::vector<uint32_t> emptyVec;
  EXPECT_NO_THROW(sen::impl::writeSequence(out, emptyVec));

  EXPECT_EQ(writer.getBuffer().size(), 4U);
}
