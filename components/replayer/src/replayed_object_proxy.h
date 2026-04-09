// === replayed_object_proxy.h =========================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_COMPONENTS_REPLAYER_SRC_REPLAYED_OBJECT_PROXY_H
#define SEN_COMPONENTS_REPLAYER_SRC_REPLAYED_OBJECT_PROXY_H

#include "replayed_object.h"

// sen
#include "sen/core/obj/detail/native_object_proxy.h"

namespace sen::components::replayer
{

class ReplayedObjectProxy: public ::sen::impl::NativeObjectProxy
{
  SEN_NOCOPY_NOMOVE(ReplayedObjectProxy)

public:
  ReplayedObjectProxy(ReplayedObject* owner, std::string_view localPrefix);
  ~ReplayedObjectProxy() override;

protected:  // implements NativeObject
  void drainInputsImpl(TimeStamp lastCommitTime) override;
  [[nodiscard]] Var senImplGetPropertyImpl(MemberHash propertyId) const override;
  void senImplWriteAllPropertiesToStream(OutputStream& out) const override;
  void senImplWriteStaticPropertiesToStream(OutputStream& out) const override;
  void senImplWriteDynamicPropertiesToStream(OutputStream& out) const override;
  void senImplRemoveTypedConnection(ConnId id) override;

private:
  std::shared_ptr<Object> guard_;
  ReplayedObject* owner_;
  std::unordered_map<MemberHash, TimeStamp> timeCache_;
};

}  // namespace sen::components::replayer

#endif  // SEN_COMPONENTS_REPLAYER_SRC_REPLAYED_OBJECT_PROXY_H
