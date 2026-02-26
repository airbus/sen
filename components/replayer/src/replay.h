// === replay.h ========================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_COMPONENTS_REPLAYER_SRC_REPLAY_H
#define SEN_COMPONENTS_REPLAYER_SRC_REPLAY_H

// component
#include "replayed_object.h"

// generated code
#include "stl/sen/components/replayer/replayer.stl.h"

// sen
#include "sen/db/input.h"
#include "sen/kernel/component_api.h"

namespace sen::components::replayer
{

class ReplayImpl: public ReplayBase
{
  SEN_NOCOPY_NOMOVE(ReplayImpl)

public:
  ReplayImpl(std::string name, std::string path, std::unique_ptr<db::Input> input, kernel::RunApi& api);
  ~ReplayImpl() override;

public:
  void update(kernel::RunApi& runApi) override;

protected:
  void playImpl() override;
  void pauseImpl() override;
  void stopImpl() override;
  void seekImpl(TimeStamp time) override;
  void advanceImpl(Duration time) override;

private:
  struct ObjectData
  {
    std::shared_ptr<ReplayedObject> instance;
    std::shared_ptr<ObjectSource> bus;
  };

  using ObjectMap = std::unordered_map<ObjectId, ObjectData>;

private:
  void advanceCursor(Duration delta);
  void doStop();
  void applyCursor();
  void apply(TimeStamp time, std::monostate value) const;
  void apply(TimeStamp time, const db::End& value);
  void apply(TimeStamp time, const db::PropertyChange& value);
  void apply(TimeStamp time, const db::Event& value);
  void apply(TimeStamp time, const db::Keyframe& value);
  void apply(TimeStamp time, const db::Creation& value);
  void apply(TimeStamp time, const db::Deletion& value);
  void expectedObject(TimeStamp time, ObjectId id, const std::vector<std::string_view>& reasons) const;
  void addObject(TimeStamp time, const db::Snapshot& snapshot, ObjectMap& map);
  void removeObject(ObjectMap::iterator itr, ObjectMap& map) const;
  void flushObjectsActivity() const;

private:
  std::unique_ptr<db::Input> input_;
  kernel::RunApi& api_;
  ObjectMap objects_;
  db::DataCursor cursor_;
  TimeStamp lastUpdateTime_;
};

}  // namespace sen::components::replayer

#endif  // SEN_COMPONENTS_REPLAYER_SRC_REPLAY_H
