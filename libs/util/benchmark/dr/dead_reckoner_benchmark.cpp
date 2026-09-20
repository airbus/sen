// === dead_reckoner_benchmark.cpp =====================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

// sen
#include "sen/core/base/checked_conversions.h"
#include "sen/util/dr/algorithms.h"
#include "sen/util/dr/dead_reckoner.h"
#include "sen/util/dr/detail/dead_reckoner_base.h"
#include "sen/util/dr/detail/dead_reckoner_impl.h"

// implementation
#include "constants.h"
#include "utils.h"

// 3rd party
#include <benchmark/benchmark.h>

// std
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <utility>
#include <variant>

namespace sen::util
{
namespace
{

// Several positions rather than one: a fixed input is hoisted out of the loop, and the equator and
// the poles take different branches through toEcef.
constexpr std::size_t positionCount = 4U;

const std::array<GeodeticWorldLocation, positionCount> geodeticPositions {{
  {48.8566, 2.3522, 35.0},
  {-33.8688, 151.2093, 120.0},
  {0.0, 0.0, 0.0},
  {78.2232, -15.6267, 11000.0},
}};

const std::array<Location, positionCount> ecefPositions {{
  {4200946.0, 172458.0, 4780110.0},
  {-4646050.0, 2553454.0, -3534640.0},
  {6378137.0, 0.0, 0.0},
  {1252172.0, -350090.0, 6237633.0},
}};

const Orientation orientation {0.4, -0.2, 1.1};
const Velocity velocity {120.0, -45.0, 8.0};
const Acceleration acceleration {1.5, 0.25, -0.75};

// The same inputs as whole situations, for the shared-rotation cases below.
const std::array<Situation, positionCount> ecefSituations {{
  {false, {}, ecefPositions[0], orientation, velocity, {}, acceleration, {}},
  {false, {}, ecefPositions[1], orientation, velocity, {}, acceleration, {}},
  {false, {}, ecefPositions[2], orientation, velocity, {}, acceleration, {}},
  {false, {}, ecefPositions[3], orientation, velocity, {}, acceleration, {}},
}};

const std::array<GeodeticSituation, positionCount> geodeticSituations {{
  {false, {}, geodeticPositions[0], orientation, velocity, {}, acceleration, {}},
  {false, {}, geodeticPositions[1], orientation, velocity, {}, acceleration, {}},
  {false, {}, geodeticPositions[2], orientation, velocity, {}, acceleration, {}},
  {false, {}, geodeticPositions[3], orientation, velocity, {}, acceleration, {}},
}};

//----------------------------------------------------------------------------------------------------------------
// Whole conversions
//----------------------------------------------------------------------------------------------------------------

// One rotation built per vector, which is the shape the callers had.
void geodeticSituationConversion(benchmark::State& state)
{
  std::size_t index = 0U;

  for (auto _: state)
  {
    const auto& ecefPosition = ecefPositions[index];
    index = (index + 1U) % positionCount;

    auto geoLocation = impl::toLla(ecefPosition);
    benchmark::DoNotOptimize(geoLocation);
    auto convertedOrientation = impl::ecefToNed(orientation, geoLocation);
    benchmark::DoNotOptimize(convertedOrientation);
    auto convertedVelocity = impl::ecefToNed(velocity, geoLocation);
    benchmark::DoNotOptimize(convertedVelocity);
    auto convertedAcceleration = impl::ecefToNed(acceleration, geoLocation);
    benchmark::DoNotOptimize(convertedAcceleration);
  }
}

BENCHMARK(geodeticSituationConversion);

// The same in reverse.
void ecefSituationConversion(benchmark::State& state)
{
  std::size_t index = 0U;

  for (auto _: state)
  {
    const auto& geoLocation = geodeticPositions[index];
    index = (index + 1U) % positionCount;

    auto ecefPosition = impl::toEcef(geoLocation);
    benchmark::DoNotOptimize(ecefPosition);
    auto convertedOrientation = impl::nedToEcef(orientation, geoLocation);
    benchmark::DoNotOptimize(convertedOrientation);
    auto convertedVelocity = impl::nedToEcef(velocity, geoLocation);
    benchmark::DoNotOptimize(convertedVelocity);
    auto convertedAcceleration = impl::nedToEcef(acceleration, geoLocation);
    benchmark::DoNotOptimize(convertedAcceleration);
  }
}

BENCHMARK(ecefSituationConversion);

// The same conversions with one rotation shared across the vectors.
void geodeticSituationShared(benchmark::State& state)
{
  std::size_t index = 0U;

  for (auto _: state)
  {
    auto result = impl::toGeodeticSituation(ecefSituations[index]);
    index = (index + 1U) % positionCount;
    benchmark::DoNotOptimize(result);
  }
}

BENCHMARK(geodeticSituationShared);

void ecefSituationShared(benchmark::State& state)
{
  std::size_t index = 0U;

  for (auto _: state)
  {
    auto result = impl::toSituation(geodeticSituations[index]);
    index = (index + 1U) % positionCount;
    benchmark::DoNotOptimize(result);
  }
}

BENCHMARK(ecefSituationShared);

//----------------------------------------------------------------------------------------------------------------
// The pieces underneath
//----------------------------------------------------------------------------------------------------------------

void toLla(benchmark::State& state)
{
  std::size_t index = 0U;

  for (auto _: state)
  {
    auto result = impl::toLla(ecefPositions[index]);
    benchmark::DoNotOptimize(result);
    index = (index + 1U) % positionCount;
  }
}

BENCHMARK(toLla);

void toEcef(benchmark::State& state)
{
  std::size_t index = 0U;

  for (auto _: state)
  {
    auto result = impl::toEcef(geodeticPositions[index]);
    benchmark::DoNotOptimize(result);
    index = (index + 1U) % positionCount;
  }
}

BENCHMARK(toEcef);

// Velocity and acceleration both reach this.
void ecefToNedVector(benchmark::State& state)
{
  std::size_t index = 0U;

  for (auto _: state)
  {
    auto result = impl::ecefToNed(velocity, geodeticPositions[index]);
    benchmark::DoNotOptimize(result);
    index = (index + 1U) % positionCount;
  }
}

BENCHMARK(ecefToNedVector);

// The orientation overload.
void ecefToNedOrientation(benchmark::State& state)
{
  std::size_t index = 0U;

  for (auto _: state)
  {
    auto result = impl::ecefToNed(orientation, geodeticPositions[index]);
    benchmark::DoNotOptimize(result);
    index = (index + 1U) % positionCount;
  }
}

BENCHMARK(ecefToNedOrientation);

// The rotation alone, with no vector applied.
void buildRotation(benchmark::State& state)
{
  std::size_t index = 0U;

  for (auto _: state)
  {
    const auto& position = geodeticPositions[index];
    index = (index + 1U) % positionCount;

    auto rotation = Quatd {toRad(position.longitude), -halfPi - toRad(position.latitude), 0.0};
    benchmark::DoNotOptimize(rotation);
  }
}

BENCHMARK(buildRotation);

//----------------------------------------------------------------------------------------------------------------
// Inside the orientation conversion
//----------------------------------------------------------------------------------------------------------------

void nedTrihedron(benchmark::State& state)
{
  std::size_t index = 0U;

  for (auto _: state)
  {
    auto result = getNedTrihedron(geodeticPositions[index]);
    index = (index + 1U) % positionCount;
    benchmark::DoNotOptimize(result);
  }
}

BENCHMARK(nedTrihedron);

void eulerFromTrihedrons(benchmark::State& state)
{
  const auto trihedron = getNedTrihedron(geodeticPositions[1]);
  const Quatd input {orientation.psi, orientation.theta, orientation.phi};
  const auto xf = input * Vec3d {1, 0, 0};
  const auto yf = input * Vec3d {0, 1, 0};

  for (auto _: state)
  {
    auto result = eulerAnglesFromTrihedrons(trihedron[0], trihedron[1], trihedron[2], xf, yf);
    benchmark::DoNotOptimize(result);
  }
}

BENCHMARK(eulerFromTrihedrons);

// One quaternion applied to one vector.
void rotateVector(benchmark::State& state)
{
  const Quatd rotation {orientation.psi, orientation.theta, orientation.phi};
  const Vec3d input {1.0, 2.0, 3.0};

  for (auto _: state)
  {
    auto result = rotation * input;
    benchmark::DoNotOptimize(result);
  }
}

BENCHMARK(rotateVector);

// A bare atan2, for scale against the three in the Euler recovery.
void atan2Call(benchmark::State& state)
{
  double numerator = 0.7;

  for (auto _: state)
  {
    auto result = std::atan2(numerator, 1.3);
    numerator += 1e-9;
    benchmark::DoNotOptimize(result);
  }
}

BENCHMARK(atan2Call);

// Angles are f32 and reach the quaternion through a checked conversion each. Against
// buildRotation, which takes f64 directly, the pair prices those conversions.
void orientationToQuat(benchmark::State& state)
{
  for (auto _: state)
  {
    auto result = fromOrientationToQuat(orientation);
    benchmark::DoNotOptimize(result);
  }
}

BENCHMARK(orientationToQuat);

// Widening cannot lose a value, so both range guards inside are dead.
void checkedWidening(benchmark::State& state)
{
  float input = 1.25F;

  for (auto _: state)
  {
    auto result = std_util::checkedConversion<double>(input);
    input += 1e-6F;
    benchmark::DoNotOptimize(result);
  }
}

BENCHMARK(checkedWidening);

void plainWidening(benchmark::State& state)
{
  float input = 1.25F;

  for (auto _: state)
  {
    auto result = static_cast<double>(input);
    input += 1e-6F;
    benchmark::DoNotOptimize(result);
  }
}

BENCHMARK(plainWidening);

//----------------------------------------------------------------------------------------------------------------
// The rest of the per cycle work
//----------------------------------------------------------------------------------------------------------------

const Situation extrapolationInput {false,
                                    sen::TimeStamp {std::chrono::seconds(0)},
                                    ecefPositions[1],
                                    orientation,
                                    velocity,
                                    AngularVelocity {0.05, -0.02, 0.01},
                                    acceleration,
                                    AngularAcceleration {0.001, 0.002, -0.001}};

// The lightest algorithm.
void extrapolateFpw(benchmark::State& state)
{
  const sen::TimeStamp at {std::chrono::milliseconds(16)};

  for (auto _: state)
  {
    auto result = drFpw(extrapolationInput, at);
    benchmark::DoNotOptimize(result);
  }
}

BENCHMARK(extrapolateFpw);

// The heaviest algorithm.
void extrapolateRvb(benchmark::State& state)
{
  const sen::TimeStamp at {std::chrono::milliseconds(16)};

  for (auto _: state)
  {
    auto result = drRvb(extrapolationInput, at);
    benchmark::DoNotOptimize(result);
  }
}

BENCHMARK(extrapolateRvb);

// The algorithm the reckoner base uses.
void extrapolateRvw(benchmark::State& state)
{
  const sen::TimeStamp at {std::chrono::milliseconds(16)};

  for (auto _: state)
  {
    auto result = drRvw(extrapolationInput, at);
    benchmark::DoNotOptimize(result);
  }
}

BENCHMARK(extrapolateRvw);

//----------------------------------------------------------------------------------------------------------------
// A whole reckoner: the conversions plus the cache, the smoothing and the dispatch
//----------------------------------------------------------------------------------------------------------------

const GeodeticSituation publishInput {false,
                                      sen::TimeStamp {std::chrono::seconds(0)},
                                      geodeticPositions[1],
                                      orientation,
                                      velocity,
                                      AngularVelocity {0.05, -0.02, 0.01},
                                      acceleration,
                                      AngularAcceleration {0.001, 0.002, -0.001}};

// The publish side of a cycle.
void reckonerUpdate(benchmark::State& state)
{
  DeadReckonerBase reckoner {DrConfig {}};

  for (auto _: state)
  {
    reckoner.updateGeodeticSituation(publishInput);
  }
}

BENCHMARK(reckonerUpdate);

// Timestamp advancing, so every query misses the cache.
void reckonerReadFresh(benchmark::State& state)
{
  DrConfig config {};
  config.smoothing = state.range(0) != 0;
  if (state.range(0) == 2)
  {
    // Forces smooth() down its early return, isolating the smoothing cost.
    config.maxDeltaTime = sen::Duration {0};
  }
  DeadReckonerBase reckoner {config};
  reckoner.updateGeodeticSituation(publishInput);

  int64_t tick = 1;

  for (auto _: state)
  {
    auto result = reckoner.geodeticSituation(sen::TimeStamp {std::chrono::milliseconds(16 * tick)});
    ++tick;
    benchmark::DoNotOptimize(result);
  }
}

BENCHMARK(reckonerReadFresh)->Arg(0)->Arg(1)->Arg(2);

// The same query repeated, answered by the cache.
void reckonerReadCached(benchmark::State& state)
{
  DeadReckonerBase reckoner {DrConfig {}};
  reckoner.updateGeodeticSituation(publishInput);
  const sen::TimeStamp at {std::chrono::milliseconds(16)};
  auto warm = reckoner.geodeticSituation(at);
  benchmark::DoNotOptimize(warm);

  for (auto _: state)
  {
    auto result = reckoner.geodeticSituation(at);
    benchmark::DoNotOptimize(result);
  }
}

BENCHMARK(reckonerReadCached);

// Smoothing steps from the last smoothed time to the query in smoothingInterval increments, so its
// cost follows the gap between queries. The argument is that gap in milliseconds.
void reckonerReadByInterval(benchmark::State& state)
{
  DeadReckonerBase reckoner {DrConfig {}};
  reckoner.updateGeodeticSituation(publishInput);

  const auto gap = std::chrono::milliseconds(state.range(0));
  int64_t tick = 1;

  for (auto _: state)
  {
    auto result = reckoner.geodeticSituation(sen::TimeStamp {gap * tick});
    ++tick;
    benchmark::DoNotOptimize(result);
  }
}

BENCHMARK(reckonerReadByInterval)->Arg(16)->Arg(40)->Arg(100)->Arg(200)->Arg(500)->Arg(1000);

//----------------------------------------------------------------------------------------------------------------
// The templated reckoner, which is the layer a consumer over a Sen object actually uses
//----------------------------------------------------------------------------------------------------------------

// The templated reckoner is the layer a consumer over a Sen object instantiates. Every other
// reckoner case here drives DeadReckonerBase directly and never sees a spatial variant.
//
// Per query this adds the variant comparison in updateSpatial, the algorithm switch on the
// variant's index, the five fromRpr conversions, and the body-reference check.
//
// The stub's spatial never changes, so updateSpatial compares and returns: this measures the
// steady-state query, not the cost of an arriving update.

// One struct per field: each fromRpr helper reads the member names its RPR counterpart has, so one
// struct of three doubles for all five does not compile.
// The comparisons are free functions: a struct carrying member functions trips the check that
// forbids public data members on a class that has any.
struct BenchLocation
{
  double x, y, z;
};

struct BenchOrientation
{
  double psi, theta, phi;
};

struct BenchVelocity
{
  double xVelocity, yVelocity, zVelocity;
};

struct BenchAcceleration
{
  double xAcceleration, yAcceleration, zAcceleration;
};

struct BenchAngularVelocity
{
  double xAngularVelocity, yAngularVelocity, zAngularVelocity;
};

// One struct per spatial kind, five covering nine variant positions: the body-referenced algorithms
// reuse the world-referenced structs. Making all nine the same type leaves the visitor's overload
// set ambiguous. Each carries the full member set; a lambda reads only what its algorithm needs.
struct BenchStaticSpatial
{
  bool isFrozen;
  BenchLocation worldLocation;
  BenchOrientation orientation;
  BenchVelocity velocityVector;
  BenchAngularVelocity angularVelocity;
  BenchAcceleration accelerationVector;
};

bool operator==(const BenchStaticSpatial& left, const BenchStaticSpatial& right) noexcept
{
  return left.isFrozen == right.isFrozen && left.worldLocation.x == right.worldLocation.x &&
         left.worldLocation.y == right.worldLocation.y && left.worldLocation.z == right.worldLocation.z;
}

bool operator!=(const BenchStaticSpatial& left, const BenchStaticSpatial& right) noexcept { return !(left == right); }

struct BenchFpsSpatial
{
  bool isFrozen;
  BenchLocation worldLocation;
  BenchOrientation orientation;
  BenchVelocity velocityVector;
  BenchAngularVelocity angularVelocity;
  BenchAcceleration accelerationVector;
};

bool operator==(const BenchFpsSpatial& left, const BenchFpsSpatial& right) noexcept
{
  return left.isFrozen == right.isFrozen && left.worldLocation.x == right.worldLocation.x &&
         left.worldLocation.y == right.worldLocation.y && left.worldLocation.z == right.worldLocation.z;
}

bool operator!=(const BenchFpsSpatial& left, const BenchFpsSpatial& right) noexcept { return !(left == right); }

struct BenchRpsSpatial
{
  bool isFrozen;
  BenchLocation worldLocation;
  BenchOrientation orientation;
  BenchVelocity velocityVector;
  BenchAngularVelocity angularVelocity;
  BenchAcceleration accelerationVector;
};

bool operator==(const BenchRpsSpatial& left, const BenchRpsSpatial& right) noexcept
{
  return left.isFrozen == right.isFrozen && left.worldLocation.x == right.worldLocation.x &&
         left.worldLocation.y == right.worldLocation.y && left.worldLocation.z == right.worldLocation.z;
}

bool operator!=(const BenchRpsSpatial& left, const BenchRpsSpatial& right) noexcept { return !(left == right); }

struct BenchRvsSpatial
{
  bool isFrozen;
  BenchLocation worldLocation;
  BenchOrientation orientation;
  BenchVelocity velocityVector;
  BenchAngularVelocity angularVelocity;
  BenchAcceleration accelerationVector;
};

bool operator==(const BenchRvsSpatial& left, const BenchRvsSpatial& right) noexcept
{
  return left.isFrozen == right.isFrozen && left.worldLocation.x == right.worldLocation.x &&
         left.worldLocation.y == right.worldLocation.y && left.worldLocation.z == right.worldLocation.z;
}

bool operator!=(const BenchRvsSpatial& left, const BenchRvsSpatial& right) noexcept { return !(left == right); }

struct BenchFvsSpatial
{
  bool isFrozen;
  BenchLocation worldLocation;
  BenchOrientation orientation;
  BenchVelocity velocityVector;
  BenchAngularVelocity angularVelocity;
  BenchAcceleration accelerationVector;
};

bool operator==(const BenchFvsSpatial& left, const BenchFvsSpatial& right) noexcept
{
  return left.isFrozen == right.isFrozen && left.worldLocation.x == right.worldLocation.x &&
         left.worldLocation.y == right.worldLocation.y && left.worldLocation.z == right.worldLocation.z;
}

bool operator!=(const BenchFvsSpatial& left, const BenchFvsSpatial& right) noexcept { return !(left == right); }

// The variant's order is the algorithm selector: DeadReckonerTemplateBase switches on index().
// Index three is RVW, which the base-class cases above measure.
using BenchSpatialVariant = std::variant<BenchStaticSpatial,  // 0 static
                                         BenchFpsSpatial,     // 1 FPW
                                         BenchRpsSpatial,     // 2 RPW
                                         BenchRvsSpatial,     // 3 RVW -- the arm exercised here
                                         BenchFvsSpatial,     // 4 FVW
                                         BenchFpsSpatial,     // 5 FPB
                                         BenchRpsSpatial,     // 6 RPB
                                         BenchRvsSpatial,     // 7 RVB
                                         BenchFvsSpatial>;    // 8 FVB

// The minimum an entity must offer for DeadReckoner<T> to compile against it.
class BenchEntity
{
public:
  [[nodiscard]] const BenchSpatialVariant& getSpatial() const noexcept { return spatial_; }

private:
  BenchSpatialVariant spatial_ {std::in_place_index<3U>,
                                BenchRvsSpatial {false,
                                                 {4200946.0, 172458.0, 4780110.0},
                                                 {0.4, -0.2, 1.1},
                                                 {120.0, -45.0, 8.0},
                                                 {0.0, 0.0, 0.0},
                                                 {1.5, 0.25, -0.75}}};
};

// Smoothing off against smoothing on, and the two must differ: smoothing walks back over the
// recorded history on every fresh query. Arms costing the same mean the flag is not reaching
// smoothIfEnabled, which extrapolateAt calls -- the templated class overrides situation(), so it
// never goes through the base's.
//
// Only world-referenced algorithms reach it. The body-referenced arms return the update unsmoothed,
// so a case pointed at one of those would compare two identical paths.
void templatedReckonerRead(benchmark::State& state)
{
  BenchEntity entity;
  DrConfig config {};
  config.smoothing = state.range(0) != 0;
  DeadReckoner<BenchEntity> reckoner {entity, config};

  int64_t tick = 1;

  for (auto _: state)
  {
    auto result = reckoner.situation(sen::TimeStamp {std::chrono::milliseconds(16 * tick)});
    ++tick;
    benchmark::DoNotOptimize(result);
  }
}

// Repetitions with the spread reported, because the claim is a difference between two arms rather
// than a value.
BENCHMARK(templatedReckonerRead)->Arg(0)->Arg(1)->Repetitions(9)->ReportAggregatesOnly(false);

// Control: reckonerReadFresh above, the same two arms on DeadReckonerBase, which nothing done to
// this path can move.

}  // namespace
}  // namespace sen::util
