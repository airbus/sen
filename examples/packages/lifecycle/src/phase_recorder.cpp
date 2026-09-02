#include "phase_recorder.h"

namespace lifecycle
{

// Running this example prints, cycle after cycle:
//
//   cycles=1  lastCycle="preDrain=0 update=0 preCommit=1"
//   cycles=2  lastCycle="preDrain=1 update=1 preCommit=2"
//
// Two things are worth reading off that. The counts confirm the per-cycle order
// is drain, update, commit. And the very first commit runs before any drain or
// update, because the kernel commits the initial state of the objects before the
// first cycle begins - so preCommit() is called once more than the others.

void PhaseRecorderImpl::registered(sen::kernel::RegistrationApi& /*api*/)
{
  // Called once, after construction and before any cycle runs on this object.
  // A good place to acquire whatever the object needs to do its work.
  preDrains_ = 0;
  updates_ = 0;
  preCommits_ = 0;
}

void PhaseRecorderImpl::unregistered(sen::kernel::RegistrationApi& /*api*/)
{
  // Called once, after the object leaves the execution context. Nothing staged
  // here reaches anyone: there is no further commit.
}

void PhaseRecorderImpl::preDrain() { ++preDrains_; }

void PhaseRecorderImpl::update(sen::kernel::RunApi& /*runApi*/)
{
  ++updates_;

  // getCycles() reads the value committed at the end of the *previous* cycle,
  // not the one preCommit() is about to stage. That one-cycle delay is the whole
  // point of staging: everyone sees the same world for the length of a cycle.
}

// --8<-- [start:precommit]
void PhaseRecorderImpl::preCommit()
{
  ++preCommits_;

  // Staging here still reaches the commit that follows, which is what makes
  // preCommit() useful: the object can publish something derived from
  // everything that happened during the cycle.
  setNextCycles(getCycles() + 1);
  setNextLastCycle(reportImpl());
}
// --8<-- [end:precommit]

std::string PhaseRecorderImpl::reportImpl() const
{
  return "preDrain=" + std::to_string(preDrains_) + " update=" + std::to_string(updates_) +
         " preCommit=" + std::to_string(preCommits_);
}

SEN_EXPORT_CLASS(PhaseRecorderImpl)

}  // namespace lifecycle
