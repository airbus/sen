// === uses_contributed_attribute.cpp ==================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "hla/site.xml.h"

namespace
{

// Range comes from the FOM module, Bearing from the extension file beside it. Both accessors are
// named here, so this file does not compile unless the contributed attribute reached the class.
[[maybe_unused]] const ::hla::Metres& range(const ::hla::SensorInterface& sensor) { return sensor.getRange(); }

[[maybe_unused]] const ::hla::Metres& bearing(const ::hla::SensorInterface& sensor) { return sensor.getBearing(); }

}  // namespace
