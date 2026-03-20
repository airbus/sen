// === sen_router.cpp ==================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "sen_router.h"

// component
#include "base_router.h"
#include "client_session.h"
#include "http_response.h"
#include "http_session.h"
#include "json_response.h"
#include "jwt.h"
#include "locators.h"
#include "object_interests_manager.h"
#include "types.h"
#include "utils.h"

// generated code
#include "stl/options.stl.h"
#include "stl/types.stl.h"

// sen
#include "sen/core/base/result.h"
#include "sen/core/io/util.h"
#include "sen/core/meta/callable.h"
#include "sen/core/meta/custom_type.h"
#include "sen/core/meta/event.h"
#include "sen/core/meta/method.h"
#include "sen/core/meta/var.h"
#include "sen/core/obj/callback.h"
#include "sen/core/obj/object.h"
#include "sen/kernel/component_api.h"
#include "sen/kernel/type_specs_utils.h"

// asio
#include <asio/buffer.hpp>
#include <asio/write.hpp>  // NOLINT(misc-include-cleaner)

// std
#include <algorithm>
#include <atomic>
#include <cstddef>
#include <exception>
#include <memory>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <string>
#include <string_view>
#include <system_error>
#include <thread>
#include <utility>
#include <variant>

// json
#include "nlohmann/json.hpp"

using Json = nlohmann::json;

namespace sen::components::rest
{

//--------------------------------------------------------------------------------------------------------------
// Constants
//--------------------------------------------------------------------------------------------------------------

constexpr std::string_view restApiHandle = "rest.";

//--------------------------------------------------------------------------------------------------------------
// Helpers
//--------------------------------------------------------------------------------------------------------------

void logClientSession(const ClientSession& clientSession, std::string_view functionName)
{
  getLogger()->debug("[ClientSession: {}] {}", clientSession.getClientId(), functionName);
}

template <typename MemberFn>
RouteCallback bindAuthRouteCallback(SenRouter* obj, MemberFn memberFn)
{
  return [obj, memberFn](HttpSession& session, const UrlParams& params)
  {
    auto bearerTokenOpt = session.getRequest().sessionToken();
    if (!bearerTokenOpt.has_value())
    {
      return JsonResponse(httpUnauthorizedError, Error {"Bearer token not found"});
    }
    if (!bearerTokenOpt->valid)
    {
      return JsonResponse(httpUnauthorizedError, Error {toString(bearerTokenOpt->error)});
    }
    auto clientSessionOpt = obj->getClientSessionFromToken(*bearerTokenOpt);
    if (!clientSessionOpt.has_value())
    {
      return JsonResponse(httpUnauthorizedError, Error {"Invalid session"});
    }
    return (obj->*memberFn)(*clientSessionOpt.value(), session, params);
  };
}

std::shared_ptr<sen::Object> SenRouter::getObject(const InterestSubscription& interestSubscription,
                                                  const std::string& objectName) const

{
  auto objectLocator = ObjectLocator::build(interestSubscription.busLocator, objectName);
  if (objectLocator.isError())
  {
    return nullptr;
  }

  auto objectAddress = std::string(restApiHandle) + objectLocator.getValue().toObjectAddress();
  auto objects = interestSubscription.objects->getUntypedObjects();
  auto objectIt = std::find_if(objects.cbegin(),
                               objects.cend(),
                               [&objectAddress](const std::shared_ptr<sen::Object> o)
                               { return o && o->getLocalName() == objectAddress; });
  if (objectIt == objects.cend())
  {
    return nullptr;
  }

  return *objectIt;
}

std::optional<Object> SenRouter::getObjectDefinition(const InterestSubscription& interestSubscription,
                                                     const std::string& urlPath,
                                                     const std::string& objectName) const
{
  auto object = getObject(interestSubscription, objectName);
  if (!object)
  {
    return std::nullopt;
  }

  auto objectClass = object->getClass();

  Links links;
  for (const auto& method: objectClass->getMethods(sen::ClassType::SearchMode::includeParents))
  {
    const std::string relUrlMethod = urlPath + "/methods/" + std::string(method->getName());
    links.emplace_back(Link {RelType::method, relUrlMethod + "/invoke", HttpMethod::httpPost});
    links.emplace_back(Link {RelType::def, relUrlMethod, HttpMethod::httpGet});
  }

  for (const auto& prop: objectClass->getProperties(sen::ClassType::SearchMode::includeParents))
  {
    // prop getters
    const std::string getterRelUrl = urlPath + "/methods/" + std::string(prop->getGetterMethod().getName());
    links.emplace_back(Link {RelType::getter, getterRelUrl + "/invoke", HttpMethod::httpPost});

    // prop setters
    const std::string setterRelUrl = urlPath + "/methods/" + std::string(prop->getSetterMethod().getName());
    links.emplace_back(Link {RelType::setter, setterRelUrl + "/invoke", HttpMethod::httpPost});
    links.emplace_back(Link {RelType::def, setterRelUrl, HttpMethod::httpGet});

    // prop change subscription
    const std::string propRelUrlEvent = urlPath + "/properties/" + std::string(prop->getName());
    links.emplace_back(Link {RelType::property, propRelUrlEvent, HttpMethod::httpGet});
    links.emplace_back(Link {RelType::propertySubscribe, propRelUrlEvent + "/subscribe", HttpMethod::httpPost});
    links.emplace_back(Link {RelType::propertyUnsubscribe, propRelUrlEvent + "/unsubscribe", HttpMethod::httpPost});
  }

  for (const auto& event: objectClass->getEvents(sen::ClassType::SearchMode::includeParents))
  {
    const std::string relUrlEvent = urlPath + "/events/" + std::string(event->getName());
    links.emplace_back(Link {RelType::evt, relUrlEvent + "/subscribe", HttpMethod::httpPost});
    links.emplace_back(Link {RelType::def, relUrlEvent, HttpMethod::httpGet});
  }

  return Object {
    object->getId().get(),
    object->getName(),
    std::string(objectClass->getQualifiedName()),
    object->getLocalName(),
    std::string(objectClass->getDescription()),
    links,
  };
}

std::optional<ObjectMethod> SenRouter::getMethodDefinition(const InterestSubscription& interestSubscription,
                                                           const MethodLocator& methodLocator) const
{
  auto object = getObject(interestSubscription, methodLocator.object());
  if (!object)
  {
    return std::nullopt;
  }

  auto objectClass = object->getClass();

  const auto& methods = objectClass->getMethods(sen::ClassType::SearchMode::includeParents);
  const auto methodIt = std::find_if(methods.cbegin(),
                                     methods.cend(),
                                     [&methodLocator](const std::shared_ptr<const Method> methodPtr)
                                     { return methodPtr && methodPtr->getName() == methodLocator.method(); });
  if (methodIt == methods.cend())
  {
    return std::nullopt;
  }

  auto method = methodIt->get();
  if (!method)
  {
    return std::nullopt;
  }

  Arguments args;
  for (const auto& arg: method->getArgs())
  {
    args.emplace_back(Argument {arg.name, arg.description, std::string(arg.type->getName())});
  }

  return ObjectMethod {std::string(method->getName()),
                       std::string(method->getDescription()),
                       std::string(method->getReturnType()->getName()),
                       args};
}

std::optional<ObjectEvent> SenRouter::getEventDefinition(const InterestSubscription& interestSubscription,
                                                         const EventLocator& eventLocator) const
{
  auto object = getObject(interestSubscription, eventLocator.object());
  if (!object)
  {
    return std::nullopt;
  }

  auto objectClass = object->getClass();

  const auto& events = objectClass->getEvents(sen::ClassType::SearchMode::includeParents);
  const auto eventIt = std::find_if(events.cbegin(),
                                    events.cend(),
                                    [&eventLocator](std::shared_ptr<const Event> eventPtr)
                                    { return eventPtr && eventPtr->getName() == eventLocator.event(); });
  if (eventIt == events.cend())
  {
    return std::nullopt;
  }

  auto event = eventIt->get();
  if (!event)
  {
    return std::nullopt;
  }

  return ObjectEvent {
    std::string(event->getName()),
    std::string(event->getDescription()),
  };
}

Result<InterestSubscription, std::string> SenRouter::interestSubscriptionFromName(ClientSession& clientSession,
                                                                                  const std::string& interestName) const
{
  auto interestSubscription = clientSession.findInterest(interestName);
  if (!interestSubscription.has_value())
  {
    return sen::Err(std::string("Interest not found"));
  }

  return sen::Ok(*interestSubscription);
}

std::optional<std::shared_ptr<ClientSession>> SenRouter::getClientSessionFromToken(const JWT& token) const
{
  try
  {
    auto clientAuth = sen::toValue<ClientAuth>(sen::fromJson(token.payload));

    std::shared_lock lock(clientSessionsMutex_);
    auto it = clientSessions_.find(clientAuth.id);
    if (it == clientSessions_.end())
    {
      return std::nullopt;
    }

    return it->second;
  }
  catch (...)
  {
    return std::nullopt;
  }
}

//--------------------------------------------------------------------------------------------------------------
// SenRouter
//--------------------------------------------------------------------------------------------------------------

SenRouter::SenRouter(kernel::RunApi& api): api_(api)
{
  addRoute(HttpMethod::httpPost, "/api/auth", bindRouteCallback(this, &SenRouter::clientAuthSessionHandler));

  // sen session endpoint
  addRoute(HttpMethod::httpGet, "/api/sessions", bindRouteCallback(this, &SenRouter::getSessionsHandler));

  // interests endpoints
  addRoute(HttpMethod::httpGet, "/api/interests", bindAuthRouteCallback(this, &SenRouter::getInterestsHandler));
  addRoute(HttpMethod::httpPost, "/api/interests", bindAuthRouteCallback(this, &SenRouter::createInterestHandler));
  addRoute(
    HttpMethod::httpGet, "/api/interests/:interest", bindAuthRouteCallback(this, &SenRouter::getInterestHandler));
  addRoute(
    HttpMethod::httpDelete, "/api/interests/:interest", bindAuthRouteCallback(this, &SenRouter::removeInterestHandler));

  // objects endpoints
  addRoute(HttpMethod::httpGet,
           "/api/interests/:interest/objects",
           bindAuthRouteCallback(this, &SenRouter::getObjectsHandler));
  addRoute(HttpMethod::httpGet,
           "/api/interests/:interest/objects/:object",
           bindAuthRouteCallback(this, &SenRouter::getObjectHandler));

  // object property endpoints
  addRoute(HttpMethod::httpGet,
           "/api/interests/:interest/objects/:object/properties/:property",
           bindAuthRouteCallback(this, &SenRouter::getPropertyHandler));
  addRoute(HttpMethod::httpPost,
           "/api/interests/:interest/objects/:object/properties/:property/subscribe",
           bindAuthRouteCallback(this, &SenRouter::subscribePropertyUpdateHandler));
  addRoute(HttpMethod::httpPost,
           "/api/interests/:interest/objects/:object/properties/:property/unsubscribe",
           bindAuthRouteCallback(this, &SenRouter::unsubscribePropertyUpdateHandler));

  // object method endpoints
  addRoute(HttpMethod::httpGet,
           "/api/interests/:interest/objects/:object/methods/:method",
           bindAuthRouteCallback(this, &SenRouter::getMethodDefinitionHandler));
  addRoute(HttpMethod::httpGet,
           "/api/interests/:interest/objects/:object/methods/:method/invoke/:id",
           bindAuthRouteCallback(this, &SenRouter::getInvokeMethodStatusHandler));
  addRoute(HttpMethod::httpPost,
           "/api/interests/:interest/objects/:object/methods/:method/invoke",
           bindAuthRouteCallback(this, &SenRouter::invokeMethodHandler));

  // object event endpoints
  addRoute(HttpMethod::httpGet,
           "/api/interests/:interest/objects/:object/events/:event",
           bindAuthRouteCallback(this, &SenRouter::getEventDefinitionHandler));
  addRoute(HttpMethod::httpPost,
           "/api/interests/:interest/objects/:object/events/:event/subscribe",
           bindAuthRouteCallback(this, &SenRouter::subscribeEventHandler));
  addRoute(HttpMethod::httpPost,
           "/api/interests/:interest/objects/:object/events/:event/unsubscribe",
           bindAuthRouteCallback(this, &SenRouter::unsubscribeEventHandler));

  // notifications as SSE endpoint
  addRoute(HttpMethod::httpGet, "/api/sse", bindAuthRouteCallback(this, &SenRouter::getNotificationsHandler));

  // types instrospection
  addRoute(HttpMethod::httpGet, "/api/types/:type", bindAuthRouteCallback(this, &SenRouter::getTypeIntrospection));
}

SenRouter::~SenRouter()
{
  std::unique_lock<std::shared_mutex> lock(clientSessionsMutex_);

  releaseAll();
  clientSessions_.clear();
}

HttpResponse SenRouter::clientAuthSessionHandler([[maybe_unused]] HttpSession& httpSession,
                                                 [[maybe_unused]] const UrlParams& urlParams)
{
  try
  {
    const auto payload = Json::parse(httpSession.getRequest().body());
    auto clientAuth = sen::toValue<ClientAuth>(sen::fromJson(httpSession.getRequest().body()));

    std::unique_lock<std::shared_mutex> lock(clientSessionsMutex_);
    if (clientSessions_.find(clientAuth.id) != clientSessions_.cend())
    {
      clientSessions_.erase(clientAuth.id);
    }
    clientSessions_[clientAuth.id] = std::make_shared<ClientSession>(clientAuth.id);

    std::string token = clientSessions_[clientAuth.id]->encodeToken();
    auto body = Json {{"token", token}}.dump();

    return HttpResponse {
      httpSuccess,
      {
        {"Set-Cookie", "token=" + token + "; Path=/; HttpOnly; Secure; SameSite=Lax"},
      },
      body,
    };
  }
  catch (const std::exception& e)
  {
    return JsonResponse(httpBadRequestError, Error {"client auth failed. error: " + std::string(e.what())});
  }
}

JsonResponse SenRouter::getSessionsHandler([[maybe_unused]] HttpSession& httpSession,
                                           [[maybe_unused]] const UrlParams& urlParams) const
{
  Sessions sessions;
  for (const auto& source: api_.getSessionsDiscoverer().getDetectedSources())
  {
    sessions.emplace_back(source);
  }
  return JsonResponse {httpSuccess, sessions};
}

JsonResponse SenRouter::getInterestHandler(ClientSession& clientSession,
                                           [[maybe_unused]] HttpSession& httpSession,
                                           const UrlParams& urlParams) const
{
  logClientSession(clientSession, "getInterest");

  if (urlParams.size() != 1)
  {
    return getErrorInvalidParams();
  }

  auto interestOpt = clientSession.getInterestSummary(urlParams[0]);
  if (!interestOpt.has_value())
  {
    return getErrorNotFound();
  }

  return JsonResponse {httpSuccess, interestOpt.value()};
}

JsonResponse SenRouter::getInterestsHandler(ClientSession& clientSession,
                                            [[maybe_unused]] HttpSession& httpSession,
                                            [[maybe_unused]] const UrlParams& urlParams) const
{
  logClientSession(clientSession, "getInterests");

  auto interests = clientSession.getAllInterestsSummary();
  return JsonResponse {httpSuccess, interests};
}

JsonResponse SenRouter::createInterestHandler(ClientSession& clientSession,
                                              [[maybe_unused]] HttpSession& httpSession,
                                              [[maybe_unused]] const UrlParams& urlParams) const
{
  logClientSession(clientSession, "createInterest");

  const auto payload = Json::parse(httpSession.getRequest().body());

  try
  {
    Interest interest;
    interest.query = payload.at("query").get<std::string>();
    interest.name = payload.at("name").get<std::string>();

    auto busLocatorOpt = BusLocator::build(interest);
    if (busLocatorOpt.isError())
    {
      if (busLocatorOpt.getError().errorType == LocatorErrorType::parseSQLError)
      {
        return JsonResponse(httpBadRequestError, Error {"create interest failed. error: invalid SQL sentence"});
      }
      return JsonResponse(httpBadRequestError,
                          Error {"create interest failed. error: " + busLocatorOpt.getError().error});
    }

    const BusLocator& busLocator = busLocatorOpt.getValue();
    auto source = api_.getSource(busLocator.toBusAddress());
    if (!source)
    {
      return JsonResponse(httpBadRequestError, Error {"source not open"});
    }

    auto interestName = clientSession.createInterest(api_, busLocator, interest.name, interest.query);
    if (interestName.isError())
    {
      return JsonResponse(
        httpBadRequestError,
        Error {"interest creation failed. error: " + std::to_string(static_cast<int>(interestName.getError()))});
    }

    auto interestJson = Json {{"name", interestName.getValue()}};
    return JsonResponse {httpSuccess, interestJson.dump()};
  }
  catch (const std::exception& e)
  {
    return JsonResponse(httpBadRequestError, Error {"create interest failed. error: " + std::string(e.what())});
  }
}

JsonResponse SenRouter::removeInterestHandler(ClientSession& clientSession,
                                              [[maybe_unused]] HttpSession& httpSession,
                                              const UrlParams& urlParams) const
{
  logClientSession(clientSession, "removeInterest");

  if (urlParams.size() != 1)
  {
    return getErrorInvalidParams();
  }

  if (!clientSession.removeInterest(urlParams[0]))
  {
    return JsonResponse(httpBadRequestError, Error {"interest deletion failed"});
  }

  return JsonResponse(httpSuccess, Success {"interest deleted"});
}

JsonResponse SenRouter::getObjectsHandler(ClientSession& clientSession,
                                          HttpSession& httpSession,
                                          const UrlParams& urlParams) const
{
  logClientSession(clientSession, "getObjects");

  if (urlParams.size() != 1)
  {
    return getErrorInvalidParams();
  }

  auto interestSubscriptionRes = interestSubscriptionFromName(clientSession, urlParams[0]);
  if (interestSubscriptionRes.isError())
  {
    return JsonResponse(httpBadRequestError, Error {interestSubscriptionRes.getError()});
  }

  const auto& interestSubscription = interestSubscriptionRes.getValue();
  std::string rootUrl = httpSession.getRequest().path();
  ObjectsSummary objects;

  for (const auto& object: interestSubscription.objects->getObjects())
  {
    const std::string objUrl = rootUrl + "/" + urlSanitizeLocalName(object->getLocalName());
    auto objectClass = object->getClass();
    objects.emplace_back(ObjectSummary {
      object->getId().get(),
      object->getName(),
      std::string(objectClass->getName()),
      object->getLocalName(),
      Link {RelType::def, objUrl, HttpMethod::httpGet},
    });
  }

  return JsonResponse {httpSuccess, objects};
}

JsonResponse SenRouter::getObjectHandler(ClientSession& clientSession,
                                         HttpSession& httpSession,
                                         const UrlParams& urlParams) const
{
  logClientSession(clientSession, "getObject");

  if (urlParams.size() != 2)
  {
    return getErrorInvalidParams();
  }

  auto interestSubscriptionRes = interestSubscriptionFromName(clientSession, urlParams[0]);
  if (interestSubscriptionRes.isError())
  {
    return JsonResponse(httpBadRequestError, Error {interestSubscriptionRes.getError()});
  }

  const auto& interestSubscription = interestSubscriptionRes.getValue();
  auto object = getObjectDefinition(interestSubscription, httpSession.getRequest().path(), urlParams[1]);
  if (!object)
  {
    return getErrorNotFound();
  }

  return JsonResponse {httpSuccess, *object};
}

JsonResponse SenRouter::getPropertyHandler(ClientSession& clientSession,
                                           [[maybe_unused]] HttpSession& httpSession,
                                           const UrlParams& urlParams) const
{
  logClientSession(clientSession, "getProperty");

  if (urlParams.size() != 3)
  {
    return getErrorInvalidParams();
  }

  auto interestSubscriptionRes = interestSubscriptionFromName(clientSession, urlParams[0]);
  if (interestSubscriptionRes.isError())
  {
    return JsonResponse(httpBadRequestError, Error {interestSubscriptionRes.getError()});
  }

  auto propertyLocatorOpt =
    PropertyLocator::build(interestSubscriptionRes.getValue().busLocator, urlParams[1], urlParams[2]);
  if (propertyLocatorOpt.isError())
  {
    return getErrorLocatorParams(propertyLocatorOpt.getError());
  }

  const PropertyLocator& locator = propertyLocatorOpt.getValue();

  auto object = getObject(interestSubscriptionRes.getValue(), locator.object());
  if (object.get())
  {
    auto prop =
      object->getClass()->searchPropertyByName(locator.property(), sen::ClassType::SearchMode::includeParents);
    if (prop)
    {
      auto result = object->getPropertyUntyped(prop);
      return JsonResponse {httpSuccess, toJson(result)};
    }
  }

  return getErrorNotFound();
}

JsonResponse SenRouter::subscribePropertyUpdateHandler(ClientSession& clientSession,
                                                       [[maybe_unused]] HttpSession& httpSession,
                                                       const UrlParams& urlParams) const
{
  logClientSession(clientSession, "subscribePropertyChange");

  if (urlParams.size() != 3)
  {
    return getErrorInvalidParams();
  }

  auto interestSubscriptionRes = interestSubscriptionFromName(clientSession, urlParams[0]);
  if (interestSubscriptionRes.isError())
  {
    return JsonResponse(httpBadRequestError, Error {interestSubscriptionRes.getError()});
  }

  auto propertyLocatorOpt =
    PropertyLocator::build(interestSubscriptionRes.getValue().busLocator, urlParams[1], urlParams[2]);
  if (propertyLocatorOpt.isError())
  {
    return getErrorLocatorParams(propertyLocatorOpt.getError());
  }

  const PropertyLocator& locator = propertyLocatorOpt.getValue();
  auto object = getObject(interestSubscriptionRes.getValue(), locator.object());
  if (!object)
  {
    return getErrorNotFound();
  }

  auto prop = object->getClass()->searchPropertyByName(locator.property(), sen::ClassType::SearchMode::includeParents);
  if (!prop)
  {
    return JsonResponse(httpNotFoundError, Error {"property not found"});
  }

  SubscriptionOptions opts;
  try
  {
    if (!httpSession.getRequest().body().empty())
    {
      opts = sen::toValue<SubscriptionOptions>(sen::fromJson(httpSession.getRequest().body()));
    }
  }
  catch (std::exception& e)
  {
    std::string errMsg = "Error: invalid subscribe options. ";
    errMsg.append(e.what());
    return JsonResponse(httpBadRequestError, Error {std::move(errMsg)});
  }

  bool hasSubscribed = clientSession.subscribeProperty(api_, object, locator, opts, urlParams[0]);
  if (hasSubscribed)
  {
    return JsonResponse(httpSuccess, Success {"property change subscription succeeded"});
  }

  return JsonResponse(httpBadRequestError, Error {"property change subscription failed"});
}

JsonResponse SenRouter::unsubscribePropertyUpdateHandler(ClientSession& clientSession,
                                                         [[maybe_unused]] HttpSession& httpSession,
                                                         const UrlParams& urlParams) const
{
  logClientSession(clientSession, "unsubscribePropertyChange");

  if (urlParams.size() != 3)
  {
    return getErrorInvalidParams();
  }

  auto interestSubscriptionRes = interestSubscriptionFromName(clientSession, urlParams[0]);
  if (interestSubscriptionRes.isError())
  {
    return JsonResponse(httpBadRequestError, Error {interestSubscriptionRes.getError()});
  }

  auto propertyLocatorOpt =
    PropertyLocator::build(interestSubscriptionRes.getValue().busLocator, urlParams[1], urlParams[2]);
  if (propertyLocatorOpt.isError())
  {
    return getErrorLocatorParams(propertyLocatorOpt.getError());
  }

  const PropertyLocator& locator = propertyLocatorOpt.getValue();
  auto object = getObject(interestSubscriptionRes.getValue(), locator.object());
  if (!object)
  {
    return getErrorNotFound();
  }

  auto prop = object->getClass()->searchPropertyByName(locator.property(), sen::ClassType::SearchMode::includeParents);
  if (!prop)
  {
    return JsonResponse(httpNotFoundError, Error {"property not found"});
  }

  bool hasUnsubscribed = clientSession.unsubscribeMember(object->getId(), prop->getId());
  if (hasUnsubscribed)
  {
    return JsonResponse(httpSuccess, Success {"property change unsubscription succeeded"});
  }

  return JsonResponse(httpBadRequestError, Error {"property change unsubscription failed"});
}

JsonResponse SenRouter::getMethodDefinitionHandler(ClientSession& clientSession,
                                                   [[maybe_unused]] HttpSession& httpSession,
                                                   const UrlParams& urlParams) const
{
  logClientSession(clientSession, "getMethodDefinition");

  if (urlParams.size() != 3)
  {
    return getErrorInvalidParams();
  }

  auto interestSubscriptionRes = interestSubscriptionFromName(clientSession, urlParams[0]);
  if (interestSubscriptionRes.isError())
  {
    return JsonResponse(httpBadRequestError, Error {interestSubscriptionRes.getError()});
  }

  const auto& interestSubscription = interestSubscriptionRes.getValue();

  auto methodLocatorOpt = MethodLocator::build(interestSubscription.busLocator, urlParams[1], urlParams[2]);
  if (methodLocatorOpt.isError())
  {
    return getErrorLocatorParams(methodLocatorOpt.getError());
  }

  const auto& locator = methodLocatorOpt.getValue();

  auto object = getMethodDefinition(interestSubscription, locator);
  if (object.has_value())
  {
    return JsonResponse {httpSuccess, object.value()};
  }
  return getErrorNotFound();
}

JsonResponse SenRouter::invokeMethodHandler(ClientSession& clientSession,
                                            [[maybe_unused]] HttpSession& httpSession,
                                            const UrlParams& urlParams) const
{
  logClientSession(clientSession, "invokeMethod");

  if (urlParams.size() != 3)
  {
    return getErrorInvalidParams();
  }

  auto interestSubscriptionRes = interestSubscriptionFromName(clientSession, urlParams[0]);
  if (interestSubscriptionRes.isError())
  {
    return JsonResponse(httpBadRequestError, Error {interestSubscriptionRes.getError()});
  }

  const auto& interestSubscription = interestSubscriptionRes.getValue();

  auto methodLocatorOpt = MethodLocator::build(interestSubscription.busLocator, urlParams[1], urlParams[2]);
  if (methodLocatorOpt.isError())
  {
    return getErrorLocatorParams(methodLocatorOpt.getError());
  }

  const auto& locator = methodLocatorOpt.getValue();
  auto object = getObject(interestSubscription, locator.object());
  if (!object)
  {
    return getErrorNotFound();
  }

  const sen::Method* method = object->getClass()->searchMethodByName(locator.method());
  if (!method)
  {
    return JsonResponse(httpNotFoundError, Error {"method not found"});
  }

  if (const auto* isGetter = std::get_if<PropertyGetter>(&method->getPropertyRelation()); isGetter != nullptr)
  {
    auto result = object->getPropertyUntyped(isGetter->property);
    Invoke invoke {
      std::nullopt,
      InvokeStatus::finished,
      sen::Ok(result),
    };
    return JsonResponse {httpSuccess, toJson(invoke)};
  }

  // It is a setter or a regular method
  VarList args;
  try
  {
    args = sen::fromJson(httpSession.getRequest().body()).get<sen::VarList>();
  }
  catch (std::exception& e)
  {
    return JsonResponse(httpBadRequestError, Error {e.what()});
  }

  ArgsError argsError = checkValidExpectedArgs(method->getArgs(), args);
  if (argsError.has_value())
  {
    return JsonResponse(httpBadRequestError, Error {"Invalid args. Error: " + argsError.value()});
  }

  Invoke invoke = clientSession.newInvoke(urlParams[0]);
  if (invoke.id.has_value())
  {
    object->invokeUntyped(method,
                          args,
                          {api_.getWorkQueue(),
                           [invoke, &clientSession, interest = urlParams[0]](const sen::MethodResult<Var>& result)
                           { clientSession.updateInvoke(interest, invoke.id.value(), result); }});

    return JsonResponse {httpSuccess, toJson(invoke)};
  }

  return JsonResponse(httpInternalServerError, Error {"unexpected condition: invalid invoke ID"});
}

JsonResponse SenRouter::getEventDefinitionHandler(ClientSession& clientSession,
                                                  [[maybe_unused]] HttpSession& httpSession,
                                                  const UrlParams& urlParams) const
{
  logClientSession(clientSession, "getEventDefinition");

  if (urlParams.size() != 3)
  {
    return getErrorInvalidParams();
  }

  auto interestSubscriptionRes = interestSubscriptionFromName(clientSession, urlParams[0]);
  if (interestSubscriptionRes.isError())
  {
    return JsonResponse(httpBadRequestError, Error {interestSubscriptionRes.getError()});
  }

  const auto& interestSubscription = interestSubscriptionRes.getValue();

  auto eventLocatorOpt = EventLocator::build(interestSubscription.busLocator, urlParams[1], urlParams[2]);
  if (eventLocatorOpt.isError())
  {
    return getErrorLocatorParams(eventLocatorOpt.getError());
  }

  const EventLocator& eventLocator = eventLocatorOpt.getValue();
  auto object = getEventDefinition(interestSubscription, eventLocator);
  if (object.has_value())
  {
    return JsonResponse {httpSuccess, object.value()};
  }

  return getErrorNotFound();
}

JsonResponse SenRouter::subscribeEventHandler(ClientSession& clientSession,
                                              [[maybe_unused]] HttpSession& httpSession,
                                              [[maybe_unused]] const UrlParams& urlParams) const
{
  logClientSession(clientSession, "subscribeEvent");

  if (urlParams.size() != 3)
  {
    return getErrorInvalidParams();
  }

  auto interestSubscriptionRes = interestSubscriptionFromName(clientSession, urlParams[0]);
  if (interestSubscriptionRes.isError())
  {
    return JsonResponse(httpBadRequestError, Error {interestSubscriptionRes.getError()});
  }

  const auto& interestSubscription = interestSubscriptionRes.getValue();

  auto eventLocatorOpt = EventLocator::build(interestSubscription.busLocator, urlParams[1], urlParams[2]);
  if (eventLocatorOpt.isError())
  {
    return getErrorLocatorParams(eventLocatorOpt.getError());
  }

  const EventLocator& locator = eventLocatorOpt.getValue();
  auto object = getObject(interestSubscriptionRes.getValue(), urlParams[1]);
  if (!object)
  {
    return getErrorNotFound();
  }

  const sen::Event* event = object->getClass()->searchEventByName(static_cast<std::string_view>(locator.event()));
  if (!event)
  {
    return JsonResponse(httpNotFoundError, Error {"event not found"});
  }

  bool hasSubscribed = clientSession.subscribeEvent(api_, object, locator, urlParams[0]);
  if (hasSubscribed)
  {
    return JsonResponse(httpSuccess, Success {"event subscription succeeded"});
  }

  return JsonResponse(httpBadRequestError, Error {"event subscription failed"});
}

JsonResponse SenRouter::unsubscribeEventHandler(ClientSession& clientSession,
                                                [[maybe_unused]] HttpSession& httpSession,
                                                [[maybe_unused]] const UrlParams& urlParams) const
{
  logClientSession(clientSession, "unsubscribeEvent");

  if (urlParams.size() != 3)
  {
    return getErrorInvalidParams();
  }

  auto interestSubscriptionRes = interestSubscriptionFromName(clientSession, urlParams[0]);
  if (interestSubscriptionRes.isError())
  {
    return JsonResponse(httpBadRequestError, Error {interestSubscriptionRes.getError()});
  }

  const auto& interestSubscription = interestSubscriptionRes.getValue();

  auto eventLocatorOpt = EventLocator::build(interestSubscription.busLocator, urlParams[1], urlParams[2]);
  if (eventLocatorOpt.isError())
  {
    return getErrorLocatorParams(eventLocatorOpt.getError());
  }

  const EventLocator& locator = eventLocatorOpt.getValue();
  auto object = getObject(interestSubscriptionRes.getValue(), urlParams[1]);
  if (!object)
  {
    return getErrorNotFound();
  }

  const sen::Event* event = object->getClass()->searchEventByName(static_cast<std::string_view>(locator.event()));
  if (!event)
  {
    return JsonResponse(httpNotFoundError, Error {"event not found"});
  }

  bool hasUnsubscribed = clientSession.unsubscribeMember(object->getId(), event->getId());
  if (hasUnsubscribed)
  {
    return JsonResponse(httpSuccess, Success {"event unsubscription succeeded"});
  }

  return JsonResponse(httpBadRequestError, Error {"event unsubscription failed"});
}

JsonResponse SenRouter::getInvokeMethodStatusHandler(ClientSession& clientSession,
                                                     [[maybe_unused]] HttpSession& httpSession,
                                                     const UrlParams& urlParams) const
{
  logClientSession(clientSession, "getInvokeMethodStatus");

  if (urlParams.size() != 4)
  {
    return getErrorInvalidParams();
  }

  try
  {
    InvokeId invokeId = std::stoi(urlParams[3]);
    auto invokeOpt = clientSession.findInvoke(invokeId);
    if (invokeOpt.has_value())
    {
      return JsonResponse {httpSuccess, toJson(invokeOpt.value())};
    }
  }
  catch (...)
  {
    return JsonResponse(httpBadRequestError, Error {"invalid invoke id"});
  }

  return JsonResponse(httpNotFoundError, Error {"invoke not found"});
}

JsonResponse SenRouter::getNotificationsHandler(ClientSession& clientSession,
                                                HttpSession& httpSession,
                                                [[maybe_unused]] const UrlParams& urlParams) const
{
  logClientSession(clientSession, "getNotificationsSSE");

  // Send SSE header to client
  auto& sessionSocket = httpSession.getSocket();
  if (!sessionSocket.is_open())
  {
    return JsonResponse(httpInternalServerError, Error {"Invalid HttpSession socket"});
  }

  HttpResponse res(httpSuccess,
                   {
                     HttpHeader("Content-Type", "text/event-stream"),
                     HttpHeader("Cache-Control", "no-cache"),
                     HttpHeader("Connection", "keep-alive"),
                   });
  std::error_code socketError = res.socketWrite(sessionSocket);
  if (socketError)
  {
    return JsonResponse(httpInternalServerError, Error {socketError.message()});
  }

  NotificationType notificationType = NotificationType::evt;
  auto membersObserver = clientSession.getObserverGuard(NotifierType::membersNotifier);
  auto invokesObserver = clientSession.getObserverGuard(NotifierType::invokesNotifier);
  auto interestsObserver = clientSession.getObserverGuard(NotifierType::interestsNotifier);

  char dummyBuffer[1];
  std::atomic<bool> connectionClosed = false;

  sessionSocket.async_read_some(
    asio::buffer(dummyBuffer, 1),
    [&connectionClosed](const std::error_code& socketError, [[maybe_unused]] std::size_t nbytes)
    {
      if (socketError && !connectionClosed)
      {
        connectionClosed = true;
      }
    });

  // block for pending notifications and send them to client
  while (!connectionClosed)
  {
    std::optional<Notification> notification = std::nullopt;
    switch (notificationType)
    {
      case NotificationType::evt:
      case NotificationType::property:
        notification = membersObserver.next();
        break;
      case NotificationType::objectAdded:
      case NotificationType::objectRemoved:
        notification = interestsObserver.next();
        break;
      case NotificationType::invoke:
      default:
        notification = invokesObserver.next();
    }

    if (notification.has_value())
    {
      std::string sse = toSSE(notification.value());
      asio::write(sessionSocket, asio::buffer(sse), socketError);  // NOLINT(misc-include-cleaner)

      if (socketError)
      {
        // write failed probably due to closed connection
        break;
      }
    }

    int nextNotificationType = (static_cast<int>(notificationType) + 1) % static_cast<int>(NotificationType::count);
    notificationType = static_cast<NotificationType>(nextNotificationType);

    std::this_thread::yield();
  }

  return JsonResponse(httpBadRequestError, Error {"broken connection"});
}

JsonResponse SenRouter::getTypeIntrospection([[maybe_unused]] const ClientSession& clientSession,
                                             [[maybe_unused]] HttpSession& httpSession,
                                             const UrlParams& urlParams) const
{
  logClientSession(clientSession, "getTypeIntrospection");

  if (urlParams.size() != 1)
  {
    return getErrorInvalidParams();
  }

  const auto& typeName = urlParams[0];
  if (auto type = api_.getTypes().get(typeName))
  {
    // Try to cast to CustomType
    if (const auto* customType = type.value()->asCustomType())
    {
      auto typeSpec = sen::kernel::makeCustomTypeSpec(customType);
      return JsonResponse {httpSuccess, typeSpec};
    }

    // If casting to CustomType is not possible then it is a native type
    sen::Var typeInfo = sen::VarMap {{"name", std::string(type.value()->getName())},
                                     {"description", std::string(type.value()->getDescription())}};
    return JsonResponse {httpSuccess, sen::toJson(typeInfo)};
  }

  return getErrorNotFound();
}

}  // namespace sen::components::rest
