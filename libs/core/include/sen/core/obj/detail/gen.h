// === gen.h ===========================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_CORE_OBJ_DETAIL_GEN_H
#define SEN_CORE_OBJ_DETAIL_GEN_H

// sen
#include "sen/core/base/compiler_macros.h"
#include "sen/core/base/numbers.h"
#include "sen/core/base/quantity.h"
#include "sen/core/base/span.h"
#include "sen/core/base/static_vector.h"
#include "sen/core/io/util.h"
#include "sen/core/lang/vm.h"
#include "sen/core/meta/class_type.h"
#include "sen/core/meta/enum_traits.h"
#include "sen/core/meta/enum_type.h"
#include "sen/core/meta/native_types.h"
#include "sen/core/meta/optional_traits.h"
#include "sen/core/meta/quantity_traits.h"
#include "sen/core/meta/quantity_type.h"
#include "sen/core/meta/sequence_traits.h"
#include "sen/core/meta/sequence_type.h"
#include "sen/core/meta/struct_traits.h"
#include "sen/core/meta/struct_type.h"
#include "sen/core/meta/time_types.h"
#include "sen/core/meta/unit_registry.h"
#include "sen/core/meta/variant_traits.h"
#include "sen/core/meta/variant_type.h"
#include "sen/core/obj/detail/event_buffer.h"
#include "sen/core/obj/detail/native_object_proxy.h"
#include "sen/core/obj/detail/property_flags.h"
#include "sen/core/obj/detail/remote_object.h"
#include "sen/core/obj/native_object.h"

// std
#include <optional>

#define SEN_IMPL_GEN_NATIVE_MEMBERS                                                                                    \
protected:                                                                                                             \
  [[nodiscard]] inline ::sen::Var senImplGetPropertyImpl(::sen::MemberHash propertyId) const override;                 \
  inline void senImplSetNextPropertyUntyped(::sen::MemberHash propertyId, const ::sen::Var& value) override;           \
  [[nodiscard]] inline ::sen::Var senImplGetNextPropertyUntyped(::sen::MemberHash propertyId) const override;          \
  [[nodiscard]] inline uint32_t senImplComputeMaxReliableSerializedPropertySizeImpl() const override;                  \
  inline void senImplStreamCall(::sen::MemberHash methodId, ::sen::InputStream& in, ::sen::StreamCallForwarder&& func) \
    override;                                                                                                          \
  inline void senImplVariantCall(                                                                                      \
    ::sen::MemberHash methodId, const ::sen::VarList& args, ::sen::VariantCallForwarder&& func) override;              \
  inline void senImplCommitImpl(::sen::TimeStamp time) override;                                                       \
  inline void senImplWriteChangedPropertiesToStream(                                                                   \
    ::sen::OutputStream& confirmed, ::sen::impl::BufferProvider uni, ::sen::impl::BufferProvider multi) override;      \
  inline void senImplWriteAllPropertiesToStream(::sen::OutputStream& out) const override;                              \
  inline void senImplWriteStaticPropertiesToStream(::sen::OutputStream& out) const override;                           \
  inline void senImplWriteDynamicPropertiesToStream(::sen::OutputStream& out) const override;                          \
  inline void senImplRemoveTypedConnection(::sen::ConnId id) override;                                                 \
  [[nodiscard]] inline ::sen::impl::FieldValueGetter senImplGetFieldValueGetter(                                       \
    ::sen::MemberHash propertyId, ::sen::Span<uint16_t> fields) const override;                                        \
  inline void invokeAllPropertyCallbacks() override;                                                                   \
                                                                                                                       \
private:                                                                                                               \
  bool somePropertyIsDirty_ = false;

/// Used by the generated code for simplicity NOLINTNEXTLINE
#define SEN_IMPL_GEN_BASE_CLASS(classname)                                                                             \
  SEN_IMPL_GEN_NATIVE_MEMBERS                                                                                          \
public:                                                                                                                \
  inline classname(std::string name, const ::sen::VarMap& args);                                                       \
  ~classname() override = default;                                                                                     \
  [[nodiscard]] inline ::sen::ConstTypeHandle<::sen::ClassType> getClass() const noexcept override;                    \
  [[nodiscard]] static inline ::sen::ConstTypeHandle<::sen::ClassType> meta() noexcept;                                \
                                                                                                                       \
private:

/// Used by the code generator NOLINTNEXTLINE
#define SEN_IMPL_GEN_REMOTE_CLASS(classname)                                                                           \
public:                                                                                                                \
  explicit classname(::sen::impl::RemoteObjectInfo&& info);                                                            \
  ~classname() override = default;                                                                                     \
                                                                                                                       \
protected:                                                                                                             \
  void senImplRemoveTypedConnection(::sen::ConnId id) override;                                                        \
  void senImplWriteAllPropertiesToStream(::sen::OutputStream& out) const override;                                     \
  void senImplWriteStaticPropertiesToStream(::sen::OutputStream& out) const override;                                  \
  void senImplWriteDynamicPropertiesToStream(::sen::OutputStream& out) const override;                                 \
  [[nodiscard]] ::sen::Var senImplGetPropertyImpl(::sen::MemberHash propertyId) const override;                        \
  [[nodiscard]] bool readPropertyFromStream(::sen::MemberHash id, ::sen::InputStream& in, bool initialState) override; \
  [[nodiscard]] ::sen::Var deserializeMethodReturnValueAsVariant(::sen::MemberHash methodId, ::sen::InputStream& in)   \
    const override;                                                                                                    \
  void serializeMethodArgumentsFromVariants(                                                                           \
    ::sen::MemberHash methodId, const ::sen::VarList& args, ::sen::OutputStream& out) const override;                  \
  void drainInputs() override;                                                                                         \
  void eventReceived(::sen::MemberHash eventId, ::sen::TimeStamp creationTime, const ::sen::Span<const uint8_t>& args) \
    override;                                                                                                          \
  void invokeAllPropertyCallbacks() override;                                                                          \
                                                                                                                       \
private:

/// Used by the code generator NOLINTNEXTLINE
#define SEN_IMPL_GEN_LOCAL_PROXY_CLASS(classname)                                                                      \
public:                                                                                                                \
  explicit classname(::sen::NativeObject* owner, const std::string& localPrefix);                                      \
  ~classname() override = default;                                                                                     \
                                                                                                                       \
protected:                                                                                                             \
  void senImplWriteAllPropertiesToStream(::sen::OutputStream& out) const override;                                     \
  void senImplWriteStaticPropertiesToStream(::sen::OutputStream& out) const override;                                  \
  void senImplWriteDynamicPropertiesToStream(::sen::OutputStream& out) const override;                                 \
  void senImplRemoveTypedConnection(::sen::ConnId id) override;                                                        \
  [[nodiscard]] ::sen::Var senImplGetPropertyImpl(::sen::MemberHash propertyId) const override;                        \
  void drainInputsImpl(::sen::TimeStamp lastCommitTime) override;                                                      \
                                                                                                                       \
private:

/// Used by the code generator NOLINTNEXTLINE
#define SEN_IMPL_GEN_UNBOUNDED_SEQUENCE(classname, elementtype)                                                        \
  struct classname final: public std::vector<elementtype>                                                              \
  {                                                                                                                    \
    using Parent = std::vector<elementtype>;                                                                           \
    using Parent::Parent;                                                                                              \
    using Parent::operator=;                                                                                           \
  };

/// Used by the code generator NOLINTNEXTLINE
#define SEN_IMPL_GEN_BOUNDED_SEQUENCE(classname, element, maxSize)                                                     \
  struct classname final: public ::sen::StaticVector<element, maxSize>                                                 \
  {                                                                                                                    \
    using Parent = ::sen::StaticVector<element, maxSize>;                                                              \
    using Parent::Parent;                                                                                              \
    using Parent::operator=;                                                                                           \
  };

/// Used by the code generator NOLINTNEXTLINE
#define SEN_IMPL_GEN_FIXED_SEQUENCE(classname, element, size)                                                          \
  struct classname final: public std::array<element, size>                                                             \
  {                                                                                                                    \
    using Parent = std::array<element, size>;                                                                          \
    using Parent::Parent;                                                                                              \
    using Parent::operator=;                                                                                           \
                                                                                                                       \
    template <typename Y, typename = std::enable_if_t<std::is_convertible_v<Y, element>, int>>                         \
    classname(std::initializer_list<Y> elems): Parent()                                                                \
    {                                                                                                                  \
      std::size_t i = 0;                                                                                               \
      for (auto itr = elems.begin(); itr != elems.end() && i < size; ++itr)                                            \
      {                                                                                                                \
        (*this)[i] = *itr;                                                                                             \
        ++i;                                                                                                           \
      }                                                                                                                \
    }                                                                                                                  \
  };

/// Used by the code generator NOLINTNEXTLINE
#define SEN_IMPL_GEN_STRUCT_OPERATORS(classname, doExport)                                                             \
  SEN_MAYBE_EXPORT(doExport) std::ostream& operator<<(std::ostream& out, const classname& val);                        \
  SEN_MAYBE_EXPORT(doExport) bool operator==(const classname& lhs, const classname& rhs);                              \
  SEN_MAYBE_EXPORT(doExport) bool operator!=(const classname& lhs, const classname& rhs);

/// Used by the code generator NOLINTNEXTLINE
#define SEN_IMPL_GEN_OPTIONAL(classname, elementtype, doExport)                                                        \
  struct classname final: public std::optional<elementtype>                                                            \
  {                                                                                                                    \
    using Parent = std::optional<elementtype>;                                                                         \
    using Parent::Parent;                                                                                              \
    using Parent::operator=;                                                                                           \
    using Parent::operator->;                                                                                          \
    using Parent::operator*;                                                                                           \
    using Parent::operator bool;                                                                                       \
  };                                                                                                                   \
                                                                                                                       \
  SEN_MAYBE_EXPORT(doExport) bool operator==(const classname& lhs, const classname& rhs);                              \
  SEN_MAYBE_EXPORT(doExport) bool operator!=(const classname& lhs, const classname& rhs);

/// Used by the code generator NOLINTNEXTLINE
#define SEN_IMPL_GEN_OPTIONAL_TRAITS(classname, doExport)                                                              \
  template <>                                                                                                          \
  struct MetaTypeTrait<classname>                                                                                      \
  {                                                                                                                    \
    [[nodiscard]] SEN_MAYBE_EXPORT(doExport) static ConstTypeHandle<OptionalType> meta();                              \
  };                                                                                                                   \
  template <>                                                                                                          \
  struct VariantTraits<classname>: private OptionalTraitsBase<classname>                                               \
  {                                                                                                                    \
    using OptionalTraitsBase<classname>::valueToVariant;                                                               \
    using OptionalTraitsBase<classname>::variantToValue;                                                               \
  };                                                                                                                   \
  template <>                                                                                                          \
  struct SerializationTraits<classname>: private OptionalTraitsBase<classname>                                         \
  {                                                                                                                    \
    using OptionalTraitsBase<classname>::write;                                                                        \
    using OptionalTraitsBase<classname>::read;                                                                         \
    using OptionalTraitsBase<classname>::serializedSize;                                                               \
  };

/// Used by the code generator NOLINTNEXTLINE
#define SEN_IMPL_GEN_ENUM_TRAITS(classname, doExport)                                                                  \
  template <>                                                                                                          \
  struct MetaTypeTrait<classname>                                                                                      \
  {                                                                                                                    \
    [[nodiscard]] SEN_MAYBE_EXPORT(doExport) static ConstTypeHandle<EnumType> meta();                                  \
  };                                                                                                                   \
  template <>                                                                                                          \
  struct SEN_MAYBE_EXPORT(doExport) VariantTraits<classname>: private EnumTraitsBase<classname>                        \
  {                                                                                                                    \
    using EnumTraitsBase<classname>::valueToVariant;                                                                   \
    using EnumTraitsBase<classname>::variantToValue;                                                                   \
  };                                                                                                                   \
  template <>                                                                                                          \
  struct SEN_MAYBE_EXPORT(doExport) SerializationTraits<classname>: private EnumTraitsBase<classname>                  \
  {                                                                                                                    \
    using EnumTraitsBase<classname>::write;                                                                            \
    using EnumTraitsBase<classname>::read;                                                                             \
    using EnumTraitsBase<classname>::serializedSize;                                                                   \
  };                                                                                                                   \
  template <>                                                                                                          \
  struct SEN_MAYBE_EXPORT(doExport) StringConversionTraits<classname>                                                  \
  {                                                                                                                    \
    [[nodiscard]] static std::string_view toString(classname val);                                                     \
    [[nodiscard]] static classname fromString(std::string_view val);                                                   \
  };                                                                                                                   \
                                                                                                                       \
  [[nodiscard]] SEN_MAYBE_EXPORT(doExport) std::string_view toString(classname value);

/// Used by the code generator NOLINTNEXTLINE
#define SEN_IMPL_GEN_SEQUENCE_TRAITS(classname, doExport)                                                              \
  template <>                                                                                                          \
  struct MetaTypeTrait<classname>                                                                                      \
  {                                                                                                                    \
    [[nodiscard]] SEN_MAYBE_EXPORT(doExport) static sen::ConstTypeHandle<SequenceType> meta();                         \
  };                                                                                                                   \
  template <>                                                                                                          \
  struct SEN_MAYBE_EXPORT(doExport) VariantTraits<classname>: private SequenceTraitsBase<classname>                    \
  {                                                                                                                    \
    using SequenceTraitsBase<classname>::valueToVariant;                                                               \
    using SequenceTraitsBase<classname>::variantToValue;                                                               \
  };                                                                                                                   \
  template <>                                                                                                          \
  struct SEN_MAYBE_EXPORT(doExport) SerializationTraits<classname>: private SequenceTraitsBase<classname>              \
  {                                                                                                                    \
    using SequenceTraitsBase<classname>::write;                                                                        \
    using SequenceTraitsBase<classname>::read;                                                                         \
    using SequenceTraitsBase<classname>::serializedSize;                                                               \
  };

/// Used by the code generator NOLINTNEXTLINE
#define SEN_IMPL_GEN_ARRAY_TRAITS(classname, doExport)                                                                 \
  template <>                                                                                                          \
  struct MetaTypeTrait<classname>                                                                                      \
  {                                                                                                                    \
    [[nodiscard]] SEN_MAYBE_EXPORT(doExport) static ConstTypeHandle<SequenceType> meta();                              \
  };                                                                                                                   \
  template <>                                                                                                          \
  struct SEN_MAYBE_EXPORT(doExport) VariantTraits<classname>: private ArrayTraitsBase<classname>                       \
  {                                                                                                                    \
    using ArrayTraitsBase<classname>::valueToVariant;                                                                  \
    using ArrayTraitsBase<classname>::variantToValue;                                                                  \
  };                                                                                                                   \
  template <>                                                                                                          \
  struct SEN_MAYBE_EXPORT(doExport) SerializationTraits<classname>: private ArrayTraitsBase<classname>                 \
  {                                                                                                                    \
    using ArrayTraitsBase<classname>::write;                                                                           \
    using ArrayTraitsBase<classname>::read;                                                                            \
    using ArrayTraitsBase<classname>::serializedSize;                                                                  \
  };

/// Used by the code generator NOLINTNEXTLINE
#define SEN_IMPL_GEN_STRUCT_TRAITS(classname, doExport)                                                                \
  template <>                                                                                                          \
  struct MetaTypeTrait<classname>                                                                                      \
  {                                                                                                                    \
    [[nodiscard]] SEN_MAYBE_EXPORT(doExport) static ConstTypeHandle<StructType> meta();                                \
  };                                                                                                                   \
  template <>                                                                                                          \
  struct SEN_MAYBE_EXPORT(doExport) VariantTraits<classname>: protected StructTraitsBase                               \
  {                                                                                                                    \
    static void valueToVariant(const classname& val, Var& var);                                                        \
    static void variantToValue(const Var& var, classname& val);                                                        \
    static std::function<lang::Value(const void*)> getFieldValueGetterFunction(Span<uint16_t> fields);                 \
  };                                                                                                                   \
  template <>                                                                                                          \
  struct SEN_MAYBE_EXPORT(doExport) SerializationTraits<classname>: protected StructTraitsBase                         \
  {                                                                                                                    \
    static void write(OutputStream& out, const classname& val);                                                        \
    static void read(InputStream& in, classname& val);                                                                 \
    [[nodiscard]] static uint32_t serializedSize(const classname& val) noexcept;                                       \
  };

/// Used by the code generator NOLINTNEXTLINE
#define SEN_IMPL_GEN_VARIANT_TRAITS(classname, doExport)                                                               \
  template <>                                                                                                          \
  struct SEN_MAYBE_EXPORT(doExport) MetaTypeTrait<classname>                                                           \
  {                                                                                                                    \
    [[nodiscard]] static ConstTypeHandle<VariantType> meta();                                                          \
  };                                                                                                                   \
  template <>                                                                                                          \
  struct SEN_MAYBE_EXPORT(doExport) VariantTraits<classname>: protected VariantTraitsBase<classname>                   \
  {                                                                                                                    \
    static void valueToVariant(const classname& val, Var& var);                                                        \
    static void variantToValue(const Var& var, classname& val);                                                        \
    static std::function<lang::Value(const void*)> getFieldValueGetterFunction(Span<uint16_t> fields);                 \
    using VariantTraitsBase<classname>::tryPrintField;                                                                 \
  };                                                                                                                   \
  template <>                                                                                                          \
  struct SEN_MAYBE_EXPORT(doExport) SerializationTraits<classname>: protected VariantTraitsBase<classname>             \
  {                                                                                                                    \
    static void write(OutputStream& out, const classname& val);                                                        \
    static void read(InputStream& in, classname& val);                                                                 \
    [[nodiscard]] static uint32_t serializedSize(const classname& val) noexcept;                                       \
  };

/// Used by the code generator NOLINTNEXTLINE
#define SEN_IMPL_GEN_CLASS_META_TRAITS(classname, doExport)                                                            \
  template <>                                                                                                          \
  struct MetaTypeTrait<classname>                                                                                      \
  {                                                                                                                    \
    [[nodiscard]] SEN_MAYBE_EXPORT(doExport) static ConstTypeHandle<ClassType> meta();                                 \
  };

/// Used by the code generator NOLINTNEXTLINE
#define SEN_IMPL_GEN_CLASS_TRAITS_TEMPLATE(interfaceName, remoteProxy, localProxy, baseName)                           \
  template <>                                                                                                          \
  struct SenClassRelation<interfaceName>                                                                               \
  {                                                                                                                    \
    using InterfaceType = interfaceName;                                                                               \
    template <typename T>                                                                                              \
    using BaseType = baseName<T>;                                                                                      \
    static constexpr bool isBaseTypeTemplate = true;                                                                   \
    using RemoteProxyType = remoteProxy;                                                                               \
    using LocalProxyType = localProxy;                                                                                 \
  };                                                                                                                   \
  template <>                                                                                                          \
  struct SenClassRelation<remoteProxy>                                                                                 \
  {                                                                                                                    \
    using InterfaceType = interfaceName;                                                                               \
    template <typename T>                                                                                              \
    using BaseType = baseName<T>;                                                                                      \
    static constexpr bool isBaseTypeTemplate = true;                                                                   \
    using RemoteProxyType = remoteProxy;                                                                               \
    using LocalProxyType = localProxy;                                                                                 \
  };                                                                                                                   \
  template <>                                                                                                          \
  struct SenClassRelation<localProxy>                                                                                  \
  {                                                                                                                    \
    using InterfaceType = interfaceName;                                                                               \
    template <typename T>                                                                                              \
    using BaseType = baseName<T>;                                                                                      \
    static constexpr bool isBaseTypeTemplate = true;                                                                   \
    using RemoteProxyType = remoteProxy;                                                                               \
    using LocalProxyType = localProxy;                                                                                 \
  };                                                                                                                   \
  template <typename T>                                                                                                \
  struct SenClassRelation<baseName<T>>                                                                                 \
  {                                                                                                                    \
    using InterfaceType = interfaceName;                                                                               \
    template <typename U>                                                                                              \
    using BaseType = baseName<U>;                                                                                      \
    static constexpr bool isBaseTypeTemplate = true;                                                                   \
    using RemoteProxyType = remoteProxy;                                                                               \
    using LocalProxyType = localProxy;                                                                                 \
  };

/// Used by the code generator NOLINTNEXTLINE
#define SEN_IMPL_GEN_CLASS_TRAITS_NORMAL(interfaceName, remoteProxy, localProxy, baseName)                             \
  template <>                                                                                                          \
  struct SenClassRelation<interfaceName>                                                                               \
  {                                                                                                                    \
    using InterfaceType = interfaceName;                                                                               \
    using BaseType = baseName;                                                                                         \
    static constexpr bool isBaseTypeTemplate = false;                                                                  \
    using RemoteProxyType = remoteProxy;                                                                               \
    using LocalProxyType = localProxy;                                                                                 \
  };                                                                                                                   \
  template <>                                                                                                          \
  struct SenClassRelation<remoteProxy>                                                                                 \
  {                                                                                                                    \
    using InterfaceType = interfaceName;                                                                               \
    using BaseType = baseName;                                                                                         \
    static constexpr bool isBaseTypeTemplate = false;                                                                  \
    using RemoteProxyType = remoteProxy;                                                                               \
    using LocalProxyType = localProxy;                                                                                 \
  };                                                                                                                   \
  template <>                                                                                                          \
  struct SenClassRelation<localProxy>                                                                                  \
  {                                                                                                                    \
    using InterfaceType = interfaceName;                                                                               \
    using BaseType = baseName;                                                                                         \
    static constexpr bool isBaseTypeTemplate = false;                                                                  \
    using RemoteProxyType = remoteProxy;                                                                               \
    using LocalProxyType = localProxy;                                                                                 \
  };                                                                                                                   \
  template <>                                                                                                          \
  struct SenClassRelation<baseName>                                                                                    \
  {                                                                                                                    \
    using InterfaceType = interfaceName;                                                                               \
    using BaseType = baseName;                                                                                         \
    static constexpr bool isBaseTypeTemplate = false;                                                                  \
    using RemoteProxyType = remoteProxy;                                                                               \
    using LocalProxyType = localProxy;                                                                                 \
  };

/// Used by the code generator NOLINTNEXTLINE
#define SEN_IMPL_GEN_QUANTITY_TRAITS(classname, maxVal, minVal, doExport)                                              \
  template <>                                                                                                          \
  struct MetaTypeTrait<classname>                                                                                      \
  {                                                                                                                    \
    [[nodiscard]] SEN_MAYBE_EXPORT(doExport) static ConstTypeHandle<QuantityType> meta();                              \
  };                                                                                                                   \
  template <>                                                                                                          \
  struct SEN_MAYBE_EXPORT(doExport) VariantTraits<classname>: private QuantityTraitsBase<classname>                    \
  {                                                                                                                    \
    using QuantityTraitsBase<classname>::valueToVariant;                                                               \
    using QuantityTraitsBase<classname>::variantToValue;                                                               \
  };                                                                                                                   \
  template <>                                                                                                          \
  struct SEN_MAYBE_EXPORT(doExport) SerializationTraits<classname>: private QuantityTraitsBase<classname>              \
  {                                                                                                                    \
    using QuantityTraitsBase<classname>::write;                                                                        \
    using QuantityTraitsBase<classname>::read;                                                                         \
    using QuantityTraitsBase<classname>::serializedSize;                                                               \
  };                                                                                                                   \
  template <>                                                                                                          \
  struct SEN_MAYBE_EXPORT(doExport) QuantityTraits<classname>                                                          \
  {                                                                                                                    \
    using ValueType = typename classname::ValueType;                                                                   \
                                                                                                                       \
    static constexpr ValueType max = maxVal;                                                                           \
    static constexpr ValueType min = minVal;                                                                           \
                                                                                                                       \
    [[nodiscard]] static ::std::optional<const sen::Unit*> unit() noexcept;                                            \
    [[nodiscard]] static const sen::Unit& unit(sen::Unit::EnsureTag);                                                  \
  };

#endif  // SEN_CORE_OBJ_DETAIL_GEN_H
