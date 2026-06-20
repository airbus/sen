// === type_specs.cpp ==================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "type_specs.h"

#include "sen/core/io/util.h"
#include "sen/core/meta/custom_type.h"
#include "sen/core/meta/var.h"
#include "sen/kernel/type_specs_utils.h"
#include "stl/sen/kernel/type_specs.stl.h"
#include "variant_conversion.h"

#include <tuple>
#include <variant>

namespace sen::db::python
{

pybind11::object customTypeToPython(const sen::CustomType& type)
{
  auto spec = sen::kernel::makeCustomTypeSpec(&type);

  // sen.TimeStamp is a QuantityType but values serialize as UTC strings; rewrite as an alias
  // so the spec describes the value.
  if (spec.qualifiedName == "sen.TimeStamp" && std::holds_alternative<sen::kernel::QuantityTypeSpec>(spec.data))
  {
    spec.data = sen::kernel::AliasTypeSpec {"TimeStamp"};
  }

  // adaptVariant(useStrings=true) rewrites KeyedVar tags from variant indices to type names.
  auto var = sen::toVariant(spec);
  std::ignore = sen::impl::adaptVariant(*sen::MetaTypeTrait<sen::kernel::CustomTypeSpec>::meta(),
                                        var,
                                        std::nullopt,
                                        /*useStrings=*/true);
  return visitVar(var);
}

}  // namespace sen::db::python
