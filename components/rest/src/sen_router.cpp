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
#include "notification_loop.h"
#include "object_interests_manager.h"
#include "response_adapter.h"
#include "types.h"
#include "utils.h"

// generated code
#include "stl/options.stl.h"
#include "stl/types.stl.h"

// sen
#include "sen/core/base/result.h"
#include "sen/core/base/version.h"
#include "sen/core/io/util.h"
#include "sen/core/meta/callable.h"
#include "sen/core/meta/event.h"
#include "sen/core/meta/method.h"
#include "sen/core/meta/property.h"
#include "sen/core/meta/var.h"
#include "sen/core/obj/callback.h"
#include "sen/core/obj/object.h"
#include "sen/kernel/component_api.h"
#include "sen/kernel/type_specs_utils.h"

// asio
#include <asio/buffer.hpp>
#include <asio/ip/tcp.hpp>
#include <asio/write.hpp>  // NOLINT(misc-include-cleaner)

// std
#include <algorithm>
#include <exception>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <system_error>
#include <unordered_map>
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

struct AuthResult
{
  ClientSession& session;
};

inline std::variant<AuthResult, JsonResponse> validateAuth(SenRouter* obj, HttpSession& httpSession)
{
  auto bearerTokenOpt = httpSession.getRequest().sessionToken();
  if (!bearerTokenOpt.has_value())
  {
    return JsonResponse(httpUnauthorizedError, Error {"bearer token not found"});
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
  return AuthResult {*clientSessionOpt.value()};
}

template <typename MemberFn>
RouteCallback bindAuthRouteCallback(SenRouter* obj, MemberFn memberFn)
{
  return [obj, memberFn](HttpSession& httpSession, const UrlParams& urlParams, const QueryParams& queryParams)
  {
    auto result = validateAuth(obj, httpSession);
    if (auto* error = std::get_if<JsonResponse>(&result))
    {
      return *error;
    }
    return (obj->*memberFn)(std::get<AuthResult>(result).session, httpSession, urlParams, queryParams);
  };
}

template <typename MemberFn>
StreamRouteCallback bindAuthStreamRouteCallback(SenRouter* obj, MemberFn memberFn)
{
  return [obj, memberFn](std::shared_ptr<HttpSession> httpSession,
                         const UrlParams& urlParams,
                         const QueryParams& queryParams,
                         asio::ip::tcp::socket socket)
  {
    auto result = validateAuth(obj, *httpSession);
    if (auto* error = std::get_if<JsonResponse>(&result))
    {
      return *error;
    }
    return (obj->*memberFn)(
      std::get<AuthResult>(result).session, httpSession, urlParams, queryParams, std::move(socket));
  };
}

void SenRouter::logClientSession(const ClientSession& clientSession, std::string_view functionName) const
{
  getLogger()->debug("[ClientSession: {}] {}", clientSession.getClientId(), functionName);
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
  auto objects = interestSubscription.subscription->list.getUntypedObjects();
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

Object SenRouter::getObjectDefinition(const std::string& urlPath, const sen::Object& object) const
{
  auto objectClass = object.getClass();
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
    const auto& getter = prop->getGetterMethod();
    std::string getterRelUrl = urlPath + "/methods/" + std::string(getter.getName());
    links.emplace_back(Link {RelType::getter, getterRelUrl + "/invoke", HttpMethod::httpPost});
    links.emplace_back(Link {RelType::def, std::move(getterRelUrl), HttpMethod::httpGet});

    // prop setters
    if (const auto& setter = prop->getSetterMethod(); prop->getCategory() == PropertyCategory::dynamicRW)
    {
      std::string setterRelUrl = urlPath + "/methods/" + std::string(setter.getName());
      links.emplace_back(Link {RelType::setter, setterRelUrl + "/invoke", HttpMethod::httpPost});
      links.emplace_back(Link {RelType::def, std::move(setterRelUrl), HttpMethod::httpGet});
    }

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
    object.getId().get(),
    object.getName(),
    std::string(objectClass->getQualifiedName()),
    object.getLocalName(),
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

  const auto method =
    objectClass->searchMethodByName(methodLocator.method(), sen::ClassType::SearchMode::includeParents);
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

[[nodiscard]] Result<InterestSubscription, std::string> SenRouter::interestSubscriptionFromName(
  ClientSession& clientSession,
  const std::string& interestName) const
{
  auto interestSubscription = clientSession.interestsManager().findInterest(interestName);
  if (!interestSubscription.has_value())
  {
    return sen::Err(std::string("interest not found"));
  }

  return sen::Ok(*interestSubscription);
}

std::optional<std::shared_ptr<ClientSession>> SenRouter::getClientSessionFromToken(const JWT& token) const
{
  try
  {
    auto clientAuth = sen::toValue<ClientAuth>(sen::fromJson(token.payload));

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

Result<PropertyLocatorMap, JsonResponse> SenRouter::createPropertyLocators(
  const InterestSubscription& interestSubscription,
  const std::shared_ptr<sen::Object> object,
  const Properties& properties) const
{
  std::unordered_map<std::string, PropertyLocator> locators;

  for (const auto& prop: properties)
  {
    auto propertyLocatorRes = PropertyLocator::build(interestSubscription.busLocator, object->getName(), prop);
    if (propertyLocatorRes.isError())
    {
      return Err(getErrorLocatorParams(propertyLocatorRes.getError()));
    }

    const auto& locator = propertyLocatorRes.getValue();

    if (auto subProp = object->getClass()->searchPropertyByName(locator.property()); !subProp)
    {
      return Err(JsonResponse(httpNotFoundError, Error {"property " + prop + " not found"}));
    }

    locators.emplace(prop, locator);
  }

  return Ok(std::move(locators));
}

Result<EventLocatorMap, JsonResponse> SenRouter::createEventLocators(const InterestSubscription& interestSubscription,
                                                                     const std::shared_ptr<sen::Object> object,
                                                                     const Events& events) const
{
  std::unordered_map<std::string, EventLocator> locators;

  for (const auto& event: events)
  {
    auto eventLocatorRes = EventLocator::build(interestSubscription.busLocator, object->getName(), event);
    if (eventLocatorRes.isError())
    {
      return Err(getErrorLocatorParams(eventLocatorRes.getError()));
    }

    const auto& locator = eventLocatorRes.getValue();
    if (const auto subEvent = object->getClass()->searchEventByName(locator.event()); !subEvent)
    {
      return Err(JsonResponse(httpNotFoundError, Error {"event " + event + " not found"}));
    }

    locators.emplace(event, locator);
  }

  return Ok(std::move(locators));
}

void SenRouter::bulkUnsubscribeProperties(ClientSession& clientSession,
                                          const std::shared_ptr<sen::Object> object,
                                          PropertyLocatorMap& propertyLocators) const
{
  for (const auto& prop: object->getClass()->getProperties(sen::ClassType::SearchMode::includeParents))
  {
    if (clientSession.membersManager().isSubscribedTo(object->getId(), prop->getId()))
    {
      if (const auto propIt = propertyLocators.find(std::string(prop->getName())); propIt == propertyLocators.end())
      {
        clientSession.membersManager().unsubscribeProperty(object->getId(), prop->getId());
      }
      else
      {
        propertyLocators.erase(propIt);
      }
    }
  }
}

void SenRouter::bulkUnsubscribeEvents(ClientSession& clientSession,
                                      const std::shared_ptr<sen::Object> object,
                                      EventLocatorMap& eventLocators) const
{
  for (const auto& event: object->getClass()->getEvents(sen::ClassType::SearchMode::includeParents))
  {
    if (clientSession.membersManager().isSubscribedTo(object->getId(), event->getId()))
    {
      if (const auto eventIt = eventLocators.find(std::string(event->getName())); eventIt == eventLocators.end())
      {
        clientSession.membersManager().unsubscribeEvent(object->getId(), event->getId());
      }
      else
      {
        eventLocators.erase(eventIt);
      }
    }
  }
}

JsonResponse SenRouter::bulkSubscribeProperties(ClientSession& clientSession,
                                                const InterestSubscription& interestSubscription,
                                                const std::shared_ptr<sen::Object> object,
                                                const PropertyLocatorMap& propertyLocators) const
{
  for (const auto& [_, locator]: propertyLocators)
  {
    SubscriptionOptions opts;
    if (bool hasSubscribed = clientSession.membersManager().subscribeProperty(
          api_, interestSubscription.interest->getQueryString(), object, locator, opts);
        !hasSubscribed)
    {
      return JsonResponse(httpInternalServerError,
                          Error {"property " + locator.property() + " update subscription failed"});
    }
  }

  return JsonResponse(httpSuccess, Success {"property subscriptions updated"});
}

JsonResponse SenRouter::bulkSubscribeEvents(ClientSession& clientSession,
                                            const InterestSubscription& interestSubscription,
                                            const std::shared_ptr<sen::Object> object,
                                            const EventLocatorMap& eventLocators) const
{
  for (const auto& [_, locator]: eventLocators)
  {
    if (bool hasSubscribed = clientSession.membersManager().subscribeEvent(
          api_, interestSubscription.interest->getQueryString(), object, locator);
        !hasSubscribed)
    {
      return JsonResponse(httpInternalServerError, Error {"event " + locator.event() + " update subscription failed"});
    }
  }

  return JsonResponse(httpSuccess, Success {"event subscriptions updated"});
}

//--------------------------------------------------------------------------------------------------------------
// SenRouter
//--------------------------------------------------------------------------------------------------------------

SenRouter::SenRouter(kernel::RunApi& api): api_(api)
{
  addRoute(HttpMethod::httpPost, "/api/auth", bindRouteCallback(this, &SenRouter::clientAuthSessionHandler));
  addRoute(HttpMethod::httpGet, "/api/version", bindRouteCallback(this, &SenRouter::getVersionHandler));

  // sen session endpoint
  addRoute(HttpMethod::httpGet, "/api/sessions", bindAuthRouteCallback(this, &SenRouter::getSessionsHandler));
  addRoute(HttpMethod::httpGet, "/api/sessions/:session", bindAuthRouteCallback(this, &SenRouter::getSessionHandler));

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
  addRoute(HttpMethod::httpPut,
           "/api/interests/:interest/objects/:object/subscription",
           bindAuthRouteCallback(this, &SenRouter::updateSubscriptionsHandler));
  addRoute(HttpMethod::httpGet,
           "/api/interests/:interest/objects/:object/subscription",
           bindAuthRouteCallback(this, &SenRouter::getSubscriptionsHandler));

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
  addStreamRoute(
    HttpMethod::httpGet, "/api/sse", bindAuthStreamRouteCallback(this, &SenRouter::getNotificationsHandler));

  // types introspection
  addRoute(HttpMethod::httpGet, "/api/types/:type", bindAuthRouteCallback(this, &SenRouter::getTypeIntrospection));
}

SenRouter::~SenRouter()
{
  getLogger()->trace("Destroying SenRouter");

  releaseAll();

  getLogger()->trace("SenRouter destroyed");
}

void SenRouter::releaseAll()
{
  for (auto& clientSession: clientSessions_)
  {
    clientSession.second.reset();
  }
  clientSessions_.clear();
  BaseRouter::releaseAll();
}

HttpResponse SenRouter::clientAuthSessionHandler([[maybe_unused]] HttpSession& httpSession,
                                                 [[maybe_unused]] const UrlParams& urlParams,
                                                 [[maybe_unused]] const QueryParams& queryParams)
{
  try
  {
    const auto payload = Json::parse(httpSession.getRequest().body());
    auto clientAuth = sen::toValue<ClientAuth>(sen::fromJson(httpSession.getRequest().body()));

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

JsonResponse SenRouter::getVersionHandler([[maybe_unused]] HttpSession& httpSession,
                                          [[maybe_unused]] const UrlParams& urlParams,
                                          [[maybe_unused]] const QueryParams& queryParams) const
{
  return JsonResponse {httpSuccess, Version {SEN_VERSION_STRING}};
}

JsonResponse SenRouter::getSessionsHandler([[maybe_unused]] ClientSession& clientSession,
                                           [[maybe_unused]] HttpSession& httpSession,
                                           [[maybe_unused]] const UrlParams& urlParams,
                                           [[maybe_unused]] const QueryParams& queryParams) const
{
  Sessions sessions;
  for (const auto& source: api_.getSessionsDiscoverer().getDetectedSources())
  {
    sessions.emplace_back(source);
  }
  return JsonResponse {httpSuccess, sessions};
}

JsonResponse SenRouter::getSessionHandler(ClientSession& clientSession,
                                          [[maybe_unused]] HttpSession& httpSession,
                                          const UrlParams& urlParams,
                                          [[maybe_unused]] const QueryParams& queryParams) const
{
  if (urlParams.size() != 1)
  {
    return getErrorInvalidUrlParams();
  }

  auto sessions = api_.getSessionsDiscoverer().getDetectedSources();
  if (find(sessions.begin(), sessions.end(), urlParams[0]) == sessions.end())
  {
    return getErrorNotFound("session");
  }

  std::set<std::string> buses;
  const auto& interests = clientSession.interestsManager().getAllInterestsSummary();

  for (const auto& [_, session, bus]: interests)
  {
    if (session == urlParams[0])
    {
      buses.emplace(bus);
    }
  }

  return JsonResponse {httpSuccess, SessionSummary {urlParams[0], {buses.begin(), buses.end()}}};
}

JsonResponse SenRouter::getInterestHandler(ClientSession& clientSession,
                                           [[maybe_unused]] HttpSession& httpSession,
                                           const UrlParams& urlParams,
                                           [[maybe_unused]] const QueryParams& queryParams) const
{
  logClientSession(clientSession, "getInterest");

  if (urlParams.size() != 1)
  {
    return getErrorInvalidUrlParams();
  }

  auto interestOpt = clientSession.interestsManager().getInterestSummary(urlParams[0]);
  if (!interestOpt.has_value())
  {
    return getErrorNotFound("interest");
  }

  return JsonResponse {httpSuccess, interestOpt.value()};
}

JsonResponse SenRouter::getInterestsHandler(ClientSession& clientSession,
                                            [[maybe_unused]] HttpSession& httpSession,
                                            [[maybe_unused]] const UrlParams& urlParams,
                                            [[maybe_unused]] const QueryParams& queryParams) const
{
  logClientSession(clientSession, "getInterests");

  auto interests = clientSession.interestsManager().getAllInterestsSummary();
  return JsonResponse {httpSuccess, interests};
}

JsonResponse SenRouter::createInterestHandler(ClientSession& clientSession,
                                              [[maybe_unused]] HttpSession& httpSession,
                                              [[maybe_unused]] const UrlParams& urlParams,
                                              [[maybe_unused]] const QueryParams& queryParams) const
{
  logClientSession(clientSession, "createInterest");

  try
  {
    const auto payload = Json::parse(httpSession.getRequest().body());

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

    bool autoSubscribeProperties = false;
    bool autoSubscribeEvents = false;

    if (payload.contains("autoSubscribe"))
    {
      if (payload["autoSubscribe"].contains("properties"))
      {
        autoSubscribeProperties = payload["autoSubscribe"].at("properties").get<bool>();
      }

      if (payload["autoSubscribe"].contains("events"))
      {
        autoSubscribeEvents = payload["autoSubscribe"].at("events").get<bool>();
      }
    }

    auto interestName = clientSession.createInterest(
      api_, busLocator, interest.name, interest.query, autoSubscribeProperties, autoSubscribeEvents);
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
                                              const UrlParams& urlParams,
                                              [[maybe_unused]] const QueryParams& queryParams) const
{
  logClientSession(clientSession, "removeInterest");

  if (urlParams.size() != 1)
  {
    return getErrorInvalidUrlParams();
  }

  if (!clientSession.interestsManager().removeInterest(urlParams[0]))
  {
    return JsonResponse(httpBadRequestError, Error {"interest deletion failed"});
  }

  return JsonResponse(httpSuccess, Success {"interest deleted"});
}

JsonResponse SenRouter::getObjectsHandler(ClientSession& clientSession,
                                          HttpSession& httpSession,
                                          const UrlParams& urlParams,
                                          [[maybe_unused]] const QueryParams& queryParams) const
{
  logClientSession(clientSession, "getObjects");

  if (urlParams.size() != 1)
  {
    return getErrorInvalidUrlParams();
  }

  auto interestSubscriptionRes = interestSubscriptionFromName(clientSession, urlParams[0]);
  if (interestSubscriptionRes.isError())
  {
    return JsonResponse(httpBadRequestError, Error {interestSubscriptionRes.getError()});
  }

  const auto& interestSubscription = interestSubscriptionRes.getValue();
  std::string rootUrl = httpSession.getRequest().path();
  ObjectsSummary objects;

  for (const auto& object: interestSubscription.subscription->list.getObjects())
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
                                         const UrlParams& urlParams,
                                         [[maybe_unused]] const QueryParams& queryParams) const
{
  logClientSession(clientSession, "getObject");

  if (urlParams.size() != 2)
  {
    return getErrorInvalidUrlParams();
  }

  auto interestSubscriptionRes = interestSubscriptionFromName(clientSession, urlParams[0]);
  if (interestSubscriptionRes.isError())
  {
    return JsonResponse(httpBadRequestError, Error {interestSubscriptionRes.getError()});
  }

  const auto& interestSubscription = interestSubscriptionRes.getValue();
  auto objectReference = getObject(interestSubscription, urlParams[1]);

  if (!objectReference)
  {
    return getErrorNotFound("object");
  }

  auto object = getObjectDefinition(httpSession.getRequest().path(), *objectReference);
  auto var = sen::toVariant(object);
  adaptForJsonResponse(var, sen::MetaTypeTrait<Object>::meta().type());
  auto jsonObject = nlohmann::json::parse(toJson(var));

  if (!queryParams.empty() && queryParams.find("includeValues")->second == "true")
  {
    nlohmann::json values;

    for (const auto& prop: objectReference->getClass()->getProperties(sen::ClassType::SearchMode::includeParents))
    {
      auto value = objectReference->getPropertyUntyped(prop.get());
      values[prop->getName()] = nlohmann::json::parse(toJson(value));
    }

    jsonObject["properties"] = values;
  }

  return JsonResponse {httpSuccess, jsonObject.dump()};
}

JsonResponse SenRouter::updateSubscriptionsHandler(ClientSession& clientSession,
                                                   [[maybe_unused]] HttpSession& httpSession,
                                                   const UrlParams& urlParams,
                                                   [[maybe_unused]] const QueryParams& queryParams) const
{
  logClientSession(clientSession, "updateSubscriptions");

  try
  {
    const auto payload = Json::parse(httpSession.getRequest().body());

    if (urlParams.size() != 2)
    {
      return getErrorInvalidUrlParams();
    }

    auto interestSubscriptionRes = interestSubscriptionFromName(clientSession, urlParams[0]);
    if (interestSubscriptionRes.isError())
    {
      return JsonResponse(httpBadRequestError, Error {interestSubscriptionRes.getError()});
    }

    const auto& interestSubscription = interestSubscriptionRes.getValue();
    const auto object = getObject(interestSubscriptionRes.getValue(), urlParams[1]);
    if (!object)
    {
      return getErrorNotFound("object");
    }

    Result<PropertyLocatorMap, JsonResponse> propertyLocatorsRes =
      Err(JsonResponse {httpInternalServerError, Error {"unexpected condition"}});
    Result<EventLocatorMap, JsonResponse> eventLocatorRes =
      Err(JsonResponse {httpInternalServerError, Error {"unexpected condition"}});

    if (payload.contains("properties"))
    {
      auto subProperties = payload.at("properties").get<Properties>();
      propertyLocatorsRes = createPropertyLocators(interestSubscription, object, subProperties);
      if (propertyLocatorsRes.isError())
      {
        return propertyLocatorsRes.getError();
      }
    }

    if (payload.contains("events"))
    {
      auto subEvents = payload.at("events").get<Events>();
      eventLocatorRes = createEventLocators(interestSubscription, object, subEvents);
      if (eventLocatorRes.isError())
      {
        return eventLocatorRes.getError();
      }
    }

    if (payload.contains("properties"))
    {
      auto locators = propertyLocatorsRes.getValue();

      bulkUnsubscribeProperties(clientSession, object, locators);

      auto propertiesResponse = bulkSubscribeProperties(clientSession, interestSubscription, object, locators);
      if (propertiesResponse.getStatusCode() != httpSuccess)
      {
        return propertiesResponse;
      }
    }

    if (payload.contains("events"))
    {
      auto locators = eventLocatorRes.getValue();

      bulkUnsubscribeEvents(clientSession, object, locators);

      auto eventsResponse = bulkSubscribeEvents(clientSession, interestSubscription, object, locators);
      if (eventsResponse.getStatusCode() != httpSuccess)
      {
        return eventsResponse;
      }
    }

    return JsonResponse {httpSuccess, Success {"Subscriptions updated"}};
  }
  catch (const std::exception& e)
  {
    return JsonResponse(httpBadRequestError, Error {"update subscriptions failed. error: " + std::string(e.what())});
  }
}

JsonResponse SenRouter::getSubscriptionsHandler(ClientSession& clientSession,
                                                [[maybe_unused]] HttpSession& httpSession,
                                                const UrlParams& urlParams,
                                                [[maybe_unused]] const QueryParams& queryParams) const
{
  logClientSession(clientSession, "getSubscriptions");

  if (urlParams.size() != 2)
  {
    return getErrorInvalidUrlParams();
  }

  auto interestSubscriptionRes = interestSubscriptionFromName(clientSession, urlParams[0]);
  if (interestSubscriptionRes.isError())
  {
    return JsonResponse(httpBadRequestError, Error {interestSubscriptionRes.getError()});
  }

  const auto object = getObject(interestSubscriptionRes.getValue(), urlParams[1]);
  if (!object)
  {
    return getErrorNotFound("object");
  }

  const auto propertyIds = clientSession.membersManager().getPropertyIds(object->getId());
  Properties propertyNames;

  for (const auto& propId: propertyIds)
  {
    auto propName = object->getClass()->searchPropertyById(propId)->getName();
    propertyNames.emplace_back(propName);
  }

  const auto eventIds = clientSession.membersManager().getEventIds(object->getId());
  Events eventNames;

  for (const auto& eventId: eventIds)
  {
    auto eventName = object->getClass()->searchEventById(eventId)->getName();
    eventNames.emplace_back(eventName);
  }

  return JsonResponse {httpSuccess, Subscriptions {propertyNames, eventNames}};
}

JsonResponse SenRouter::getPropertyHandler(ClientSession& clientSession,
                                           [[maybe_unused]] HttpSession& httpSession,
                                           const UrlParams& urlParams,
                                           [[maybe_unused]] const QueryParams& queryParams) const
{
  logClientSession(clientSession, "getProperty");

  if (urlParams.size() != 3)
  {
    return getErrorInvalidUrlParams();
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
  if (!object.get())
  {
    return getErrorNotFound("object");
  }

  if (auto prop =
        object->getClass()->searchPropertyByName(locator.property(), sen::ClassType::SearchMode::includeParents);
      prop)
  {
    return JsonResponse {httpSuccess, toJson(object->getPropertyUntyped(prop))};
  }

  return getErrorNotFound("property");
}

JsonResponse SenRouter::subscribePropertyUpdateHandler(ClientSession& clientSession,
                                                       [[maybe_unused]] HttpSession& httpSession,
                                                       const UrlParams& urlParams,
                                                       [[maybe_unused]] const QueryParams& queryParams) const
{
  logClientSession(clientSession, "subscribePropertyChange");

  if (urlParams.size() != 3)
  {
    return getErrorInvalidUrlParams();
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
    return getErrorNotFound("object");
  }

  auto prop = object->getClass()->searchPropertyByName(locator.property(), sen::ClassType::SearchMode::includeParents);
  if (!prop)
  {
    return getErrorNotFound("property");
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

  bool hasSubscribed = clientSession.membersManager().subscribeProperty(api_, urlParams[0], object, locator, opts);
  if (hasSubscribed)
  {
    return JsonResponse(httpSuccess, Success {"property change subscription succeeded"});
  }

  return JsonResponse(httpBadRequestError, Error {"property change subscription failed"});
}

JsonResponse SenRouter::unsubscribePropertyUpdateHandler(ClientSession& clientSession,
                                                         [[maybe_unused]] HttpSession& httpSession,
                                                         const UrlParams& urlParams,
                                                         [[maybe_unused]] const QueryParams& queryParams) const
{
  logClientSession(clientSession, "unsubscribePropertyChange");

  if (urlParams.size() != 3)
  {
    return getErrorInvalidUrlParams();
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
    return getErrorNotFound("object");
  }

  auto prop = object->getClass()->searchPropertyByName(locator.property(), sen::ClassType::SearchMode::includeParents);
  if (!prop)
  {
    return getErrorNotFound("property");
  }

  bool hasUnsubscribed = clientSession.membersManager().unsubscribeProperty(object->getId(), prop->getId());
  if (hasUnsubscribed)
  {
    return JsonResponse(httpSuccess, Success {"property change unsubscription succeeded"});
  }

  return JsonResponse(httpBadRequestError, Error {"property change unsubscription failed"});
}

JsonResponse SenRouter::getMethodDefinitionHandler(ClientSession& clientSession,
                                                   [[maybe_unused]] HttpSession& httpSession,
                                                   const UrlParams& urlParams,
                                                   [[maybe_unused]] const QueryParams& queryParams) const
{
  logClientSession(clientSession, "getMethodDefinition");

  if (urlParams.size() != 3)
  {
    return getErrorInvalidUrlParams();
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
                                            const UrlParams& urlParams,
                                            [[maybe_unused]] const QueryParams& queryParams) const
{
  logClientSession(clientSession, "invokeMethod");

  if (urlParams.size() != 3)
  {
    return getErrorInvalidUrlParams();
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
    return getErrorNotFound("object");
  }

  const sen::Method* method =
    object->getClass()->searchMethodByName(locator.method(), sen::ClassType::SearchMode::includeParents);
  if (!method)
  {
    return getErrorNotFound("method");
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

  Invoke invoke = clientSession.invokesManager().newInvoke(urlParams[0]);
  if (invoke.id.has_value())
  {
    object->invokeUntyped(method,
                          args,
                          {api_.getWorkQueue(),
                           [invoke, &clientSession, interest = urlParams[0]](const sen::MethodResult<Var>& result)
                           { clientSession.invokesManager().updateInvoke(interest, invoke.id.value(), result); }});

    return JsonResponse {httpSuccess, toJson(invoke)};
  }

  return JsonResponse(httpInternalServerError, Error {"unexpected condition: invalid invoke ID"});
}

JsonResponse SenRouter::getEventDefinitionHandler(ClientSession& clientSession,
                                                  [[maybe_unused]] HttpSession& httpSession,
                                                  const UrlParams& urlParams,
                                                  [[maybe_unused]] const QueryParams& queryParams) const
{
  logClientSession(clientSession, "getEventDefinition");

  if (urlParams.size() != 3)
  {
    return getErrorInvalidUrlParams();
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
                                              [[maybe_unused]] const UrlParams& urlParams,
                                              [[maybe_unused]] const QueryParams& queryParams) const
{
  logClientSession(clientSession, "subscribeEvent");

  if (urlParams.size() != 3)
  {
    return getErrorInvalidUrlParams();
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
    return getErrorNotFound("object");
  }

  const auto event = object->getClass()->searchEventByName(locator.event());
  if (!event)
  {
    return getErrorNotFound("event");
  }

  bool hasSubscribed = clientSession.membersManager().subscribeEvent(api_, urlParams[0], object, locator);
  if (hasSubscribed)
  {
    return JsonResponse(httpSuccess, Success {"event subscription succeeded"});
  }

  return JsonResponse(httpBadRequestError, Error {"event subscription failed"});
}

JsonResponse SenRouter::unsubscribeEventHandler(ClientSession& clientSession,
                                                [[maybe_unused]] HttpSession& httpSession,
                                                [[maybe_unused]] const UrlParams& urlParams,
                                                [[maybe_unused]] const QueryParams& queryParams) const
{
  logClientSession(clientSession, "unsubscribeEvent");

  if (urlParams.size() != 3)
  {
    return getErrorInvalidUrlParams();
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
    return getErrorNotFound("object");
  }

  const sen::Event* event = object->getClass()->searchEventByName(static_cast<std::string_view>(locator.event()));
  if (!event)
  {
    return getErrorNotFound("event");
  }

  bool hasUnsubscribed = clientSession.membersManager().unsubscribeEvent(object->getId(), event->getId());
  if (hasUnsubscribed)
  {
    return JsonResponse(httpSuccess, Success {"event unsubscription succeeded"});
  }

  return JsonResponse(httpBadRequestError, Error {"event unsubscription failed"});
}

JsonResponse SenRouter::getInvokeMethodStatusHandler(ClientSession& clientSession,
                                                     [[maybe_unused]] HttpSession& httpSession,
                                                     const UrlParams& urlParams,
                                                     [[maybe_unused]] const QueryParams& queryParams) const
{
  logClientSession(clientSession, "getInvokeMethodStatus");

  if (urlParams.size() != 4)
  {
    return getErrorInvalidUrlParams();
  }

  try
  {
    InvokeId invokeId = std::stoi(urlParams[3]);
    auto invokeOpt = clientSession.invokesManager().findInvoke(invokeId);
    if (invokeOpt.has_value())
    {
      return JsonResponse {httpSuccess, toJson(invokeOpt.value())};
    }
  }
  catch (...)
  {
    return JsonResponse(httpBadRequestError, Error {"invalid invocation id"});
  }

  return getErrorNotFound("invocation id");
}

JsonResponse SenRouter::getNotificationsHandler(ClientSession& clientSession,
                                                std::shared_ptr<HttpSession> httpSession,
                                                [[maybe_unused]] const UrlParams& urlParams,
                                                [[maybe_unused]] const QueryParams& queryParams,
                                                asio::ip::tcp::socket socket) const
{
  logClientSession(clientSession, "getNotificationsSSE");

  // Send SSE header to client
  HttpResponse res(httpSuccess,
                   {
                     HttpHeader("Content-Type", "text/event-stream"),
                     HttpHeader("Cache-Control", "no-cache"),
                     HttpHeader("Connection", "keep-alive"),
                   });
  std::error_code socketError = res.socketWrite(socket);
  if (socketError)
  {
    return JsonResponse(httpInternalServerError, Error {socketError.message()});
  }

  auto sharedSocket = std::make_shared<asio::ip::tcp::socket>(std::move(socket));
  auto notificationLoop = clientSession.buildNotificationLoop(httpSession, sharedSocket);
  notificationLoop->start();

  return JsonResponse(httpSuccess);
}

JsonResponse SenRouter::getTypeIntrospection([[maybe_unused]] const ClientSession& clientSession,
                                             [[maybe_unused]] HttpSession& httpSession,
                                             const UrlParams& urlParams,
                                             [[maybe_unused]] const QueryParams& queryParams) const
{
  logClientSession(clientSession, "getTypeIntrospection");

  if (urlParams.size() != 1)
  {
    return getErrorInvalidUrlParams();
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

  return getErrorNotFound("type");
}

}  // namespace sen::components::rest
