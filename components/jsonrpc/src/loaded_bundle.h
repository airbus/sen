// === loaded_bundle.h =================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_COMPONENTS_JSONRPC_LOADED_BUNDLE_H
#define SEN_COMPONENTS_JSONRPC_LOADED_BUNDLE_H

// sen
#include "sen/core/base/compiler_macros.h"
#include "sen/core/base/result.h"

// std
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace sen::components::jsonrpc
{

/// One file inside a `LoadedBundle`: raw bytes, MIME type, and a strong ETag computed at
/// construction. The ETag uses the `"<hex>"` form so it can be inlined directly into the
/// `ETag` / `If-None-Match` HTTP headers.
struct LoadedFile
{
  std::string path;
  std::string contentType;
  std::string etag;
  std::string contents;
};

/// Plain construction inputs for `LoadedBundle::make`. Decouples the data structure from the
/// generated STL types so the path-normalization tests don't need them.
struct LoadedBundleInput
{
  struct FileInput
  {
    std::string path;
    std::string contentType;
    std::string contents;
  };

  std::string urlPrefix;
  std::string indexFileName;
  std::vector<FileInput> files;
};

/// In-memory bundle of files served at `urlPrefix`. Distinct from the STL wire type
/// `sen::components::jsonrpc::StaticBundle` (the inbound manifest)
class LoadedBundle
{
  SEN_PRIVATE_TAG

public:
  /// Validates the input and builds a bundle.
  [[nodiscard]] static sen::Result<LoadedBundle, std::string> make(LoadedBundleInput input);

  /// use `make()`
  LoadedBundle(std::string urlPrefix, std::string indexFileName, std::vector<LoadedFile> files, Private notUsable);

public:
  /// O(log N) by-path lookup; returns nullptr if not present.
  [[nodiscard]] const LoadedFile* find(std::string_view pathInBundle) const;
  [[nodiscard]] const LoadedFile* indexFile() const;
  [[nodiscard]] std::string_view urlPrefix() const noexcept { return urlPrefix_; }
  [[nodiscard]] std::string_view indexFileName() const noexcept { return indexFileName_; }
  [[nodiscard]] std::size_t fileCount() const noexcept { return files_.size(); }

private:
  std::string urlPrefix_;
  std::string indexFileName_;
  std::vector<LoadedFile> files_;  // sorted by path; immutable post-make
};

/// Strips `prefix` from `url`, percent-decodes the remainder, and returns the path within the bundle.
[[nodiscard]] std::optional<std::string> stripAndNormalize(std::string_view prefix, std::string_view url);

}  // namespace sen::components::jsonrpc

#endif  // SEN_COMPONENTS_JSONRPC_LOADED_BUNDLE_H
