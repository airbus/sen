// === dead_reckoner_base.cpp ==========================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "sen/util/dr/detail/dead_reckoner_base.h"

// sen
#include "sen/core/base/timestamp.h"
#include "sen/util/dr/algorithms.h"
#include "sen/util/dr/detail/dead_reckoner_impl.h"

// std
#include <utility>

namespace sen::util
{

DeadReckonerBase::DeadReckonerBase(DrConfig config): config_ {std::move(config)} {}

Situation DeadReckonerBase::situation(sen::TimeStamp timeStamp)
{
  if (!isSituationCached(timeStamp))
  {
    const auto update = drRvw(lastSituation_, timeStamp);
    smooth(update);
    setCachedSituation(config_.smoothing ? smoothSituation_ : update);
  }

  return cachedSituation_;
}

GeodeticSituation DeadReckonerBase::geodeticSituation(sen::TimeStamp timeStamp)
{
  if (!isGeodeticSituationCached(timeStamp))
  {
    const auto sitVal = situation(timeStamp);
    const auto geoLocation = impl::toLla(sitVal.worldLocation);
    setCachedGeodeticSituation(GeodeticSituation {sitVal.isFrozen,
                                                  sitVal.timeStamp,
                                                  geoLocation,
                                                  impl::ecefToNed(sitVal.orientation, geoLocation),
                                                  impl::ecefToNed(sitVal.velocityVector, geoLocation),
                                                  sitVal.angularVelocity,
                                                  impl::ecefToNed(sitVal.accelerationVector, geoLocation),
                                                  sitVal.angularAcceleration});
  }

  return cachedGeodeticSituation_;
}

void DeadReckonerBase::updateSituation(const Situation& value)
{
  lastSituation_ = value;
  invalidateCache();
}

void DeadReckonerBase::updateGeodeticSituation(const GeodeticSituation& value)
{
  lastSituation_ = Situation {value.isFrozen,
                              value.timeStamp,
                              impl::toEcef(value.worldLocation),
                              impl::nedToEcef(value.orientation, value.worldLocation),
                              impl::nedToEcef(value.velocityVector, value.worldLocation),
                              value.angularVelocity,
                              impl::nedToEcef(value.accelerationVector, value.worldLocation),
                              value.angularAcceleration};
  invalidateCache();
}

const DrConfig& DeadReckonerBase::getConfig() const noexcept { return config_; }

void DeadReckonerBase::setConfig(const DrConfig& config) { config_ = config; }

const Situation& DeadReckonerBase::getSmoothSituation() const noexcept { return smoothSituation_; }

void DeadReckonerBase::smooth(const Situation& update)
{
  // do not smooth when frozen and for long differences in time or distance
  if (update.isFrozen || update.timeStamp - smoothSituation_.timeStamp > config_.maxDeltaTime ||
      impl::computeDistance(smoothSituation_.worldLocation, update.worldLocation) > config_.maxDistance)
  {
    smoothSituation_ = update;
    return;
  }

  impl::smoothImpl(smoothSituation_, update, config_);
}

}  // namespace sen::util
