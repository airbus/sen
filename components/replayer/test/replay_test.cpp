// === replay_test.cpp =================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

// component
#include "replay.h"
#include "replayer_test_helpers.h"

// sen
#include "sen/core/base/compiler_macros.h"
#include "sen/core/base/duration.h"
#include "sen/core/base/timestamp.h"
#include "sen/core/obj/interest.h"
#include "sen/core/obj/object.h"
#include "sen/core/obj/object_list.h"
#include "sen/core/obj/object_source.h"
#include "sen/core/obj/subscription.h"
#include "sen/db/input.h"
#include "sen/db/output.h"
#include "sen/kernel/component.h"
#include "sen/kernel/component_api.h"
#include "sen/kernel/test_kernel.h"

// generated code
#include "stl/replay_test_class.stl.h"
#include "stl/sen/components/replayer/replayer.stl.h"
#include "stl/sen/db/basic_types.stl.h"

// google test
#include <gtest/gtest.h>

// std
#include <chrono>
#include <cstddef>
#include <filesystem>
#include <functional>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace replayer::test
{
using sen::components::replayer::ReplayStatus;

class TestReplayImpl final: public sen::components::replayer::ReplayImpl
{
public:
  SEN_NOCOPY_NOMOVE(TestReplayImpl)
  using ReplayImpl::ReplayImpl;
  ~TestReplayImpl() override = default;

  void playDirect() { playImpl(); }
  void pauseDirect() { pauseImpl(); }
  void stopDirect() { stopImpl(); }
  void advanceDirect(sen::Duration time) { advanceImpl(time); }
  void seekDirect(sen::TimeStamp time) { seekImpl(time); }
};

struct ReplaySetup
{
  SEN_NOCOPY_NOMOVE(ReplaySetup)
  TempDir tempDir;                                  // NOLINT(misc-non-private-member-variables-in-classes)
  std::shared_ptr<sen::ObjectSource> controlBus;    // NOLINT(misc-non-private-member-variables-in-classes)
  std::shared_ptr<TestReplayImpl> replay;           // NOLINT(misc-non-private-member-variables-in-classes)
  sen::kernel::TestComponent component;             // NOLINT(misc-non-private-member-variables-in-classes)
  std::unique_ptr<sen::kernel::TestKernel> kernel;  // NOLINT(misc-non-private-member-variables-in-classes)

  std::filesystem::path archivePath;  // NOLINT(misc-non-private-member-variables-in-classes)

  explicit ReplaySetup(const std::string& archiveName = "test_replay",
                       std::function<void(sen::db::Output&, sen::kernel::TestKernel&)> populateDb = nullptr)
  {
    archivePath = makeArchivePath(archiveName, tempDir);
    auto settings = makeArchiveSettings(archiveName, tempDir);

    {
      sen::db::Output output(std::move(settings), []() {});
      if (populateDb)
      {
        sen::kernel::TestComponent dummyComponent;
        sen::kernel::TestKernel dummyKernel(&dummyComponent);
        populateDb(output, dummyKernel);
      }
      else
      {
        sen::TimeStamp t(std::chrono::seconds(0));
        output.keyframe(t, {});
        t += std::chrono::seconds(10);
        output.keyframe(t, {});
        t += std::chrono::seconds(10);
        output.keyframe(t, {});
      }
    }

    component.onInit(
      [this](sen::kernel::InitApi&& api) -> sen::kernel::PassResult
      {
        controlBus = api.getSource("local.replay_test");
        return sen::kernel::done();
      });

    component.onRun(
      [this, archiveName](sen::kernel::RunApi& api)
      {
        auto input = std::make_unique<sen::db::Input>(archivePath.string(), api.getTypes());
        replay = std::make_shared<TestReplayImpl>(archiveName, archivePath.string(), std::move(input), api);
        controlBus->add(replay);
        return api.execLoop(std::chrono::seconds(1),
                            [this, &api]()
                            {
                              if (replay)
                              {
                                replay->update(api);
                              }
                            });
      });

    kernel = std::make_unique<sen::kernel::TestKernel>(&component);
  }

  ~ReplaySetup()
  {
    if (controlBus != nullptr && replay != nullptr)
    {
      replay->stopDirect();
      controlBus->remove(replay);

      step(2);
      replay.reset();
    }

    controlBus.reset();
    kernel.reset();
  }

  void step(std::size_t count = 1) const { kernel->step(count); }
};

void registerDummyReplayType(ReplaySetup& setup)
{
  setup.kernel->getTypes().add(replayer_test::DummyReplayObjBase::meta());
}

[[nodiscard]] TestReplayImpl& startReplay(ReplaySetup& setup)
{
  setup.step();
  if (!setup.replay)
  {
    throw std::runtime_error("startReplay: replay was not created");
  }
  return *setup.replay;
}

[[nodiscard]] sen::Subscription<sen::Object> observeReplayObjects(const ReplaySetup& setup)
{
  if (setup.controlBus == nullptr)
  {
    throw std::runtime_error("observeReplayObjects: control bus was not created");
  }

  sen::Subscription<sen::Object> subscription;
  subscription.attachTo(
    setup.controlBus, sen::Interest::make("SELECT * FROM local.replay_test", sen::CustomTypeRegistry()), false);
  return subscription;
}

[[nodiscard]] bool hasObjectNamed(const sen::ObjectList<sen::Object>& objects, std::string_view name)
{
  for (const auto* object: objects.getObjects())
  {
    if (object != nullptr && object->getName() == name)
    {
      return true;
    }
  }
  return false;
}

[[nodiscard]] std::size_t countObjectsNamed(const sen::ObjectList<sen::Object>& objects, std::string_view name)
{
  std::size_t count = 0;
  for (const auto* object: objects.getObjects())
  {
    if (object != nullptr && object->getName() == name)
    {
      ++count;
    }
  }
  return count;
}

/// @test
/// Transitions through playing, paused, and stopped on the matching calls, tolerates repeated
/// calls in the same state, and resets playback time to the start on stop after an advance
/// moved it forward.
/// @requirements(SEN-364)
TEST(ReplayTest, StopChangesStatusAndResetsTime)
{
  ReplaySetup setup;
  auto& replay = startReplay(setup);

  EXPECT_EQ(replay.getNextStatus(), ReplayStatus::stopped);

  auto initialTime = replay.getNextPlaybackTime();

  replay.playDirect();
  setup.step();
  EXPECT_EQ(replay.getNextStatus(), ReplayStatus::playing);
  EXPECT_NO_THROW(replay.playDirect());

  replay.pauseDirect();
  setup.step();
  EXPECT_EQ(replay.getNextStatus(), ReplayStatus::paused);
  EXPECT_NO_THROW(replay.pauseDirect());

  replay.advanceDirect(std::chrono::seconds(10));

  EXPECT_GT(replay.getNextPlaybackTime(), initialTime);

  replay.stopDirect();
  setup.step();
  EXPECT_EQ(replay.getNextStatus(), ReplayStatus::stopped);
  EXPECT_EQ(replay.getNextPlaybackTime(), initialTime);

  EXPECT_NO_THROW(replay.stopDirect());
}

/// @test
/// Advances playback time by the requested amount while stopped, ignores a manual advance
/// while playing, and pauses when an advance reaches the end of the archive.
/// @requirements(SEN-364)
TEST(ReplayTest, AdvanceForwardWhenPaused)
{
  ReplaySetup setup;
  auto& replay = startReplay(setup);

  auto initialTime = replay.getNextPlaybackTime();
  replay.advanceDirect(std::chrono::seconds(5));
  setup.step();

  EXPECT_EQ(replay.getNextPlaybackTime(), initialTime + std::chrono::seconds(5));

  replay.playDirect();
  setup.step();
  auto playingTime = replay.getNextPlaybackTime();

  replay.advanceDirect(std::chrono::seconds(10));
  setup.step();

  EXPECT_LT(replay.getNextPlaybackTime(), playingTime + std::chrono::seconds(5));

  ReplaySetup eofSetup;
  auto& eofReplay = startReplay(eofSetup);

  eofReplay.advanceDirect(std::chrono::seconds(40));
  EXPECT_EQ(eofReplay.getNextStatus(), ReplayStatus::paused);
}

/// @test
/// Seeks to a time inside the archive window so playback time lands exactly on the target,
/// and throws for a seek one hour past the end.
/// @requirements(SEN-364)
TEST(ReplayTest, SeekingToValidTimeUpdatesTime)
{
  ReplaySetup setup;
  auto& replay = startReplay(setup);

  auto initialTime = replay.getNextPlaybackTime();
  sen::TimeStamp seekTarget = initialTime;

  EXPECT_NO_THROW(replay.seekDirect(seekTarget));
  setup.step();

  EXPECT_EQ(replay.getNextPlaybackTime(), seekTarget);

  sen::TimeStamp outOfBounds = initialTime + std::chrono::hours(1);
  EXPECT_ANY_THROW(replay.seekDirect(outOfBounds));
}

/// @test
/// Seeks to a time lying between two indexed keyframes, playback time lands exactly on the
/// requested intermediate target.
/// @requirements(SEN-364)
TEST(ReplayTest, SeekToIntermediateTime)
{
  ReplaySetup setup("test_seek",
                    [](sen::db::Output& output, sen::kernel::TestKernel&)
                    {
                      auto t = makeTime(0);
                      output.keyframe(t, {});

                      auto dummyObj = std::make_shared<DummyReplayObjImpl>("dummy", sen::VarMap {});
                      auto info = makeObjectInfo(dummyObj, "local", "dummy_registry");

                      t += std::chrono::seconds(10);
                      output.creation(t, info, true);
                      output.propertyChange(t, dummyObj->getId(), firstPropertyId(*dummyObj), {});
                      output.keyframe(t, {info});

                      t += std::chrono::seconds(10);
                      output.keyframe(t, {info});
                    });

  registerDummyReplayType(setup);
  auto& replay = startReplay(setup);

  auto initialTime = replay.getNextPlaybackTime();

  sen::TimeStamp seekTarget = initialTime + std::chrono::seconds(15);

  EXPECT_NO_THROW(replay.seekDirect(seekTarget));
  setup.step();

  EXPECT_EQ(replay.getNextPlaybackTime(), seekTarget);
}

/// @test
/// Replaces the visible object set on each keyframe without duplicating a carried-over
/// object, tolerates a repeated creation of the same object, and honors its deletion.
/// @requirements(SEN-364)
TEST(ReplayTest, KeyframeHandling)
{
  ReplaySetup setup("test_keyframe",
                    [](sen::db::Output& output, sen::kernel::TestKernel&)
                    {
                      auto obj1 = std::make_shared<DummyReplayObjImpl>("obj1", sen::VarMap {});
                      auto info1 = makeObjectInfo(obj1, "local", "replay_test");
                      auto obj2 = std::make_shared<DummyReplayObjImpl>("obj2", sen::VarMap {});
                      auto info2 = makeObjectInfo(obj2, "local", "replay_test");

                      auto t = makeTime(0);
                      output.keyframe(t, {info1});

                      t += std::chrono::seconds(10);
                      output.keyframe(t, {info2});

                      t += std::chrono::seconds(10);
                      output.keyframe(t, {info2});
                    });

  registerDummyReplayType(setup);
  auto& replay = startReplay(setup);
  auto objects = observeReplayObjects(setup);

  EXPECT_FALSE(hasObjectNamed(objects.list, "obj1"));
  EXPECT_FALSE(hasObjectNamed(objects.list, "obj2"));

  replay.advanceDirect(std::chrono::seconds(10));
  setup.step();

  EXPECT_FALSE(hasObjectNamed(objects.list, "obj1"));
  EXPECT_TRUE(hasObjectNamed(objects.list, "obj2"));
  EXPECT_EQ(countObjectsNamed(objects.list, "obj2"), 1U);

  replay.advanceDirect(std::chrono::seconds(10));
  setup.step();

  EXPECT_FALSE(hasObjectNamed(objects.list, "obj1"));
  EXPECT_TRUE(hasObjectNamed(objects.list, "obj2"));
  EXPECT_EQ(countObjectsNamed(objects.list, "obj2"), 1U);

  ReplaySetup lifecycleSetup("test_lifecycle",
                             [](sen::db::Output& output, sen::kernel::TestKernel&)
                             {
                               auto obj1 = std::make_shared<DummyReplayObjImpl>("obj1", sen::VarMap {});
                               auto info1 = makeObjectInfo(obj1, "local", "replay_test");

                               auto t = makeTime(0);
                               output.creation(t, info1, true);

                               t += std::chrono::seconds(5);
                               output.creation(t, info1, true);

                               t += std::chrono::seconds(5);
                               output.deletion(t, obj1->getId());

                               t += std::chrono::seconds(5);
                               output.deletion(t, sen::ObjectId(999));
                             });

  registerDummyReplayType(lifecycleSetup);
  auto& lifecycleReplay = startReplay(lifecycleSetup);
  auto lifecycleObjects = observeReplayObjects(lifecycleSetup);
  lifecycleReplay.advanceDirect(std::chrono::seconds(1));

  lifecycleSetup.step(1);
  EXPECT_TRUE(hasObjectNamed(lifecycleObjects.list, "obj1"));

  lifecycleReplay.advanceDirect(std::chrono::seconds(10));
  lifecycleSetup.step(1);
  EXPECT_FALSE(hasObjectNamed(lifecycleObjects.list, "obj1"));
}

/// @test
/// Plays an archive whose property change and event entries target an already deleted object,
/// surviving them and pausing at the end of the archive.
/// @requirements(SEN-364)
TEST(ReplayTest, PayloadApplication)
{
  ReplaySetup setup("test_payloads",
                    [](sen::db::Output& output, sen::kernel::TestKernel&)
                    {
                      auto obj1 = std::make_shared<DummyReplayObjImpl>("obj1", sen::VarMap {});
                      auto info1 = makeObjectInfo(obj1, "local", "replay_test");
                      auto obj2 = std::make_shared<DummyReplayObjImpl>("obj2", sen::VarMap {});
                      auto info2 = makeObjectInfo(obj2, "local", "replay_test");

                      auto t = makeTime(0);
                      output.creation(t, info1, true);
                      output.creation(t, info2, true);

                      t += std::chrono::seconds(5);
                      output.propertyChange(t, obj1->getId(), firstPropertyId(*obj1), {});

                      t += std::chrono::seconds(5);
                      output.deletion(t, obj2->getId());

                      t += std::chrono::seconds(5);
                      output.propertyChange(t, obj2->getId(), firstPropertyId(*obj1), {});
                      output.event(t, obj2->getId(), firstEventId(*obj1), {});

                      t += std::chrono::seconds(5);
                    });

  registerDummyReplayType(setup);
  auto& replay = startReplay(setup);

  replay.advanceDirect(std::chrono::seconds(40));
  setup.step();

  EXPECT_EQ(replay.getNextStatus(), ReplayStatus::paused);
}

/// @test
/// Throws for a seek to a time inside the archive's span but not covered by any keyframe
/// index entry.
/// @requirements(SEN-364)
TEST(ReplayTest, SeekNoIndexThrows)
{
  ReplaySetup setup("test_no_index",
                    [](sen::db::Output& output, sen::kernel::TestKernel&)
                    {
                      auto obj1 = std::make_shared<DummyReplayObjImpl>("obj1", sen::VarMap {});
                      auto info1 = makeObjectInfo(obj1, "local", "replay_test");

                      auto t = makeTime(5);
                      output.creation(t, info1, false);

                      t = makeTime(20);
                      output.deletion(t, obj1->getId());
                    });

  registerDummyReplayType(setup);
  auto& replay = startReplay(setup);

  EXPECT_ANY_THROW(replay.seekDirect(makeTime(10)));
}

}  // namespace replayer::test
