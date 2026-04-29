// === recorder_test.cpp ===============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

// components
#include "recorder_test_helpers.h"
#include "util.h"

// sen
#include "sen/core/base/numbers.h"
#include "sen/db/creation.h"
#include "sen/db/deletion.h"
#include "sen/db/event.h"
#include "sen/db/input.h"
#include "sen/db/property_change.h"

// generated code
#include "stl/sen/components/recorder/recorder.stl.h"
#include "stl/sen/db/basic_types.stl.h"

// google test
#include <gtest/gtest.h>

// std
#include <chrono>
#include <memory>
#include <variant>

namespace recorder::test
{

using sen::components::recorder::RecorderState;

/// @test
/// getLogger reuses the recorder logger instance
/// @requirements(SEN-364)
TEST(RecorderTest, GetLoggerReturnsSingletonInstance)
{
  const auto first = sen::components::recorder::getLogger();
  const auto second = sen::components::recorder::getLogger();

  ASSERT_NE(first, nullptr);
  EXPECT_EQ(first, second);
  EXPECT_EQ(first->name(), "recorder");
}

/// @test
/// A recorder that is not started reports empty stats and keeps the stopped state
/// @requirements(SEN-364)
TEST(RecorderTest, FetchStatsReturnsDefaultValueWhenStopped)
{
  RecorderSetup setup({"SELECT * FROM local.test"}, false);
  setup.step();

  ASSERT_NE(setup.recorder, nullptr);
  EXPECT_EQ(setup.recorder->getState(), RecorderState::stopped);
  EXPECT_EQ(setup.recorder->fetchStatsDirect().maxQueueSize, 0U);
}

/// @test
/// Starting the recorder fails when the selection does not define a source bus
/// @requirements(SEN-364)
TEST(RecorderTest, StartThrowsWhenSelectionHasNoSource)
{
  RecorderSetup setup({"SELECT *"}, false);
  setup.step();

  ASSERT_NE(setup.recorder, nullptr);
  EXPECT_ANY_THROW(setup.recorder->startDirect());
  EXPECT_EQ(setup.recorder->getState(), RecorderState::stopped);
}

/// @test
/// Auto-started recorders write creation and deletion entries for objects added after recording starts
/// @requirements(SEN-364)
TEST(RecorderTest, AutoStartRecordsCreationAndDeletionEntries)
{
  RecorderSetup setup;
  setup.step();

  ASSERT_NE(setup.recorder, nullptr);
  EXPECT_EQ(setup.recorder->getState(), RecorderState::recording);

  auto trackedObject = std::make_shared<TestObjImpl>("trackedObj", sen::VarMap {});
  setup.objectSource->add(trackedObject);
  setup.step(2);

  setup.objectSource->remove(trackedObject);
  setup.step(2);
  setup.recorder->stopDirect();
  sen::db::Input input(setup.archivePath.string(), setup.kernel->getTypes());

  bool foundCreation = false;
  bool foundDeletion = false;
  auto cursor = input.begin();

  while (!cursor.atEnd())
  {
    ++cursor;
    if (cursor.atEnd())
    {
      break;
    }

    const auto& entry = cursor.get();

    if (std::holds_alternative<sen::db::Creation>(entry.payload))
    {
      const auto& creation = std::get<sen::db::Creation>(entry.payload);
      const auto& snapshot = creation.getSnapshot();

      if (snapshot.getName() == "trackedObj")
      {
        EXPECT_EQ(snapshot.getSessionName(), "local");
        EXPECT_EQ(snapshot.getBusName(), "test");
        foundCreation = true;
      }
    }

    if (std::holds_alternative<sen::db::Deletion>(entry.payload))
    {
      const auto& deletion = std::get<sen::db::Deletion>(entry.payload);
      if (deletion.getObjectId() == trackedObject->getId())
      {
        foundDeletion = true;
      }
    }
  }

  EXPECT_TRUE(foundCreation) << "A creation entry should be written for the tracked object";
  EXPECT_TRUE(foundDeletion) << "A deletion entry should be written when the tracked object is removed";
}

/// @test
/// Recorder subscriptions write property changes and events from tracked objects
/// @requirements(SEN-364)
TEST(RecorderTest, RecordsPropertyChangesAndEvents)
{
  RecorderSetup setup;
  setup.step();

  ASSERT_NE(setup.recorder, nullptr);
  setup.object->setNextSpeed(250.0);
  setup.object->valueChanged(99.1);
  setup.step(2);
  setup.recorder->stopDirect();

  sen::db::Input input(setup.archivePath.string(), setup.kernel->getTypes());

  bool foundPropertyChange = false;
  bool foundEvent = false;
  auto cursor = input.begin();

  while (!cursor.atEnd())
  {
    ++cursor;
    if (cursor.atEnd())
    {
      break;
    }

    const auto& entry = cursor.get();

    if (std::holds_alternative<sen::db::PropertyChange>(entry.payload))
    {
      const auto& change = std::get<sen::db::PropertyChange>(entry.payload);
      EXPECT_EQ(change.getObjectId(), setup.object->getId());
      ASSERT_NE(change.getProperty(), nullptr);
      EXPECT_EQ(change.getProperty()->getName(), "speed");
      EXPECT_DOUBLE_EQ(change.getValueAsVariant().getCopyAs<float64_t>(), 250.0);
      foundPropertyChange = true;
    }

    if (std::holds_alternative<sen::db::Event>(entry.payload))
    {
      const auto& event = std::get<sen::db::Event>(entry.payload);
      EXPECT_EQ(event.getObjectId(), setup.object->getId());
      ASSERT_NE(event.getEvent(), nullptr);
      EXPECT_EQ(event.getEvent()->getName(), "valueChanged");

      const auto args = event.getArgsAsVariants();
      ASSERT_EQ(args.size(), 1U);
      EXPECT_DOUBLE_EQ(args[0].getCopyAs<float64_t>(), 99.1);
      foundEvent = true;
    }
  }

  EXPECT_TRUE(foundPropertyChange) << "A property change entry should be written for speed updates";
  EXPECT_TRUE(foundEvent) << "An event entry should be written for emitted events";
}

/// @test
/// Recorder updates emit periodic keyframes when a keyframe period is configured
/// @requirements(SEN-364)
TEST(RecorderTest, RecordsPeriodicKeyframes)
{
  RecorderSetup setup({"SELECT * FROM local.test"}, true, std::chrono::seconds(1));
  setup.step(3);

  ASSERT_NE(setup.recorder, nullptr);
  setup.recorder->stopDirect();

  sen::db::Input input(setup.archivePath.string(), setup.kernel->getTypes());
  const auto& summary = input.getSummary();

  EXPECT_EQ(summary.keyframeCount, 1U);
}

}  // namespace recorder::test
