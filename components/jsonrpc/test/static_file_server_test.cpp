// === static_file_server_test.cpp =====================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

// component
#include "static_file_server.h"

// google test
#include <gtest/gtest.h>

// std
#include <memory>
#include <stdexcept>
#include <string>

namespace sen::components::jsonrpc::test
{

namespace
{

[[nodiscard]] StaticBundle makeWireBundle(std::string urlPrefix = "/explorer", std::string indexFileName = "index.html")
{
  StaticBundle wire;
  wire.urlPrefix = std::move(urlPrefix);
  wire.indexFileName = std::move(indexFileName);
  StaticFile file;
  file.path = "index.html";
  file.contentType = "text/html";
  const std::string body = "<html></html>";
  file.contents.assign(body.begin(), body.end());
  wire.files.push_back(std::move(file));
  return wire;
}

}  // namespace

//--------------------------------------------------------------------------------------------------------------
// BundleRegistry
//--------------------------------------------------------------------------------------------------------------

TEST(BundleRegistry, addReturnsMonotonicIdsStartingAtOne)
{
  BundleRegistry registry;
  LoadedBundleInput inputA;
  inputA.urlPrefix = "/a";
  inputA.indexFileName = "i.html";
  inputA.files.push_back({"i.html", "text/html", "a"});
  LoadedBundleInput inputB = inputA;
  inputB.urlPrefix = "/b";

  const auto idA =
    registry.add(std::make_shared<LoadedBundle>(LoadedBundle::make(std::move(inputA)).getValue())).getValue();
  const auto idB =
    registry.add(std::make_shared<LoadedBundle>(LoadedBundle::make(std::move(inputB)).getValue())).getValue();
  EXPECT_EQ(idA, 1U);
  EXPECT_EQ(idB, 2U);
}

TEST(BundleRegistry, findReturnsAddedBundle)
{
  BundleRegistry registry;
  LoadedBundleInput input;
  input.urlPrefix = "/a";
  input.indexFileName = "i.html";
  input.files.push_back({"i.html", "text/html", "a"});
  auto bundle = std::make_shared<LoadedBundle>(LoadedBundle::make(std::move(input)).getValue());
  const auto id = registry.add(bundle).getValue();

  const auto found = registry.find(id);
  ASSERT_NE(found, nullptr);
  EXPECT_EQ(found->urlPrefix(), "/a");
}

TEST(BundleRegistry, findReturnsNullForUnknownId)
{
  BundleRegistry registry;
  EXPECT_EQ(registry.find(0U), nullptr);
  EXPECT_EQ(registry.find(42U), nullptr);
}

TEST(BundleRegistry, removeClearsSlot)
{
  BundleRegistry registry;
  LoadedBundleInput input;
  input.urlPrefix = "/a";
  input.indexFileName = "i.html";
  input.files.push_back({"i.html", "text/html", "a"});
  const auto id =
    registry.add(std::make_shared<LoadedBundle>(LoadedBundle::make(std::move(input)).getValue())).getValue();
  registry.remove(id);
  EXPECT_EQ(registry.find(id), nullptr);
}

TEST(BundleRegistry, removeUnknownIsNoop) { BundleRegistry().remove(99U); }

TEST(BundleRegistry, idsArentReusedAfterRemoval)
{
  BundleRegistry registry;
  LoadedBundleInput input;
  input.urlPrefix = "/a";
  input.indexFileName = "i.html";
  input.files.push_back({"i.html", "text/html", "a"});
  const auto id1 =
    registry.add(std::make_shared<LoadedBundle>(LoadedBundle::make(std::move(input)).getValue())).getValue();
  registry.remove(id1);

  LoadedBundleInput input2;
  input2.urlPrefix = "/b";
  input2.indexFileName = "i.html";
  input2.files.push_back({"i.html", "text/html", "b"});
  const auto id2 =
    registry.add(std::make_shared<LoadedBundle>(LoadedBundle::make(std::move(input2)).getValue())).getValue();
  EXPECT_GT(id2, id1);
}

TEST(BundleRegistry, addRejectsDuplicateUrlPrefix)
{
  BundleRegistry registry;
  LoadedBundleInput inputA;
  inputA.urlPrefix = "/explorer";
  inputA.indexFileName = "i.html";
  inputA.files.push_back({"i.html", "text/html", "a"});
  ASSERT_FALSE(
    registry.add(std::make_shared<LoadedBundle>(LoadedBundle::make(std::move(inputA)).getValue())).isError());

  // Second add with the same urlPrefix must fail; the first registration stays intact.
  LoadedBundleInput inputB;
  inputB.urlPrefix = "/explorer";
  inputB.indexFileName = "i.html";
  inputB.files.push_back({"i.html", "text/html", "b"});
  auto duplicate = registry.add(std::make_shared<LoadedBundle>(LoadedBundle::make(std::move(inputB)).getValue()));
  EXPECT_TRUE(duplicate.isError());
}

TEST(BundleRegistry, addAcceptsSamePrefixAfterRemoval)
{
  BundleRegistry registry;
  LoadedBundleInput input;
  input.urlPrefix = "/explorer";
  input.indexFileName = "i.html";
  input.files.push_back({"i.html", "text/html", "a"});
  const auto id1 =
    registry.add(std::make_shared<LoadedBundle>(LoadedBundle::make(std::move(input)).getValue())).getValue();
  registry.remove(id1);

  LoadedBundleInput input2;
  input2.urlPrefix = "/explorer";
  input2.indexFileName = "i.html";
  input2.files.push_back({"i.html", "text/html", "b"});
  EXPECT_FALSE(
    registry.add(std::make_shared<LoadedBundle>(LoadedBundle::make(std::move(input2)).getValue())).isError());
}

//--------------------------------------------------------------------------------------------------------------
// StaticFileServer
//--------------------------------------------------------------------------------------------------------------

constexpr auto serverName = "static_file_server";

TEST(StaticFileServer, registerStoresBundleAndReturnsId)
{
  auto server = std::make_shared<StaticFileServer>(serverName, nullptr);
  const auto id = server->registerStaticBundleImpl(makeWireBundle());
  EXPECT_GT(id, 0U);

  const auto loaded = server->registry()->find(id);
  ASSERT_NE(loaded, nullptr);
  EXPECT_EQ(loaded->urlPrefix(), "/explorer");
  EXPECT_EQ(loaded->fileCount(), 1U);

  const auto* idx = loaded->indexFile();
  ASSERT_NE(idx, nullptr);
  EXPECT_EQ(idx->contentType, "text/html");
  EXPECT_EQ(idx->contents, "<html></html>");
  EXPECT_FALSE(idx->etag.empty());
}

TEST(StaticFileServer, unregisterClearsRegistrySlot)
{
  auto server = std::make_shared<StaticFileServer>(serverName, nullptr);
  const auto id = server->registerStaticBundleImpl(makeWireBundle());
  server->unregisterStaticBundleImpl(id);
  EXPECT_EQ(server->registry()->find(id), nullptr);
}

TEST(StaticFileServer, unregisterUnknownIsNoop)
{
  std::make_shared<StaticFileServer>(serverName, nullptr)->unregisterStaticBundleImpl(999U);
}

TEST(StaticFileServer, registerThrowsForInvalidUrlPrefix)
{
  auto server = std::make_shared<StaticFileServer>(serverName, nullptr);
  auto bundle = makeWireBundle("explorer");  // missing leading '/'
  EXPECT_THROW(static_cast<void>(server->registerStaticBundleImpl(bundle)), std::runtime_error);
}

TEST(StaticFileServer, registerThrowsWhenIndexNotInFiles)
{
  auto server = std::make_shared<StaticFileServer>(serverName, nullptr);
  auto bundle = makeWireBundle("/explorer", "missing.html");
  EXPECT_THROW(static_cast<void>(server->registerStaticBundleImpl(bundle)), std::runtime_error);
}

TEST(StaticFileServer, registerThrowsOnDuplicateUrlPrefix)
{
  auto server = std::make_shared<StaticFileServer>(serverName, nullptr);
  std::ignore = server->registerStaticBundleImpl(makeWireBundle("/explorer"));
  // Second registration with the same urlPrefix surfaces as a runtime_error so Sen's invoke
  // path exposes it to the caller as `MethodResult::error()`.
  EXPECT_THROW(static_cast<void>(server->registerStaticBundleImpl(makeWireBundle("/explorer"))), std::runtime_error);
}

TEST(StaticFileServer, nextRegisteredBundlesReflectsRegistration)
{
  auto server = std::make_shared<StaticFileServer>(serverName, nullptr);
  EXPECT_TRUE(server->getNextRegisteredBundles().empty());

  const auto id = server->registerStaticBundleImpl(makeWireBundle());
  const auto& list = server->getNextRegisteredBundles();
  ASSERT_EQ(list.size(), 1U);
  EXPECT_EQ(list[0].bundleId, id);
  EXPECT_EQ(list[0].urlPrefix, "/explorer");
  EXPECT_EQ(list[0].fileCount, 1U);

  server->unregisterStaticBundleImpl(id);
  EXPECT_TRUE(server->getNextRegisteredBundles().empty());
}

}  // namespace sen::components::jsonrpc::test
