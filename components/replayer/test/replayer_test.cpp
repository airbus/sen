// === replayer_test.cpp ===============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

// components
#include "replayer.h"
#include "replayer_test_helpers.h"

// sen
#include "sen/core/obj/object_source.h"
#include "sen/db/output.h"
#include "sen/kernel/component.h"
#include "sen/kernel/component_api.h"
#include "sen/kernel/test_kernel.h"

// generated code
#include "stl/sen/db/basic_types.stl.h"

// google test
#include <gtest/gtest.h>

// std
#include <chrono>
#include <cstddef>
#include <filesystem>
#include <memory>
#include <string>
#include <utility>

namespace replayer::test
{

class TestReplayerImpl final: public sen::components::replayer::ReplayerImpl
{
public:
  using ReplayerImpl::ReplayerImpl;

  TestReplayerImpl(const TestReplayerImpl&) = delete;
  TestReplayerImpl& operator=(const TestReplayerImpl&) = delete;
  TestReplayerImpl(TestReplayerImpl&&) = delete;
  TestReplayerImpl& operator=(TestReplayerImpl&&) = delete;
  ~TestReplayerImpl() override = default;

  void openDirect(const std::string& name, const std::string& path) { openImpl(name, path); }
  void closeDirect(const std::string& name) { closeImpl(name); }
  void closeAllDirect() { closeAllImpl(); }
};

struct ReplayerSetup
{
  TempDir tempDir;                                  // NOLINT(misc-non-private-member-variables-in-classes)
  std::shared_ptr<sen::ObjectSource> controlBus;    // NOLINT(misc-non-private-member-variables-in-classes)
  std::shared_ptr<TestReplayerImpl> replayer;       // NOLINT(misc-non-private-member-variables-in-classes)
  sen::kernel::TestComponent component;             // NOLINT(misc-non-private-member-variables-in-classes)
  std::unique_ptr<sen::kernel::TestKernel> kernel;  // NOLINT(misc-non-private-member-variables-in-classes)

  explicit ReplayerSetup(bool autoPlay = false)
  {
    component.onInit(
      [this](sen::kernel::InitApi&& api) -> sen::kernel::PassResult
      {
        controlBus = api.getSource("local.replayer");
        return sen::kernel::done();
      });

    component.onRun(
      [this, autoPlay](sen::kernel::RunApi& api)
      {
        replayer = std::make_shared<TestReplayerImpl>("test_replayer", autoPlay, controlBus, api);
        controlBus->add(replayer);
        return api.execLoop(std::chrono::seconds(1), []() {});
      });

    kernel = std::make_unique<sen::kernel::TestKernel>(&component);
  }

  ReplayerSetup(const ReplayerSetup&) = delete;
  ReplayerSetup& operator=(const ReplayerSetup&) = delete;
  ReplayerSetup(ReplayerSetup&&) = delete;
  ReplayerSetup& operator=(ReplayerSetup&&) = delete;

  ~ReplayerSetup()
  {
    if (controlBus != nullptr && replayer != nullptr)
    {
      replayer->closeAllDirect();
      controlBus->remove(replayer);
      step(2);
      replayer.reset();
    }
    controlBus.reset();
    kernel.reset();
  }

  void step(std::size_t count = 1) const { kernel->step(count); }

  [[nodiscard]] std::string createValidArchive(const std::string& name) const
  {
    const auto fullPath = makeArchivePath(name, tempDir);
    auto settings = makeArchiveSettings(name, tempDir);

    sen::db::Output output(std::move(settings), []() {});
    output.keyframe(kernel->getTime(), {});

    return fullPath.string();
  }
};

/// @test
/// Opening a non-exist recording throws error
/// requirements(SEN-364)
TEST(ReplayerTest, OpeningNonExistentRecordingThrows)
{
  ReplayerSetup setup;
  setup.step();

  ASSERT_NE(setup.replayer, nullptr);

  // try to open a non-existing path
  const std::string fakePath = (setup.tempDir.path() / "missing_dir").string();

  EXPECT_ANY_THROW(setup.replayer->openDirect("bad_session", fakePath));
}

/// @test
/// Opening a valid recording succeeds
/// requirements(SEN-364)
TEST(ReplayerTest, OpeningValidRecordingSucceeds)
{
  ReplayerSetup setup;
  setup.step();

  const std::string archivePath = setup.createValidArchive("valid_rec");

  EXPECT_NO_THROW(setup.replayer->openDirect("session_1", archivePath));
  EXPECT_NO_THROW(setup.replayer->closeDirect("session_1"));
}

/// @test
/// Opening with a duplicate name throws an error
/// requirements(SEN-364)
TEST(ReplayerTest, OpeningWithDuplicateNameThrows)
{
  ReplayerSetup setup;
  setup.step();

  const std::string archivePath1 = setup.createValidArchive("valid_rec1");
  const std::string archivePath2 = setup.createValidArchive("valid_rec2");

  setup.replayer->openDirect("session_same", archivePath1);
  EXPECT_ANY_THROW(setup.replayer->openDirect("session_same", archivePath2));
}

/// @test
/// Opening the same archive path twice throws an error
/// requirements(SEN-364)
TEST(ReplayerTest, OpeningWithDuplicatePathThrows)
{
  ReplayerSetup setup;
  setup.step();

  const std::string archivePath = setup.createValidArchive("valid_rec");

  setup.replayer->openDirect("session_1", archivePath);
  EXPECT_ANY_THROW(setup.replayer->openDirect("session_2", archivePath));
}

/// @test
/// Closing a non-existent replay throws an error
/// requirements(SEN-364)
TEST(ReplayerTest, ClosingNonExistentReplayThrows)
{
  ReplayerSetup setup;
  setup.step();

  EXPECT_ANY_THROW(setup.replayer->closeDirect("does_not_exist"));
}

/// @test
/// CloseAll completely clears all managed replays
/// requirements(SEN-364)
TEST(ReplayerTest, CloseAllClearsReplays)
{
  ReplayerSetup setup;
  setup.step();

  const std::string archivePath1 = setup.createValidArchive("valid_rec1");
  const std::string archivePath2 = setup.createValidArchive("valid_rec2");

  setup.replayer->openDirect("session_1", archivePath1);
  setup.replayer->openDirect("session_2", archivePath2);

  setup.replayer->closeAllDirect();

  // Now they can be reopened because they were cleared, and trying to close throws
  EXPECT_ANY_THROW(setup.replayer->closeDirect("session_1"));
  EXPECT_ANY_THROW(setup.replayer->closeDirect("session_2"));

  // Adding them again should not throw duplicate path/name errors
  EXPECT_NO_THROW(setup.replayer->openDirect("session_1", archivePath1));
}

/// @test
/// AutoPlay configuration does not crash when opening an archive
/// requirements(SEN-364)
TEST(ReplayerTest, AutoPlayDoesNotCrash)
{
  ReplayerSetup setup(true);
  setup.step();

  const std::string archivePath = setup.createValidArchive("auto_rec");
  EXPECT_NO_THROW(setup.replayer->openDirect("auto_session", archivePath));
}

}  // namespace replayer::test
