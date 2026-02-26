// === generic_remote_object.h =========================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_LIBS_KERNEL_SRC_BUS_GENERIC_REMOTE_OBJECT_H
#define SEN_LIBS_KERNEL_SRC_BUS_GENERIC_REMOTE_OBJECT_H

// sen
#include "sen/core/base/compiler_macros.h"
#include "sen/core/base/timestamp.h"
#include "sen/core/io/input_stream.h"
#include "sen/core/io/output_stream.h"
#include "sen/core/meta/class_type.h"
#include "sen/core/meta/method.h"
#include "sen/core/meta/type.h"
#include "sen/core/meta/var.h"
#include "sen/core/obj/detail/property_flags.h"
#include "sen/core/obj/detail/remote_object.h"
#include "sen/core/obj/object.h"

// std
#include <array>
#include <unordered_map>

namespace sen::kernel::impl
{

class GenericRemoteObject final: public ::sen::impl::RemoteObject
{
public:
  SEN_NOCOPY_NOMOVE(GenericRemoteObject)

public:  // special members
  explicit GenericRemoteObject(::sen::impl::RemoteObjectInfo&& info);
  ~GenericRemoteObject() override;

protected:
  void copyStateFrom(const RemoteObject& other) override;
  void drainInputs() override;
  void eventReceived(MemberHash eventId, TimeStamp creationTime, const Span<const uint8_t>& args) override;
  [[nodiscard]] bool readPropertyFromStream(MemberHash id, InputStream& in, bool advanceCurrent) override;
  [[nodiscard]] Var senImplGetPropertyImpl(MemberHash propertyId) const override;
  void serializeMethodArgumentsFromVariants(MemberHash methodId, const VarList& args, OutputStream& out) const override;
  [[nodiscard]] Var deserializeMethodReturnValueAsVariant(MemberHash methodId, InputStream& in) const override;
  void senImplRemoveTypedConnection(ConnId id) override;
  void senImplWriteAllPropertiesToStream(OutputStream& out) const override;
  void senImplWriteStaticPropertiesToStream(OutputStream& out) const override;
  void senImplWriteDynamicPropertiesToStream(OutputStream& out) const override;
  void invokeAllPropertyCallbacks() override;

private:
  struct PropertyData
  {
    ::sen::impl::PropertyFlags flags;
    std::array<Var, 2> value;
  };

private:
  [[nodiscard]] const Method* searchMethodById(MemberHash methodId) const;

private:
  std::unordered_map<MemberHash, PropertyData> properties_;
  PropertyList metaProperties_;
};

}  // namespace sen::kernel::impl

#endif  // SEN_LIBS_KERNEL_SRC_BUS_GENERIC_REMOTE_OBJECT_H
