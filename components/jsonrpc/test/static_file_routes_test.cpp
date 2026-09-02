// === static_file_routes_test.cpp =====================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

// local
#include "auth.h"
#include "messages.h"
#include "static_file_server.h"
#include "ws_server.h"

// sen
#include "sen/core/base/numbers.h"

// generated
#include "stl/static_file_server.stl.h"

// asio
#include <asio/buffer.hpp>
#include <asio/connect.hpp>  // NOLINT(misc-include-cleaner)
#include <asio/error.hpp>
#include <asio/io_context.hpp>
#include <asio/ip/tcp.hpp>
#include <asio/write.hpp>  // NOLINT(misc-include-cleaner)

// google test
#include <gtest/gtest.h>

// std
#include <array>
#include <cctype>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <thread>
#include <utility>

namespace sen::components::jsonrpc::test
{

namespace
{

const NoAuth defaultAuthenticator {};

constexpr auto routeReadinessPollInterval = std::chrono::milliseconds(5);
constexpr auto routeReadinessTimeout = std::chrono::seconds(2);

struct HttpResponse
{
  int statusCode {0};
  std::string statusText;
  std::string headers;  // raw header block, lowercased keys + values, one per line
  std::string body;
};

[[nodiscard]] std::string toLower(std::string s)
{
  for (auto& c: s)
  {
    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  }
  return s;
}

/// Minimal HTTP/1.1 client: opens a connection, writes the request, reads until EOF (the server
/// closes after the response because we send `Connection: close`). Parses status line and splits
/// the body. Headers are kept as a single lowercased block for substring matching.
[[nodiscard]] HttpResponse httpGet(const std::string& host,
                                   uint16_t port,
                                   const std::string& path,
                                   const std::string& extraHeaders = {})
{
  asio::io_context ctx;
  asio::ip::tcp::socket socket {ctx};
  asio::ip::tcp::resolver resolver {ctx};
  asio::connect(socket,
                resolver.resolve(host, std::to_string(port)));  // NOLINT(misc-include-cleaner): asio's public header is
                                                                // included; the check maps the symbol to an impl header

  std::ostringstream req;
  req << "GET " << path << " HTTP/1.1\r\n"
      << "Host: " << host << ":" << port << "\r\n"
      << "Connection: close\r\n"
      << extraHeaders << "\r\n";
  const auto reqStr = req.str();
  asio::write(socket, asio::buffer(reqStr));  // NOLINT(misc-include-cleaner): asio's public header is included; the
                                              // check maps the symbol to an impl header

  std::string raw;
  std::array<char, 1024> buf {};
  std::error_code ec;
  while (true)
  {
    const std::size_t n = socket.read_some(asio::buffer(buf), ec);
    if (n > 0)
    {
      raw.append(buf.data(), n);
    }
    if (ec == asio::error::eof || ec)
    {
      break;
    }
  }

  HttpResponse out;
  const auto headerEnd = raw.find("\r\n\r\n");
  std::string headerBlock = headerEnd == std::string::npos ? raw : raw.substr(0, headerEnd);
  out.body = headerEnd == std::string::npos ? "" : raw.substr(headerEnd + 4);

  const auto firstLineEnd = headerBlock.find("\r\n");
  const auto statusLine = headerBlock.substr(0, firstLineEnd);
  // "HTTP/1.1 200 OK"
  const auto firstSpace = statusLine.find(' ');
  const auto secondSpace = statusLine.find(' ', firstSpace + 1);
  out.statusCode = std::stoi(statusLine.substr(firstSpace + 1, secondSpace - firstSpace - 1));
  out.statusText = secondSpace == std::string::npos ? "" : statusLine.substr(secondSpace + 1);
  out.headers = toLower(firstLineEnd == std::string::npos ? "" : headerBlock.substr(firstLineEnd + 2));
  return out;
}

[[nodiscard]] StaticBundle makeWireBundle(std::string urlPrefix = "/explorer")
{
  StaticBundle wire;
  wire.urlPrefix = std::move(urlPrefix);
  wire.indexFileName = "index.html";

  StaticFile index;
  index.path = "index.html";
  index.contentType = "text/html";
  const std::string indexBody = "<!doctype html><title>idx</title>";
  index.contents.assign(indexBody.begin(), indexBody.end());
  wire.files.push_back(std::move(index));

  StaticFile asset;
  asset.path = "assets/main.js";
  asset.contentType = "application/javascript";
  const std::string assetBody = "console.log('hi');";
  asset.contents.assign(assetBody.begin(), assetBody.end());
  wire.files.push_back(std::move(asset));

  return wire;
}

/// Owns the queues + servers for one test. Construction blocks until at least one route returns
/// 200 for the bundle's index, so subsequent requests don't race the deferred `app.get()` call.
class StaticFileFixture
{
public:
  StaticFileFixture()
  {
    auto serverResult = WebSocketServer::make("127.0.0.1", 0, inboundQueue_, outboundQueue_, defaultAuthenticator);
    if (serverResult.isError())
    {
      throw std::runtime_error("StaticFileFixture: WebSocketServer::make failed: " + serverResult.getError());
    }
    server_ = std::move(serverResult).getValue();
    staticFileServer_ = std::make_shared<StaticFileServer>("static_file_server", server_.get());
    bundleId_ = staticFileServer_->registerStaticBundleImpl(makeWireBundle());
    waitForRoute();
  }

  [[nodiscard]] uint16_t port() const noexcept { return server_->getPort(); }
  [[nodiscard]] u64 bundleId() const noexcept { return bundleId_; }
  [[nodiscard]] StaticFileServer& staticFileServer() noexcept { return *staticFileServer_; }

private:
  /// `registerHttpGet` defers the actual `app.get()` to the next loop tick; without this poll the
  /// first request may land before the route exists and get a 426 from the WebSocket fallback.
  void waitForRoute()
  {
    const auto deadline = std::chrono::steady_clock::now() + routeReadinessTimeout;
    while (std::chrono::steady_clock::now() < deadline)
    {
      const auto resp = httpGet("127.0.0.1", port(), "/explorer/index.html");
      if (resp.statusCode == 200)
      {
        return;
      }
      std::this_thread::sleep_for(routeReadinessPollInterval);
    }
    throw std::runtime_error("StaticFileFixture: route never became ready");
  }

  InboundQueue inboundQueue_;
  OutboundQueue outboundQueue_;
  std::unique_ptr<WebSocketServer> server_;
  std::shared_ptr<StaticFileServer> staticFileServer_;
  u64 bundleId_ {0};
};

[[nodiscard]] std::string headerValue(const std::string& lowercasedHeaders, std::string_view key)
{
  // Headers come in lowercased and \r\n-separated. Look for "<key>: " and return up to the next
  // \r\n. Trims trailing whitespace.
  std::string needle {key};
  needle += ": ";
  const auto pos = lowercasedHeaders.find(needle);
  if (pos == std::string::npos)
  {
    return {};
  }
  const auto valueStart = pos + needle.size();
  const auto valueEnd = lowercasedHeaders.find("\r\n", valueStart);
  return lowercasedHeaders.substr(valueStart, valueEnd - valueStart);
}

}  // namespace

//--------------------------------------------------------------------------------------------------------------
// Happy paths
//--------------------------------------------------------------------------------------------------------------

TEST(StaticFileRoutes, servesIndexAtNestedPath)
{
  StaticFileFixture f;
  const auto resp = httpGet("127.0.0.1", f.port(), "/explorer/index.html");
  EXPECT_EQ(resp.statusCode, 200);
  EXPECT_EQ(resp.body, "<!doctype html><title>idx</title>");
  EXPECT_EQ(headerValue(resp.headers, "content-type"), "text/html");
  EXPECT_FALSE(headerValue(resp.headers, "etag").empty());
  EXPECT_EQ(headerValue(resp.headers, "cache-control"), "no-cache");
}

TEST(StaticFileRoutes, servesAsset)
{
  StaticFileFixture f;
  const auto resp = httpGet("127.0.0.1", f.port(), "/explorer/assets/main.js");
  EXPECT_EQ(resp.statusCode, 200);
  EXPECT_EQ(resp.body, "console.log('hi');");
  EXPECT_EQ(headerValue(resp.headers, "content-type"), "application/javascript");
}

TEST(StaticFileRoutes, servesIndexAtBundleRoot)
{
  StaticFileFixture f;
  const auto resp = httpGet("127.0.0.1", f.port(), "/explorer/");
  EXPECT_EQ(resp.statusCode, 200);
  EXPECT_EQ(resp.body, "<!doctype html><title>idx</title>");
}

TEST(StaticFileRoutes, servesIndexAtBareUrlPrefix)
{
  StaticFileFixture f;
  const auto resp = httpGet("127.0.0.1", f.port(), "/explorer");
  EXPECT_EQ(resp.statusCode, 200);
  EXPECT_EQ(resp.body, "<!doctype html><title>idx</title>");
}

TEST(StaticFileRoutes, spaFallbackServesIndexForUnknownPath)
{
  StaticFileFixture f;
  const auto resp = httpGet("127.0.0.1", f.port(), "/explorer/some/client-route");
  EXPECT_EQ(resp.statusCode, 200);
  EXPECT_EQ(resp.body, "<!doctype html><title>idx</title>");
}

//--------------------------------------------------------------------------------------------------------------
// Caching
//--------------------------------------------------------------------------------------------------------------

TEST(StaticFileRoutes, ifNoneMatchReturns304)
{
  StaticFileFixture f;
  const auto first = httpGet("127.0.0.1", f.port(), "/explorer/index.html");
  ASSERT_EQ(first.statusCode, 200);
  const auto etag = headerValue(first.headers, "etag");
  ASSERT_FALSE(etag.empty());

  const auto second = httpGet("127.0.0.1", f.port(), "/explorer/index.html", "If-None-Match: " + etag + "\r\n");
  EXPECT_EQ(second.statusCode, 304);
  EXPECT_EQ(second.body, "");
  EXPECT_EQ(headerValue(second.headers, "etag"), etag);
}

//--------------------------------------------------------------------------------------------------------------
// Unregister - the captured bundleId becomes inert
//--------------------------------------------------------------------------------------------------------------

TEST(StaticFileRoutes, unregisterMakesRouteReturn404)
{
  StaticFileFixture f;
  f.staticFileServer().unregisterStaticBundleImpl(f.bundleId());

  const auto resp = httpGet("127.0.0.1", f.port(), "/explorer/index.html");
  EXPECT_EQ(resp.statusCode, 404);
}

/// @test
/// register -> unregister -> register-with-the-same-urlPrefix succeeds and serves the new bundle.
/// uWS's `HttpRouter::add` replaces a route at the same pattern, so the inert handler captured
/// by the first registration is dropped when the second registers.
TEST(StaticFileRoutes, registerSamePrefixAfterUnregisterServesNewBundle)
{
  StaticFileFixture f;
  f.staticFileServer().unregisterStaticBundleImpl(f.bundleId());

  StaticBundle replacement;
  replacement.urlPrefix = "/explorer";
  replacement.indexFileName = "index.html";
  StaticFile newIndex;
  newIndex.path = "index.html";
  newIndex.contentType = "text/html";
  const std::string newBody = "<!doctype html><title>v2</title>";
  newIndex.contents.assign(newBody.begin(), newBody.end());
  replacement.files.push_back(std::move(newIndex));
  std::ignore = f.staticFileServer().registerStaticBundleImpl(replacement);

  // Loop until the new route lands; the registration is deferred to the uWS loop.
  HttpResponse resp;
  for (int i = 0; i < 50; ++i)
  {
    resp = httpGet("127.0.0.1", f.port(), "/explorer/index.html");
    if (resp.statusCode == 200 && resp.body == newBody)
    {
      break;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
  }
  EXPECT_EQ(resp.statusCode, 200);
  EXPECT_EQ(resp.body, newBody);
}

}  // namespace sen::components::jsonrpc::test
