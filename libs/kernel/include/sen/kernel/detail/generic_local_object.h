// === generic_local_object.h ==========================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_KERNEL_DETAIL_GENERIC_LOCAL_OBJECT_H
#define SEN_KERNEL_DETAIL_GENERIC_LOCAL_OBJECT_H

// sen
#include "sen/core/base/compiler_macros.h"
#include "sen/core/base/span.h"
#include "sen/core/base/timestamp.h"
#include "sen/core/io/input_stream.h"
#include "sen/core/io/output_stream.h"
#include "sen/core/meta/class_type.h"
#include "sen/core/meta/property.h"
#include "sen/core/meta/type.h"
#include "sen/core/meta/var.h"
#include "sen/core/obj/detail/native_object_impl.h"
#include "sen/core/obj/detail/property_flags.h"
#include "sen/core/obj/native_object.h"
#include "sen/core/obj/object.h"
#include "sen/kernel/detail/generic_local_object_proxy.h"

// std
#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace sen::kernel::impl
{

/// An object of a class with no generated code, such as one read from a FOM at runtime. It holds
/// each property as its Sen encoding and otherwise behaves like a generated object. Only its
/// property getters can be called.
class SEN_EXPORT GenericLocalObject final: public NativeObject, public EncodedPropertyValues
{
public:
  SEN_NOCOPY_NOMOVE(GenericLocalObject)

public:
  /// An object of `type` with no property set. Throws if `type` has generated proxy makers.
  GenericLocalObject(const std::string& name, ConstTypeHandle<ClassType> type);
  ~GenericLocalObject() override;

public:  // implements Object
  [[nodiscard]] ConstTypeHandle<ClassType> getClass() const noexcept override;
  void invokeAllPropertyCallbacks() override;

public:  // implements NativeObject
  [[nodiscard]] std::shared_ptr<::sen::impl::NativeObjectProxy> senImplMakeLocalProxy(
    const std::string& localPrefix) override;

public:  // implements EncodedPropertyValues
  /// The property's current value; empty for an unknown id.
  [[nodiscard]] Span<const uint8_t> getPropertyEncoding(MemberHash propertyId) const override;

public:
  /// The class's properties, inherited ones included, in the order `index` counts them.
  [[nodiscard]] const PropertyList& properties() const noexcept { return properties_; }

  /// Sets a property from its Sen encoding: a dynamic one at the next commit, a static one at once.
  /// Throws if `value` is empty.
  /// @pre index < properties().size()
  void set(std::size_t index, Span<const uint8_t> value);

  /// Sets the properties in `records`, framed as Sen property updates (u32 id, u32 size, value).
  /// @returns false, setting nothing, if any record is malformed or names an unknown property.
  [[nodiscard]] bool apply(Span<const uint8_t> records);

  /// Makes pending values current. Runners do this each cycle; call it only without a runner.
  using NativeObject::commit;

  /// A property's current value, empty if never set. Call it on the owning thread; the view is
  /// valid until the property next changes.
  /// @pre index < properties().size()
  [[nodiscard]] Span<const uint8_t> current(std::size_t index) const noexcept;

  /// A property's change flags. Call it on the owning thread.
  /// @pre index < properties().size()
  [[nodiscard]] const ::sen::impl::PropertyFlags& flags(std::size_t index) const noexcept;

protected:  // implements Object
  void senImplRemoveTypedConnection(ConnId id) override;
  void senImplWriteAllPropertiesToStream(OutputStream& out) const override;
  void senImplWriteStaticPropertiesToStream(OutputStream& out) const override;
  void senImplWriteDynamicPropertiesToStream(OutputStream& out) const override;

protected:  // implements NativeObject
  [[nodiscard]] Var senImplGetPropertyImpl(MemberHash propertyId) const override;
  void senImplSetNextPropertyUntyped(MemberHash propertyId, const Var& value) override;
  [[nodiscard]] Var senImplGetNextPropertyUntyped(MemberHash propertyId) const override;
  void senImplCommitImpl(TimeStamp time) override;
  void senImplWriteChangedPropertiesToStream(OutputStream& confirmed,
                                             ::sen::impl::BufferProvider uni,
                                             ::sen::impl::BufferProvider multi) override;
  void senImplStreamCall(MemberHash methodId, InputStream& in, StreamCallForwarder&& fwd) override;
  void senImplVariantCall(MemberHash methodId, const VarList& args, VariantCallForwarder&& fwd) override;
  [[nodiscard]] ::sen::impl::FieldValueGetter senImplGetFieldValueGetter(MemberHash propertyId,
                                                                         Span<uint16_t> fields) const override;

private:  // implements NativeObject
  [[nodiscard]] uint32_t senImplComputeMaxReliableSerializedPropertySizeImpl() const override;

private:
  struct Slot
  {
    const Property* property = nullptr;
    bool dynamic = true;
    std::array<std::vector<uint8_t>, 2U> values;
    ::sen::impl::PropertyFlags flags;  ///< which value is current, and whether the last commit changed it
    bool pending = false;              ///< whether a value waits for the next commit
  };

  /// A record apply() has checked: the property's index, and the value's offset and size.
  struct Record
  {
    std::size_t index = 0U;
    std::size_t offset = 0U;
    std::size_t size = 0U;
  };

  [[nodiscard]] static const std::vector<uint8_t>& currentOf(const Slot& slot) noexcept;
  [[nodiscard]] static std::vector<uint8_t>& currentOf(Slot& slot) noexcept;
  [[nodiscard]] static const std::vector<uint8_t>& nextOf(const Slot& slot) noexcept;
  [[nodiscard]] static std::vector<uint8_t>& nextOf(Slot& slot) noexcept;

  /// Under the writer lock.
  void setLocked(std::size_t index, Span<const uint8_t> value);
  [[nodiscard]] const Slot* slotOf(MemberHash propertyId) const noexcept;
  [[nodiscard]] const Property* getterOf(MemberHash methodId) const noexcept;
  [[nodiscard]] std::string refusal(MemberHash methodId) const;
  void writeAll(OutputStream& out, bool statics, bool dynamics) const;

private:
  ConstTypeHandle<ClassType> type_;
  PropertyList properties_;
  std::vector<Slot> slots_;
  std::unordered_map<uint32_t, std::size_t> byId_;
  std::vector<std::size_t> pending_;  ///< set since the last commit
  std::vector<std::size_t> changed_;  ///< made current by the last commit, and sent as changes
  std::vector<Record> records_;       ///< apply()'s checked records
};

}  // namespace sen::kernel::impl

#endif  // SEN_KERNEL_DETAIL_GENERIC_LOCAL_OBJECT_H
