// === util.cpp ========================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "util.h"

// sen
#include "sen/core/base/hash32.h"

// inja
#include <inja/environment.hpp>
#include <inja/json.hpp>

// std
#include <filesystem>
#include <fstream>

void writeFile(std::filesystem::path path,
               const unsigned* templateData,
               unsigned templateDataSize,
               const inja::json& packageData)
{
  const auto templateStr = sen::decompressSymbolToString(templateData, templateDataSize);

  std::ofstream file(path);
  inja::render_to(file, templateStr, packageData);
}
