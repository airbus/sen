// === object_reentrancy_test.cpp ======================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

// sen
#include "sen/core/base/class_helpers.h"
#include "sen/core/obj/native_object.h"
#include "sen/core/obj/object_source.h"
#include "sen/kernel/component_api.h"
#include "sen/kernel/kernel_config.h"
#include "sen/kernel/test_kernel.h"

// generated
#include "test_kernel/stl/my_class.stl.h"

// gtest
#include <gtest/gtest.h>

// std
#include <memory>
#include <string>
#include <utility>

namespace
{

/// Counts its own preDrain calls into storage the test owns, so a call that should not have
/// happened is visible without a sanitizer: the test keeps the object alive on purpose, which
/// turns the defect from a use after free into a count that is too high.
class ReentrantObject: public test::MyClassBase
{
public:
  SEN_NOCOPY_NOMOVE(ReentrantObject)

  using MyClassBase::MyClassBase;

  ~ReentrantObject() override = default;

  void removeSelfWhenRegistered(std::shared_ptr<sen::ObjectSource> source, bool addAgain)
  {
    source_ = std::move(source);
    addAgain_ = addAgain;
  }

  void setPreDrainCount(std::shared_ptr<int> count) { preDrainCount_ = std::move(count); }

  [[nodiscard]] bool needsPreDrainOrPreCommit() const noexcept override { return true; }

  void preDrain() override
  {
    if (preDrainCount_)
    {
      ++*preDrainCount_;
    }
  }

  void registered(sen::kernel::RegistrationApi& api) override
  {
    test::MyClassBase::registered(api);

    if (source_ == nullptr || acted_)
    {
      return;
    }
    acted_ = true;

    auto self = std::static_pointer_cast<sen::NativeObject>(shared_from_this());
    source_->remove(self);
    if (addAgain_)
    {
      std::ignore = source_->add(self);
    }
  }

private:
  std::shared_ptr<sen::ObjectSource> source_;
  std::shared_ptr<int> preDrainCount_;
  bool addAgain_ {false};
  bool acted_ {false};
};

std::shared_ptr<int> runWithSelfRemovingObject(bool addAgain, std::size_t cycles)
{
  auto count = std::make_shared<int>(0);
  auto object = std::make_shared<ReentrantObject>("reentrant", sen::VarMap {});
  object->setPreDrainCount(count);

  sen::kernel::TestComponent component;
  component.onRun(
    [&object, addAgain](sen::kernel::RunApi& api)
    {
      auto source = api.getSource("local.test");
      object->removeSelfWhenRegistered(source, addAgain);
      std::ignore = source->add(object);
      return api.execLoop(sen::Duration::fromHertz(100.0));
    });

  sen::kernel::TestKernel kernel(&component);
  kernel.step(cycles);
  return count;
}

}  // namespace

/// @test
/// An object removed from inside its own registered() is not left in the runner's pre-drain list,
/// so preDrain is never called for it.
/// @requirements(SEN-1733)
TEST(ObjectReentrancy, RemoveFromRegisteredLeavesNoPreDrainEntry)
{
  const auto count = runWithSelfRemovingObject(false, 5U);
  EXPECT_EQ(*count, 0) << "preDrain ran for an object removed before any drain";
}

/// @test
/// An object that removes and then re-adds itself from inside registered() is registered once, so
/// preDrain is called once per cycle rather than twice.
/// @requirements(SEN-1733)
TEST(ObjectReentrancy, RemoveAndAddAgainFromRegisteredRegistersOnce)
{
  constexpr std::size_t cycles = 5U;
  const auto count = runWithSelfRemovingObject(true, cycles);
  EXPECT_LE(*count, static_cast<int>(cycles)) << "preDrain ran more than once per cycle, so the "
                                                 "object holds more than one pre-drain entry";
}
