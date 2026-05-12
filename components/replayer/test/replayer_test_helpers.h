// === replayer_test_helpers.h =========================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_COMPONENTS_REPLAYER_TEST_REPLAYER_TEST_HELPERS_H
#define SEN_COMPONENTS_REPLAYER_TEST_REPLAYER_TEST_HELPERS_H

// shared test helpers
#include "archive_test_helpers.h"

// generated code
#include "stl/replay_test_class.stl.h"

// sen
#include "sen/core/base/compiler_macros.h"

namespace replayer::test
{

using sen::test::firstEventId;
using sen::test::firstPropertyId;
using sen::test::makeArchivePath;
using sen::test::makeArchiveSettings;
using sen::test::makeObjectInfo;
using sen::test::makeTime;
using sen::test::TempDir;

class DummyReplayObjImpl: public replayer_test::DummyReplayObjBase
{
public:
  SEN_NOCOPY_NOMOVE(DummyReplayObjImpl)
  using DummyReplayObjBase::DummyReplayObjBase;
  ~DummyReplayObjImpl() override = default;
};

}  // namespace replayer::test

#endif  // SEN_COMPONENTS_REPLAYER_TEST_REPLAYER_TEST_HELPERS_H
