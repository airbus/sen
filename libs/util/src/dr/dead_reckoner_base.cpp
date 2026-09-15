// === dead_reckoner_base.cpp ==========================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "sen/util/dr/detail/dead_reckoner_base.h"

// sen
#include "sen/core/base/compiler_macros.h"
#include "sen/core/base/timestamp.h"
#include "sen/util/dr/algorithms.h"
#include "sen/util/dr/detail/dead_reckoner_impl.h"

// std
#include <algorithm>
#include <utility>

namespace sen::util
{

namespace
{

[[nodiscard]] bool isBodyReferenced(SpatialAlgorithm algorithm) noexcept
{
  return std::find(bodyAlgorithms.begin(), bodyAlgorithms.end(), algorithm) != bodyAlgorithms.end();
}

[[nodiscard]] Situation applyAlgorithm(SpatialAlgorithm algorithm, const Situation& value, sen::TimeStamp time)
{
  switch (algorithm)
  {
    case SpatialAlgorithm::drRVW:
      return drRvw(value, time);
    case SpatialAlgorithm::drFVW:
      return drFvw(value, time);
    case SpatialAlgorithm::drFPW:
      return drFpw(value, time);
    case SpatialAlgorithm::drRPW:
      return drRpw(value, time);
    case SpatialAlgorithm::drRVB:
      return drRvb(value, time);
    case SpatialAlgorithm::drFVB:
      return drFvb(value, time);
    case SpatialAlgorithm::drFPB:
      return drFpb(value, time);
    case SpatialAlgorithm::drRPB:
      return drRpb(value, time);
    case SpatialAlgorithm::drStatic:
      return {value.isFrozen, time, value.worldLocation, value.orientation};
    default:
      SEN_UNREACHABLE();
  }
}

}  // namespace

DeadReckonerBase::DeadReckonerBase(DrConfig config): config_ {std::move(config)} {}

Situation DeadReckonerBase::situation(sen::TimeStamp timeStamp)
{
  if (!isSituationCached(timeStamp))
  {
    const auto update = applyAlgorithm(config_.algorithm, lastSituation_, timeStamp);
    // Smoothing is only available for world centered algorithms, as in DeadReckoner.
    setCachedSituation(isBodyReferenced(config_.algorithm) ? update : smoothIfEnabled(update));
  }

  return cachedSituation_;
}

GeodeticSituation DeadReckonerBase::geodeticSituation(sen::TimeStamp timeStamp)
{
  if (!isGeodeticSituationCached(timeStamp))
  {
    setCachedGeodeticSituation(impl::toGeodeticSituation(situation(timeStamp)));
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
  lastSituation_ = impl::toSituation(value);
  invalidateCache();
}

const DrConfig& DeadReckonerBase::getConfig() const noexcept { return config_; }

void DeadReckonerBase::setConfig(const DrConfig& config) { config_ = config; }

const Situation& DeadReckonerBase::getSmoothSituation() const noexcept { return smoothSituation_; }

const Situation& DeadReckonerBase::smoothIfEnabled(const Situation& update)
{
  // The walk costs around 250 ns per fresh query; see dead_reckoner_benchmark.
  if (!config_.smoothing)
  {
    return update;
  }

  smooth(update);
  return smoothSituation_;
}

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
