// === replayer.h ======================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_COMPONENTS_REPLAYER_SRC_REPLAYER_H
#define SEN_COMPONENTS_REPLAYER_SRC_REPLAYER_H

// generated code
#include "stl/sen/components/replayer/replayer.stl.h"

// component
#include "replay.h"

// sen
#include "sen/kernel/component_api.h"

namespace sen::components::replayer
{

class ReplayerImpl: public ReplayerBase
{
  SEN_NOCOPY_NOMOVE(ReplayerImpl)

public:
  ReplayerImpl(std::string name, bool autoPlay, std::shared_ptr<ObjectSource> controlBus, kernel::RunApi& api);
  ~ReplayerImpl() override;

protected:
  void openImpl(const std::string& name, const std::string& path) override;
  void closeImpl(const std::string& name) override;
  void closeAllImpl() override;

private:
  using ReplayList = std::vector<std::shared_ptr<ReplayImpl>>;

private:
  void doCloseAll();
  [[nodiscard]] ReplayList::iterator findByName(const std::string& name);
  [[nodiscard]] ReplayList::iterator findByPath(const std::string& path);

private:
  std::shared_ptr<ObjectSource> controlBus_;
  kernel::RunApi& api_;
  ReplayList replays_;
};

}  // namespace sen::components::replayer

#endif  // SEN_COMPONENTS_REPLAYER_SRC_REPLAYER_H
