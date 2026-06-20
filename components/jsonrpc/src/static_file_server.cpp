// === static_file_server.cpp ==========================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "static_file_server.h"

// local
#include "loaded_bundle.h"
#include "ws_server.h"

// sen
#include "sen/core/base/numbers.h"
#include "sen/core/base/result.h"

// generated
#include "stl/static_file_server.stl.h"

// uWebSockets
#include <HttpParser.h>
#include <HttpResponse.h>

// std
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace sen::components::jsonrpc
{

//--------------------------------------------------------------------------------------------------------------
// BundleRegistry
//--------------------------------------------------------------------------------------------------------------

sen::Result<u64, std::string> BundleRegistry::add(std::shared_ptr<const LoadedBundle> bundle)
{
  const auto prefix = bundle->urlPrefix();
  const std::unique_lock lock {mutex_};
  for (const auto& [_, existing]: bundles_)
  {
    if (existing->urlPrefix() == prefix)
    {
      return sen::Err("BundleRegistry: urlPrefix already registered: " + std::string {prefix});
    }
  }
  const auto id = nextId_++;
  bundles_.emplace(id, std::move(bundle));
  return sen::Ok(id);
}

bool BundleRegistry::remove(u64 bundleId)
{
  const std::unique_lock lock {mutex_};
  return bundles_.erase(bundleId) != 0U;
}

std::shared_ptr<const LoadedBundle> BundleRegistry::find(u64 bundleId) const
{
  const std::shared_lock lock {mutex_};
  const auto it = bundles_.find(bundleId);
  return it == bundles_.end() ? nullptr : it->second;
}

RegisteredBundleInfoList BundleRegistry::snapshot() const
{
  const std::shared_lock lock {mutex_};
  RegisteredBundleInfoList out;
  out.reserve(bundles_.size());
  for (const auto& [id, bundle]: bundles_)
  {
    out.push_back(RegisteredBundleInfo {id, std::string {bundle->urlPrefix()}, bundle->fileCount()});
  }
  return out;
}

//--------------------------------------------------------------------------------------------------------------
// Helpers
//--------------------------------------------------------------------------------------------------------------

namespace
{

/// Translates the inbound STL `StaticBundle` into a validated `LoadedBundle`. Throws on any
/// validation error so the failure surfaces as a `MethodResult.error()` to the Sen caller.
[[nodiscard]] LoadedBundle loadBundleOrThrow(const ::sen::components::jsonrpc::StaticBundle& wire)
{
  LoadedBundleInput input;
  input.urlPrefix = wire.urlPrefix;
  input.indexFileName = wire.indexFileName;
  input.files.reserve(wire.files.size());
  for (const auto& file: wire.files)
  {
    LoadedBundleInput::FileInput entry;
    entry.path = file.path;
    entry.contentType = file.contentType;
    entry.contents.assign(file.contents.begin(), file.contents.end());
    input.files.push_back(std::move(entry));
  }
  auto result = LoadedBundle::make(std::move(input));
  if (result.isError())
  {
    throw std::runtime_error(std::move(result).getError());
  }
  return std::move(result).getValue();
}

/// Builds a uWS GET handler that serves files from `bundleId` in `registry`. The handler
/// captures the registry by `shared_ptr` (so the route's lookup keeps working past the owning
/// `StaticFileServer`'s teardown) and the bundle id by value. After unregistration the registry
/// slot is null, so the handler falls through to 404.
void writeNotFound(uWS::HttpResponse<false>* res)
{
  res->writeStatus("404 Not Found")->writeHeader("Content-Type", "text/plain")->end("Not Found\n");
}

[[nodiscard]] HttpGetHandler makeBundleHandler(std::shared_ptr<BundleRegistry> registry, u64 bundleId)
{
  return [registry = std::move(registry), bundleId](uWS::HttpResponse<false>* res, uWS::HttpRequest* req)
  {
    auto bundle = registry->find(bundleId);
    if (!bundle)
    {
      writeNotFound(res);
      return;
    }
    auto normalized = stripAndNormalize(bundle->urlPrefix(), req->getUrl());
    if (!normalized)
    {
      writeNotFound(res);
      return;
    }

    const LoadedFile* file = bundle->find(*normalized);
    if (file == nullptr)
    {
      // SPA fallback: any path under the prefix that doesn't match a file serves the index.
      file = bundle->indexFile();
      if (file == nullptr)
      {
        writeNotFound(res);
        return;
      }
    }

    if (req->getHeader("if-none-match") == file->etag)
    {
      // RFC 7232 §4.1: 304 echoes ETag + Cache-Control so caches stay consistent across
      // 200/304 responses for the same resource.
      res->writeStatus("304 Not Modified")
        ->writeHeader("ETag", file->etag)
        ->writeHeader("Cache-Control", "no-cache")
        ->end();
      return;
    }

    res->writeHeader("Content-Type", file->contentType)
      ->writeHeader("ETag", file->etag)
      ->writeHeader("Cache-Control", "no-cache")
      ->end(file->contents);
  };
}

}  // namespace

//--------------------------------------------------------------------------------------------------------------
// StaticFileServer
//--------------------------------------------------------------------------------------------------------------

StaticFileServer::StaticFileServer(std::string name, WebSocketServer* httpRouter)
  : StaticFileServerBase(std::move(name)), httpRouter_(httpRouter)
{
}

u64 StaticFileServer::registerStaticBundleImpl(const ::sen::components::jsonrpc::StaticBundle& bundle)
{
  auto loaded = std::make_shared<LoadedBundle>(loadBundleOrThrow(bundle));
  const std::string urlPrefix {loaded->urlPrefix()};
  auto idResult = registry_->add(std::move(loaded));
  if (idResult.isError())
  {
    throw std::runtime_error(std::move(idResult).getError());
  }
  const u64 bundleId = idResult.getValue();
  publishRegisteredBundles();
  registerHttpRoutes(urlPrefix, bundleId);
  return bundleId;
}

void StaticFileServer::unregisterStaticBundleImpl(u64 bundleId)
{
  if (!registry_->remove(bundleId))
  {
    return;  // unknown id - silent no-op per the STL contract
  }
  publishRegisteredBundles();
  // The uWS route stays registered. Subsequent requests under the prefix hit the captured
  // `bundleId`, fail the registry lookup, and return 404 until either a re-register replaces
  // the route (uWS's `HttpRouter::add` removes the old entry first) or the server shuts down.
}

void StaticFileServer::publishRegisteredBundles() { setNextRegisteredBundles(registry_->snapshot()); }

void StaticFileServer::registerHttpRoutes(const std::string& urlPrefix, u64 bundleId)
{
  if (httpRouter_ == nullptr)
  {
    return;
  }
  // Two patterns so the bare prefix (`/explorer`), the empty path (`/explorer/`), and any sub-path
  // (`/explorer/foo`) all reach the handler. uWS's `*` wildcard requires a slash before it, so
  // `/explorer/*` does not match `/explorer` on its own.
  httpRouter_->registerHttpGet(urlPrefix, makeBundleHandler(registry_, bundleId));
  httpRouter_->registerHttpGet(urlPrefix + "/*", makeBundleHandler(registry_, bundleId));
}

}  // namespace sen::components::jsonrpc
