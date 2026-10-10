// === object_reentrancy_test.cpp ======================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

// sen
#include "sen/core/base/compiler_macros.h"
#include "sen/core/base/duration.h"
#include "sen/core/base/numbers.h"
#include "sen/core/obj/native_object.h"
#include "sen/core/obj/object_source.h"
#include "sen/core/obj/subscription.h"
#include "sen/kernel/component_api.h"
#include "sen/kernel/test_kernel.h"

// generated
#include "test_kernel/stl/my_class.stl.h"

// gtest
#include <gtest/gtest.h>

// The walk tests cover use-after-free on a std::list node. Their assertions catch a walk that stops
// early; a walk that reads the freed node is caught by the address sanitizer, so the sanitizer lane
// is where those are evidence rather than luck.
//
// Two hazards the sanitizer does not cover: ActiveCursor and the in-flight cancel batch hold
// pointers to stack locals, so a guard failing to pop is a stack-use-after-return, which needs
// detect_stack_use_after_return -- not set in this repository. Scoping holds those two.

// std
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <memory>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

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

  /// Reads the component's object count from inside registered(), after the self-removal. That is
  /// where an increment placed after the hook shows: by the time add() has returned, a decrement
  /// ahead of its increment has wrapped and come back, and the count reads correct either way.
  void reportCountFromRegistered(sen::kernel::RunApi* api, std::shared_ptr<std::size_t> count)
  {
    countApi_ = api;
    countInsideHook_ = std::move(count);
  }

  /// Breaks the cycle between this object and the source it holds, as the other fixtures do, and
  /// drops the raw RunApi pointer: it outlives the kernel otherwise.
  void releaseSource()
  {
    source_ = nullptr;
    countApi_ = nullptr;
  }

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
      readCount();
      return;
    }
    acted_ = true;

    auto self = std::static_pointer_cast<sen::NativeObject>(shared_from_this());
    source_->remove(self);
    if (addAgain_)
    {
      std::ignore = source_->add(self);
    }

    readCount();
  }

private:
  void readCount()
  {
    if (countApi_ != nullptr && countInsideHook_)
    {
      *countInsideHook_ = countApi_->fetchComponentMonitoringInfo().objectCount;
    }
  }

  std::shared_ptr<sen::ObjectSource> source_;
  std::shared_ptr<int> preDrainCount_;
  std::shared_ptr<std::size_t> countInsideHook_;
  sen::kernel::RunApi* countApi_ {nullptr};
  bool addAgain_ {false};
  bool acted_ {false};
};

std::shared_ptr<int> runWithSelfRemovingObject(bool removeSelf, bool addAgain, std::size_t cycles)
{
  auto count = std::make_shared<int>(0);
  auto object = std::make_shared<ReentrantObject>("reentrant", sen::VarMap {});
  object->setPreDrainCount(count);

  sen::kernel::TestComponent component;
  component.onRun(
    [&object, removeSelf, addAgain](sen::kernel::RunApi& api)
    {
      auto source = api.getSource("local.test");
      if (removeSelf)
      {
        object->removeSelfWhenRegistered(source, addAgain);
      }
      std::ignore = source->add(object);
      return api.execLoop(sen::Duration::fromHertz(100.0));
    });

  sen::kernel::TestKernel kernel(&component);
  kernel.step(cycles);
  object->releaseSource();
  return count;
}

struct ChurnResult
{
  std::vector<std::string> names;
  bool churned {false};
  bool added {false};
};

[[nodiscard]] std::ptrdiff_t occurrences(const ChurnResult& result, std::string_view name)
{
  return std::count(result.names.begin(), result.names.end(), name);
}

/// Publishes one object so the subscription's added callback fires, and from inside that callback
/// optionally adds a second object, optionally removing it again. Reports what it observed as well
/// as what it did, so one run cannot agree with another for want of having run.
ChurnResult countAnnouncedAdditions(bool churnInsideCallback, bool removeAgain, std::size_t cycles)
{
  auto announced = std::make_shared<std::vector<std::string>>();
  auto churned = std::make_shared<bool>(false);
  auto added = std::make_shared<bool>(false);
  auto started = std::make_shared<bool>(false);
  std::shared_ptr<sen::Subscription<test::MyClassInterface>> subscription;

  sen::kernel::TestComponent component;
  component.onRun(
    [&subscription, announced, churned, added, started, churnInsideCallback, removeAgain](sen::kernel::RunApi& api)
    {
      if (!*started)
      {
        *started = true;

        auto source = api.getSource("local.test");
        subscription = api.selectAllFrom<test::MyClassInterface>(
          "local.test",
          [announced, churned, added, source, churnInsideCallback, removeAgain](const auto& addedObjects)
          {
            for (auto* object: addedObjects)
            {
              // The list hands over the interface; the name lives on NativeObject, and the
              // generated class inherits the interface virtually, so this needs a dynamic_cast.
              if (const auto* native = dynamic_cast<const sen::NativeObject*>(object); native != nullptr)
              {
                announced->push_back(native->getName());
              }
            }

            if (!churnInsideCallback || *churned)
            {
              return;
            }
            *churned = true;

            auto transient = std::static_pointer_cast<sen::NativeObject>(
              std::make_shared<test::MyClassBase>("transient", sen::VarMap {}));

            // Not ignored: a refused add would make both arms agree for a reason that has nothing
            // to do with the defect.
            *added = source->add(transient);
            if (removeAgain)
            {
              source->remove(transient);
            }
          },
          nullptr);

        auto trigger =
          std::static_pointer_cast<sen::NativeObject>(std::make_shared<test::MyClassBase>("trigger", sen::VarMap {}));
        std::ignore = source->add(trigger);
      }

      return api.execLoop(sen::Duration::fromHertz(100.0));
    });

  sen::kernel::TestKernel kernel(&component);
  kernel.step(cycles);

  // The subscription holds the last strong reference to the participant, and it is declared before
  // the kernel, so without this it would be released after the kernel it outlives.
  subscription.reset();

  return ChurnResult {*announced, *churned, *added};
}

/// Removes itself from inside its own unregistered() hook, up to a bound. On the pre-change
/// ordering the recursion is bounded by this fixture anyway; what takes the binary down is the
/// repeated erase of an already-erased iterator as those frames unwind. The bound is what turns
/// that crash into a count the test can read.
class SelfRemovingOnUnregister: public test::MyClassBase
{
public:
  SEN_NOCOPY_NOMOVE(SelfRemovingOnUnregister)

  using MyClassBase::MyClassBase;

  ~SelfRemovingOnUnregister() override = default;

  static constexpr int hookCallBound = 4;

  void setUp(std::shared_ptr<sen::ObjectSource> source,
             std::shared_ptr<int> hookCalls,
             std::shared_ptr<int> removalAttempts)
  {
    source_ = std::move(source);
    hookCalls_ = std::move(hookCalls);
    removalAttempts_ = std::move(removalAttempts);
  }

  /// Breaks the cycle between this object and the source it holds.
  void releaseSource() { source_ = nullptr; }

  void unregistered(sen::kernel::RegistrationApi& api) override
  {
    test::MyClassBase::unregistered(api);

    ++*hookCalls_;
    if (source_ == nullptr || *hookCalls_ >= hookCallBound)
    {
      return;
    }

    // Counted on the way in, so a hook that never reached the source is told apart from one whose
    // nested removal was refused. Without it the test reads the same either way.
    ++*removalAttempts_;
    source_->remove(std::static_pointer_cast<sen::NativeObject>(shared_from_this()));
  }

private:
  std::shared_ptr<sen::ObjectSource> source_;
  std::shared_ptr<int> hookCalls_;
  std::shared_ptr<int> removalAttempts_;
};

struct UnregisterHookCounts
{
  std::shared_ptr<int> hookCalls {std::make_shared<int>(0)};
  std::shared_ptr<int> removalAttempts {std::make_shared<int>(0)};
};

UnregisterHookCounts countUnregisteredHookCalls()
{
  UnregisterHookCounts counts;
  auto object = std::make_shared<SelfRemovingOnUnregister>("self-removing", sen::VarMap {});

  sen::kernel::TestComponent component;
  component.onRun(
    [&object, &counts](sen::kernel::RunApi& api)
    {
      auto source = api.getSource("local.test");
      object->setUp(source, counts.hookCalls, counts.removalAttempts);

      auto self = std::static_pointer_cast<sen::NativeObject>(object);
      const bool added = source->add(self);
      EXPECT_TRUE(added) << "the add was refused, so this run measures nothing";
      source->remove(self);

      return api.execLoop(sen::Duration::fromHertz(100.0));
    });

  sen::kernel::TestKernel kernel(&component);
  kernel.step(2U);
  object->releaseSource();
  return counts;
}

struct ChurnCounts
{
  int unregistered {0};
  int removalsIssued {0};
  bool readdAccepted {false};
};

/// Re-adds itself once from inside its own unregistered() hook. A re-add that leaves the object on
/// a list node with no registration behind it cannot answer a later removal, so the hook count
/// after a second removal is what separates the two outcomes.
class ReaddingOnUnregister: public test::MyClassBase
{
public:
  SEN_NOCOPY_NOMOVE(ReaddingOnUnregister)

  using MyClassBase::MyClassBase;

  ~ReaddingOnUnregister() override = default;

  void setUp(std::shared_ptr<sen::ObjectSource> source, std::shared_ptr<ChurnCounts> counts)
  {
    source_ = std::move(source);
    counts_ = std::move(counts);
  }

  void releaseSource() { source_ = nullptr; }

  void unregistered(sen::kernel::RegistrationApi& api) override
  {
    test::MyClassBase::unregistered(api);

    if (counts_)
    {
      ++counts_->unregistered;
    }

    if (source_ == nullptr || readded_)
    {
      return;
    }
    readded_ = true;

    const bool added = source_->add(std::static_pointer_cast<sen::NativeObject>(shared_from_this()));
    if (counts_)
    {
      counts_->readdAccepted = added;
    }
  }

private:
  std::shared_ptr<sen::ObjectSource> source_;
  std::shared_ptr<ChurnCounts> counts_;
  bool readded_ {false};
};

std::shared_ptr<ChurnCounts> runReaddingOnUnregister()
{
  auto counts = std::make_shared<ChurnCounts>();
  auto object = std::make_shared<ReaddingOnUnregister>("re-adding", sen::VarMap {});

  sen::kernel::TestComponent component;
  component.onRun(
    [&object, counts](sen::kernel::RunApi& api)
    {
      auto source = api.getSource("local.test");
      object->setUp(source, counts);

      auto self = std::static_pointer_cast<sen::NativeObject>(object);
      std::ignore = source->add(self);

      // Both removals are issued here rather than across cycles, so the test does not depend on
      // how many times run() is called. remove() reaches unregisterObjects synchronously.
      source->remove(self);
      ++counts->removalsIssued;
      source->remove(self);
      ++counts->removalsIssued;

      return api.execLoop(sen::Duration::fromHertz(100.0));
    });

  sen::kernel::TestKernel kernel(&component);
  kernel.step(2U);
  object->releaseSource();
  return counts;
}

/// Removes itself from inside needsPreDrainOrPreCommit(), which the runner consults between
/// registering the object and adding its pre-drain entry. The query is const noexcept, so the
/// object reaches itself through a weak_ptr and records through shared state.
class SelfRemovingOnPreDrainQuery: public test::MyClassBase
{
public:
  SEN_NOCOPY_NOMOVE(SelfRemovingOnPreDrainQuery)

  using MyClassBase::MyClassBase;

  ~SelfRemovingOnPreDrainQuery() override = default;

  void setUp(std::shared_ptr<sen::ObjectSource> source, std::shared_ptr<int> preDrainCount, bool removeSelf)
  {
    source_ = std::move(source);
    preDrainCount_ = std::move(preDrainCount);
    removeSelf_ = removeSelf;

    // The query below is const, where weak_from_this() would hand back a pointer to const, so the
    // non-const one is taken here instead.
    self_ = std::static_pointer_cast<sen::NativeObject>(shared_from_this());
  }

  void releaseSource() { source_ = nullptr; }

  [[nodiscard]] bool needsPreDrainOrPreCommit() const noexcept override
  {
    if (removeSelf_ && !acted_ && source_ != nullptr)
    {
      acted_ = true;
      if (auto self = self_.lock(); self != nullptr)
      {
        source_->remove(self);
      }
    }
    return true;
  }

  void preDrain() override
  {
    if (preDrainCount_)
    {
      ++*preDrainCount_;
    }
  }

private:
  std::shared_ptr<sen::ObjectSource> source_;
  std::shared_ptr<int> preDrainCount_;
  std::weak_ptr<sen::NativeObject> self_;
  bool removeSelf_ {false};
  mutable bool acted_ {false};
};

std::shared_ptr<int> countPreDrainsWithQueryRemoval(bool removeSelf)
{
  auto preDrainCount = std::make_shared<int>(0);
  auto object = std::make_shared<SelfRemovingOnPreDrainQuery>("query-removing", sen::VarMap {});

  sen::kernel::TestComponent component;
  component.onRun(
    [&object, preDrainCount, removeSelf](sen::kernel::RunApi& api)
    {
      auto source = api.getSource("local.test");
      object->setUp(source, preDrainCount, removeSelf);
      std::ignore = source->add(std::static_pointer_cast<sen::NativeObject>(object));
      return api.execLoop(sen::Duration::fromHertz(100.0));
    });

  sen::kernel::TestKernel kernel(&component);
  kernel.step(3U);
  object->releaseSource();
  return preDrainCount;
}

/// Sets its property from inside registered(), and optionally removes itself afterwards. The
/// property's visible value only moves when the registration commits, so reading it says whether
/// the commit ran.
class PropSettingOnRegister: public test::MyClassBase
{
public:
  SEN_NOCOPY_NOMOVE(PropSettingOnRegister)

  using MyClassBase::MyClassBase;

  ~PropSettingOnRegister() override = default;

  void setUp(std::shared_ptr<sen::ObjectSource> source, bool removeSelf)
  {
    source_ = std::move(source);
    removeSelf_ = removeSelf;
  }

  void releaseSource() { source_ = nullptr; }

  void registered(sen::kernel::RegistrationApi& api) override
  {
    test::MyClassBase::registered(api);

    if (acted_)
    {
      return;
    }
    acted_ = true;

    setNextProp(1);

    if (removeSelf_ && source_ != nullptr)
    {
      source_->remove(std::static_pointer_cast<sen::NativeObject>(shared_from_this()));
    }
  }

private:
  std::shared_ptr<sen::ObjectSource> source_;
  bool removeSelf_ {false};
  bool acted_ {false};
};

struct PropValues
{
  i32 visible {0};
  i32 staged {0};
};

PropValues propAfterRun(bool removeSelf)
{
  auto object = std::make_shared<PropSettingOnRegister>("prop-setting", sen::VarMap {});

  sen::kernel::TestComponent component;
  component.onRun(
    [&object, removeSelf](sen::kernel::RunApi& api)
    {
      auto source = api.getSource("local.test");
      object->setUp(source, removeSelf);
      std::ignore = source->add(std::static_pointer_cast<sen::NativeObject>(object));
      return api.execLoop(sen::Duration::fromHertz(100.0));
    });

  sen::kernel::TestKernel kernel(&component);
  kernel.step(1U);
  object->releaseSource();
  return PropValues {object->getProp(), object->getNextProp()};
}

/// Counts its own update() calls. The runner walks its object list every cycle, so an object the
/// participant refused but the runner registered anyway shows up as a count that should be zero.
class UpdateCountingObject: public test::MyClassBase
{
public:
  SEN_NOCOPY_NOMOVE(UpdateCountingObject)

  using MyClassBase::MyClassBase;

  ~UpdateCountingObject() override = default;

  void setUpdateCount(std::shared_ptr<int> count) { updateCount_ = std::move(count); }

  void update(sen::kernel::RunApi& api) override
  {
    test::MyClassBase::update(api);

    if (updateCount_)
    {
      ++*updateCount_;
    }
  }

private:
  std::shared_ptr<int> updateCount_;
};

struct DuplicateNameCounts
{
  std::shared_ptr<int> accepted {std::make_shared<int>(0)};
  std::shared_ptr<int> refused {std::make_shared<int>(0)};
  std::shared_ptr<bool> addSucceeded {std::make_shared<bool>(true)};
};

/// Adds two objects of the same name in one call. The participant accepts the first and refuses the
/// second, and both counts are read to tell a refusal from a failure to run at all.
DuplicateNameCounts runWithDuplicateName(std::size_t cycles)
{
  DuplicateNameCounts counts;
  auto first = std::make_shared<UpdateCountingObject>("duplicate", sen::VarMap {});
  auto second = std::make_shared<UpdateCountingObject>("duplicate", sen::VarMap {});
  first->setUpdateCount(counts.accepted);
  second->setUpdateCount(counts.refused);

  sen::kernel::TestComponent component;
  component.onRun(
    [&first, &second, &counts](sen::kernel::RunApi& api)
    {
      auto source = api.getSource("local.test");
      const std::vector<std::shared_ptr<sen::NativeObject>> both {std::static_pointer_cast<sen::NativeObject>(first),
                                                                  std::static_pointer_cast<sen::NativeObject>(second)};
      *counts.addSucceeded = source->add(both);
      return api.execLoop(sen::Duration::fromHertz(100.0));
    });

  sen::kernel::TestKernel kernel(&component);
  kernel.step(cycles);
  return counts;
}

std::shared_ptr<sen::NativeObject> makePlainObject(std::string name)
{
  return std::static_pointer_cast<sen::NativeObject>(
    std::make_shared<test::MyClassBase>(std::move(name), sen::VarMap {}));
}

/// Subscribes before anything matches, so the first announcement comes from the flush's evaluation
/// rather than from the inline seeding addSubscriber does. From inside that callback it adds one
/// object, re-enters the flush, and adds another. The redirect of new objects has to survive the
/// re-entry or the second object is merged into the live set without ever being evaluated locally.
struct NestedFlushResult
{
  std::shared_ptr<std::vector<std::string>> announced {std::make_shared<std::vector<std::string>>()};

  /// Whether the callback reached its re-entry at all. Without this the name assertions below could
  /// all hold for a run in which nothing nested.
  std::shared_ptr<bool> acted {std::make_shared<bool>(false)};
};

NestedFlushResult namesAnnouncedAcrossNestedFlush(std::size_t cycles)
{
  NestedFlushResult result;
  auto announced = result.announced;
  auto acted = result.acted;
  auto started = std::make_shared<bool>(false);
  std::shared_ptr<sen::Subscription<test::MyClassInterface>> subscription;

  sen::kernel::TestComponent component;
  component.onRun(
    [&subscription, announced, acted, started](sen::kernel::RunApi& api)
    {
      if (!*started)
      {
        *started = true;

        auto source = api.getSource("local.test");
        auto* const apiPtr = &api;

        subscription = api.selectAllFrom<test::MyClassInterface>(
          "local.test",
          [announced, acted, source, apiPtr](const auto& addedObjects)
          {
            for (auto* added: addedObjects)
            {
              // The list hands over the interface; the name lives on NativeObject, and the generated
              // class inherits the interface virtually, so this needs a dynamic_cast.
              if (const auto* native = dynamic_cast<const sen::NativeObject*>(added); native != nullptr)
              {
                announced->push_back(native->getName());
              }
            }

            if (*acted)
            {
              return;
            }
            *acted = true;

            std::ignore = source->add(makePlainObject("before-nesting"));
            apiPtr->commit();
            std::ignore = source->add(makePlainObject("after-nesting"));
          },
          nullptr);

        std::ignore = source->add(makePlainObject("trigger"));
      }

      return api.execLoop(sen::Duration::fromHertz(100.0));
    });

  sen::kernel::TestKernel kernel(&component);
  kernel.step(cycles);
  subscription.reset();
  return result;
}

/// Removes a sibling of its own add() batch from inside its own registered() hook, and counts its
/// own update() calls, so a real run can be told from one that did nothing.
class SiblingRemovingObject: public test::MyClassBase
{
public:
  SEN_NOCOPY_NOMOVE(SiblingRemovingObject)

  using MyClassBase::MyClassBase;

  ~SiblingRemovingObject() override = default;

  /// removeFrom is the source the hook calls remove() on, which is not always the one that holds
  /// the sibling: a removal addressed to a participant that never had it has to stay a no-op.
  void setUp(std::shared_ptr<sen::ObjectSource> removeFrom,
             std::shared_ptr<sen::NativeObject> sibling,
             std::shared_ptr<int> updateCount,
             std::shared_ptr<int> removalAttempts)
  {
    source_ = std::move(removeFrom);
    sibling_ = std::move(sibling);
    updateCount_ = std::move(updateCount);
    removalAttempts_ = std::move(removalAttempts);
  }

  void releaseRefs()
  {
    source_ = nullptr;
    sibling_ = nullptr;
  }

  void update(sen::kernel::RunApi& api) override
  {
    test::MyClassBase::update(api);

    if (updateCount_)
    {
      ++*updateCount_;
    }
  }

  void registered(sen::kernel::RegistrationApi& api) override
  {
    test::MyClassBase::registered(api);

    if (acted_ || source_ == nullptr || sibling_ == nullptr)
    {
      return;
    }
    acted_ = true;

    // Counted on the way in: an update count alone cannot tell a hook that removed the sibling from
    // one that found its own wiring null and returned.
    if (removalAttempts_)
    {
      ++*removalAttempts_;
    }
    source_->remove(sibling_);
  }

private:
  std::shared_ptr<sen::ObjectSource> source_;
  std::shared_ptr<sen::NativeObject> sibling_;
  std::shared_ptr<int> updateCount_;
  std::shared_ptr<int> removalAttempts_;
  bool acted_ {false};
};

struct BatchCancelCounts
{
  std::shared_ptr<int> first {std::make_shared<int>(0)};
  std::shared_ptr<int> sibling {std::make_shared<int>(0)};
  std::shared_ptr<int> removalAttempts {std::make_shared<int>(0)};
};

BatchCancelCounts runWithSiblingRemovedFromRegistered(std::size_t cycles)
{
  BatchCancelCounts counts;
  auto first = std::make_shared<SiblingRemovingObject>("first", sen::VarMap {});
  auto sibling = std::make_shared<UpdateCountingObject>("sibling", sen::VarMap {});
  sibling->setUpdateCount(counts.sibling);

  sen::kernel::TestComponent component;
  component.onRun(
    [&first, &sibling, &counts](sen::kernel::RunApi& api)
    {
      auto source = api.getSource("local.test");
      first->setUp(source, std::static_pointer_cast<sen::NativeObject>(sibling), counts.first, counts.removalAttempts);

      const std::vector<std::shared_ptr<sen::NativeObject>> both {std::static_pointer_cast<sen::NativeObject>(first),
                                                                  std::static_pointer_cast<sen::NativeObject>(sibling)};
      const bool added = source->add(both);
      EXPECT_TRUE(added) << "the batch was refused, so this run measures nothing";

      return api.execLoop(sen::Duration::fromHertz(100.0));
    });

  sen::kernel::TestKernel kernel(&component);
  kernel.step(cycles);
  first->releaseRefs();
  return counts;
}

struct ObjectCountReadings
{
  /// Seeded away from every asserted value, so a check for zero cannot be satisfied by a
  /// run in which the hook never fired and nothing was ever read.
  static constexpr std::size_t notRead {~std::size_t {0}};

  std::shared_ptr<std::size_t> insideHook {std::make_shared<std::size_t>(notRead)};
  std::shared_ptr<std::size_t> afterAdd {std::make_shared<std::size_t>(notRead)};
};

/// Reads the component's reported object count from inside registered() and again after add() has
/// returned, for a registration in which the object may remove itself. The count is unsigned, so a
/// decrement that runs before its matching increment does not go to minus one: it wraps, and the
/// monitoring view renders the wrapped value. The second reading is taken after both arithmetic
/// steps have happened and so cannot see an ordering fault at all.
ObjectCountReadings reportedObjectCount(bool removeSelf, std::size_t cycles)
{
  ObjectCountReadings readings;
  auto object = std::make_shared<ReentrantObject>("counted", sen::VarMap {});

  sen::kernel::TestComponent component;
  component.onRun(
    [&object, &readings, removeSelf](sen::kernel::RunApi& api)
    {
      auto source = api.getSource("local.test");
      if (removeSelf)
      {
        object->removeSelfWhenRegistered(source, false);
      }
      object->reportCountFromRegistered(&api, readings.insideHook);
      std::ignore = source->add(object);

      *readings.afterAdd = api.fetchComponentMonitoringInfo().objectCount;

      return api.execLoop(sen::Duration::fromHertz(100.0));
    });

  sen::kernel::TestKernel kernel(&component);
  kernel.step(cycles);
  object->releaseSource();
  return readings;
}

/// Adds one object to two buses of the same runner. Each participant keeps its own names, so both
/// adds are accepted, and both reach the runner with the same object. The runner must register it
/// once: a second registration would put a second node in the list the cycle walks.
std::shared_ptr<int> updatesWhenAddedToBuses(bool secondBus, std::size_t cycles)
{
  auto updates = std::make_shared<int>(0);
  auto object = std::make_shared<UpdateCountingObject>("shared", sen::VarMap {});
  object->setUpdateCount(updates);

  sen::kernel::TestComponent component;
  component.onRun(
    [&object, secondBus](sen::kernel::RunApi& api)
    {
      auto first = api.getSource("local.one");

      auto asNative = std::static_pointer_cast<sen::NativeObject>(object);
      const bool addedToFirst = first->add(asNative);
      EXPECT_TRUE(addedToFirst) << "the first add was refused, so this run measures nothing";

      if (secondBus)
      {
        // Each participant keeps its own names and Bus::isObjectNameUsedLocally walks only its own
        // bus, so this is accepted; if it were refused, nothing would reach registerObjects twice
        // and the run would agree with the control for the wrong reason.
        const bool addedToSecond = api.getSource("local.two")->add(asNative);
        EXPECT_TRUE(addedToSecond) << "the second add was refused, so this run measures nothing";
      }

      return api.execLoop(sen::Duration::fromHertz(100.0));
    });

  sen::kernel::TestKernel kernel(&component);
  kernel.step(cycles);
  return updates;
}

/// Thrown by one subscriber callback and caught by the commit the test drives, so a catch proves
/// the throw came from here and not from the kernel.
struct SubscriberBoom: std::exception
{
  [[nodiscard]] const char* what() const noexcept override { return "subscriber boom"; }
};

struct ThrowingSubscriberResult
{
  std::shared_ptr<std::vector<std::string>> announced {std::make_shared<std::vector<std::string>>()};
  std::shared_ptr<bool> caught {std::make_shared<bool>(false)};
};

/// Optionally throws out of a subscriber callback during a commit the component itself drives and
/// catches, then adds another object from the next cycle. The participant aims new objects at a
/// temporary while it evaluates, so a throw that leaves without putting the aim back makes every
/// later add take one cycle longer to be announced. The run that does not throw calibrates the
/// cycle budget.
ThrowingSubscriberResult namesAnnouncedAfterASubscriberThrows(bool throwOnce, std::size_t cycles)
{
  ThrowingSubscriberResult result;
  auto threw = std::make_shared<bool>(!throwOnce);
  auto addedLater = std::make_shared<bool>(false);
  auto started = std::make_shared<bool>(false);
  std::shared_ptr<sen::Subscription<test::MyClassInterface>> subscription;

  sen::kernel::TestComponent component;
  component.onRun(
    [&subscription, result, threw, addedLater, started](sen::kernel::RunApi& api)
    {
      if (!*started)
      {
        *started = true;

        auto source = api.getSource("local.test");
        subscription = api.selectAllFrom<test::MyClassInterface>(
          "local.test",
          [announced = result.announced, threw](const auto& addedObjects)
          {
            for (auto* object: addedObjects)
            {
              if (const auto* native = dynamic_cast<const sen::NativeObject*>(object); native != nullptr)
              {
                announced->push_back(native->getName());
              }
            }

            if (!*threw)
            {
              *threw = true;
              throw SubscriberBoom {};
            }
          },
          nullptr);

        std::ignore = source->add(makePlainObject("trigger"));

        try
        {
          api.commit();
        }
        catch (const SubscriberBoom&)
        {
          *result.caught = true;
        }

        // Added from a cycle callback, because run() is entered once and execLoop drives the rest.
        return api.execLoop(sen::Duration::fromHertz(100.0),
                            [source, addedLater]
                            {
                              if (!*addedLater)
                              {
                                *addedLater = true;
                                std::ignore = source->add(makePlainObject("after-throw"));
                              }
                            });
      }

      return api.execLoop(sen::Duration::fromHertz(100.0));
    });

  sen::kernel::TestKernel kernel(&component);
  kernel.step(cycles);
  subscription.reset();
  return result;
}

/// Counts each cycle hook separately, and asks for the pre-drain and pre-commit calls so that it
/// sits in the pre-drain list as well as the object list. Two of the three hooks are driven from a
/// different container, so a fixture that only counted update() could not see a broken walk over
/// the other one.
class HookCountingObject: public test::MyClassBase
{
public:
  SEN_NOCOPY_NOMOVE(HookCountingObject)

  using MyClassBase::MyClassBase;

  ~HookCountingObject() override = default;

  struct Counts
  {
    std::shared_ptr<int> updates {std::make_shared<int>(0)};
    std::shared_ptr<int> preDrains {std::make_shared<int>(0)};
    std::shared_ptr<int> preCommits {std::make_shared<int>(0)};

    /// Runner-level, not participant-level: `add()` returning true says the participant gave every
    /// instance a name, not that the runner registered them. A victim's hook count of zero only
    /// means "removed before the walk reached it" if the runner registered it at all.
    std::shared_ptr<int> registrations {std::make_shared<int>(0)};
  };

  void setCounts(Counts counts) { counts_ = std::move(counts); }

  [[nodiscard]] bool needsPreDrainOrPreCommit() const noexcept override { return true; }

  void registered(sen::kernel::RegistrationApi& api) override
  {
    test::MyClassBase::registered(api);
    ++*counts_.registrations;
  }

  void update(sen::kernel::RunApi& api) override
  {
    test::MyClassBase::update(api);
    ++*counts_.updates;
  }

  void preDrain() override { ++*counts_.preDrains; }

  void preCommit() override { ++*counts_.preCommits; }

private:
  Counts counts_;
};

/// Removes an object from inside a cycle hook: itself, or a named victim registered after it. Both
/// shapes erase a node of the list the runner is walking, and which one of them breaks a walk
/// depends on where the walk keeps its cursor, so a suite needs both. The fixture keeps a strong
/// reference to the victim, so what a run can go wrong about is the walk and not a lifetime.
class RemovingOnCycleHook: public test::MyClassBase
{
public:
  SEN_NOCOPY_NOMOVE(RemovingOnCycleHook)

  using MyClassBase::MyClassBase;

  ~RemovingOnCycleHook() override = default;

  enum class Hook : std::uint8_t
  {
    update,
    preDrain,
    preCommit
  };

  /// A null victim means remove itself. With both a victim and removeSelfToo the hook takes out two
  /// nodes in one hook call, which is the shape neither naive walk form survives.
  void setUp(std::shared_ptr<sen::ObjectSource> source,
             Hook hook,
             std::shared_ptr<sen::NativeObject> victim,
             std::shared_ptr<int> removals,
             bool removeSelfToo = false)
  {
    source_ = std::move(source);
    hook_ = hook;
    victim_ = std::move(victim);
    removals_ = std::move(removals);
    removeSelfToo_ = removeSelfToo;
  }

  void releaseRefs()
  {
    source_ = nullptr;
    victim_ = nullptr;
  }

  [[nodiscard]] bool needsPreDrainOrPreCommit() const noexcept override { return true; }

  void update(sen::kernel::RunApi& api) override
  {
    test::MyClassBase::update(api);
    removeOnce(Hook::update);
  }

  void preDrain() override { removeOnce(Hook::preDrain); }

  void preCommit() override { removeOnce(Hook::preCommit); }

private:
  void removeOnce(Hook hook)
  {
    if (hook != hook_ || acted_ || source_ == nullptr)
    {
      return;
    }
    acted_ = true;

    auto self = std::static_pointer_cast<sen::NativeObject>(shared_from_this());

    if (removals_)
    {
      ++*removals_;
    }

    source_->remove(victim_ != nullptr ? victim_ : self);

    if (removeSelfToo_ && victim_ != nullptr)
    {
      if (removals_)
      {
        ++*removals_;
      }
      source_->remove(self);
    }
  }

  std::shared_ptr<sen::ObjectSource> source_;
  std::shared_ptr<sen::NativeObject> victim_;
  std::shared_ptr<int> removals_;
  Hook hook_ {Hook::update};
  bool acted_ {false};
  bool removeSelfToo_ {false};
};

struct CycleHookRemovalCounts
{
  std::shared_ptr<int> removals {std::make_shared<int>(0)};
  HookCountingObject::Counts before;
  HookCountingObject::Counts after;

  /// The count for whichever container the named hook is driven from: update() walks the object
  /// list, preDrain() and preCommit() walk the pre-drain list.
  [[nodiscard]] static int forHook(const HookCountingObject::Counts& counts, RemovingOnCycleHook::Hook hook)
  {
    switch (hook)
    {
      case RemovingOnCycleHook::Hook::update:
        return *counts.updates;
      case RemovingOnCycleHook::Hook::preDrain:
        return *counts.preDrains;
      case RemovingOnCycleHook::Hook::preCommit:
        return *counts.preCommits;
    }
    return 0;
  }
};

/// What the removing object takes out.
enum class Victim : std::uint8_t
{
  itself,
  theObjectAfterIt,
  itselfAndTheObjectAfterIt
};

/// Registers three objects in one batch and has the middle one remove, from the named cycle hook,
/// either itself or the object registered after it. Counts the hooks of the two on either side of
/// it. Both shapes have to work: a walk holding its cursor on the current element dies on the first
/// and survives the second, and a walk holding it one ahead does the opposite.
CycleHookRemovalCounts runWithRemovalFromACycleHook(RemovingOnCycleHook::Hook hook, Victim victim, std::size_t cycles)
{
  CycleHookRemovalCounts counts;
  auto before = std::make_shared<HookCountingObject>("before", sen::VarMap {});
  auto remover = std::make_shared<RemovingOnCycleHook>("remover", sen::VarMap {});
  auto after = std::make_shared<HookCountingObject>("after", sen::VarMap {});
  before->setCounts(counts.before);
  after->setCounts(counts.after);

  sen::kernel::TestComponent component;
  component.onRun(
    [&before, &remover, &after, &counts, hook, victim](sen::kernel::RunApi& api)
    {
      auto source = api.getSource("local.test");
      remover->setUp(source,
                     hook,
                     victim == Victim::itself ? nullptr : std::static_pointer_cast<sen::NativeObject>(after),
                     counts.removals,
                     victim == Victim::itselfAndTheObjectAfterIt);

      const std::vector<std::shared_ptr<sen::NativeObject>> all {std::static_pointer_cast<sen::NativeObject>(before),
                                                                 std::static_pointer_cast<sen::NativeObject>(remover),
                                                                 std::static_pointer_cast<sen::NativeObject>(after)};
      const bool added = source->add(all);
      EXPECT_TRUE(added) << "the batch was refused, so this run measures nothing";

      return api.execLoop(sen::Duration::fromHertz(100.0));
    });

  sen::kernel::TestKernel kernel(&component);
  kernel.step(cycles);
  remover->releaseRefs();
  return counts;
}

/// Adds one already-built object from inside its own registered(), which opens a second
/// registration batch while the first is still running.
class AddingOnRegister: public test::MyClassBase
{
public:
  SEN_NOCOPY_NOMOVE(AddingOnRegister)

  using MyClassBase::MyClassBase;

  ~AddingOnRegister() override = default;

  void setUp(std::shared_ptr<sen::ObjectSource> source,
             std::shared_ptr<sen::NativeObject> toAdd,
             std::shared_ptr<int> updateCount)
  {
    source_ = std::move(source);
    toAdd_ = std::move(toAdd);
    updateCount_ = std::move(updateCount);
  }

  void releaseRefs()
  {
    source_ = nullptr;
    toAdd_ = nullptr;
  }

  void update(sen::kernel::RunApi& api) override
  {
    test::MyClassBase::update(api);

    if (updateCount_)
    {
      ++*updateCount_;
    }
  }

  void registered(sen::kernel::RegistrationApi& api) override
  {
    test::MyClassBase::registered(api);

    if (acted_ || source_ == nullptr || toAdd_ == nullptr)
    {
      return;
    }
    acted_ = true;

    const bool added = source_->add(toAdd_);
    EXPECT_TRUE(added) << "the nested add was refused, so this run measures nothing";
  }

private:
  std::shared_ptr<sen::ObjectSource> source_;
  std::shared_ptr<sen::NativeObject> toAdd_;
  std::shared_ptr<int> updateCount_;
  bool acted_ {false};
};

struct NestedCancelCounts
{
  std::shared_ptr<int> opener {std::make_shared<int>(0)};
  std::shared_ptr<int> nested {std::make_shared<int>(0)};
  std::shared_ptr<int> target {std::make_shared<int>(0)};
  std::shared_ptr<int> removalAttempts {std::make_shared<int>(0)};
};

/// Adds a batch of two, where the first member's registered() adds a third object, and that third
/// object's own registered() removes the second member of the outer batch. The removal has to reach
/// the outer batch, which is not the one open at the time.
NestedCancelCounts runWithACancelAimedAtTheOuterBatch(std::size_t cycles)
{
  NestedCancelCounts counts;
  auto opener = std::make_shared<AddingOnRegister>("opener", sen::VarMap {});
  auto nested = std::make_shared<SiblingRemovingObject>("nested", sen::VarMap {});
  auto target = std::make_shared<UpdateCountingObject>("target", sen::VarMap {});
  target->setUpdateCount(counts.target);

  sen::kernel::TestComponent component;
  component.onRun(
    [&opener, &nested, &target, &counts](sen::kernel::RunApi& api)
    {
      auto source = api.getSource("local.test");
      nested->setUp(source, std::static_pointer_cast<sen::NativeObject>(target), counts.nested, counts.removalAttempts);
      opener->setUp(source, std::static_pointer_cast<sen::NativeObject>(nested), counts.opener);

      const std::vector<std::shared_ptr<sen::NativeObject>> outer {std::static_pointer_cast<sen::NativeObject>(opener),
                                                                   std::static_pointer_cast<sen::NativeObject>(target)};
      const bool added = source->add(outer);
      EXPECT_TRUE(added) << "the outer batch was refused, so this run measures nothing";

      return api.execLoop(sen::Duration::fromHertz(100.0));
    });

  sen::kernel::TestKernel kernel(&component);
  kernel.step(cycles);
  opener->releaseRefs();
  nested->releaseRefs();
  return counts;
}

/// Adds one object whose registered() removes it, and reports whether the kernel's type registry
/// knows its class afterwards. add() is documented as registering the object including its type.
struct TypeRegistrationResult
{
  bool known {false};
  std::size_t countInsideHook {0U};
};

/// countInsideHook is the object count read from inside registered(), after the self-removal: it is
/// how the removing run proves its removal happened. Without it the run would stay green if the
/// fixture's wiring silently stopped removing.
TypeRegistrationResult typeIsKnownAfterASelfRemovingRegistration(bool removeSelf)
{
  auto object = std::make_shared<ReentrantObject>("typed", sen::VarMap {});
  const std::string className {object->getClass()->getQualifiedName()};
  auto insideHook = std::make_shared<std::size_t>(99U);

  sen::kernel::TestComponent component;
  component.onRun(
    [&object, removeSelf, insideHook](sen::kernel::RunApi& api)
    {
      auto source = api.getSource("local.test");
      if (removeSelf)
      {
        object->removeSelfWhenRegistered(source, false);
      }
      object->reportCountFromRegistered(&api, insideHook);
      std::ignore = source->add(object);

      return api.execLoop(sen::Duration::fromHertz(100.0));
    });

  sen::kernel::TestKernel kernel(&component);
  kernel.step(2U);
  const bool known = kernel.getTypes().get(className).has_value();
  object->releaseSource();
  return TypeRegistrationResult {known, *insideHook};
}

/// Counts its update() and unregistered() calls, so a removal that never reached the runner is told
/// apart from one that did.
class LifecycleCountingObject: public test::MyClassBase
{
public:
  SEN_NOCOPY_NOMOVE(LifecycleCountingObject)

  using MyClassBase::MyClassBase;

  ~LifecycleCountingObject() override = default;

  void setCounts(std::shared_ptr<int> updates, std::shared_ptr<int> unregistrations)
  {
    updates_ = std::move(updates);
    unregistrations_ = std::move(unregistrations);
  }

  /// Reads the component's object count from inside unregistered(), the other side of the pair
  /// registered() reads. The count is unsigned, so a decrement that lands before its matching
  /// increment does not read as minus one: it wraps.
  void reportCountFromUnregistered(sen::kernel::RunApi* api, std::shared_ptr<std::size_t> count)
  {
    countApi_ = api;
    countInsideHook_ = std::move(count);
  }

  /// Drops the raw RunApi pointer, which outlives the kernel otherwise.
  void releaseApi() { countApi_ = nullptr; }

  void update(sen::kernel::RunApi& api) override
  {
    test::MyClassBase::update(api);
    if (updates_)
    {
      ++*updates_;
    }
  }

  void unregistered(sen::kernel::RegistrationApi& api) override
  {
    test::MyClassBase::unregistered(api);
    if (unregistrations_)
    {
      ++*unregistrations_;
    }
    if (countApi_ != nullptr && countInsideHook_)
    {
      *countInsideHook_ = countApi_->fetchComponentMonitoringInfo().objectCount;
    }
  }

private:
  std::shared_ptr<int> updates_;
  std::shared_ptr<int> unregistrations_;
  std::shared_ptr<std::size_t> countInsideHook_;
  sen::kernel::RunApi* countApi_ {nullptr};
};

struct OrdinaryRemovalCounts
{
  std::shared_ptr<int> updates {std::make_shared<int>(0)};
  std::shared_ptr<int> unregistrations {std::make_shared<int>(0)};

  /// Only meaningful on the run that removes: left at its sentinel otherwise.
  std::shared_ptr<std::size_t> countInsideUnregistered {std::make_shared<std::size_t>(notRead)};

  static constexpr std::size_t notRead {~std::size_t {0}};
};

/// Publishes one object, lets a flush promote it out of the staged set, and then optionally removes
/// it from a later cycle. The plainest thing a component does, and the one the participant's filter
/// on what it hands the runner could break without any callback being involved.
OrdinaryRemovalCounts runWithAnOrdinaryRemoval(bool removeIt, std::size_t cycles)
{
  OrdinaryRemovalCounts counts;
  auto object = std::make_shared<LifecycleCountingObject>("ordinary", sen::VarMap {});
  object->setCounts(counts.updates, counts.unregistrations);
  auto cyclesSeen = std::make_shared<std::size_t>(0U);

  sen::kernel::TestComponent component;
  component.onRun(
    [&object, &counts, cyclesSeen, removeIt](sen::kernel::RunApi& api)
    {
      auto source = api.getSource("local.test");
      object->reportCountFromUnregistered(&api, counts.countInsideUnregistered);
      auto asNative = std::static_pointer_cast<sen::NativeObject>(object);
      const bool added = source->add(asNative);
      EXPECT_TRUE(added) << "the add was refused, so this run measures nothing";

      return api.execLoop(sen::Duration::fromHertz(100.0),
                          [source, asNative, cyclesSeen, removeIt]
                          {
                            // The second cycle, so a flush has promoted the object out of the staged
                            // set first: that is the state in which nothing but the participant's own
                            // record says the runner holds it.
                            if (++*cyclesSeen == 2U && removeIt)
                            {
                              source->remove(asNative);
                            }
                          });
    });

  sen::kernel::TestKernel kernel(&component);
  kernel.step(cycles);
  object->releaseApi();
  return counts;
}

/// Throws out of its own registered() once. ScopedCancelBatch exists for exactly this: add() is
/// called by user code, so the throw unwinds through a frame that can catch it, and a registration
/// batch left on the runner's in-flight list points at a set that no longer exists. The next
/// removal that finds nothing writes through it.
class ThrowingOnRegister: public test::MyClassBase
{
public:
  SEN_NOCOPY_NOMOVE(ThrowingOnRegister)

  using MyClassBase::MyClassBase;

  ~ThrowingOnRegister() override = default;

  void throwOnce() { shouldThrow_ = true; }

  void registered(sen::kernel::RegistrationApi& api) override
  {
    test::MyClassBase::registered(api);

    if (shouldThrow_)
    {
      shouldThrow_ = false;
      throw SubscriberBoom {};
    }
  }

private:
  bool shouldThrow_ {false};
};

struct AfterRegisterThrowCounts
{
  std::shared_ptr<bool> caught {std::make_shared<bool>(false)};
  std::shared_ptr<int> survivor {std::make_shared<int>(0)};
  std::shared_ptr<int> cancelled {std::make_shared<int>(0)};
  std::shared_ptr<int> removalAttempts {std::make_shared<int>(0)};
};

/// Throws out of registered() and catches it where add() was called, then does the thing that reads
/// the in-flight list: a removal of a batch member that is not registered yet, from inside another
/// member's hook. If the throw left a dangling batch pointer behind, that removal writes through it.
AfterRegisterThrowCounts runAfterARegisteredThrow(std::size_t cycles)
{
  AfterRegisterThrowCounts counts;
  auto thrower = std::make_shared<ThrowingOnRegister>("thrower", sen::VarMap {});
  thrower->throwOnce();
  auto opener = std::make_shared<SiblingRemovingObject>("opener", sen::VarMap {});
  auto victim = std::make_shared<UpdateCountingObject>("victim", sen::VarMap {});
  auto survivor = std::make_shared<UpdateCountingObject>("survivor", sen::VarMap {});
  victim->setUpdateCount(counts.cancelled);
  survivor->setUpdateCount(counts.survivor);

  sen::kernel::TestComponent component;
  component.onRun(
    [&thrower, &opener, &victim, &survivor, counts](sen::kernel::RunApi& api)
    {
      auto source = api.getSource("local.test");

      // First: throw out of registered(), caught here. The guard's job is to leave the runner's
      // in-flight list as it found it.
      try
      {
        std::ignore = source->add(std::static_pointer_cast<sen::NativeObject>(thrower));
      }
      catch (const SubscriberBoom&)
      {
        *counts.caught = true;
      }

      // Then exercise the list the throw unwound through: opener's hook removes victim, a member of
      // the same batch that the loop has not reached, which is filed into every batch in flight.
      opener->setUp(
        source, std::static_pointer_cast<sen::NativeObject>(victim), std::make_shared<int>(0), counts.removalAttempts);

      const std::vector<std::shared_ptr<sen::NativeObject>> batch {
        std::static_pointer_cast<sen::NativeObject>(opener),
        std::static_pointer_cast<sen::NativeObject>(victim),
        std::static_pointer_cast<sen::NativeObject>(survivor)};
      const bool added = source->add(batch);
      EXPECT_TRUE(added) << "the batch was refused, so this run measures nothing";

      return api.execLoop(sen::Duration::fromHertz(100.0));
    });

  sen::kernel::TestKernel kernel(&component);
  kernel.step(cycles);
  opener->releaseRefs();
  return counts;
}

/// Opens a second walk over the same list from inside a cycle hook, by calling RunApi::commit()
/// from update(). Both walks register a cursor, so the runner's registry holds two entries at once
/// -- the only shape in which its vector-ness, its unbroken repair loop and the balance of its
/// pushes and pops are distinguishable from a single member holding a single cursor.
class NestingOnUpdate: public test::MyClassBase
{
public:
  SEN_NOCOPY_NOMOVE(NestingOnUpdate)

  using MyClassBase::MyClassBase;

  ~NestingOnUpdate() override = default;

  void setUp(sen::kernel::RunApi* api, std::shared_ptr<int> nestings)
  {
    api_ = api;
    nestings_ = std::move(nestings);
  }

  void update(sen::kernel::RunApi& api) override
  {
    test::MyClassBase::update(api);

    if (acted_ || api_ == nullptr)
    {
      return;
    }
    acted_ = true;

    if (nestings_)
    {
      ++*nestings_;
    }

    // Opens the commit walk while this update walk is still live.
    api_->commit();
  }

private:
  sen::kernel::RunApi* api_ {nullptr};
  std::shared_ptr<int> nestings_;
  bool acted_ {false};
};

struct NestedWalkCounts
{
  std::shared_ptr<int> nestings {std::make_shared<int>(0)};
  std::shared_ptr<int> removals {std::make_shared<int>(0)};
  HookCountingObject::Counts before;
  HookCountingObject::Counts after;
};

/// Removes itself from inside preCommit(), which is reached from the commit walk. Paired with a
/// hook that opens that walk from inside update(), it puts a live cursor on the *object list* while
/// the innermost cursor is on the *pre-drain list* -- so repairing only the innermost registered
/// cursor leaves the outer one on the node being freed.
class SelfRemovingOnPreCommit: public test::MyClassBase
{
public:
  SEN_NOCOPY_NOMOVE(SelfRemovingOnPreCommit)

  using MyClassBase::MyClassBase;

  ~SelfRemovingOnPreCommit() override = default;

  /// A null victim means remove itself. The nested-walk test passes the *outer* walk's current
  /// object, because that is the node the outer cursor is standing on while its hook runs -- the
  /// only shape in which repairing the innermost cursor alone is not enough.
  void setUp(std::shared_ptr<sen::ObjectSource> source,
             std::shared_ptr<int> removals,
             std::shared_ptr<sen::NativeObject> victim = nullptr)
  {
    source_ = std::move(source);
    removals_ = std::move(removals);
    victim_ = std::move(victim);
  }

  void releaseRefs()
  {
    source_ = nullptr;
    victim_ = nullptr;
  }

  [[nodiscard]] bool needsPreDrainOrPreCommit() const noexcept override { return true; }

  void preCommit() override
  {
    if (acted_ || source_ == nullptr)
    {
      return;
    }
    acted_ = true;

    if (removals_)
    {
      ++*removals_;
    }

    source_->remove(victim_ != nullptr ? victim_ : std::static_pointer_cast<sen::NativeObject>(shared_from_this()));
  }

private:
  std::shared_ptr<sen::ObjectSource> source_;
  std::shared_ptr<int> removals_;
  std::shared_ptr<sen::NativeObject> victim_;
  bool acted_ {false};
};

/// Four objects. The second opens the commit walk from its update(), so the update walk's cursor is
/// live on the third object while the commit walk runs; the third removes itself from preCommit(),
/// which erases the node the *outer* cursor is standing on while the innermost cursor is in the
/// other container. Repairing every registered cursor keeps the outer walk alive; repairing only
/// the innermost one does not, and the fourth object is what is lost.
NestedWalkCounts runWithARemovalInsideANestedWalk(std::size_t cycles)
{
  NestedWalkCounts counts;
  auto nester = std::make_shared<NestingOnUpdate>("nester", sen::VarMap {});
  auto victim = std::make_shared<SelfRemovingOnPreCommit>("victim", sen::VarMap {});
  auto after = std::make_shared<HookCountingObject>("after", sen::VarMap {});
  after->setCounts(counts.after);

  sen::kernel::TestComponent component;
  component.onRun(
    [&nester, &victim, &after, &counts](sen::kernel::RunApi& api)
    {
      auto source = api.getSource("local.test");
      nester->setUp(&api, counts.nestings);
      // The victim is the nester: the outer update walk's cursor is on the nester's node while the
      // nested walk runs, so this is the removal that needs a cursor other than the innermost one
      // repaired. Removing itself would leave the outer cursor untouched and the test would pass
      // with the repair looking at only the innermost entry.
      victim->setUp(source, counts.removals, std::static_pointer_cast<sen::NativeObject>(nester));

      const std::vector<std::shared_ptr<sen::NativeObject>> all {std::static_pointer_cast<sen::NativeObject>(nester),
                                                                 std::static_pointer_cast<sen::NativeObject>(victim),
                                                                 std::static_pointer_cast<sen::NativeObject>(after)};
      const bool added = source->add(all);
      EXPECT_TRUE(added) << "the batch was refused, so this run measures nothing";

      return api.execLoop(sen::Duration::fromHertz(100.0));
    });

  sen::kernel::TestKernel kernel(&component);
  kernel.step(cycles);
  nester->setUp(nullptr, nullptr);
  victim->releaseRefs();
  return counts;
}

/// Removes a sibling of the batch being registered, but through a *different* bus of the same
/// runner. The runner is shared, so the removal reaches it; the participant that owns the batch
/// never hears about it. A cancel filed on that basis stops a registration whose owner never asked
/// for it to stop.
class ForeignBusRemovingObject: public test::MyClassBase
{
public:
  SEN_NOCOPY_NOMOVE(ForeignBusRemovingObject)

  using MyClassBase::MyClassBase;

  ~ForeignBusRemovingObject() override = default;

  void setUp(std::shared_ptr<sen::ObjectSource> foreign,
             std::shared_ptr<sen::NativeObject> sibling,
             std::shared_ptr<int> removals)
  {
    foreign_ = std::move(foreign);
    sibling_ = std::move(sibling);
    removals_ = std::move(removals);
  }

  void releaseRefs()
  {
    foreign_ = nullptr;
    sibling_ = nullptr;
  }

  void registered(sen::kernel::RegistrationApi& api) override
  {
    test::MyClassBase::registered(api);

    if (acted_ || foreign_ == nullptr || sibling_ == nullptr)
    {
      return;
    }
    acted_ = true;

    if (removals_)
    {
      ++*removals_;
    }
    foreign_->remove(sibling_);
  }

private:
  std::shared_ptr<sen::ObjectSource> foreign_;
  std::shared_ptr<sen::NativeObject> sibling_;
  std::shared_ptr<int> removals_;
  bool acted_ {false};
};

/// Adds a batch of two to one bus, and has the first member's registered() hook remove the second
/// through a second bus of the same runner. The second bus never staged it, so nothing should be
/// cancelled: the sibling must end up registered and driven.
BatchCancelCounts runWithASiblingRemovedThroughAnotherBus(std::size_t cycles)
{
  BatchCancelCounts counts;
  auto first = std::make_shared<ForeignBusRemovingObject>("first", sen::VarMap {});
  auto sibling = std::make_shared<UpdateCountingObject>("sibling", sen::VarMap {});
  sibling->setUpdateCount(counts.sibling);

  sen::kernel::TestComponent component;
  component.onRun(
    [&first, &sibling, &counts](sen::kernel::RunApi& api)
    {
      auto owning = api.getSource("local.one");
      auto foreign = api.getSource("local.two");
      first->setUp(foreign, std::static_pointer_cast<sen::NativeObject>(sibling), counts.removalAttempts);

      const std::vector<std::shared_ptr<sen::NativeObject>> both {std::static_pointer_cast<sen::NativeObject>(first),
                                                                  std::static_pointer_cast<sen::NativeObject>(sibling)};
      const bool added = owning->add(both);
      EXPECT_TRUE(added) << "the batch was refused, so this run measures nothing";

      return api.execLoop(sen::Duration::fromHertz(100.0));
    });

  sen::kernel::TestKernel kernel(&component);
  kernel.step(cycles);
  first->releaseRefs();
  return counts;
}

}  // namespace

/// @test
/// @requirements(SEN-1733)
/// An object removed from inside its own registered() is not left in the runner's pre-drain list,
/// so preDrain is never called for it.
TEST(ObjectReentrancy, RemoveFromRegisteredLeavesNoPreDrainEntry)
{
  constexpr std::size_t cycles = 5U;

  ASSERT_GT(*runWithSelfRemovingObject(false, false, cycles), 0) << "preDrain never ran for an object "
                                                                    "that stayed registered, so this "
                                                                    "test cannot see the defect";

  EXPECT_EQ(*runWithSelfRemovingObject(true, false, cycles), 0) << "preDrain ran for an object removed "
                                                                   "before any drain";
}

/// @test
/// @requirements(SEN-1733)
/// An object that removes and then re-adds itself from inside registered() is registered once, so
/// preDrain is called once per cycle rather than twice.
TEST(ObjectReentrancy, RemoveAndAddAgainFromRegisteredRegistersOnce)
{
  constexpr std::size_t cycles = 5U;
  const auto plain = runWithSelfRemovingObject(false, false, cycles);
  ASSERT_GT(*plain, 0) << "preDrain never ran for an object that stayed registered, so an equality "
                          "here would hold at zero and this test cannot see the defect";

  const auto readded = runWithSelfRemovingObject(true, true, cycles);

  EXPECT_EQ(*readded, *plain) << "preDrain ran a different number of times than for an object that "
                                 "registered once, so the re-added object holds a second pre-drain entry";
}

/// @test
/// An object added and then removed from inside a subscription's added callback is never announced
/// as added, because it was never published. The count is compared against a run that does no
/// churn, so it does not depend on how often the trigger object is announced.
TEST(ObjectReentrancy, AddAndRemoveInsideAnAddedCallbackAnnouncesNothing)
{
  // At least two, so an object left in the staging map has a later flush to be promoted by: one
  // cycle could hide the defect by never reaching the announcement.
  constexpr std::size_t cycles = 5U;
  static_assert(cycles >= 2U, "the transient needs a later flush to be promoted by");

  const auto baseline = countAnnouncedAdditions(false, false, cycles);
  ASSERT_GE(occurrences(baseline, "trigger"), 1) << "the added callback never fired, so this test "
                                                    "cannot see the defect";
  ASSERT_EQ(occurrences(baseline, "transient"), 0) << "an arm that does no churn announced the "
                                                      "transient, so the names are not what this "
                                                      "test thinks they are";

  // Shows an extra announcement is visible at all: add inside the callback and keep
  // the object. Without it the runs below could agree because nothing was ever announced.
  const auto kept = countAnnouncedAdditions(true, false, cycles);
  ASSERT_TRUE(kept.churned) << "the churn never ran";
  ASSERT_TRUE(kept.added) << "the add inside the callback was refused, so this test is measuring a "
                             "refusal rather than the defect";
  ASSERT_GE(occurrences(kept, "transient"), 1) << "an object added inside the callback and kept was "
                                                  "not announced, so an announcement of it is "
                                                  "invisible to this test";

  const auto removed = countAnnouncedAdditions(true, true, cycles);
  ASSERT_TRUE(removed.churned) << "the churn never ran";
  ASSERT_TRUE(removed.added) << "the add inside the callback was refused";

  EXPECT_EQ(occurrences(removed, "transient"), 0) << "an object added and removed inside the "
                                                     "callback was still announced as added";
}

/// @test
/// A hook that removes its own object from inside unregistered() does not see the hook run a second
/// time, because the registration is already gone by the time the hook is called.
TEST(ObjectReentrancy, RemoveFromUnregisteredDoesNotReenter)
{
  const auto counts = countUnregisteredHookCalls();

  ASSERT_EQ(*counts.removalAttempts, 1) << "the hook asked for a number of nested removals other "
                                           "than one, so an equality on the hook count below says "
                                           "nothing about re-entry -- the pre-change ordering asks "
                                           "three times";

  EXPECT_EQ(*counts.hookCalls, 1) << "unregistered() ran again for a registration that was already "
                                     "gone, so a hook removing its own object re-enters";
}

/// @test
/// An object that re-adds itself from inside unregistered() is registered again in full, so a later
/// removal still reaches it.
TEST(ObjectReentrancy, ReaddFromUnregisteredIsRegisteredAgain)
{
  const auto counts = runReaddingOnUnregister();

  ASSERT_EQ(counts->removalsIssued, 2) << "the test did not issue both removals, so the hook count "
                                          "below is not evidence of anything";
  ASSERT_TRUE(counts->readdAccepted) << "the re-add was refused, so this test is measuring a "
                                        "refusal rather than a re-registration";

  EXPECT_EQ(counts->unregistered, 2) << "the second removal did not reach the object, so the re-add "
                                        "left it with no registration behind it";

  // The guarded clear of the registration stamp is NOT gated here. It would take one assertion on
  // getRegistrationTime(), which is private on NativeObject behind a curated friend list with no
  // test entry -- so reaching it means widening a public core header's friends for a kernel test.
  // Left unasserted: getRegistrationTime() is private behind a curated friend list.
}

/// @test
/// An object removed from inside needsPreDrainOrPreCommit() gets no pre-drain entry. The run that
/// does not remove itself shows an entry would otherwise have been added.
TEST(ObjectReentrancy, RemoveFromPreDrainQueryLeavesNoEntry)
{
  ASSERT_GT(*countPreDrainsWithQueryRemoval(false), 0) << "preDrain never ran for an object that "
                                                          "stayed registered, so this test cannot "
                                                          "see the defect";

  EXPECT_EQ(*countPreDrainsWithQueryRemoval(true), 0) << "preDrain ran for an object removed while "
                                                         "the runner was asking whether it needed it";
}

/// @test
/// An object removed from inside its own registered() is not committed afterwards, so a property it
/// set on the way out never becomes visible. The run that does not remove itself shows the
/// commit would otherwise have made it visible.
TEST(ObjectReentrancy, RemoveFromRegisteredSkipsTheCommit)
{
  const auto kept = propAfterRun(false);
  ASSERT_EQ(kept.visible, 1) << "the property never became visible for an object that stayed "
                                "registered, so this test cannot see the defect";

  const auto removed = propAfterRun(true);

  // The staged value proves the hook ran and set it. Checking both halves is what makes this about
  // the registration's own commit: the visible value alone would also be 0 if the hook had never
  // run, and the kept arm alone only shows that *some* commit moves the value.
  EXPECT_EQ(removed.staged, 1) << "the hook never set the property, so the visible value below says "
                                  "nothing";

  EXPECT_EQ(removed.visible, 0) << "the registration committed an object that registered() had "
                                   "already removed";
}

/// @test
/// An object the participant refuses for a duplicate name is not registered in the runner, so the
/// cycle never updates it. The accepted object's count shows updates happen at all.
TEST(ObjectReentrancy, ARefusedDuplicateNameIsNotDrivenByTheRunner)
{
  const auto counts = runWithDuplicateName(3U);

  ASSERT_FALSE(*counts.addSucceeded) << "the add reported success, so the second instance was not "
                                        "refused and this test is measuring something else";

  ASSERT_GT(*counts.accepted, 0) << "the accepted object was never updated, so this test cannot see "
                                    "the defect";

  EXPECT_EQ(*counts.refused, 0) << "an object refused for a duplicate name is still registered and "
                                   "driven by the runner, invisible to the participant";
}

/// @test
/// An object added AFTER a flush has been re-entered from inside an added callback still reaches the
/// subscription. The object added BEFORE the re-entry does not, and that is a separate defect this
/// change does not close: the nested flush moves it out of the staging map, so the outer frame
/// merges it into the live set with no local evaluation. Asserted by name, because the announcement
/// count alone is misleading — the trigger is announced twice, once from a cache the nested
/// evaluation re-reads.
TEST(ObjectReentrancy, AnAddAfterANestedFlushStillReachesTheSubscription)
{
  const auto result = namesAnnouncedAcrossNestedFlush(8U);

  const auto occurrences = [&result](std::string_view name)
  { return std::count(result.announced->begin(), result.announced->end(), name); };

  ASSERT_TRUE(*result.acted) << "the callback never reached its re-entry, so no flush was nested and "
                                "the names below say nothing about nesting";
  ASSERT_GE(occurrences("trigger"), 1) << "the added callback never fired, so this test cannot see "
                                          "the defect";

  EXPECT_GE(occurrences("after-nesting"), 1) << "the object added after the re-entered flush never "
                                                "reached the subscription, so the redirect of new "
                                                "objects did not survive the re-entry";

  EXPECT_EQ(occurrences("before-nesting"), 0) << "the object added before the re-entry reached the "
                                                 "subscription; if that now works, the comment "
                                                 "above is stale";
}

/// @test
/// A member of an add() batch removed by an earlier member's registered() hook is cancelled rather
/// than registered: added and removed before the add took effect means nothing happened, so the
/// runner never drives it. The first object's count shows registration happened at all.
TEST(ObjectReentrancy, ASiblingRemovedFromRegisteredIsCancelled)
{
  const auto counts = runWithSiblingRemovedFromRegistered(3U);

  ASSERT_EQ(*counts.removalAttempts, 1) << "the hook never asked for the removal, so this test "
                                           "cannot see the defect";
  ASSERT_GT(*counts.first, 0) << "the object whose hook does the removing was never updated, so "
                                 "this test cannot see the defect";

  EXPECT_EQ(*counts.sibling, 0) << "a batch member removed before the loop reached it was registered "
                                   "anyway, so the runner drives an object the participant unstaged";
}

/// @test
/// An object that removes itself from inside its own registered() leaves the reported object count
/// at zero. The count is unsigned, so a decrement ahead of its increment wraps instead of going
/// negative, and the monitoring view renders whatever it holds. The run that keeps the object shows
/// that the count is really being read.
TEST(ObjectReentrancy, SelfRemovalDuringRegistrationDoesNotWrapTheObjectCount)
{
  const auto kept = reportedObjectCount(false, 3U);
  ASSERT_EQ(*kept.afterAdd, 1U) << "the object that stayed registered was not counted, so this test "
                                   "is not reading the count";
  EXPECT_EQ(*kept.insideHook, 1U) << "the object was not counted yet when its own registered() ran, "
                                     "so the count is raised after the hook and a removal from the "
                                     "hook decrements ahead of it";

  const auto removed = reportedObjectCount(true, 3U);
  ASSERT_NE(*removed.insideHook, ObjectCountReadings::notRead) << "registered() never read the "
                                                                  "count, so the reading below is "
                                                                  "the fixture's seed";
  EXPECT_EQ(*removed.insideHook, 0U) << "the reported object count is wrong inside registered() "
                                        "after a self-removal; a wrapped unsigned count reaches the "
                                        "monitoring view";
  EXPECT_EQ(*removed.afterAdd, 0U) << "the reported object count is wrong after a self-removal "
                                      "during registration";
}

/// @test
/// One object published on two buses of the same runner is updated as often as the same object on
/// one bus, and no more: the runner's map is per runner while names are per bus, so both adds are
/// accepted and the second registration has to be refused. The one-bus run is the quantity rather
/// than a bound, so a change in how many cycles update at all cannot absorb a doubling.
TEST(ObjectReentrancy, AddingOneObjectToTwoBusesUpdatesItOnce)
{
  constexpr std::size_t cycles = 3U;

  const auto oneBus = updatesWhenAddedToBuses(false, cycles);
  ASSERT_GT(*oneBus, 0) << "the object was never updated on one bus, so this test cannot see the "
                           "defect";

  const auto twoBuses = updatesWhenAddedToBuses(true, cycles);

  EXPECT_EQ(*twoBuses, *oneBus) << "the object was updated more often when published on two buses, "
                                   "so it holds more than one entry in the list the cycle walks";
}

/// @test
/// An object added after a subscriber callback threw is announced in the same cycle as one added
/// with nothing throwing. The throw leaves the participant in the middle of an evaluation, where
/// new objects are aimed at a temporary rather than at the staged set, and unless the restore runs
/// on the unwind path that aim stays there: the object is still announced, one cycle late, because
/// the end of the flush moves the temporary into the staged set. The run that does not throw shows
/// that one cycle is enough, and the caught exception confirms the throw really happened.
TEST(ObjectReentrancy, AnAddAfterASubscriberThrewIsNotDelayed)
{
  constexpr std::size_t cycles = 1U;

  const auto occurrences = [](const ThrowingSubscriberResult& result, std::string_view name)
  { return std::count(result.announced->begin(), result.announced->end(), name); };

  const auto plain = namesAnnouncedAfterASubscriberThrows(false, cycles);
  ASSERT_GE(occurrences(plain, "after-throw"), 1) << "one cycle does not announce an add even with "
                                                     "nothing throwing, so this test cannot see the "
                                                     "defect";

  const auto thrown = namesAnnouncedAfterASubscriberThrows(true, cycles);
  ASSERT_TRUE(*thrown.caught) << "nothing escaped the commit, so this test cannot see the defect";
  ASSERT_GE(occurrences(thrown, "trigger"), 1) << "the subscriber callback never fired, so this test "
                                                  "cannot see the defect";

  EXPECT_GE(occurrences(thrown, "after-throw"), 1) << "an object added after a subscriber threw took "
                                                      "longer to be announced than the same add "
                                                      "without a throw, so the participant is still "
                                                      "aiming new objects at the temporary";
}

/// Drives the named cycle hook for each of the three hook families and names which one is running,
/// so a failure says where. The three victim shapes are separate tests because a broken walk fails
/// by crash in some of them, and a crash takes the process with it: separated, each shape can be
/// run on its own and attributed.
void checkTheWalkSurvives(Victim victim, std::size_t cycles)
{
  for (const auto hook:
       {RemovingOnCycleHook::Hook::update, RemovingOnCycleHook::Hook::preDrain, RemovingOnCycleHook::Hook::preCommit})
  {
    SCOPED_TRACE(hook == RemovingOnCycleHook::Hook::update     ? "hook: update, on the object list"
                 : hook == RemovingOnCycleHook::Hook::preDrain ? "hook: preDrain, on the pre-drain list"
                                                               : "hook: preCommit, on the pre-drain list");

    const auto counts = runWithRemovalFromACycleHook(hook, victim, cycles);
    const int expectedRemovals = victim == Victim::itselfAndTheObjectAfterIt ? 2 : 1;

    ASSERT_EQ(*counts.removals, expectedRemovals) << "the hook did not remove what this shape is "
                                                     "about, so this run cannot see the defect";

    // Both objects reached the runner. Without this the victim oracles below are satisfied by a
    // batch registration that silently stopped early, which would leave the victim unregistered and
    // its hook count at zero for a reason that has nothing to do with the walk.
    ASSERT_EQ(*counts.before.registrations, 1) << "the object ahead of the remover was not "
                                                  "registered exactly once";
    ASSERT_EQ(*counts.after.registrations, 1) << "the victim was not registered exactly once, so a "
                                                 "zero hook count below would say nothing about the "
                                                 "removal";

    if (victim == Victim::itself)
    {
      ASSERT_GT(CycleHookRemovalCounts::forHook(counts.before, hook), 0) << "the object ahead of the "
                                                                            "remover was never "
                                                                            "called, so this run "
                                                                            "cannot see the defect";

      EXPECT_EQ(CycleHookRemovalCounts::forHook(counts.after, hook), static_cast<int>(cycles))
        << "the object behind the remover was not called on every cycle, so the walk did not survive";
      continue;
    }

    // The victim sits behind the remover, so the hook takes it out before the walk reaches it and
    // its own count stays at zero. The batch add is checked in the harness, so a zero here is a
    // removal rather than an object that was never registered.
    ASSERT_EQ(CycleHookRemovalCounts::forHook(counts.after, hook), 0) << "the victim's hook ran even "
                                                                         "though it was removed "
                                                                         "before the walk reached it";

    // Exact: the object ahead of the remover is called on every cycle, and a walk that survived the
    // removal and then lost later cycles is as wrong as one that died on the spot.
    EXPECT_EQ(CycleHookRemovalCounts::forHook(counts.before, hook), static_cast<int>(cycles))
      << "the object ahead of the remover was not called on every cycle, so the walk did not survive";
  }
}

/// Adds a second object from inside its own update(), optionally removing itself first. The walk
/// has to reach the new node in the same cycle: the runner's list is walked with a cursor that the
/// container repairs, and a cursor already moved past the end never sees an appended node. The
/// remove-first case is the one shape that is still a cycle late, and it is asserted as such rather
/// than left unstated.
class AddingOnUpdate: public test::MyClassBase
{
public:
  SEN_NOCOPY_NOMOVE(AddingOnUpdate)

  using MyClassBase::MyClassBase;

  ~AddingOnUpdate() override = default;

  void setUp(std::shared_ptr<sen::ObjectSource> source,
             std::shared_ptr<UpdateCountingObject> spawned,
             bool removeSelfFirst)
  {
    source_ = std::move(source);
    spawned_ = std::move(spawned);
    removeSelfFirst_ = removeSelfFirst;
  }

  void releaseRefs()
  {
    source_ = nullptr;
    spawned_ = nullptr;
  }

  void update(sen::kernel::RunApi& api) override
  {
    test::MyClassBase::update(api);

    if (acted_ || source_ == nullptr)
    {
      return;
    }
    acted_ = true;

    if (removeSelfFirst_)
    {
      source_->remove(std::static_pointer_cast<sen::NativeObject>(shared_from_this()));
    }
    std::ignore = source_->add(std::static_pointer_cast<sen::NativeObject>(spawned_));
  }

private:
  std::shared_ptr<sen::ObjectSource> source_;
  std::shared_ptr<UpdateCountingObject> spawned_;
  bool removeSelfFirst_ {false};
  bool acted_ {false};
};

/// Runs one cycle only, so the count can distinguish "driven in the cycle it was added" from
/// "driven on the next one".
std::shared_ptr<int> updatesOfAnObjectAddedFromAHook(bool removeSelfFirst)
{
  auto updates = std::make_shared<int>(0);
  auto adder = std::make_shared<AddingOnUpdate>("adder", sen::VarMap {});
  auto spawned = std::make_shared<UpdateCountingObject>("spawned", sen::VarMap {});
  spawned->setUpdateCount(updates);

  sen::kernel::TestComponent component;
  component.onRun(
    [&adder, &spawned, removeSelfFirst](sen::kernel::RunApi& api)
    {
      auto source = api.getSource("local.test");
      adder->setUp(source, spawned, removeSelfFirst);
      const bool added = source->add(std::static_pointer_cast<sen::NativeObject>(adder));
      EXPECT_TRUE(added) << "the adder was refused, so this run measures nothing";
      return api.execLoop(sen::Duration::fromHertz(100.0));
    });

  sen::kernel::TestKernel kernel(&component);
  kernel.step(1U);
  adder->releaseRefs();
  return updates;
}

/// @test
/// An object that removes itself from a cycle hook does not break the walk it is being driven by.
/// A walk holding its cursor on the current element dies here and survives the next test, so a
/// suite needs both shapes.
TEST(ObjectReentrancy, ASelfRemovalFromACycleHookDoesNotBreakTheWalk) { checkTheWalkSurvives(Victim::itself, 4U); }

/// @test
/// An object that removes the object registered *after* it does not break the walk either. This is
/// the shape the range-for the change replaced handled and an advance-before-the-hook walk does
/// not: the cursor is standing on exactly the node the hook frees.
TEST(ObjectReentrancy, ARemovalOfTheNextObjectFromACycleHookDoesNotBreakTheWalk)
{
  checkTheWalkSurvives(Victim::theObjectAfterIt, 4U);
}

/// @test
/// A hook that removes itself *and* the object after it, freeing two nodes in one call. Neither
/// naive walk form survives this one: a cursor on the current node dies on the first erase and a
/// cursor on the successor dies on the second, so only repairing whatever cursor stands on a node
/// being freed passes.
TEST(ObjectReentrancy, ARemovalOfItselfAndTheNextObjectDoesNotBreakTheWalk)
{
  checkTheWalkSurvives(Victim::itselfAndTheObjectAfterIt, 4U);
}

/// @test
/// A removal aimed at a member of an outer registration batch is honoured while a nested batch is
/// open. A hook can add an object, and that object's own hook can remove a member of the batch the
/// first hook came from; a runner that tracks only the innermost batch files the cancellation
/// where nobody reads it and registers an object the participant has already unstaged. The opener
/// and the nested object pin that both registrations happened.
TEST(ObjectReentrancy, ACancelAimedAtTheOuterBatchIsHonoured)
{
  const auto counts = runWithACancelAimedAtTheOuterBatch(3U);

  ASSERT_EQ(*counts.removalAttempts, 1) << "the nested object's hook never asked for the removal, "
                                           "so this test cannot see the defect";
  ASSERT_GT(*counts.opener, 0) << "the object that opened the nested batch was never updated, so "
                                  "this test cannot see the defect";
  ASSERT_GT(*counts.nested, 0) << "the nested object was never updated, so the nested batch did not "
                                  "register and this test cannot see the defect";

  EXPECT_EQ(*counts.target, 0) << "a batch member removed from inside a nested registration was "
                                  "registered anyway, so the cancellation reached only the batch "
                                  "that was open at the time";
}

/// @test
/// Fails against release/0.7.x, but not for the reason it tests: there the object-count arithmetic
/// sits after the hooks, so the count read from inside registered() is 0 on the kept arm and a
/// wrapped SIZE_MAX on the removing one, and the two vacuity guards fire before the type assertion
/// is reached. The type half alone would hold there, since 0.7.x has no early exit between
/// registered() and the type registration.
///
/// An object removed by its own registered() still leaves its class in the kernel's type registry.
/// add() registers the object and its type together, and the registration stops short once the
/// object is gone, so the type has to be recorded before any hook runs. The run that keeps the
/// object shows the registry is really being read.
TEST(ObjectReentrancy, ASelfRemovingRegistrationStillRegistersTheType)
{
  const auto kept = typeIsKnownAfterASelfRemovingRegistration(false);
  ASSERT_TRUE(kept.known) << "the class of an object that stayed registered is not in the registry, "
                             "so this test is not reading it";
  ASSERT_EQ(kept.countInsideHook, 1U) << "the arm that keeps the object did not see itself counted, "
                                         "so the fixture is not reaching registered() at all";

  const auto removed = typeIsKnownAfterASelfRemovingRegistration(true);
  ASSERT_EQ(removed.countInsideHook, 0U) << "the removing arm did not actually remove itself, so a "
                                            "green result below would say nothing";

  EXPECT_TRUE(removed.known) << "an object removed by its own registered() left its class "
                                "unregistered, so half of what add() promises is undone";
}

/// @test
/// The removal half passes against release/0.7.x, which forwarded every instance to the runner
/// unconditionally, as this does again -- so what that half gates is that the forward still happens
/// and that unregistered() runs once, not a pre-existing defect. The count read from inside
/// unregistered() does not: on 0.7.x the decrement sits after the hook, so that reading is 1 there.
/// Kept because this is the test that reddens if a participant-side record of what the runner
/// accepted is ever reintroduced and breaks the plain case.
/// An ordinary removal, with no callback involved, stops the runner driving the object and calls
/// unregistered() once. The participant hands the runner only the instances it asked it to register,
/// and that record has to survive a flush promoting the object out of the staged set: reading the
/// staged and current maps instead would make this removal a no-op for any object the rejection
/// path has already unstaged. The run that does not remove shows the updates were happening.
TEST(ObjectReentrancy, AnOrdinaryRemovalStopsTheRunnerDrivingTheObject)
{
  constexpr std::size_t cycles = 5U;

  const auto kept = runWithAnOrdinaryRemoval(false, cycles);
  ASSERT_GT(*kept.updates, 1) << "the object that stayed was updated at most once, so this test "
                                 "cannot see the defect";
  EXPECT_EQ(*kept.unregistrations, 0) << "unregistered() ran for an object nobody removed";

  const auto removed = runWithAnOrdinaryRemoval(true, cycles);

  EXPECT_EQ(*removed.unregistrations, 1) << "an ordinary removal did not reach the runner, so the "
                                            "object is still registered and nothing can ever "
                                            "unregister it";
  // Exact rather than a comparison: the object is added before the first cycle and removed from the
  // second cycle's callback, so two updates is the whole of what it can get.
  EXPECT_EQ(*removed.updates, 2) << "the removed object was not updated exactly twice, so either the "
                                    "removal did not take effect on the cycle it was made or the "
                                    "runner is still driving it";

  // The count as the hook itself sees it. The decrement is ahead of the hook, so the object is
  // already out: one object registered, zero left. A decrement placed after its matching increment
  // would read one here, and one that ran against an empty count would have wrapped to a huge
  // number instead of going negative.
  ASSERT_NE(*removed.countInsideUnregistered, OrdinaryRemovalCounts::notRead)
    << "unregistered() never read the count, so the reading below is the fixture's sentinel";
  EXPECT_EQ(*removed.countInsideUnregistered, 0U) << "the object count unregistered() reads does not "
                                                     "have this object already taken out of it";
}

/// @test
/// A batch registered after a throw out of registered() still cancels its own members correctly.
/// Read what this does and does not gate. It drives the state `ScopedCancelBatch` exists for -- a
/// throw unwinding out of `add()`, which user code calls, leaving the in-flight list to be unwound
/// by a destructor rather than by a statement -- and then performs the removal that reads that
/// list. But its oracle is the cancellation, which another test already gates, so a non-popping
/// guard leaves the test green: the live batch still cancels its victim. What catches that is the
/// address sanitizer, where the stale entry is a write through a destroyed set reached from a dead
/// frame. So this is a gate on the cancellation surviving a throw, and ASan is the gate on the
/// guard. The caught exception and the removal attempt pin that the state was reached.
TEST(ObjectReentrancy, ARegisteredThrowLeavesTheInFlightBatchListIntact)
{
  constexpr std::size_t cycles = 3U;
  const auto counts = runAfterARegisteredThrow(cycles);

  ASSERT_TRUE(*counts.caught) << "nothing escaped add(), so this test cannot see the defect";
  ASSERT_EQ(*counts.removalAttempts, 1) << "the second batch's hook never asked for the removal, so "
                                           "nothing read the in-flight list";

  EXPECT_EQ(*counts.survivor, static_cast<int>(cycles))
    << "the member after the removal was never updated, so the batch "
       "after a throw does not complete";
  EXPECT_EQ(*counts.cancelled, 0) << "the removed member was registered anyway, so the cancellation "
                                     "did not reach the batch it was filed against";
}

/// @test
/// A removal taken inside a nested walk repairs the outer walk's cursor too, not only the innermost
/// one. The first object opens the commit walk from its own update(), so the update walk's cursor is
/// standing on it; an object reached by the nested walk then removes *that* object from preCommit(),
/// erasing the node the outer cursor holds while the innermost registered cursor is in the other
/// container altogether. This is the shape the cursor registry is a vector for.
///
/// The victim is named rather than removing itself, and it has to be: a walk holds its cursor on
/// the object whose hook is running, so an object removing itself leaves the outer cursor untouched
/// and nothing here would need a cursor other than the innermost repaired. A partial repair fails
/// by crash rather than by assertion, so the object behind the nester is counted -- if the process
/// survives, the count is the evidence.
TEST(ObjectReentrancy, ARemovalInsideANestedWalkRepairsTheOuterCursor)
{
  constexpr std::size_t cycles = 4U;
  const auto counts = runWithARemovalInsideANestedWalk(cycles);

  ASSERT_EQ(*counts.nestings, 1) << "no hook opened a nested walk, so only one cursor was ever "
                                    "registered and this test cannot see the defect";
  ASSERT_EQ(*counts.removals, 1) << "nothing was removed from preCommit(), so no node was freed "
                                    "under the outer cursor";

  // Exact: a walk that survived the removal and then lost later cycles is as wrong as one that died
  // on the spot, and `> 0` cannot tell them apart.
  EXPECT_EQ(CycleHookRemovalCounts::forHook(counts.after, RemovingOnCycleHook::Hook::update), static_cast<int>(cycles))
    << "the object behind the nester was not updated on every cycle, so the outer walk did not "
       "survive a removal taken inside the nested one";
}

/// @test
/// A removal addressed to a bus that never staged the object does not cancel the registration the
/// owning bus is in the middle of. Both buses share one runner, so the removal reaches
/// unregisterObjects and finds no registration -- which is also what a genuine member of a batch in
/// flight looks like. Only the batch's own member list separates the two, and without that check
/// the sibling is cancelled: published by its own participant next flush, never registered, never
/// driven, with a zero registration stamp that wins every name arbitration. The first object's
/// removal count confirms the hook ran.
TEST(ObjectReentrancy, ARemovalThroughAnotherBusDoesNotCancelAPendingRegistration)
{
  constexpr std::size_t cycles = 3U;
  const auto counts = runWithASiblingRemovedThroughAnotherBus(cycles);

  ASSERT_EQ(*counts.removalAttempts, 1) << "the hook never asked for the removal, so this test "
                                           "cannot see the defect";

  EXPECT_EQ(*counts.sibling, static_cast<int>(cycles))
    << "the sibling was never updated, so a removal sent to a bus that "
       "never staged it cancelled a registration its own bus had asked "
       "for";
}

/// @test
/// An object added from inside a cycle hook is driven in that same cycle. The adder is the only
/// object in the list, so it is the last one the walk visits -- the position where a walk that
/// advanced its cursor before calling the hook would be holding end() and would never reach the
/// node the hook appended. Reddens if the walks go back to advancing before the hook.
TEST(ObjectReentrancy, AnObjectAddedFromACycleHookIsDrivenInThatCycle)
{
  EXPECT_EQ(*updatesOfAnObjectAddedFromAHook(false), 1) << "an object added from the last walked "
                                                           "object's hook was not updated in the "
                                                           "cycle it was added in";
}

/// @test
/// The one shape that is still a cycle late: an object that removes itself and *then* adds, from
/// the last position. The removal steps the cursor onto the sentinel, and the object added after
/// that is linked in behind it. Pinned so the asymmetry is a recorded decision rather than a
/// surprise, and so that a later walk rewrite has to decide about it on purpose.
TEST(ObjectReentrancy, AnObjectAddedAfterTheAdderRemovedItselfWaitsACycle)
{
  EXPECT_EQ(*updatesOfAnObjectAddedFromAHook(true), 0) << "an add that follows a self-removal in the "
                                                          "same hook was driven in that cycle; the "
                                                          "walk comment says it is not";
}
