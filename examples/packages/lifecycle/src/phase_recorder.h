#pragma once

#include "stl/lifecycle/phase_recorder.stl.h"

// sen
#include "sen/kernel/component_api.h"

namespace lifecycle
{

/// Shows the hooks the kernel calls on an object, and the order it calls them in.
///
/// `preDrain()` and `preCommit()` are only called when
/// `needsPreDrainOrPreCommit()` returns true. It returns false by default, so
/// overriding the hooks alone does nothing at all - the gate is the part that is
/// easy to miss.
class PhaseRecorderImpl: public PhaseRecorderBase
{
public:
  SEN_NOCOPY_NOMOVE(PhaseRecorderImpl)

  using PhaseRecorderBase::PhaseRecorderBase;
  ~PhaseRecorderImpl() override = default;

public:
  // --8<-- [start:hooks]
  /// Opt in to preDrain() and preCommit(). Without this they never run.
  [[nodiscard]] bool needsPreDrainOrPreCommit() const noexcept override { return true; }

  void registered(sen::kernel::RegistrationApi& api) override;
  void unregistered(sen::kernel::RegistrationApi& api) override;

  /// Runs before the component drains its inputs, so the object sees the world
  /// as it was at the end of the previous cycle.
  void preDrain() override;

  void update(sen::kernel::RunApi& runApi) override;

  /// Runs immediately before the commit, which makes it the last point at which
  /// a property staged with setNext...() still lands in this cycle.
  void preCommit() override;
  // --8<-- [end:hooks]

protected:
  std::string reportImpl() const override;

private:
  /// Plain members, not properties: these count phases *within* a cycle, and a
  /// property only becomes visible to others once the cycle commits.
  int32_t preDrains_ = 0;
  int32_t updates_ = 0;
  int32_t preCommits_ = 0;
};

}  // namespace lifecycle
