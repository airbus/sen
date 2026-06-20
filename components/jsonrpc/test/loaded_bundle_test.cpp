// === loaded_bundle_test.cpp ==========================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

// component
#include "loaded_bundle.h"

// google test
#include <gtest/gtest.h>

// std
#include <string>
#include <utility>

namespace sen::components::jsonrpc::test
{

namespace
{

constexpr std::string_view bundlePrefix = "/explorer";

[[nodiscard]] LoadedBundle makeBundle(std::vector<LoadedBundleInput::FileInput> files,
                                      std::string indexFileName = "index.html")
{
  LoadedBundleInput input;
  input.urlPrefix = std::string {bundlePrefix};
  input.indexFileName = std::move(indexFileName);
  input.files = std::move(files);
  auto result = LoadedBundle::make(std::move(input));
  EXPECT_FALSE(result.isError()) << "make failed: " << (result.isError() ? result.getError() : "");
  return std::move(result).getValue();
}

}  // namespace

//--------------------------------------------------------------------------------------------------------------
// stripAndNormalize: happy paths
//--------------------------------------------------------------------------------------------------------------

TEST(StripAndNormalize, normalizesNormalPath)
{
  const auto out = stripAndNormalize(bundlePrefix, "/explorer/index.html");
  ASSERT_TRUE(out.has_value());
  EXPECT_EQ(*out, "index.html");
}

TEST(StripAndNormalize, rootPathReturnsEmpty)
{
  const auto out = stripAndNormalize(bundlePrefix, "/explorer/");
  ASSERT_TRUE(out.has_value());
  EXPECT_EQ(*out, "");
}

TEST(StripAndNormalize, prefixOnlyReturnsEmpty)
{
  const auto out = stripAndNormalize(bundlePrefix, "/explorer");
  ASSERT_TRUE(out.has_value());
  EXPECT_EQ(*out, "");
}

TEST(StripAndNormalize, normalizesNestedPath)
{
  const auto out = stripAndNormalize(bundlePrefix, "/explorer/assets/main.js");
  ASSERT_TRUE(out.has_value());
  EXPECT_EQ(*out, "assets/main.js");
}

TEST(StripAndNormalize, stripsQueryString)
{
  const auto out = stripAndNormalize(bundlePrefix, "/explorer/foo?bar=baz");
  ASSERT_TRUE(out.has_value());
  EXPECT_EQ(*out, "foo");
}

TEST(StripAndNormalize, percentDecodesNormalEscapes)
{
  // %20 = space; allowed because it's not a control or backslash char.
  const auto out = stripAndNormalize(bundlePrefix, "/explorer/hello%20world");
  ASSERT_TRUE(out.has_value());
  EXPECT_EQ(*out, "hello world");
}

TEST(StripAndNormalize, collapsesEmptySegments)
{
  const auto out = stripAndNormalize(bundlePrefix, "/explorer//foo//bar");
  ASSERT_TRUE(out.has_value());
  EXPECT_EQ(*out, "foo/bar");
}

TEST(StripAndNormalize, collapsesDotSegments)
{
  const auto out = stripAndNormalize(bundlePrefix, "/explorer/./foo/./bar");
  ASSERT_TRUE(out.has_value());
  EXPECT_EQ(*out, "foo/bar");
}

//--------------------------------------------------------------------------------------------------------------
// stripAndNormalize: traversal rejection
//--------------------------------------------------------------------------------------------------------------

TEST(StripAndNormalize, rejectsParentSegment)
{
  EXPECT_FALSE(stripAndNormalize(bundlePrefix, "/explorer/../etc").has_value());
}

TEST(StripAndNormalize, rejectsParentSegmentNested)
{
  EXPECT_FALSE(stripAndNormalize(bundlePrefix, "/explorer/foo/../bar").has_value());
}

TEST(StripAndNormalize, rejectsLowercasePercentEncodedParent)
{
  EXPECT_FALSE(stripAndNormalize(bundlePrefix, "/explorer/%2e%2e/secret").has_value());
}

TEST(StripAndNormalize, rejectsUppercasePercentEncodedParent)
{
  EXPECT_FALSE(stripAndNormalize(bundlePrefix, "/explorer/%2E%2E/secret").has_value());
}

TEST(StripAndNormalize, parentAfterEmptyCollapseStillRejected)
{
  // After empty-segment collapse, `["..", "bar"]` still contains `..`; reject.
  EXPECT_FALSE(stripAndNormalize(bundlePrefix, "/explorer//../bar").has_value());
}

//--------------------------------------------------------------------------------------------------------------
// stripAndNormalize: dangerous characters
//--------------------------------------------------------------------------------------------------------------

TEST(StripAndNormalize, rejectsRawBackslash)
{
  EXPECT_FALSE(stripAndNormalize(bundlePrefix, "/explorer/foo\\bar").has_value());
}

TEST(StripAndNormalize, rejectsPercentEncodedBackslash)
{
  EXPECT_FALSE(stripAndNormalize(bundlePrefix, "/explorer/foo%5Cbar").has_value());
}

TEST(StripAndNormalize, rejectsNulByte)
{
  EXPECT_FALSE(stripAndNormalize(bundlePrefix, "/explorer/foo%00bar").has_value());
}

TEST(StripAndNormalize, rejectsControlCharacter)
{
  EXPECT_FALSE(stripAndNormalize(bundlePrefix, "/explorer/foo%01bar").has_value());
}

TEST(StripAndNormalize, rejectsDelCharacter)
{
  EXPECT_FALSE(stripAndNormalize(bundlePrefix, "/explorer/foo%7Fbar").has_value());
}

TEST(StripAndNormalize, rejectsRawNewlineInUrl)
{
  EXPECT_FALSE(stripAndNormalize(bundlePrefix, "/explorer/foo\nbar").has_value());
}

//--------------------------------------------------------------------------------------------------------------
// stripAndNormalize: prefix matching
//--------------------------------------------------------------------------------------------------------------

TEST(StripAndNormalize, rejectsPrefixMismatch) { EXPECT_FALSE(stripAndNormalize(bundlePrefix, "/api").has_value()); }

TEST(StripAndNormalize, rejectsPrefixWithoutBoundary)
{
  EXPECT_FALSE(stripAndNormalize(bundlePrefix, "/explorerfoo").has_value());
}

TEST(StripAndNormalize, rejectsShortUrl) { EXPECT_FALSE(stripAndNormalize(bundlePrefix, "/expl").has_value()); }

//--------------------------------------------------------------------------------------------------------------
// stripAndNormalize: malformed input
//--------------------------------------------------------------------------------------------------------------

TEST(StripAndNormalize, rejectsTruncatedPercentEscape)
{
  EXPECT_FALSE(stripAndNormalize(bundlePrefix, "/explorer/foo%2").has_value());
}

TEST(StripAndNormalize, rejectsNonHexPercentEscape)
{
  EXPECT_FALSE(stripAndNormalize(bundlePrefix, "/explorer/foo%2g").has_value());
}

//--------------------------------------------------------------------------------------------------------------
// LoadedBundle: construction
//--------------------------------------------------------------------------------------------------------------

TEST(LoadedBundle, makesFromValidInput)
{
  auto bundle = makeBundle({{"index.html", "text/html", "<html></html>"}});
  EXPECT_EQ(bundle.urlPrefix(), "/explorer");
  EXPECT_EQ(bundle.indexFileName(), "index.html");
  EXPECT_EQ(bundle.fileCount(), 1U);
}

TEST(LoadedBundle, lookupHits)
{
  auto bundle = makeBundle(
    {{"index.html", "text/html", "<html></html>"}, {"assets/main.js", "application/javascript", "console.log(1);"}});
  const auto* file = bundle.find("assets/main.js");
  ASSERT_NE(file, nullptr);
  EXPECT_EQ(file->contentType, "application/javascript");
  EXPECT_EQ(file->contents, "console.log(1);");
  EXPECT_FALSE(file->etag.empty());
}

TEST(LoadedBundle, lookupMisses)
{
  auto bundle = makeBundle({{"index.html", "text/html", "<html></html>"}});
  EXPECT_EQ(bundle.find("missing.js"), nullptr);
  EXPECT_EQ(bundle.find(""), nullptr);
}

TEST(LoadedBundle, indexFileLookupResolves)
{
  auto bundle = makeBundle({{"index.html", "text/html", "<html></html>"}});
  const auto* idx = bundle.indexFile();
  ASSERT_NE(idx, nullptr);
  EXPECT_EQ(idx->contentType, "text/html");
}

TEST(LoadedBundle, etagIsStableForSameContents)
{
  auto bundle = makeBundle({{"a.txt", "text/plain", "hello"}, {"b.txt", "text/plain", "hello"}}, "a.txt");
  const auto* a = bundle.find("a.txt");
  const auto* b = bundle.find("b.txt");
  ASSERT_NE(a, nullptr);
  ASSERT_NE(b, nullptr);
  EXPECT_EQ(a->etag, b->etag);
}

TEST(LoadedBundle, etagDiffersForDifferentContents)
{
  auto bundle = makeBundle(
    {{"index.html", "text/html", "<html></html>"}, {"a.txt", "text/plain", "alpha"}, {"b.txt", "text/plain", "beta"}});
  const auto* a = bundle.find("a.txt");
  const auto* b = bundle.find("b.txt");
  ASSERT_NE(a, nullptr);
  ASSERT_NE(b, nullptr);
  EXPECT_NE(a->etag, b->etag);
}

TEST(LoadedBundle, etagIsQuotedHex)
{
  auto bundle = makeBundle({{"index.html", "text/html", "<html></html>"}});
  const auto* idx = bundle.indexFile();
  ASSERT_NE(idx, nullptr);
  ASSERT_GE(idx->etag.size(), 4U);
  EXPECT_EQ(idx->etag.front(), '"');
  EXPECT_EQ(idx->etag.back(), '"');
  for (std::size_t i = 1; i + 1 < idx->etag.size(); ++i)
  {
    const char c = idx->etag[i];
    EXPECT_TRUE((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f')) << "non-hex char in ETag: " << c;
  }
}

TEST(LoadedBundle, makeRejectsEmptyPrefix)
{
  LoadedBundleInput input;
  input.urlPrefix = "";
  input.indexFileName = "index.html";
  input.files = {{"index.html", "text/html", ""}};
  EXPECT_TRUE(LoadedBundle::make(std::move(input)).isError());
}

TEST(LoadedBundle, makeRejectsPrefixWithoutLeadingSlash)
{
  LoadedBundleInput input;
  input.urlPrefix = "explorer";
  input.indexFileName = "index.html";
  input.files = {{"index.html", "text/html", ""}};
  EXPECT_TRUE(LoadedBundle::make(std::move(input)).isError());
}

TEST(LoadedBundle, makeRejectsMissingIndex)
{
  LoadedBundleInput input;
  input.urlPrefix = "/explorer";
  input.indexFileName = "main.html";
  input.files = {{"index.html", "text/html", ""}};
  EXPECT_TRUE(LoadedBundle::make(std::move(input)).isError());
}

TEST(LoadedBundle, makeRejectsDuplicatePath)
{
  LoadedBundleInput input;
  input.urlPrefix = "/explorer";
  input.indexFileName = "index.html";
  input.files = {{"index.html", "text/html", "first"}, {"index.html", "text/html", "second"}};
  EXPECT_TRUE(LoadedBundle::make(std::move(input)).isError());
}

TEST(LoadedBundle, makeRejectsEmptyIndexFileName)
{
  LoadedBundleInput input;
  input.urlPrefix = "/explorer";
  input.indexFileName = "";
  input.files = {{"index.html", "text/html", ""}};
  EXPECT_TRUE(LoadedBundle::make(std::move(input)).isError());
}

TEST(LoadedBundle, makeRejectsEmptyFilePath)
{
  LoadedBundleInput input;
  input.urlPrefix = "/explorer";
  input.indexFileName = "index.html";
  input.files = {{"", "text/html", ""}};
  EXPECT_TRUE(LoadedBundle::make(std::move(input)).isError());
}

}  // namespace sen::components::jsonrpc::test
