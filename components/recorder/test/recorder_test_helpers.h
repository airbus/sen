// === recorder_test_helpers.h =========================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_COMPONENTS_RECORDER_TEST_RECORDER_TEST_HELPERS_H
#define SEN_COMPONENTS_RECORDER_TEST_RECORDER_TEST_HELPERS_H

// components
#include "recorder.h"

// sen
#include "sen/core/base/compiler_macros.h"
#include "sen/core/base/duration.h"
#include "sen/core/base/uuid.h"
#include "sen/core/obj/object_source.h"
#include "sen/kernel/component.h"
#include "sen/kernel/component_api.h"
#include "sen/kernel/test_kernel.h"
#include "stl/sen/db/basic_types.stl.h"

// generated code
#include "stl/recorder_test_class.stl.h"
#include "stl/sen/components/recorder/configuration.stl.h"

// std
#include <chrono>
#include <cstddef>
#include <ctime>
#include <filesystem>
#include <memory>
#include <string>
#include <utility>

namespace recorder::test
{

class TempDir
{
public:
  TempDir(): path_(std::filesystem::temp_directory_path() / sen::UuidRandomGenerator()().toString())
  {
    std::filesystem::create_directories(path_);
  }

  TempDir(const TempDir&) = delete;
  TempDir& operator=(const TempDir&) = delete;
  TempDir(TempDir&&) = delete;
  TempDir& operator=(TempDir&&) = delete;

  ~TempDir()
  {
    if (std::filesystem::exists(path_))
    {
      std::filesystem::remove_all(path_);
    }
  }

  [[nodiscard]] const std::filesystem::path& path() const { return path_; }

private:
  std::filesystem::path path_;
};

class TestObjImpl final: public recorder_test::TestObjBase
{
public:
  SEN_NOCOPY_NOMOVE(TestObjImpl)
  using TestObjBase::TestObjBase;
  ~TestObjImpl() override = default;
  using TestObjBase::valueChanged;
};

class TestRecorderImpl final: public ::recorder::RecorderImpl
{
public:
  SEN_NOCOPY_NOMOVE(TestRecorderImpl)
  using RecorderImpl::RecorderImpl;
  ~TestRecorderImpl() override = default;

  void startDirect() { startImpl(); }
  void stopDirect() { stopImpl(); }
  [[nodiscard]] sen::db::OutStats fetchStatsDirect() { return fetchStatsImpl(); }
};

struct RecorderSetup
{
  TempDir tempDir;                                  // NOLINT(misc-non-private-member-variables-in-classes)
  std::shared_ptr<TestObjImpl> object;              // NOLINT(misc-non-private-member-variables-in-classes)
  std::shared_ptr<TestRecorderImpl> recorder;       // NOLINT(misc-non-private-member-variables-in-classes)
  std::shared_ptr<sen::ObjectSource> objectSource;  // NOLINT(misc-non-private-member-variables-in-classes)
  sen::kernel::TestComponent component;             // NOLINT(misc-non-private-member-variables-in-classes)
  std::unique_ptr<sen::kernel::TestKernel> kernel;  // NOLINT(misc-non-private-member-variables-in-classes)
  std::filesystem::path archivePath;                // NOLINT(misc-non-private-member-variables-in-classes)

  explicit RecorderSetup(sen::components::recorder::SelectionList selections = {"SELECT * FROM local.test"},
                         bool autoStart = true,
                         sen::Duration keyframePeriod = sen::Duration {0})
  {
    auto settings = sen::components::recorder::RecordingSettings {};
    settings.name = "recording";
    settings.folder = tempDir.path().string();
    settings.selections = std::move(selections);
    settings.keyframePeriod = keyframePeriod;
    settings.indexKeyframes = true;
    settings.indexObjects = true;
    settings.autoStart = autoStart;

    archivePath = tempDir.path() / settings.name;
    object = std::make_shared<TestObjImpl>("testObj", sen::VarMap {});

    component.onInit(
      [this](sen::kernel::InitApi&& api) -> sen::kernel::PassResult
      {
        objectSource = api.getSource("local.test");
        objectSource->add(object);
        return sen::kernel::done();
      });

    component.onRun(
      [settings = std::move(settings), this](sen::kernel::RunApi& api)
      {
        recorder = std::make_shared<TestRecorderImpl>(settings, api);
        auto recorderSource = api.getSource("local.recorder");
        recorderSource->add(recorder);
        return api.execLoop(std::chrono::seconds(1), []() {});
      });

    kernel = std::make_unique<sen::kernel::TestKernel>(&component);
  }

  ~RecorderSetup()
  {
    objectSource.reset();
    kernel.reset();
  }

  // prevent object copy and movable type
  RecorderSetup(const RecorderSetup&) = delete;
  RecorderSetup& operator=(const RecorderSetup&) = delete;
  RecorderSetup(RecorderSetup&&) = delete;
  RecorderSetup& operator=(RecorderSetup&&) = delete;

  void step(std::size_t count = 1) const { kernel->step(count); }

  void removeObject() const { objectSource->remove(object); }
};

}  // namespace recorder::test

#endif  // SEN_COMPONENTS_RECORDER_TEST_RECORDER_TEST_HELPERS_H
