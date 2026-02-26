// === shape_listener.cpp ==============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

// generated code
#include "stl/shape_listener.stl.h"
#include "stl/shapes.stl.h"

// sen
#include "sen/core/base/checked_conversions.h"
#include "sen/core/base/compiler_macros.h"
#include "sen/core/meta/class_type.h"
#include "sen/core/obj/connection_guard.h"
#include "sen/core/obj/interest.h"
#include "sen/core/obj/object.h"
#include "sen/core/obj/object_list.h"
#include "sen/core/obj/subscription.h"
#include "sen/kernel/component_api.h"

// std
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <list>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>

namespace shapes
{

class ShapeListenerImpl: public ShapeListenerBase
{
  SEN_NOCOPY_NOMOVE(ShapeListenerImpl)

public:
  using ShapeListenerBase::ShapeListenerBase;
  ~ShapeListenerImpl() override = default;

protected:
  void registered(sen::kernel::RegistrationApi& api) override { api_ = &api; }  // save the api for later usage

protected:
  std::string startListeningToImpl(const std::string& bus,
                                   const MaybeColor& color,
                                   const MaybeInterval& xRange,
                                   const MaybeInterval& yRange) override
  {
    const auto query = makeQueryName();                                // create a name for our query
    auto sub = std::make_shared<sen::Subscription<ShapeInterface>>();  // create up a subscription
    sub->source = api_->getSource(bus);                                // get the bus

    // install the callbacks
    std::ignore = sub->list.onAdded([query, this](const auto& iterators) { shapesDetected(query, iterators); });
    std::ignore = sub->list.onRemoved([query, this](const auto& iterators) { shapesGone(query, iterators); });

    auto interest = makeInterest(query, bus, color, xRange, yRange);  // build the interest.
    sub->source->addSubscriber(interest, &sub->list, true);           // connect the list.
    subscriptions_.emplace(query, std::move(sub));                    // save the subscription.

    // update the query count
    setNextQueryCount(
      sen::std_util::checkedConversion<uint32_t, sen::std_util::ReportPolicyIgnore>(subscriptions_.size()));
    return query;
  }

  void stopListeningToImpl(const std::string& queryName) override
  {
    if (auto itr = subscriptions_.find(queryName); itr != subscriptions_.end())
    {
      itr->second->source->removeSubscriber(&itr->second->list, true);          // remove all subscriptions.
      subscriptions_.erase(itr);                                                // erase the entry.
      std::cout << getName() << ": [" << queryName << "] Stopped listening\n";  // print a log.
      setNextQueryCount(sen::std_util::checkedConversion<uint32_t, sen::std_util::ReportPolicyLog>(
        subscriptions_.size()));  // update the query count.
    }
  }

  void stopListeningImpl() override
  {
    // delete all collected entries
    auto subsCopy = subscriptions_;
    for (const auto& [query, _]: subsCopy)
    {
      stopListeningToImpl(query);
    }
    subscriptions_.clear();  // ensure we are clear
  }

  [[nodiscard]] NameList getAllDetectedShapesImpl() override
  {
    NameList result;                               // create the result container
    for (const auto& [name, sub]: subscriptions_)  // fill it in
    {
      for (const auto& obj: sub->list.getUntypedObjects())
      {
        result.push_back(obj->getName());
      }
    }
    return result;
  }

private:
  void shapesDetected(std::string query, const sen::ObjectList<ShapeInterface>::Iterators& shapes)
  {
    const std::string ourName = getName();

    for (auto shape = shapes.typedBegin; shape != shapes.typedEnd; ++shape)  // go over all the detected shapes
    {
      auto& shapeAsObject = (*shape)->asObject();             // get the raw object
      const std::string shapeName = shapeAsObject.getName();  // extract the name

      std::cout << ourName << ": [" << query << "] Got a " << (*shape)->getColor() << " "
                << getGeometryName((*shape)->getGeometry()) << " named " << shapeName << " at\n"
                << (*shape)->getPosition() << "\n";

      // print some info when we detect a collision
      auto handler = [=](auto wall)
      { std::cout << ourName << ": [" << query << "] " << shapeName << " hit the " << wall << "\n"; };

      auto guard = (*shape)->onCollidedWithWall({this, std::move(handler)});  // install the callback.
      shapeGuards_[shapeAsObject.getId()].push_back(std::move(guard));        // save the guard.
      totalShapesCount_++;                                                    // update our total shapes count.
    }

    setNextDetectedShapesCount(totalShapesCount_);
  }

  void shapesGone(std::string_view query, const sen::ObjectList<ShapeInterface>::Iterators& shapes)
  {
    // go over all the now gone shapes
    for (auto shape = shapes.typedBegin; shape != shapes.typedEnd; ++shape)
    {
      auto& shapeAsObject = (*shape)->asObject();

      // print a log
      std::cout << getName() << ": [" << query << "] Lost a " << (*shape)->getColor() << " "
                << getGeometryName((*shape)->getGeometry()) << " named " << shapeAsObject.getName() << " at\n"
                << (*shape)->getPosition() << "\n";

      shapeGuards_.erase(shapeAsObject.getId());  // delete the guards.
      totalShapesCount_--;                        // update our total shapes count.
    }

    setNextDetectedShapesCount(totalShapesCount_);
  }

  static std::string_view getGeometryName(const Geometry& geom)
  {
    // NOLINTNEXTLINE(misc-include-cleaner)
    return sen::MetaTypeTrait<Geometry>::meta()
      ->getFieldFromKey(sen::std_util::checkedConversion<uint32_t, sen::std_util::ReportPolicyIgnore>(geom.index()))
      ->type->getName();
  }

private:
  std::shared_ptr<sen::Interest> makeInterest(std::string_view queryName,
                                              const std::string& bus,
                                              const MaybeColor& color,
                                              const MaybeInterval& xRange,
                                              const MaybeInterval& yRange)
  {
    // we build a string encoding our interest using the Sen Query Language
    std::string query("SELECT ");  // NOLINTNEXTLINE(misc-include-cleaner)
    query.append(sen::MetaTypeTrait<ShapeInterface>::meta()->getQualifiedName()).append(" FROM ").append(bus);

    bool whereWritten = false;
    addColorCondition(color, whereWritten, query);
    addRangeCondition("position.x", xRange, whereWritten, query);
    addRangeCondition("position.y", yRange, whereWritten, query);

    std::cout << getName() << ": " << queryName << " = '" << query << "'\n";  // log the query that we create.
    return sen::Interest::make(query, api_->getTypes());                      // make the interest.
  }

  std::string makeQueryName() { return std::string("query_").append(std::to_string(nextQueryId_++)); }

  static void writeWhereOrAnd(bool& whereWritten, std::string& query)
  {
    query.append(whereWritten ? " AND " : " WHERE ");
    whereWritten = true;
  }

  static void addColorCondition(const MaybeColor& color, bool& whereWritten, std::string& query)
  {
    if (color.has_value())
    {
      writeWhereOrAnd(whereWritten, query);  // NOLINTNEXTLINE(misc-include-cleaner)
      query.append("color IN (\"").append(sen::toString(color.value())).append("\")");
    }
  }

  static void addRangeCondition(const std::string& variable,
                                const MaybeInterval& interval,
                                bool& whereWritten,
                                std::string& query)
  {
    if (interval.has_value())
    {
      writeWhereOrAnd(whereWritten, query);
      query.append(variable)
        .append(" BETWEEN ")
        .append(std::to_string(interval.value().min))
        .append(" AND ")
        .append(std::to_string(interval.value().max));
    }
  }

private:
  std::unordered_map<std::string, std::shared_ptr<sen::Subscription<ShapeInterface>>> subscriptions_;
  std::unordered_map<sen::ObjectId, std::list<sen::ConnectionGuard>> shapeGuards_;
  sen::kernel::KernelApi* api_ = nullptr;
  uint32_t nextQueryId_ = 1;
  uint32_t totalShapesCount_ = 0;
};

SEN_EXPORT_CLASS(ShapeListenerImpl)

}  // namespace shapes
