// === response_adapter.h ==============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_COMPONENTS_REST_SRC_RESPONSE_ADAPTER_H
#define SEN_COMPONENTS_REST_SRC_RESPONSE_ADAPTER_H

// sen
#include <sen/core/meta/type.h>
#include <sen/core/meta/var.h>

// std
#include <memory>

namespace sen::components::rest
{

void adaptForJsonResponse(sen::Var& var, const sen::Type* type);

}

#endif  // SEN_COMPONENTS_REST_SRC_RESPONSE_ADAPTER_H
