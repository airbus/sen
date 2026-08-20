// === auth.h ==========================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_COMPONENTS_JSONRPC_AUTH_H
#define SEN_COMPONENTS_JSONRPC_AUTH_H

// sen
#include "sen/core/base/compiler_macros.h"
#include "sen/core/base/result.h"

// generated
#include "stl/jsonrpc.stl.h"

// std
#include <string>
#include <string_view>

namespace sen::components::jsonrpc
{

/// Per-connection authentication seam invoked from the WebSocket upgrade callback.
class Authenticator
{
  SEN_NOCOPY_NOMOVE(Authenticator)

public:
  Authenticator() = default;
  virtual ~Authenticator() = default;

  /// Validates the upgrade `Authorization` header.
  [[nodiscard]] virtual sen::Result<Identity, std::string> verify(std::string_view authorizationHeader) const = 0;
};

/// Authenticator that accepts every connection as `anonymous`.
class NoAuth final: public Authenticator
{
public:
  [[nodiscard]] sen::Result<Identity, std::string> verify(std::string_view /*authorizationHeader*/) const override
  {
    return sen::Ok(Identity {"anonymous"});
  }
};

}  // namespace sen::components::jsonrpc

#endif  // SEN_COMPONENTS_JSONRPC_AUTH_H
