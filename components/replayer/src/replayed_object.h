// === replayed_object.h ===============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_COMPONENTS_REPLAYER_SRC_REPLAYED_OBJECT_H
#define SEN_COMPONENTS_REPLAYER_SRC_REPLAYED_OBJECT_H

// sen
#include "sen/core/base/timestamp.h"
#include "sen/core/meta/class_type.h"
#include "sen/core/meta/property.h"
#include "sen/core/meta/type.h"
#include "sen/core/obj/detail/gen.h"
#include "sen/core/obj/native_object.h"
#include "sen/db/creation.h"
#include "sen/db/event.h"
#include "sen/db/property_change.h"
#include "sen/db/snapshot.h"

// std
#include <memory>

namespace sen::components::replayer
{

class ReplayedObject: public NativeObject
{
public:
  SEN_NOCOPY_NOMOVE(ReplayedObject)
  SEN_IMPL_GEN_NATIVE_MEMBERS

public:
  ReplayedObject(const db::Snapshot& snapshot, TimeStamp timeStamp);
  ~ReplayedObject() override;

public:  // implements Object
  [[nodiscard]] TimeStamp getLastCommitTime() const noexcept override;
  [[nodiscard]] ConstTypeHandle<ClassType> getClass() const noexcept override;

public:
  void flushPendingChanges(TimeStamp entryTime);
  void inject(TimeStamp entryTime, const db::PropertyChange& propertyChange);
  void inject(TimeStamp entryTime, const db::Event& event);
  void inject(TimeStamp entryTime, const db::Snapshot& snapshot);
  TimeStamp getPropertyLastTime(const Property* property) const;
  const PropertyList& getAllProps() const noexcept;

private:
  inline void writePropertyToStream(impl::BufferProvider& provider,
                                    const Property* property,
                                    const std::vector<uint8_t>& valueBuffer) const;
  inline void writePropertyToStream(OutputStream& out,
                                    const Property* property,
                                    const std::vector<uint8_t>& valueBuffer) const;

private:
  struct ChangedPropertyData
  {
    Var var;
    std::vector<uint8_t> buffer;
    TimeStamp time;
  };

private:
  ObjectId originalId_;
  ConstTypeHandle<ClassType> meta_;
  TimeStamp lastCommitTime_;
  PropertyList allProps_;
  std::unordered_map<MemberHash, Var> varCache_;
  std::unordered_map<MemberHash, std::vector<uint8_t>> bufferCache_;
  std::unordered_map<MemberHash, TimeStamp> timeCache_;
  std::unordered_map<const Property*, std::unique_ptr<ChangedPropertyData>> changedProperties_;
  std::vector<std::function<void()>> pendingChanges_;
};

}  // namespace sen::components::replayer

#endif  // SEN_COMPONENTS_REPLAYER_SRC_REPLAYED_OBJECT_H
