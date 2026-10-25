// === generic_local_object_proxy.h ====================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_KERNEL_DETAIL_GENERIC_LOCAL_OBJECT_PROXY_H
#define SEN_KERNEL_DETAIL_GENERIC_LOCAL_OBJECT_PROXY_H

// sen
#include "sen/core/base/compiler_macros.h"
#include "sen/core/base/span.h"
#include "sen/core/base/timestamp.h"
#include "sen/core/io/output_stream.h"
#include "sen/core/meta/property.h"
#include "sen/core/meta/type.h"
#include "sen/core/meta/var.h"
#include "sen/core/obj/detail/native_object_proxy.h"
#include "sen/core/obj/native_object.h"
#include "sen/core/obj/object.h"

// std
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace sen::kernel::impl
{

/// An object's properties as Sen encodings, for an object without generated members. A
/// GenericLocalObjectProxy reads the object through it.
class SEN_EXPORT EncodedPropertyValues
{
  SEN_NOCOPY_NOMOVE(EncodedPropertyValues)

public:
  EncodedPropertyValues() = default;
  virtual ~EncodedPropertyValues();

public:
  /// The property's current encoding, or empty if it has none. Called under the object's reader
  /// lock, so the object must change its values only under its writer lock.
  [[nodiscard]] virtual Span<const uint8_t> getPropertyEncoding(MemberHash propertyId) const = 0;
};

/// When each of an object's properties last changed, such as a replayed object's recorded times. A
/// GenericLocalObjectProxy given these notifies a property whenever its time moves, with that time,
/// even if the value is unchanged.
class SEN_EXPORT PropertyChangeTimes
{
  SEN_NOCOPY_NOMOVE(PropertyChangeTimes)

public:
  PropertyChangeTimes() = default;
  virtual ~PropertyChangeTimes();

public:
  /// When the property last changed. Called under the object's reader lock.
  [[nodiscard]] virtual TimeStamp getPropertyChangeTime(MemberHash propertyId) const = 0;
};

/// The local proxy of an object without generated members. Like a generated proxy, it copies the
/// object's values at each drain and notifies the properties that changed. Use it from the
/// subscriber's thread. Without `encodings`, it reads the object through its stream writers.
class SEN_EXPORT GenericLocalObjectProxy final: public ::sen::impl::NativeObjectProxy
{
public:
  SEN_NOCOPY_NOMOVE(GenericLocalObjectProxy)

public:
  GenericLocalObjectProxy(NativeObject* owner,
                          const EncodedPropertyValues* encodings,
                          const PropertyChangeTimes* times,
                          std::string_view localPrefix);
  ~GenericLocalObjectProxy() override;

protected:  // implements NativeObjectProxy
  void drainInputsImpl(TimeStamp lastCommitTime) override;
  [[nodiscard]] Var senImplGetPropertyImpl(MemberHash propertyId) const override;

protected:  // implements Object
  void senImplWriteAllPropertiesToStream(OutputStream& out) const override;
  void senImplWriteStaticPropertiesToStream(OutputStream& out) const override;
  void senImplWriteDynamicPropertiesToStream(OutputStream& out) const override;
  void senImplRemoveTypedConnection(ConnId id) override;

private:
  struct Slot
  {
    const Property* property = nullptr;
    bool dynamic = true;
    bool hasValue = false;       ///< whether the object has given it a value yet
    std::vector<uint8_t> value;  ///< as of the last drain
    TimeStamp time;              ///< the change time as of the last drain
  };

  void takeWritten(TimeStamp lastCommitTime);
  void takeEncodings(TimeStamp lastCommitTime);
  [[nodiscard]] static bool take(Slot& slot, Span<const uint8_t> now);
  void writeAll(OutputStream& out, bool statics, bool dynamics) const;

private:
  NativeObject* owner_;
  const EncodedPropertyValues* encodings_;  ///< may be null
  const PropertyChangeTimes* times_;        ///< may be null
  std::vector<Slot> slots_;
  std::unordered_map<uint32_t, std::size_t> byId_;
  std::vector<uint8_t> written_;  ///< what the object wrote at the last full write
  bool taken_ = false;            ///< whether the first drain has run
};

/// A GenericLocalObjectProxy, for an object's senImplMakeLocalProxy. Never throws: on failure it logs
/// once per class and returns nullptr.
[[nodiscard]] std::shared_ptr<GenericLocalObjectProxy> makeGenericLocalProxy(NativeObject* object,
                                                                             const EncodedPropertyValues* encodings,
                                                                             const PropertyChangeTimes* times,
                                                                             const std::string& localPrefix);

}  // namespace sen::kernel::impl

#endif  // SEN_KERNEL_DETAIL_GENERIC_LOCAL_OBJECT_PROXY_H
