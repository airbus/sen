// === type_visitor.cpp ================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "sen/core/meta/type_visitor.h"

// sen
#include "sen/core/meta/alias_type.h"
#include "sen/core/meta/class_type.h"
#include "sen/core/meta/custom_type.h"
#include "sen/core/meta/enum_type.h"
#include "sen/core/meta/native_types.h"
#include "sen/core/meta/optional_type.h"
#include "sen/core/meta/quantity_type.h"
#include "sen/core/meta/sequence_type.h"
#include "sen/core/meta/struct_type.h"
#include "sen/core/meta/time_types.h"
#include "sen/core/meta/type.h"
#include "sen/core/meta/variant_type.h"

// std
#include <tuple>

namespace sen
{

void TypeVisitor::apply(const Type& type) { std::ignore = type; }

void TypeVisitor::apply(const VoidType& type) { apply(static_cast<const Type&>(type)); }

void TypeVisitor::apply(const NativeType& type) { apply(static_cast<const Type&>(type)); }

void TypeVisitor::apply(const BoolType& type) { apply(static_cast<const NativeType&>(type)); }

void TypeVisitor::apply(const NumericType& type) { apply(static_cast<const NativeType&>(type)); }

void TypeVisitor::apply(const IntegralType& type) { apply(static_cast<const NumericType&>(type)); }

void TypeVisitor::apply(const RealType& type) { apply(static_cast<const NumericType&>(type)); }

void TypeVisitor::apply(const UInt8Type& type) { apply(static_cast<const IntegralType&>(type)); }

void TypeVisitor::apply(const Int16Type& type) { apply(static_cast<const IntegralType&>(type)); }

void TypeVisitor::apply(const UInt16Type& type) { apply(static_cast<const IntegralType&>(type)); }

void TypeVisitor::apply(const Int32Type& type) { apply(static_cast<const IntegralType&>(type)); }

void TypeVisitor::apply(const UInt32Type& type) { apply(static_cast<const IntegralType&>(type)); }

void TypeVisitor::apply(const Int64Type& type) { apply(static_cast<const IntegralType&>(type)); }

void TypeVisitor::apply(const UInt64Type& type) { apply(static_cast<const IntegralType&>(type)); }

void TypeVisitor::apply(const Float32Type& type) { apply(static_cast<const RealType&>(type)); }

void TypeVisitor::apply(const Float64Type& type) { apply(static_cast<const RealType&>(type)); }

void TypeVisitor::apply(const StringType& type) { apply(static_cast<const NativeType&>(type)); }

void TypeVisitor::apply(const CustomType& type) { apply(static_cast<const Type&>(type)); }

void TypeVisitor::apply(const EnumType& type) { apply(static_cast<const CustomType&>(type)); }

void TypeVisitor::apply(const StructType& type) { apply(static_cast<const CustomType&>(type)); }

void TypeVisitor::apply(const VariantType& type) { apply(static_cast<const CustomType&>(type)); }

void TypeVisitor::apply(const SequenceType& type) { apply(static_cast<const CustomType&>(type)); }

void TypeVisitor::apply(const ClassType& type) { apply(static_cast<const CustomType&>(type)); }

void TypeVisitor::apply(const QuantityType& type) { apply(static_cast<const CustomType&>(type)); }

void TypeVisitor::apply(const DurationType& type) { apply(static_cast<const QuantityType&>(type)); }

void TypeVisitor::apply(const TimestampType& type) { apply(static_cast<const QuantityType&>(type)); }

void TypeVisitor::apply(const AliasType& type) { apply(static_cast<const CustomType&>(type)); }

void TypeVisitor::apply(const OptionalType& type) { apply(static_cast<const CustomType&>(type)); }

}  // namespace sen
