// === recorder.h ======================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_COMPONENTS_RECORDER_SRC_RECORDER_H
#define SEN_COMPONENTS_RECORDER_SRC_RECORDER_H

// generated code
#include "stl/sen/components/recorder/recorder.stl.h"

// sen
#include "sen/core/obj/object_list.h"
#include "sen/core/obj/object_mux.h"
#include "sen/core/obj/object_source.h"
#include "sen/db/output.h"

// component
#include "util.h"

namespace recorder
{

class RecorderImpl: public sen::components::recorder::RecorderBase
{
  SEN_NOCOPY_NOMOVE(RecorderImpl)

public:
  RecorderImpl(const std::string& name, const sen::VarMap& args);
  RecorderImpl(const sen::components::recorder::RecordingSettings& settings, sen::kernel::RunApi& api);
  ~RecorderImpl() override = default;

protected:
  void startImpl() override;
  void stopImpl() override;
  [[nodiscard]] sen::db::OutStats fetchStatsImpl() override;
  void registered(sen::kernel::RegistrationApi& api) override;
  void update(sen::kernel::RunApi& runApi) override;

private:
  void objectAdded(Object* object);
  void subscribeToEvents(Object* object);
  void subscribeToPropertyChanges(Object* object);
  void objectRemoved(Object* object);
  void sendKeyframe(const sen::TimeStamp& time);
  void sendCreation(const sen::TimeStamp& time, const Object* object) const;
  void sendDeletion(const sen::TimeStamp& time, const Object* object) const;
  void eventProduced(const Object* instance,
                     const sen::Event* event,
                     const sen::EventInfo& info,
                     const sen::VarList& args) const;
  void propChanged(const Object* instance, const sen::Property* prop, const sen::EventInfo& info) const;
  sen::db::ObjectInfo getObjectInfo(const Object* instance) const;

private:
  std::shared_ptr<sen::db::Output> out_;
  std::unique_ptr<sen::ObjectMux> mux_;
  std::unique_ptr<sen::ObjectList<Object>> trackedObjects_;
  std::unordered_map<sen::ObjectId, std::vector<sen::ConnectionGuard>> guards_ = {};
  std::vector<std::shared_ptr<sen::ObjectSource>> sources_ = {};
  bool additionsAndRemovalsEnabled_ = false;
  sen::TimeStamp lastKeyframeTime_;
  sen::kernel::RunApi* api_ = nullptr;
};

}  // namespace recorder

#endif  // SEN_COMPONENTS_RECORDER_SRC_RECORDER_H
