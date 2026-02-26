// === object.h ========================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_CORE_OBJ_OBJECT_H
#define SEN_CORE_OBJ_OBJECT_H

// sen
#include "sen/core/base/compiler_macros.h"
#include "sen/core/base/strong_type.h"
#include "sen/core/base/timestamp.h"
#include "sen/core/io/input_stream.h"
#include "sen/core/io/output_stream.h"
#include "sen/core/meta/event.h"
#include "sen/core/meta/method.h"
#include "sen/core/meta/property.h"
#include "sen/core/meta/type.h"
#include "sen/core/meta/var.h"
#include "sen/core/obj/callback.h"
#include "sen/core/obj/connection_guard.h"

// std
#include <cstdint>
#include <memory>
#include <string_view>

namespace sen
{
class NativeObject;
}  // namespace sen

namespace sen::impl
{
class ProxyObject;
template <typename... T>
class EventBuffer;
void writeAllPropertiesToStream(const Object* instance, OutputStream& out);
}  // namespace sen::impl

namespace sen
{

/// \addtogroup obj
/// @{

/// How to emit an event.
enum class Emit
{
  now,       ///< Directly when it happens.
  onCommit,  ///< When commit is called.
};

SEN_STRONG_TYPE(ConnId, uint32_t)

template <>
struct ShouldBePassedByValue<ConnId>: std::true_type
{
};

/// Unique identifier for any object instance
SEN_STRONG_TYPE(ObjectId, uint32_t)

template <>
struct ShouldBePassedByValue<ObjectId>: std::true_type
{
};

// Forward declarations
class ClassType;

/// A sen object.
class SEN_EXPORT Object: public std::enable_shared_from_this<Object>
{
  SEN_NOCOPY_NOMOVE(Object)

public:
  Object() = default;
  virtual ~Object() = default;

public:
  /// The name given to the object upon construction
  [[nodiscard]] virtual const std::string& getName() const noexcept = 0;

  /// Global unique object identification.
  [[nodiscard]] virtual ObjectId getId() const noexcept = 0;

  /// Reflection information
  // [[nodiscard]] virtual const ClassType* getClass() const noexcept = 0;
  [[nodiscard]] virtual ConstTypeHandle<ClassType> getClass() const noexcept = 0;

  /// Helper that checks if the object is local (without dynamic casts).
  [[nodiscard]] virtual NativeObject* asNativeObject() noexcept { return nullptr; }

  /// Helper that checks if the object is local (without dynamic casts).
  [[nodiscard]] virtual const NativeObject* asNativeObject() const noexcept { return nullptr; }

  /// Helper that checks if the object is a proxy (without dynamic casts).
  [[nodiscard]] virtual impl::ProxyObject* asProxyObject() noexcept { return nullptr; }

  /// Helper that checks if the object is a proxy (without dynamic casts).
  [[nodiscard]] virtual const impl::ProxyObject* asProxyObject() const noexcept { return nullptr; }

  /// An alias used as an alternate way of identifying this object locally
  [[nodiscard]] virtual const std::string& getLocalName() const noexcept = 0;

  /// The point in time when the last commit was called.
  [[nodiscard]] virtual TimeStamp getLastCommitTime() const noexcept = 0;

public:  // Reflection-based interface (do not use for real-time applications)
  /// Variant-based property getter.
  [[nodiscard]] virtual Var getPropertyUntyped(const Property* prop) const = 0;

  /// Reflection- and variant- based interface for (asynchronously) invoking a method on this object.
  /// This mechanism is meant to be used only when the natively generated functions are not
  /// available or when performance or type safety are of no particular concern.
  /// NOLINTNEXTLINE(google-default-arguments)
  virtual void invokeUntyped(const Method* method, const VarList& args, MethodCallback<Var>&& onDone = {}) = 0;

  /// Reflection-based interface for detecting property changes.
  /// @note Only one callback per property is stored.
  /// This mechanism is meant to be used only when the natively generated functions are not
  /// available or when performance or type safety are of no particular concern.
  [[nodiscard]] virtual ConnectionGuard onPropertyChangedUntyped(const Property* prop,
                                                                 EventCallback<VarList>&& callback) = 0;

  /// Reflection-based interface for reacting to events.
  /// @note Only one callback per event is stored.
  /// This mechanism is meant to be used only when the natively generated functions are not
  /// available or when performance or type safety are of no particular concern.
  [[nodiscard]] virtual ConnectionGuard onEventUntyped(const Event* ev, EventCallback<VarList>&& callback) = 0;

  /// Helper method that invokes all the registered property callbacks, even if/when
  /// the property has not changed. This might prove helpful in situations where
  /// the initialization code is similar to the one of these callbacks.
  virtual void invokeAllPropertyCallbacks() = 0;

protected:
  friend class ConnectionGuard;
  virtual void senImplRemoveTypedConnection(ConnId id) = 0;
  virtual void senImplRemoveUntypedConnection(ConnId id, MemberHash memberHash) = 0;

protected:
  template <typename... T>
  friend class impl::EventBuffer;

  virtual void senImplEventEmitted(MemberHash id, std::function<VarList()>&& argsGetter, const EventInfo& info) = 0;

  /// Creates a unique id
  [[nodiscard]] ObjectId senImplMakeId(std::string_view objectName) const;

  /// Creates a guard for a callback
  [[nodiscard]] ConnectionGuard senImplMakeConnectionGuard(ConnId id, MemberHash member, bool typed);

  /// Throws std::exception if the name is not valid
  static void senImplValidateName(std::string_view name);

  /// The name, with a prefix (if any)
  [[nodiscard]] static std::string senImplComputeLocalName(std::string_view name, std::string_view prefix);

protected:
  friend void impl::writeAllPropertiesToStream(const Object* instance, OutputStream& out);

  /// Writes all properties to out
  virtual void senImplWriteAllPropertiesToStream(OutputStream& out) const = 0;

  /// Writes all static properties to out
  virtual void senImplWriteStaticPropertiesToStream(OutputStream& out) const = 0;

  /// Writes all dynamic properties to out
  virtual void senImplWriteDynamicPropertiesToStream(OutputStream& out) const = 0;
};

/// @}

}  // namespace sen

SEN_STRONG_TYPE_HASHABLE(::sen::ConnId)
SEN_STRONG_TYPE_HASHABLE(::sen::ObjectId)

#endif  // SEN_CORE_OBJ_OBJECT_H
