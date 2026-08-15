# === CMakeLists.txt ===================================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================

# Web explorer: the React + TypeScript SPA in `frontend/` plus the C++ component in `backend/`
# that bakes the production HTML bundle and registers it with the jsonrpc StaticFileServer at
# runtime, so the whole UI ships inside the Sen binary and the browser loads it on the same
# origin as the JSON-RPC WebSocket.

sen_internal_component_guard(NAME webexplorer)

add_subdirectory(frontend)
add_subdirectory(backend)
