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

/// Opaque identifier for a property-change or event callback connection.
/// Stored inside a ConnectionGuard; destroyed when the guard goes out of scope.
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

/// Abstract base class for all Sen objects, both local (`NativeObject`) and remote (proxy).
///
/// An `Object` exposes a reflection-based interface (untyped property access, method invocation,
/// and event subscription) that works uniformly regardless of whether the object lives in the
/// same process or in a remote component. Generated typed subclasses wrap these interfaces with
/// compile-time-checked helpers.
///
/// `Object` instances are always reference-counted (`shared_from_this`). Do not allocate them
/// on the stack or store raw pointers across component boundaries.
class SEN_EXPORT Object: public std::enable_shared_from_this<Object>
{
  SEN_NOCOPY_NOMOVE(Object)

public:
  Object() = default;
  virtual ~Object() = default;

public:
  /// @return The name assigned to this object at construction time (unique within its bus).
  [[nodiscard]] virtual const std::string& getName() const noexcept = 0;

  /// @return Globally unique numeric identifier for this object instance.
  [[nodiscard]] virtual ObjectId getId() const noexcept = 0;

  /// @return Type handle for the reflected `ClassType` of this object.
  [[nodiscard]] virtual ConstTypeHandle<ClassType> getClass() const noexcept = 0;

  /// Downcasts to `NativeObject` without a dynamic cast.
  /// @return Pointer to the `NativeObject` subclass, or `nullptr` if this is a proxy.
  [[nodiscard]] virtual NativeObject* asNativeObject() noexcept { return nullptr; }

  /// @copydoc asNativeObject()
  [[nodiscard]] virtual const NativeObject* asNativeObject() const noexcept { return nullptr; }

  /// Downcasts to `ProxyObject` without a dynamic cast.
  /// @return Pointer to the `ProxyObject` subclass, or `nullptr` if this is a native object.
  [[nodiscard]] virtual impl::ProxyObject* asProxyObject() noexcept { return nullptr; }

  /// @copydoc asProxyObject()
  [[nodiscard]] virtual const impl::ProxyObject* asProxyObject() const noexcept { return nullptr; }

  /// @return A locally-scoped name alias (may include a prefix added by proxy construction).
  [[nodiscard]] virtual const std::string& getLocalName() const noexcept = 0;

  /// @return The wall-clock timestamp of the most recent `commit()` call on this object.
  [[nodiscard]] virtual TimeStamp getLastCommitTime() const noexcept = 0;

public:  // Reflection-based interface (do not use for real-time applications)
  /// Returns the current value of a property as a type-erased `Var`.
  /// @param prop Property descriptor (must belong to this object's class).
  /// @return Current property value wrapped in a `Var`.
  [[nodiscard]] virtual Var getPropertyUntyped(const Property* prop) const = 0;

  /// Reflection- and variant-based interface for asynchronously invoking a method on this object.
  /// Prefer the natively generated typed methods for performance-sensitive or type-safe code.
  /// @param method  Descriptor of the method to call (must belong to this object's class).
  /// @param args    Arguments as a list of type-erased variants, in declaration order.
  /// @param onDone  Optional callback invoked with the return value once the call completes.
  /// NOLINTNEXTLINE(google-default-arguments)
  virtual void invokeUntyped(const Method* method, const VarList& args, MethodCallback<Var>&& onDone = {}) = 0;

  /// Reflection-based interface for subscribing to property changes.
  /// @note Only one untyped callback per property is retained; a second call replaces the first.
  ///       Prefer the generated typed `on*Changed` helpers for performance-sensitive code.
  /// @param prop     The property to observe (must belong to this object's class).
  /// @param callback Invoked with the new property values whenever the property changes.
  /// @return A ConnectionGuard; the subscription is automatically removed when the guard is destroyed.
  [[nodiscard]] virtual ConnectionGuard onPropertyChangedUntyped(const Property* prop,
                                                                 EventCallback<VarList>&& callback) = 0;

  /// Reflection-based interface for subscribing to events.
  /// @note Only one untyped callback per event is retained; a second call replaces the first.
  ///       Prefer the generated typed `on*` helpers for performance-sensitive code.
  /// @param ev       The event to observe (must belong to this object's class).
  /// @param callback Invoked with the event arguments whenever the event is emitted.
  /// @return A ConnectionGuard; the subscription is automatically removed when the guard is destroyed.
  [[nodiscard]] virtual ConnectionGuard onEventUntyped(const Event* ev, EventCallback<VarList>&& callback) = 0;

  /// Forces all registered property-change callbacks to fire immediately, regardless of whether
  /// any property has actually changed. Useful during component initialisation to apply the
  /// same logic that the callbacks would normally apply on change.
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
