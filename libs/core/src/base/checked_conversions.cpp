// === checked_conversions.cpp =========================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

// sen
#include "sen/core/base/checked_conversions.h"

// spdlog
#include <spdlog/spdlog.h>

// std
#include <string_view>

void sen::std_util::ReportPolicyLog::report(std::string_view message) { SPDLOG_WARN(message); }
