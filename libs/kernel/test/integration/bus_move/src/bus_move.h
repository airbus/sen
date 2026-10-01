// === bus_move.h ======================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_LIBS_KERNEL_TEST_INTEGRATION_BUS_MOVE_SRC_BUS_MOVE_H
#define SEN_LIBS_KERNEL_TEST_INTEGRATION_BUS_MOVE_SRC_BUS_MOVE_H

// sen
#include "sen/core/base/compiler_macros.h"
#include "sen/core/meta/var.h"
#include "sen/core/obj/connection_guard.h"
#include "sen/core/obj/interest.h"
#include "sen/core/obj/object.h"
#include "sen/core/obj/object_mux.h"
#include "sen/core/obj/object_provider.h"
#include "sen/core/obj/object_source.h"
#include "sen/core/obj/subscription.h"
#include "sen/kernel/component_api.h"

// generated code
#include "stl/bus_move.stl.h"
#include "stl/sen/kernel/kernel_objects.stl.h"

// spdlog
#include <spdlog/logger.h>

// std
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace sen::test::bus_move
{

/// The object that changes bus.
class TravellerImpl final: public TravellerBase
{
public:
  SEN_NOCOPY_NOMOVE(TravellerImpl)
  using TravellerBase::TravellerBase;
  ~TravellerImpl() override = default;
};

/// Publishes the traveller on the first bus, then moves it to the second and back. Each move is a
/// removal and an addition in one cycle, which is what the report describes; the two moves are a
/// few cycles apart so the watcher sees them separately.
class MoverImpl final: public MoverBase
{
public:
  SEN_NOCOPY_NOMOVE(MoverImpl)
  MoverImpl(std::string name, const sen::VarMap& args);
  ~MoverImpl() override = default;

public:
  void registered(sen::kernel::RegistrationApi& api) override;
  void update(sen::kernel::RunApi& runApi) override;

protected:
  void shutdownKernelImpl() override;

private:
  std::shared_ptr<sen::ObjectSource> first_;
  std::shared_ptr<sen::ObjectSource> second_;
  std::shared_ptr<TravellerImpl> traveller_;
  std::shared_ptr<sen::Subscription<WatcherInterface>> watchersSub_;
  std::shared_ptr<sen::Subscription<sen::kernel::KernelApiInterface>> kernelApiSub_;
  std::unordered_map<sen::ObjectId, sen::ConnectionGuard> guards_;
  sen::kernel::KernelApiInterface* kernelApi_ = nullptr;
  std::shared_ptr<spdlog::logger> logger_;
  std::uint32_t heardByWatcher_ = 0U;
  bool movedOut_ = false;
  bool movedBack_ = false;
};

/// Records what one muxed view of both buses is told, in arrival order.
class PresenceRecorder final: public sen::MuxedProviderListener
{
public:
  struct Entry
  {
    std::string kind;
    sen::InterestId interest;
  };

  std::vector<Entry> entries;          // NOLINT(misc-non-private-member-variables-in-classes)
  sen::InterestId firstInterest {0};   // NOLINT(misc-non-private-member-variables-in-classes)
  sen::InterestId secondInterest {0};  // NOLINT(misc-non-private-member-variables-in-classes)

  void onObjectsAdded(const sen::ObjectAdditionList& additions) override;
  void onObjectsRemoved(const sen::ObjectRemovalList& removals) override;
  void onExistingObjectsReadded(const sen::ObjectAdditionList& additions) override;
  void onObjectsRefCountReduced(const sen::ObjectRemovalList& removals) override;

  [[nodiscard]] std::size_t count(const std::string& kind) const;
  [[nodiscard]] std::string describe() const;

private:
  void record(const std::string& kind, const sen::ObjectAdditionList& additions);
  void record(const std::string& kind, const sen::ObjectRemovalList& removals);
};

/// Subscribes to both buses through one mux and says what it heard. It reports as soon as it has
/// everything a correct run produces, and on a deadline otherwise, so a run that hears too little
/// still says what it heard rather than going quiet.
class WatcherImpl final: public WatcherBase
{
public:
  SEN_NOCOPY_NOMOVE(WatcherImpl)
  WatcherImpl(std::string name, const sen::VarMap& args);
  ~WatcherImpl() override;

public:
  void registered(sen::kernel::RegistrationApi& api) override;
  void update(sen::kernel::RunApi& runApi) override;

protected:
  void shutdownKernelImpl() override;

private:
  void report(bool passed);

private:
  std::unique_ptr<sen::ObjectMux> mux_;
  PresenceRecorder recorder_;
  std::shared_ptr<sen::ObjectSource> first_;
  std::shared_ptr<sen::ObjectSource> second_;
  std::shared_ptr<sen::Subscription<sen::kernel::KernelApiInterface>> kernelApiSub_;
  sen::kernel::KernelApiInterface* kernelApi_ = nullptr;
  std::shared_ptr<spdlog::logger> logger_;
  std::size_t ticks_ = 0U;
  bool reported_ = false;
};

}  // namespace sen::test::bus_move

#endif  // SEN_LIBS_KERNEL_TEST_INTEGRATION_BUS_MOVE_SRC_BUS_MOVE_H
