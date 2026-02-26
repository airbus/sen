// === native_object_proxy.h ===========================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_CORE_OBJ_DETAIL_NATIVE_OBJECT_PROXY_H
#define SEN_CORE_OBJ_DETAIL_NATIVE_OBJECT_PROXY_H

// sen
#include "sen/core/base/compiler_macros.h"
#include "sen/core/base/timestamp.h"
#include "sen/core/meta/class_type.h"
#include "sen/core/meta/property.h"
#include "sen/core/meta/type.h"
#include "sen/core/obj/callback.h"
#include "sen/core/obj/connection_guard.h"
#include "sen/core/obj/detail/proxy_object.h"

// std
#include <mutex>

namespace sen
{

// Forward declarations
class NativeObject;

namespace impl
{

/// Proxy for an object that resides in this process
class SEN_EXPORT NativeObjectProxy: public ProxyObject
{
  SEN_NOCOPY_NOMOVE(NativeObjectProxy)

public:  // special members
  NativeObjectProxy(NativeObject* owner, std::string_view localPrefix);
  ~NativeObjectProxy() override = default;

public:  // implements Object
  [[nodiscard]] ObjectId getId() const noexcept final;
  [[nodiscard]] const std::string& getName() const noexcept final;
  [[nodiscard]] ConstTypeHandle<ClassType> getClass() const noexcept override;
  [[nodiscard]] const ProxyObject* asProxyObject() const noexcept final;
  [[nodiscard]] ProxyObject* asProxyObject() noexcept final;
  [[nodiscard]] RemoteObject* asRemoteObject() noexcept final;
  [[nodiscard]] const RemoteObject* asRemoteObject() const noexcept final;
  [[nodiscard]] const std::string& getLocalName() const noexcept final;
  [[nodiscard]] TimeStamp getLastCommitTime() const noexcept final;
  [[nodiscard]] ConnectionGuard onEventUntyped(const Event* ev, EventCallback<VarList>&& callback) final;
  [[nodiscard]] ConnectionGuard onPropertyChangedUntyped(const Property* prop, EventCallback<VarList>&& callback) final;
  void invokeAllPropertyCallbacks() final;

  // NOLINTNEXTLINE
  void invokeUntyped(const Method* method, const VarList& args, MethodCallback<Var>&& onDone = {}) final;
  [[nodiscard]] Var getPropertyUntyped(const Property* prop) const final;
  void drainInputs() override;
  bool isRemote() const noexcept final;

protected:  // called by the generated code
  void sendWork(std::function<void()>&& work, bool forcePush) const;
  virtual void drainInputsImpl(TimeStamp lastCommitTime) = 0;
  [[nodiscard]] virtual Var senImplGetPropertyImpl(MemberHash propertyId) const = 0;
  [[nodiscard]] ConnId senImplMakeConnectionId() const noexcept;
  void senImplEventEmitted(MemberHash id, std::function<VarList()>&& argsGetter, const EventInfo& info) final;
  void removeTypedConnectionOnOwner(ConnId connId);
  void senImplRemoveUntypedConnection(ConnId id, MemberHash memberHash) override;

private:
  struct EventCallbackData
  {
    uint32_t id;
    EventCallback<VarList> callback;
  };

  using EventCallbackList = std::vector<EventCallbackData>;

  struct EventData
  {
    EventCallbackList list;
    TransportMode transportMode;
  };

  using EventCallbackMap = std::unordered_map<MemberHash, EventData>;

private:
  NativeObject* owner_;
  std::shared_ptr<Object> guard_;
  std::string localName_;
  TimeStamp lastCommitTime_;
  std::unique_ptr<EventCallbackMap> eventCallbacks_;
  std::recursive_mutex eventCallbacksMutex_;
};

}  // namespace impl

}  // namespace sen

#endif  // SEN_CORE_OBJ_DETAIL_NATIVE_OBJECT_PROXY_H
