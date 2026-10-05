// === constants.h =====================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

// std
#include <cstdint>
#include <string_view>

namespace sen::db
{

constexpr uint32_t keyframeIndexId = 0U;
constexpr uint32_t beginMagic = 0xcafebabeU;

// The archive layout this build reads and writes, stored in every file's v1::FileHeader. A new layout
// adds a value and a reader for it rather than raising this one, so older archives stay readable.
constexpr uint32_t supportedFileVersion = 1U;

constexpr std::string_view runtimeFileName = "runtime";
constexpr std::string_view typesFileName = "types";
constexpr std::string_view indexesFileName = "indexes";
constexpr std::string_view annotationsFileName = "annotations";
constexpr std::string_view summaryFileName = "summary";

}  // namespace sen::db
