// === recorder.h ======================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_COMPONENTS_INFLUX_SRC_RECORDER_H
#define SEN_COMPONENTS_INFLUX_SRC_RECORDER_H

// component
#include "database.h"

// sen
#include "sen/core/obj/object_list.h"
#include "sen/core/obj/object_mux.h"
#include "sen/core/obj/object_source.h"
#include "sen/kernel/component_api.h"

namespace sen::components::influx
{

class Recorder
{
  SEN_NOCOPY_NOMOVE(Recorder)

public:
  Recorder(std::unique_ptr<Database> database, const std::vector<std::string>& selections, kernel::RunApi& runApi);

  ~Recorder();

private:
  inline void objectAdded(Object* object);
  inline void subscribeToEvents(Object* object);
  inline void subscribeToPropertyChanges(Object* object);
  inline void objectRemoved(Object* object);
  inline void sendCreation(const TimeStamp& time, const Object* object);
  inline void sendDeletion(const TimeStamp& time, const Object* object) const;
  inline void eventProduced(const Object* instance, const Event* event, const EventInfo& info, const VarList& args);
  inline void propChanged(const Object* instance, const Property* prop, const EventInfo& info);

private:
  kernel::RunApi& runApi_;
  std::unique_ptr<Database> database_;
  std::unique_ptr<ObjectMux> mux_;
  std::unique_ptr<ObjectList<Object>> trackedObjects_;
  std::unordered_map<ObjectId, std::vector<ConnectionGuard>> guards_;
  std::vector<std::shared_ptr<ObjectSource>> sources_;
};

}  // namespace sen::components::influx

#endif  // SEN_COMPONENTS_INFLUX_SRC_RECORDER_H
