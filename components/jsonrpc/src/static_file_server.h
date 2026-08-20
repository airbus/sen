// === static_file_server.h ============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_COMPONENTS_JSONRPC_STATIC_FILE_SERVER_H
#define SEN_COMPONENTS_JSONRPC_STATIC_FILE_SERVER_H

// local
#include "loaded_bundle.h"

// sen
#include "sen/core/base/compiler_macros.h"
#include "sen/core/base/result.h"

// generated
#include "stl/static_file_server.stl.h"

// std
#include <memory>
#include <shared_mutex>
#include <string>
#include <unordered_map>

namespace sen::components::jsonrpc
{

/// Server-wide registry of static-file bundles. Owned by `StaticFileServer`; exposed separately
/// so the upcoming uWS HTTP routes (Stage 4) can hold a `shared_ptr<const BundleRegistry>` and
/// look bundles up from the uWS thread without going through the kernel object.
///
/// Threading: `add` / `remove` run on the kernel run() thread (called from the meta-dispatch
/// path). `find` is read-only and may be called from any thread.
class BundleRegistry
{
  SEN_NOCOPY_NOMOVE(BundleRegistry)

public:
  BundleRegistry() = default;
  ~BundleRegistry() = default;

  /// Inserts `bundle` and returns its newly-assigned id, or `Err` if the bundle's `urlPrefix` is
  /// already in use by a registered bundle. Ids are monotonic starting at 1; 0 is reserved and
  /// never returned.
  [[nodiscard]] sen::Result<u64, std::string> add(std::shared_ptr<const LoadedBundle> bundle);

  /// Drops the slot at `bundleId`. Returns true if a slot was removed, false on unknown id.
  bool remove(u64 bundleId);

  /// Returns the bundle for `bundleId` or `nullptr` if unknown / removed.
  [[nodiscard]] std::shared_ptr<const LoadedBundle> find(u64 bundleId) const;

  /// Snapshot of (id, urlPrefix, fileCount) tuples for the `registeredBundles` property.
  [[nodiscard]] RegisteredBundleInfoList snapshot() const;

private:
  mutable std::shared_mutex mutex_;
  u64 nextId_ {1};
  std::unordered_map<u64, std::shared_ptr<const LoadedBundle>> bundles_;
};

class WebSocketServer;

/// Concrete `StaticFileServer` implementation. One instance per `jsonrpc` component, published
/// on the configurable control bus (`local.<controlBusName>`).
class StaticFileServer final: public StaticFileServerBase
{
  SEN_NOCOPY_NOMOVE(StaticFileServer)

public:
  /// `httpRouter` may be null in unit tests that exercise the registry without HTTP wiring; when
  /// non-null, each successful `registerStaticBundleImpl` call adds a uWS GET route that serves
  /// the bundle.
  StaticFileServer(std::string name, WebSocketServer* httpRouter);
  ~StaticFileServer() override = default;

  /// Stable handle the uWS routes hold. Outlives the `StaticFileServer` only via `shared_ptr`;
  /// routes that capture this pointer must be torn down before the server is.
  [[nodiscard]] std::shared_ptr<BundleRegistry> registry() const noexcept { return registry_; }

public:  // mirrors `Server` in jsonrpc/server.h: widens access on `*Impl` overrides so unit
         // tests can drive the synchronous path without going through the async kernel callback.
  [[nodiscard]] u64 registerStaticBundleImpl(const ::sen::components::jsonrpc::StaticBundle& bundle) final;
  void unregisterStaticBundleImpl(u64 bundleId) final;

private:
  /// Pushes a fresh snapshot into the `registeredBundles` property. Must run on the run() thread.
  void publishRegisteredBundles();

  /// Adds the per-bundle GET routes (`urlPrefix` + `urlPrefix/*`) on the uWS loop.
  void registerHttpRoutes(const std::string& urlPrefix, u64 bundleId);

  std::shared_ptr<BundleRegistry> registry_ {std::make_shared<BundleRegistry>()};
  WebSocketServer* httpRouter_ {nullptr};
};

}  // namespace sen::components::jsonrpc

#endif  // SEN_COMPONENTS_JSONRPC_STATIC_FILE_SERVER_H
