// === component.cpp ===================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "sen/kernel/component.h"

// sen
#include "sen/core/base/result.h"

// kernel
#include "sen/kernel/component_api.h"

// generated code
#include "stl/sen/kernel/basic_types.stl.h"
#include "stl/struct_inheritance.stl.h"

using sen::kernel::ErrorCategory;

struct MyComponent: public sen::kernel::Component
{
  sen::kernel::FuncResult run(sen::kernel::RunApi& api) override
  {
    // Publish the base Type
    auto& typeRegistry = api.getTypes();
    typeRegistry.add(StructInheritance::CheckerInterface::meta());

    // Check the dependencies
    const auto& typeMap = typeRegistry.getAll();
    if (typeMap.find("StructInheritance.Child") == typeMap.end())
    {
      return sen::Err(sen::kernel::ExecError {ErrorCategory::runtimeError, "Child not found"});
    }

    if (typeMap.find("StructInheritance.Parent") == typeMap.end())
    {
      return sen::Err(sen::kernel::ExecError {ErrorCategory::runtimeError, "Parent not found"});
    }

    if (typeMap.find("StructInheritance.GrandParent") == typeMap.end())
    {
      return sen::Err(sen::kernel::ExecError {ErrorCategory::runtimeError, "GrandParent not found"});
    }

    // Test successfull
    api.requestKernelStop();

    return done();
  }
};

SEN_COMPONENT(MyComponent)
