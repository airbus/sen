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
#include <cstddef>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

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

/// @test
/// A path under the prefix becomes the file name relative to the bundle.
TEST(StripAndNormalize, normalizesNormalPath)
{
  const auto out = stripAndNormalize(bundlePrefix, "/explorer/index.html");
  ASSERT_TRUE(out.has_value());
  EXPECT_EQ(*out, "index.html");
}

/// @test
/// The prefix with a trailing slash resolves to the empty path, which is the index.
TEST(StripAndNormalize, rootPathReturnsEmpty)
{
  const auto out = stripAndNormalize(bundlePrefix, "/explorer/");
  ASSERT_TRUE(out.has_value());
  EXPECT_EQ(*out, "");
}

/// @test
/// The bare prefix resolves to the empty path.
TEST(StripAndNormalize, prefixOnlyReturnsEmpty)
{
  const auto out = stripAndNormalize(bundlePrefix, "/explorer");
  ASSERT_TRUE(out.has_value());
  EXPECT_EQ(*out, "");
}

/// @test
/// A nested path keeps its directories.
TEST(StripAndNormalize, normalizesNestedPath)
{
  const auto out = stripAndNormalize(bundlePrefix, "/explorer/assets/main.js");
  ASSERT_TRUE(out.has_value());
  EXPECT_EQ(*out, "assets/main.js");
}

/// @test
/// A query string is dropped from the path.
TEST(StripAndNormalize, stripsQueryString)
{
  const auto out = stripAndNormalize(bundlePrefix, "/explorer/foo?bar=baz");
  ASSERT_TRUE(out.has_value());
  EXPECT_EQ(*out, "foo");
}

/// @test
/// A percent escape that is not a control character or a backslash is decoded.
TEST(StripAndNormalize, percentDecodesNormalEscapes)
{
  // %20 = space; allowed because it's not a control or backslash char.
  const auto out = stripAndNormalize(bundlePrefix, "/explorer/hello%20world");
  ASSERT_TRUE(out.has_value());
  EXPECT_EQ(*out, "hello world");
}

/// @test
/// Repeated slashes collapse to one.
TEST(StripAndNormalize, collapsesEmptySegments)
{
  const auto out = stripAndNormalize(bundlePrefix, "/explorer//foo//bar");
  ASSERT_TRUE(out.has_value());
  EXPECT_EQ(*out, "foo/bar");
}

/// @test
/// A dot segment is removed.
TEST(StripAndNormalize, collapsesDotSegments)
{
  const auto out = stripAndNormalize(bundlePrefix, "/explorer/./foo/./bar");
  ASSERT_TRUE(out.has_value());
  EXPECT_EQ(*out, "foo/bar");
}

//--------------------------------------------------------------------------------------------------------------
// stripAndNormalize: traversal rejection
//--------------------------------------------------------------------------------------------------------------

/// @test
/// A parent segment is refused, so a request cannot climb out of the bundle.
TEST(StripAndNormalize, rejectsParentSegment)
{
  EXPECT_FALSE(stripAndNormalize(bundlePrefix, "/explorer/../etc").has_value());
}

/// @test
/// A parent segment deeper in the path is refused too.
TEST(StripAndNormalize, rejectsParentSegmentNested)
{
  EXPECT_FALSE(stripAndNormalize(bundlePrefix, "/explorer/foo/../bar").has_value());
}

/// @test
/// A parent segment written with lowercase percent escapes is refused.
TEST(StripAndNormalize, rejectsLowercasePercentEncodedParent)
{
  EXPECT_FALSE(stripAndNormalize(bundlePrefix, "/explorer/%2e%2e/secret").has_value());
}

/// @test
/// A parent segment written with uppercase percent escapes is refused.
TEST(StripAndNormalize, rejectsUppercasePercentEncodedParent)
{
  EXPECT_FALSE(stripAndNormalize(bundlePrefix, "/explorer/%2E%2E/secret").has_value());
}

/// @test
/// A parent segment that only appears once empty segments collapse is still refused.
TEST(StripAndNormalize, parentAfterEmptyCollapseStillRejected)
{
  // After empty-segment collapse, `["..", "bar"]` still contains `..`; reject.
  EXPECT_FALSE(stripAndNormalize(bundlePrefix, "/explorer//../bar").has_value());
}

//--------------------------------------------------------------------------------------------------------------
// stripAndNormalize: dangerous characters
//--------------------------------------------------------------------------------------------------------------

/// @test
/// A backslash is refused, since it separates paths on Windows.
TEST(StripAndNormalize, rejectsRawBackslash)
{
  EXPECT_FALSE(stripAndNormalize(bundlePrefix, "/explorer/foo\\bar").has_value());
}

/// @test
/// A percent encoded backslash is refused, since it also separates paths on Windows.
TEST(StripAndNormalize, rejectsPercentEncodedBackslash)
{
  EXPECT_FALSE(stripAndNormalize(bundlePrefix, "/explorer/foo%5Cbar").has_value());
}

/// @test
/// A nul byte is refused.
TEST(StripAndNormalize, rejectsNulByte)
{
  EXPECT_FALSE(stripAndNormalize(bundlePrefix, "/explorer/foo%00bar").has_value());
}

/// @test
/// A control character is refused.
TEST(StripAndNormalize, rejectsControlCharacter)
{
  EXPECT_FALSE(stripAndNormalize(bundlePrefix, "/explorer/foo%01bar").has_value());
}

/// @test
/// A delete character is refused.
TEST(StripAndNormalize, rejectsDelCharacter)
{
  EXPECT_FALSE(stripAndNormalize(bundlePrefix, "/explorer/foo%7Fbar").has_value());
}

/// @test
/// A raw newline is refused, so a request cannot inject a header.
TEST(StripAndNormalize, rejectsRawNewlineInUrl)
{
  EXPECT_FALSE(stripAndNormalize(bundlePrefix, "/explorer/foo\nbar").has_value());
}

//--------------------------------------------------------------------------------------------------------------
// stripAndNormalize: prefix matching
//--------------------------------------------------------------------------------------------------------------

/// @test
/// A path under a different prefix is refused.
TEST(StripAndNormalize, rejectsPrefixMismatch) { EXPECT_FALSE(stripAndNormalize(bundlePrefix, "/api").has_value()); }

/// @test
/// A path whose prefix is only the start of a longer segment is refused.
TEST(StripAndNormalize, rejectsPrefixWithoutBoundary)
{
  EXPECT_FALSE(stripAndNormalize(bundlePrefix, "/explorerfoo").has_value());
}

/// @test
/// A path shorter than the prefix is refused.
TEST(StripAndNormalize, rejectsShortUrl) { EXPECT_FALSE(stripAndNormalize(bundlePrefix, "/expl").has_value()); }

//--------------------------------------------------------------------------------------------------------------
// stripAndNormalize: malformed input
//--------------------------------------------------------------------------------------------------------------

/// @test
/// A percent escape cut short is refused.
TEST(StripAndNormalize, rejectsTruncatedPercentEscape)
{
  EXPECT_FALSE(stripAndNormalize(bundlePrefix, "/explorer/foo%2").has_value());
}

/// @test
/// A percent escape whose digits are not hexadecimal is refused.
TEST(StripAndNormalize, rejectsNonHexPercentEscape)
{
  EXPECT_FALSE(stripAndNormalize(bundlePrefix, "/explorer/foo%2g").has_value());
}

//--------------------------------------------------------------------------------------------------------------
// LoadedBundle: construction
//--------------------------------------------------------------------------------------------------------------

/// @test
/// A bundle with a prefix, an index and files is built.
TEST(LoadedBundle, makesFromValidInput)
{
  auto bundle = makeBundle({{"index.html", "text/html", "<html></html>"}});
  EXPECT_EQ(bundle.urlPrefix(), "/explorer");
  EXPECT_EQ(bundle.indexFileName(), "index.html");
  EXPECT_EQ(bundle.fileCount(), 1U);
}

/// @test
/// A file in the bundle is found by its path.
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

/// @test
/// A path the bundle does not hold finds nothing.
TEST(LoadedBundle, lookupMisses)
{
  auto bundle = makeBundle({{"index.html", "text/html", "<html></html>"}});
  EXPECT_EQ(bundle.find("missing.js"), nullptr);
  EXPECT_EQ(bundle.find(""), nullptr);
}

/// @test
/// The index file is reachable as the named index.
TEST(LoadedBundle, indexFileLookupResolves)
{
  auto bundle = makeBundle({{"index.html", "text/html", "<html></html>"}});
  const auto* idx = bundle.indexFile();
  ASSERT_NE(idx, nullptr);
  EXPECT_EQ(idx->contentType, "text/html");
}

/// @test
/// Two files with the same contents carry the same etag.
TEST(LoadedBundle, etagIsStableForSameContents)
{
  auto bundle = makeBundle({{"a.txt", "text/plain", "hello"}, {"b.txt", "text/plain", "hello"}}, "a.txt");
  const auto* a = bundle.find("a.txt");
  const auto* b = bundle.find("b.txt");
  ASSERT_NE(a, nullptr);
  ASSERT_NE(b, nullptr);
  EXPECT_EQ(a->etag, b->etag);
}

/// @test
/// Two files with different contents carry different etags.
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

/// @test
/// An etag is hexadecimal inside double quotes, as the header requires.
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

/// @test
/// A bundle with an empty url prefix is refused.
TEST(LoadedBundle, makeRejectsEmptyPrefix)
{
  LoadedBundleInput input;
  input.urlPrefix = "";
  input.indexFileName = "index.html";
  input.files = {{"index.html", "text/html", ""}};
  EXPECT_TRUE(LoadedBundle::make(std::move(input)).isError());
}

/// @test
/// A url prefix without a leading slash is refused.
TEST(LoadedBundle, makeRejectsPrefixWithoutLeadingSlash)
{
  LoadedBundleInput input;
  input.urlPrefix = "explorer";
  input.indexFileName = "index.html";
  input.files = {{"index.html", "text/html", ""}};
  EXPECT_TRUE(LoadedBundle::make(std::move(input)).isError());
}

/// @test
/// A bundle whose named index is not among its files is refused.
TEST(LoadedBundle, makeRejectsMissingIndex)
{
  LoadedBundleInput input;
  input.urlPrefix = "/explorer";
  input.indexFileName = "main.html";
  input.files = {{"index.html", "text/html", ""}};
  EXPECT_TRUE(LoadedBundle::make(std::move(input)).isError());
}

/// @test
/// A bundle carrying the same path twice is refused.
TEST(LoadedBundle, makeRejectsDuplicatePath)
{
  LoadedBundleInput input;
  input.urlPrefix = "/explorer";
  input.indexFileName = "index.html";
  input.files = {{"index.html", "text/html", "first"}, {"index.html", "text/html", "second"}};
  EXPECT_TRUE(LoadedBundle::make(std::move(input)).isError());
}

/// @test
/// A bundle with an empty index name is refused.
TEST(LoadedBundle, makeRejectsEmptyIndexFileName)
{
  LoadedBundleInput input;
  input.urlPrefix = "/explorer";
  input.indexFileName = "";
  input.files = {{"index.html", "text/html", ""}};
  EXPECT_TRUE(LoadedBundle::make(std::move(input)).isError());
}

/// @test
/// A bundle holding a file with an empty path is refused.
TEST(LoadedBundle, makeRejectsEmptyFilePath)
{
  LoadedBundleInput input;
  input.urlPrefix = "/explorer";
  input.indexFileName = "index.html";
  input.files = {{"", "text/html", ""}};
  EXPECT_TRUE(LoadedBundle::make(std::move(input)).isError());
}

}  // namespace sen::components::jsonrpc::test
