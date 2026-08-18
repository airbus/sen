// === class_export_test.cpp ===========================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

// sen
#include "sen/core/base/compiler_macros.h"
#include "sen/core/base/result.h"
#include "sen/core/meta/class_type.h"
#include "sen/core/meta/var.h"

// generated code
#include "stl/example_class.stl.h"

// google test
#include <gtest/gtest.h>

// std
#include <string>

namespace class_export_test
{

class ValidatedClass final: public example_class::ExampleClassBase
{
public:
  SEN_NOCOPY_NOMOVE(ValidatedClass)

public:
  using ExampleClassBase::ExampleClassBase;
  ~ValidatedClass() override = default;

  [[nodiscard]] static sen::Result<void, std::string> validateArguments(const std::string& name,
                                                                        const sen::VarMap& args)
  {
    if (name.empty())
    {
      return sen::Err(std::string("object name is required"));
    }

    if (const auto requiredArgument = args.find("requiredArgument");
        requiredArgument == args.end() || requiredArgument->second.get<std::string>().empty())
    {
      return sen::Err(std::string("requiredArgument is missing"));
    }

    return sen::Ok();
  }
};

SEN_EXPORT_CLASS(ValidatedClass, ValidatedClass::validateArguments)

}  // namespace class_export_test

/// @test
/// Checks that SEN_EXPORT_CLASS exposes the configured construction validator
/// @requirements(SEN-1049)
TEST(ClassExport, constructionValidator)
{
  const auto validator = class_export_test::getConstructionValidatorValidatedClass();
  ASSERT_EQ(validator, class_export_test::ValidatedClass::validateArguments);

  const sen::VarMap validArguments {{"requiredArgument", "validValue"}};
  EXPECT_TRUE(validator("validName", validArguments).isOk());

  const auto invalidNameResult = validator("", validArguments);
  ASSERT_TRUE(invalidNameResult.isError());
  EXPECT_EQ(invalidNameResult.getError(), "object name is required");

  const auto invalidArgResult = validator("validName", {});
  ASSERT_TRUE(invalidArgResult.isError());
  EXPECT_EQ(invalidArgResult.getError(), "requiredArgument is missing");
}
