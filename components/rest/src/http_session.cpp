// === http_session.cpp ================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "http_session.h"

// component
#include "base_router.h"
#include "http_header.h"
#include "http_response.h"
#include "utils.h"

// sen
#include "sen/core/base/assert.h"

// llhttp
#include "llhttp.h"

// asio
#include <asio/buffer.hpp>
#include <asio/error.hpp>
#include <asio/io_context.hpp>
#include <asio/ip/tcp.hpp>
#include <asio/post.hpp>
#include <asio/thread_pool.hpp>

// std
#include <cstddef>
#include <memory>
#include <string>
#include <system_error>
#include <tuple>
#include <utility>
#include <variant>

namespace sen::components::rest
{

//--------------------------------------------------------------------------------------------------------------
// Constants
//--------------------------------------------------------------------------------------------------------------

constexpr size_t maxPayloadSize = 1024;

//--------------------------------------------------------------------------------------------------------------
// HttpSession
//--------------------------------------------------------------------------------------------------------------

HttpSession::HttpSession(asio::io_context& ctx, asio::ip::tcp::socket socket, std::shared_ptr<BaseRouter> router)
  : ctx_(ctx), socket_(std::move(socket)), router_(std::move(router))
{
  llhttp_settings_init(&settings_);
  settings_.on_url = onURL;
  settings_.on_method = onMethod;
  settings_.on_message_complete = onMessageComplete;
  settings_.on_body = onBody;
  settings_.on_header_value = onHeaderValue;
  settings_.on_header_field = onHeaderField;
  llhttp_init(&parser_, HTTP_REQUEST, &settings_);
}

HttpSession::~HttpSession() { getLogger()->trace("HttpSession destroyed"); }

void HttpSession::start()
{
  SEN_ASSERT(socket_.is_open());

  auto self = shared_from_this();
  socket_.async_read_some(asio::buffer(data_, maxReadLength),
                          [self](std::error_code ec, std::size_t length)
                          {
                            if (ec)
                            {
                              getLogger()->error("HttpSession error on read {} {}", ec.value(), ec.message());
                              return;
                            }

                            asio::post(self->ctx_,
                                       [self, length]() mutable
                                       {
                                         self->parser_.data = &self;
                                         llhttp_execute(&self->parser_, self->data_.data(), length);
                                       });
                          });
}

int HttpSession::onMethod(llhttp_t* parser, const char* path, size_t nbytes)
{
  std::ignore = path;
  std::ignore = nbytes;

  if (!parser || !parser->data)
  {
    return -1;
  }

  auto session = *static_cast<std::shared_ptr<HttpSession>*>(parser->data);
  auto method = static_cast<llhttp_method_t>(llhttp_get_method(parser));
  session->request_.method_ += llhttp_method_name(method);

  return 0;
}

int HttpSession::onMessageComplete(llhttp_t* parser)
{
  if (!parser || !parser->data)
  {
    return -1;
  }

  auto session = *static_cast<std::shared_ptr<HttpSession>*>(parser->data);
  const auto method = fromString(session->request_.method_);
  if (!method.has_value())
  {
    return -1;
  }

  HttpResponse response;
  auto route = session->router_->matchPath(method.value(), session->request_.path_);
  if (route.has_value())
  {
    if (std::holds_alternative<RouteCallback>(route->callback))
    {
      response = std::get<RouteCallback>(route->callback)(*session, route.value().params);
    }
    else if (std::holds_alternative<StreamRouteCallback>(route->callback))
    {
      response = std::get<StreamRouteCallback>(route->callback)(session, route.value().params, session->takeSocket());
    }
  }

  if (session->socket_.is_open())
  {
    std::error_code ec = response.socketWrite(session->socket_);
    if (ec && ec != asio::error::broken_pipe && ec != asio::error::connection_reset && ec != asio::error::not_connected)
    {
      getLogger()->error(ec.message());
      return -1;
    }
  }

  return 0;
}

int HttpSession::onURL(llhttp_t* parser, const char* path, size_t nbytes)
{
  if (parser && parser->data)
  {
    auto session = *static_cast<std::shared_ptr<HttpSession>*>(parser->data);
    session->request_.path_ += std::string(path, nbytes);
    return 0;
  }
  return -1;
}

int HttpSession::onBody([[maybe_unused]] llhttp_t* parser, const char* data, size_t nbytes)
{
  if (parser && parser->data && nbytes < maxPayloadSize)
  {
    auto session = *static_cast<std::shared_ptr<HttpSession>*>(parser->data);
    session->request_.body_ += std::string(data, nbytes);
    return 0;
  }
  return -1;
}

int HttpSession::onHeaderField([[maybe_unused]] llhttp_t* parser, const char* data, size_t nbytes)
{
  if (parser && parser->data && nbytes > 0 && nbytes < HttpHeader::maxHeaderFieldLen)
  {
    auto session = *static_cast<std::shared_ptr<HttpSession>*>(parser->data);

    // Field names are case insensitive (https://www.rfc-editor.org/rfc/rfc9110.html#name-field-names)
    session->currentHttpField_ = tolower(std::string(data, nbytes));
    session->request_.headers_[session->currentHttpField_] = HttpHeader();

    return 0;
  }
  return -1;
}

int HttpSession::onHeaderValue([[maybe_unused]] llhttp_t* parser, const char* data, size_t nbytes)
{
  if (parser && parser->data && nbytes > 0 && nbytes < HttpHeader::maxHeaderValueLen)
  {
    auto session = *static_cast<std::shared_ptr<HttpSession>*>(parser->data);

    if (session->currentHttpField_.empty())
    {
      return -1;
    }

    std::string value(data, nbytes);
    session->request_.headers_[session->currentHttpField_] = HttpHeader(session->currentHttpField_, value);
    return 0;
  }
  return -1;
}

}  // namespace sen::components::rest
