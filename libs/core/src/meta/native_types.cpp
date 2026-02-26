// === native_types.cpp ================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "sen/core/meta/native_types.h"

// sen
#include "sen/core/base/span.h"
#include "sen/core/meta/time_types.h"
#include "sen/core/meta/type.h"

namespace sen
{

NativeType::NativeType(MemberHash hash) noexcept: Type(hash) {}

NumericType::NumericType(MemberHash hash) noexcept: NativeType(hash) {}

IntegralType::IntegralType(MemberHash hash) noexcept: NumericType(hash) {}

RealType::RealType(MemberHash hash) noexcept: NumericType(hash) {}

const Span<ConstTypeHandle<> const>& getNativeTypes()
{
  static ConstTypeHandle<> nativeTypes[] = {UInt8Type::get(),  // NOSONAR
                                            Int16Type::get(),
                                            UInt16Type::get(),
                                            Int32Type::get(),
                                            UInt32Type::get(),
                                            Int64Type::get(),
                                            UInt64Type::get(),
                                            Float32Type::get(),
                                            Float64Type::get(),
                                            BoolType::get(),
                                            StringType::get(),
                                            TimestampType::get(),
                                            DurationType::get(),
                                            VoidType::get()};

  static auto span {makeConstSpan(nativeTypes)};
  return span;
}

sen::TypeHandle<const VoidType> VoidType::get()
{
  static VoidType instance;
  return sen::makeNonOwningTypeHandle(&instance);
}

}  // namespace sen
