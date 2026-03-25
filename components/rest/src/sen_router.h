// === sen_router.h ====================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_COMPONENTS_REST_SRC_SEN_ROUTER_H
#define SEN_COMPONENTS_REST_SRC_SEN_ROUTER_H

// component
#include "base_router.h"
#include "client_session.h"
#include "http_response.h"
#include "http_session.h"
#include "json_response.h"
#include "jwt.h"
#include "locators.h"
#include "object_interests_manager.h"

// generated code
#include "stl/types.stl.h"

// sen
#include "sen/core/base/compiler_macros.h"
#include "sen/core/obj/object.h"
#include "sen/kernel/component_api.h"

// asio
#include <asio/ip/tcp.hpp>

// std
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>

namespace sen::components::rest
{

/// SenRouter class is a specialized router for handling
/// HTTP requests and exposing Sen resources to clients
class SenRouter: public BaseRouter
{
  SEN_NOCOPY_NOMOVE(SenRouter)

public:
  explicit SenRouter(kernel::RunApi& api);
  ~SenRouter() override;

  [[nodiscard]] std::optional<std::shared_ptr<ClientSession>> getClientSessionFromToken(const JWT& token) const;

  void releaseAll() override;

private:
  /// Authorize and returns a new client session token for the requesting client.
  /// Token is returned as a cookie and also as the response body.
  HttpResponse clientAuthSessionHandler(HttpSession& httpSession, const UrlParams& urlParams);

  /// Returns all the available Sen sessions.
  JsonResponse getSessionsHandler(HttpSession& httpSession, const UrlParams& urlParams) const;

  /// Returns all the available objects for a given interest.
  JsonResponse getObjectsHandler(ClientSession& clientSession,
                                 HttpSession& httpSession,
                                 const UrlParams& urlParams) const;

  /// Returns all the information of a given object for a given interest.
  /// It also returns all the links to other resources such as URL for methods and properties.
  JsonResponse getObjectHandler(ClientSession& clientSession,
                                HttpSession& httpSession,
                                const UrlParams& urlParams) const;

  /// Returns all the information of a Sen object property for a given interest.
  JsonResponse getPropertyHandler(ClientSession& clientSession,
                                  HttpSession& httpSession,
                                  const UrlParams& urlParams) const;

  /// Subscribe to property updates of a Sen object for a given interest.
  /// Notifications related to the subscription will be returned as SSE (server-sent-events) on the notifications
  /// endpoint.
  JsonResponse subscribePropertyUpdateHandler(ClientSession& clientSession,
                                              HttpSession& httpSession,
                                              const UrlParams& urlParams) const;

  /// Unsubscribe from property updates of a Sen object for a given interest.
  JsonResponse unsubscribePropertyUpdateHandler(ClientSession& clientSession,
                                                HttpSession& httpSession,
                                                const UrlParams& urlParams) const;

  /// Returns the definition of a Sen object method for a given interest.
  JsonResponse getMethodDefinitionHandler(ClientSession& clientSession,
                                          HttpSession& httpSession,
                                          const UrlParams& urlParams) const;

  /// Invokes a Sen object method. It will return the invocation result if it is a setter or a getter,
  /// or asynchronously as a SSE notification otherwise.
  JsonResponse invokeMethodHandler(ClientSession& clientSession,
                                   HttpSession& httpSession,
                                   const UrlParams& urlParams) const;

  /// Returns the status of a Sen object method invocation.
  JsonResponse getInvokeMethodStatusHandler(ClientSession& clientSession,
                                            HttpSession& httpSession,
                                            const UrlParams& urlParams) const;

  /// Returns the definition of a Sen object event for a given interest.
  JsonResponse getEventDefinitionHandler(ClientSession& clientSession,
                                         HttpSession& httpSession,
                                         const UrlParams& urlParams) const;

  /// Subscribe to events updates of a Sen object for a given interest.
  /// Notifications related to the subscription will be returned as SSE (server-sent-events) on the notifications
  /// endpoint.
  JsonResponse subscribeEventHandler(ClientSession& clientSession,
                                     HttpSession& httpSession,
                                     const UrlParams& urlParams) const;

  /// Unsubscribe from event updates of a Sen object for a given interest.
  JsonResponse unsubscribeEventHandler(ClientSession& clientSession,
                                       HttpSession& httpSession,
                                       const UrlParams& urlParams) const;

  /// Returns information related to a previously created interest.
  JsonResponse getInterestHandler(ClientSession& clientSession,
                                  HttpSession& httpSession,
                                  const UrlParams& urlParams) const;

  /// Returns all the current interests.
  JsonResponse getInterestsHandler(ClientSession& clientSession,
                                   HttpSession& httpSession,
                                   const UrlParams& urlParams) const;

  /// Creates a new interest using a SQL query and an unique interest name.
  JsonResponse createInterestHandler(ClientSession& clientSession,
                                     HttpSession& httpSession,
                                     const UrlParams& urlParams) const;

  /// Removes a given interest
  JsonResponse removeInterestHandler(ClientSession& clientSession,
                                     HttpSession& httpSession,
                                     const UrlParams& urlParams) const;

  /// Blocking handler returning server-sent-events notifications for a client session.
  JsonResponse getNotificationsHandler(ClientSession& clientSession,
                                       std::shared_ptr<HttpSession> httpSession,
                                       const UrlParams& urlParams,
                                       asio::ip::tcp::socket socket) const;

  /// Returns type introspection
  JsonResponse getTypeIntrospection(const ClientSession& clientSession,
                                    HttpSession& httpSession,
                                    const UrlParams& urlParams) const;

private:
  // Helpers
  void logClientSession(const ClientSession& clientSession, std::string_view functionName) const;

  [[nodiscard]] std::shared_ptr<sen::Object> getObject(const InterestSubscription& interestSubscription,
                                                       const std::string& objectName) const;
  [[nodiscard]] std::optional<Object> getObjectDefinition(const InterestSubscription& interestSubscription,
                                                          const std::string& urlPath,
                                                          const std::string& objectName) const;
  [[nodiscard]] std::optional<ObjectMethod> getMethodDefinition(const InterestSubscription& interestSubscription,
                                                                const MethodLocator& methodLocator) const;
  [[nodiscard]] std::optional<ObjectEvent> getEventDefinition(const InterestSubscription& interestSubscription,
                                                              const EventLocator& eventLocator) const;
  [[nodiscard]] Result<InterestSubscription, std::string> interestSubscriptionFromName(
    ClientSession& clientSession,
    const std::string& interestName) const;

  kernel::RunApi& api_;
  std::unordered_map<std::string, std::shared_ptr<ClientSession>> clientSessions_;
};

}  // namespace sen::components::rest

#endif  // SEN_COMPONENTS_REST_SRC_SEN_ROUTER_H
