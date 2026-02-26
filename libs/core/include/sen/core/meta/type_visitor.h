// === type_visitor.h ==================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_CORE_META_TYPE_VISITOR_H
#define SEN_CORE_META_TYPE_VISITOR_H

// sen
#include "sen/core/meta/detail/types_fwd.h"
#include "sen/core/meta/type.h"

namespace sen
{

/// \addtogroup type_utils
/// @{

/// A Visitor for constant types.
/// This class is based on the GoF Visitor design pattern. You can use
/// it to perform operations on types without having to do
/// dynamic_casts (or switch-based casts).
///
/// Use FullTypeVisitor if you must handle all the types.
class FullTypeVisitor
{
public:
  SEN_NOCOPY_NOMOVE(FullTypeVisitor)

public:
  FullTypeVisitor() = default;
  virtual ~FullTypeVisitor() = default;

public:
  // root type
  virtual void apply(const Type&) = 0;

  // native types
  virtual void apply(const NativeType& type) = 0;
  virtual void apply(const VoidType& type) = 0;
  virtual void apply(const BoolType& type) = 0;
  virtual void apply(const NumericType& type) = 0;
  virtual void apply(const IntegralType& type) = 0;
  virtual void apply(const RealType& type) = 0;
  virtual void apply(const UInt8Type& type) = 0;
  virtual void apply(const Int16Type& type) = 0;
  virtual void apply(const UInt16Type& type) = 0;
  virtual void apply(const Int32Type& type) = 0;
  virtual void apply(const UInt32Type& type) = 0;
  virtual void apply(const Int64Type& type) = 0;
  virtual void apply(const UInt64Type& type) = 0;
  virtual void apply(const Float32Type& type) = 0;
  virtual void apply(const Float64Type& type) = 0;
  virtual void apply(const DurationType& type) = 0;
  virtual void apply(const TimestampType& type) = 0;
  virtual void apply(const StringType& type) = 0;

  // custom types
  virtual void apply(const CustomType& type) = 0;
  virtual void apply(const EnumType& type) = 0;
  virtual void apply(const StructType& type) = 0;
  virtual void apply(const VariantType& type) = 0;
  virtual void apply(const SequenceType& type) = 0;
  virtual void apply(const ClassType& type) = 0;
  virtual void apply(const QuantityType& type) = 0;
  virtual void apply(const AliasType& type) = 0;
  virtual void apply(const OptionalType& type) = 0;
};

/// All the apply virtual methods are overriden to empty methods.
/// Use TypeVisitor if you only need to handle some of the types.
class TypeVisitor: public FullTypeVisitor
{
public:
  SEN_NOCOPY_NOMOVE(TypeVisitor)

public:
  TypeVisitor() = default;
  ~TypeVisitor() override = default;

public:
  using FullTypeVisitor::apply;

  // root type
  void apply(const Type& type) override;
  void apply(const VoidType& type) override;

  // native types
  void apply(const NativeType& type) override;
  void apply(const EnumType& type) override;
  void apply(const BoolType& type) override;
  void apply(const NumericType& type) override;
  void apply(const IntegralType& type) override;
  void apply(const RealType& type) override;
  void apply(const UInt8Type& type) override;
  void apply(const Int16Type& type) override;
  void apply(const UInt16Type& type) override;
  void apply(const Int32Type& type) override;
  void apply(const UInt32Type& type) override;
  void apply(const Int64Type& type) override;
  void apply(const UInt64Type& type) override;
  void apply(const Float32Type& type) override;
  void apply(const Float64Type& type) override;
  void apply(const DurationType& type) override;
  void apply(const TimestampType& type) override;
  void apply(const StringType& type) override;

  // custom types
  void apply(const CustomType& type) override;
  void apply(const StructType& type) override;
  void apply(const VariantType& type) override;
  void apply(const SequenceType& type) override;
  void apply(const ClassType& type) override;
  void apply(const QuantityType& type) override;
  void apply(const AliasType& type) override;
  void apply(const OptionalType& type) override;
};

/// @}

}  // namespace sen

#endif  // SEN_CORE_META_TYPE_VISITOR_H
