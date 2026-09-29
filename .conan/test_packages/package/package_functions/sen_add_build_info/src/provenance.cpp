// === provenance.cpp ==================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// ======================================================================================================================

// Calls every provenance accessor component.h offers. Compiled into a target with a generated
// build_info.cpp and into one without, so both branches of the header's guard have to link.

#include <sen/kernel/component.h>

namespace
{

const char* const gitRef = sen::kernel::getGitRef();
const char* const gitHash = sen::kernel::getGitHash();
const char* const buildTime = sen::kernel::getBuildTime();
const sen::kernel::GitStatus gitStatus = sen::kernel::getGitStatus();

}  // namespace

// Exported, so nothing above is discarded before the link has to resolve it.
extern "C" bool senProvenanceResolves()
{
  return gitRef != nullptr && gitHash != nullptr && buildTime != nullptr && gitStatus != sen::kernel::GitStatus::clean;
}
