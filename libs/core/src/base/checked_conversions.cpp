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
#include <iostream>
#include <string_view>

// The view is streamed whole rather than through data(): a string_view need not be NUL terminated,
// and the previous data() read ran past the end for one that is not.
void sen::std_util::reportToStandardError(std::string_view message) { std::cerr << message << std::endl; }

void sen::std_util::ReportPolicyLog::report(std::string_view message) { SPDLOG_WARN(message); }
