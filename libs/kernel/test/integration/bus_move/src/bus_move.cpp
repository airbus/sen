// === bus_move.cpp ====================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "bus_move.h"

// sen
#include "sen/core/meta/class_type.h"
#include "sen/kernel/component_api.h"

// generated code
#include "stl/sen/kernel/kernel_objects.stl.h"

// std
#include <algorithm>
#include <cstdint>
#include <tuple>
#include <utility>

namespace sen::test::bus_move
{

namespace
{

/// What the watcher must have heard before each move: the first publication before the move out,
/// and that plus the first move's departure and arrival before the move back. Waiting on the
/// watcher's own count rather than on a number of cycles is what keeps the two moves in separate
/// drains over there; a stall long enough to put both in one drain would make the totals come out
/// wrong with nothing actually broken.
constexpr std::uint32_t heardBeforeMovingOut = 1U;
constexpr std::uint32_t heardBeforeMovingBack = 3U;

/// What a correct run tells the watcher: the first publication, then one departure and one
/// arrival for each of the two moves, and nothing else. Exact rather than a lower bound, so a
/// doubled announcement is a failure that prints its sequence instead of passing unnoticed.
constexpr std::size_t expectedAdded = 3U;
constexpr std::size_t expectedRemoved = 2U;
constexpr std::size_t expectedEvents = expectedAdded + expectedRemoved;

/// How long the watcher waits before saying what it has, when it never reaches the above. Its own
/// cycles rather than the clock, so a loaded box slows the deadline with everything else.
constexpr std::size_t verdictDeadlineCycles = 200U;

}  // namespace

//----------------------------------------------------------------------------------------------------------------------
// MoverImpl
//----------------------------------------------------------------------------------------------------------------------

MoverImpl::MoverImpl(std::string name, const sen::VarMap& args)
  : MoverBase(name, args), logger_(sen::kernel::KernelApi::getOrCreateLogger(name))
{
}

void MoverImpl::registered(sen::kernel::RegistrationApi& api)
{
  first_ = api.getSource("session.first");
  second_ = api.getSource("session.second");

  traveller_ = std::make_shared<TravellerImpl>("traveller", sen::VarMap {});
  if (!first_->add(traveller_))
  {
    logger_->error("BUS MOVE FAIL: the traveller could not be published on the first bus");
  }

  kernelApiSub_ = api.selectAllFrom<sen::kernel::KernelApiInterface>(
    "local.kernel", [this](const auto& added) { kernelApi_ = *added.begin(); });

  // The move waits for the watcher to say it is subscribed to both buses. Starting on a timer
  // instead would let a slow discovery turn a real failure into a missed first publication.
  watchersSub_ = api.selectAllFrom<WatcherInterface>(
    "session.ctrl",
    [this](const auto& watchers)
    {
      for (auto* watcher: watchers)
      {
        heardByWatcher_ = std::max(heardByWatcher_, watcher->getHeard());

        guards_.emplace(watcher->asObject().getId(),
                        watcher->onHeardChanged({this,
                                                 [this, watcher]()
                                                 {
                                                   heardByWatcher_ = std::max(heardByWatcher_, watcher->getHeard());
                                                   logger_->info("mover: the watcher has heard {}", heardByWatcher_);
                                                 }}));
      }
    },
    [this](const auto& removed)
    {
      for (auto* watcher: removed)
      {
        guards_.erase(watcher->asObject().getId());
      }
    });
}

void MoverImpl::update(sen::kernel::RunApi& runApi)
{
  std::ignore = runApi;

  if (movedBack_)
  {
    return;
  }

  // Each move is a removal and an addition in the same cycle: that is the case the report is
  // about, and the pair has to stay together for the test to mean anything.
  if (!movedOut_ && heardByWatcher_ >= heardBeforeMovingOut)
  {
    logger_->info("mover: moving the traveller from the first bus to the second");
    first_->remove(traveller_);
    if (!second_->add(traveller_))
    {
      logger_->error("BUS MOVE FAIL: the traveller could not be published on the second bus");
    }
    movedOut_ = true;
    return;
  }

  if (movedOut_ && heardByWatcher_ >= heardBeforeMovingBack)
  {
    logger_->info("mover: moving the traveller back to the first bus");
    second_->remove(traveller_);
    if (!first_->add(traveller_))
    {
      logger_->error("BUS MOVE FAIL: the traveller could not be published on the first bus again");
    }
    movedBack_ = true;
  }
}

void MoverImpl::shutdownKernelImpl()
{
  if (kernelApi_ != nullptr)
  {
    kernelApi_->shutdown();
  }
}

SEN_EXPORT_CLASS(MoverImpl)
SEN_EXPORT_CLASS(TravellerImpl)

//----------------------------------------------------------------------------------------------------------------------
// PresenceRecorder
//----------------------------------------------------------------------------------------------------------------------

void PresenceRecorder::onObjectsAdded(const sen::ObjectAdditionList& additions) { record("added", additions); }

void PresenceRecorder::onObjectsRemoved(const sen::ObjectRemovalList& removals) { record("removed", removals); }

void PresenceRecorder::onExistingObjectsReadded(const sen::ObjectAdditionList& additions)
{
  record("readded", additions);
}

void PresenceRecorder::onObjectsRefCountReduced(const sen::ObjectRemovalList& removals)
{
  record("refReduced", removals);
}

std::size_t PresenceRecorder::count(const std::string& kind) const
{
  return static_cast<std::size_t>(
    std::count_if(entries.begin(), entries.end(), [&kind](const Entry& entry) { return entry.kind == kind; }));
}

std::string PresenceRecorder::describe() const
{
  if (entries.empty())
  {
    return "<nothing>";
  }

  std::string out;
  for (const auto& entry: entries)
  {
    if (!out.empty())
    {
      out.append(" ");
    }
    out.append(entry.kind).append("(");
    if (entry.interest == firstInterest)
    {
      out.append("first");
    }
    else if (entry.interest == secondInterest)
    {
      out.append("second");
    }
    else
    {
      out.append("other");
    }
    out.append(")");
  }
  return out;
}

void PresenceRecorder::record(const std::string& kind, const sen::ObjectAdditionList& additions)
{
  for (const auto& addition: additions)
  {
    entries.push_back({kind, sen::getInterestId(addition)});
  }
}

void PresenceRecorder::record(const std::string& kind, const sen::ObjectRemovalList& removals)
{
  for (const auto& removal: removals)
  {
    entries.push_back({kind, removal.interestId});
  }
}

//----------------------------------------------------------------------------------------------------------------------
// WatcherImpl
//----------------------------------------------------------------------------------------------------------------------

WatcherImpl::WatcherImpl(std::string name, const sen::VarMap& args)
  : WatcherBase(name, args)
  , mux_(std::make_unique<sen::ObjectMux>())
  , logger_(sen::kernel::KernelApi::getOrCreateLogger(name))
{
}

WatcherImpl::~WatcherImpl()
{
  // The same release a Subscription does for itself. A listener still attached when the kernel
  // tears down asserts in a Debug build.
  if (first_)
  {
    first_->removeSubscriber(mux_.get(), false);
  }
  if (second_)
  {
    second_->removeSubscriber(mux_.get(), false);
  }
  if (mux_ && mux_->hasMuxedListener(&recorder_))
  {
    mux_->removeMuxedListener(&recorder_, false);
  }
}

void WatcherImpl::registered(sen::kernel::RegistrationApi& api)
{
  kernelApiSub_ = api.selectAllFrom<sen::kernel::KernelApiInterface>(
    "local.kernel", [this](const auto& added) { kernelApi_ = *added.begin(); });

  mux_->addMuxedListener(&recorder_, false);

  first_ = api.getSource("session.first");
  second_ = api.getSource("session.second");

  auto firstInterest = sen::Interest::make("SELECT * FROM session.first", api.getTypes());
  auto secondInterest = sen::Interest::make("SELECT * FROM session.second", api.getTypes());
  recorder_.firstInterest = firstInterest->getId();
  recorder_.secondInterest = secondInterest->getId();

  // Seeded with what is already there. The mover publishes the traveller while it registers, which
  // may be before this process exists at all, and without this the watcher would never hear about
  // an object that was published before it subscribed.
  first_->addSubscriber(firstInterest, mux_.get(), true);
  second_->addSubscriber(secondInterest, mux_.get(), true);
}

void WatcherImpl::update(sen::kernel::RunApi& runApi)
{
  std::ignore = runApi;

  if (reported_)
  {
    return;
  }

  ++ticks_;

  // What the mover waits on. Publishing the count rather than a flag lets it hold each move until
  // this side has actually been told about the one before it.
  const auto heard = static_cast<std::uint32_t>(recorder_.entries.size());
  if (heard != getHeard())
  {
    setNextHeard(heard);
  }

  // Triggered on the count of everything heard, then judged on the breakdown: waiting for the
  // arrivals alone would stop counting before an extra event could show up.
  if (recorder_.entries.size() >= expectedEvents)
  {
    report(recorder_.count("added") == expectedAdded && recorder_.count("removed") == expectedRemoved &&
           recorder_.count("readded") == 0U && recorder_.count("refReduced") == 0U);
    return;
  }

  if (ticks_ >= verdictDeadlineCycles)
  {
    report(false);
  }
}

void WatcherImpl::report(bool passed)
{
  reported_ = true;

  if (passed)
  {
    logger_->info("BUS MOVE OK: {}", recorder_.describe());
    return;
  }

  logger_->error(
    "BUS MOVE FAIL: wanted {} added and {} removed, heard {} added {} removed {} readded {} refReduced: {}",
    expectedAdded,
    expectedRemoved,
    recorder_.count("added"),
    recorder_.count("removed"),
    recorder_.count("readded"),
    recorder_.count("refReduced"),
    recorder_.describe());
}

void WatcherImpl::shutdownKernelImpl()
{
  if (kernelApi_ != nullptr)
  {
    kernelApi_->shutdown();
  }
}

SEN_EXPORT_CLASS(WatcherImpl)

}  // namespace sen::test::bus_move
