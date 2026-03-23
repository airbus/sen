// === gradient_noise.h ================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_CORE_BASE_GRADIENT_NOISE_H
#define SEN_CORE_BASE_GRADIENT_NOISE_H

#include "sen/core/base/assert.h"
#include "sen/core/base/class_helpers.h"

#include <array>
#include <mutex>
#include <random>

namespace sen
{

namespace impl
{
template <typename T>
[[nodiscard]] inline T makeRandomSeed();
}  // namespace impl

/// Utility class for generating n-dimensional gradient (Perlin-style) noise. \ingroup hash
/// Mainly useful in tests and examples.
/// @tparam FloatType       Floating-point type used for positions and output values (e.g. `float` or `double`).
/// @tparam dimensionCount  Number of spatial dimensions (e.g. `1` for 1-D, `2` for 2-D noise).
template <typename FloatType, std::size_t dimensionCount>
class GradientNoise final
{
  SEN_MOVE_ONLY(GradientNoise)

public:
  using EngineType = std::default_random_engine;
  using DistType = std::uniform_real_distribution<FloatType>;
  using SeedType = EngineType::result_type;

  /// The number of random numbers discarded after a new engine seed.
  static constexpr unsigned int discardCount = 1;

public:
  explicit GradientNoise(): seed_(impl::makeRandomSeed<FloatType>()) {}

  ~GradientNoise() = default;

public:
  /// Seeds the pseudo-random generator used for gradient computation.
  /// @param val  New seed value; defaults to the engine's built-in default seed.
  void seed(SeedType val = EngineType::default_seed) { seed_ = val; }

  /// Returns a noise value sampled at the given n-dimensional position.
  /// @param position  Array of `dimensionCount` coordinates at which to evaluate the noise.
  /// @return Noise value in the range `[-1, 1]`.
  FloatType operator()(std::array<FloatType, dimensionCount> position);

private:
  SeedType seed_;
};

//----------------------------------------------------------------------------------------------------------------------
// Inline implementation
//----------------------------------------------------------------------------------------------------------------------

namespace impl
{

template <typename T>
[[nodiscard]] constexpr T cubicInterpolation(T y0, T y1, T y2, T y3, T mu) noexcept
{
  const auto a0 = y3 - y2 - y0 + y1;
  const auto a1 = y0 - y1 - a0;
  const auto a2 = y2 - y0;
  const auto a3 = y1;
  const auto mu2 = mu * mu;
  return a0 * mu * mu2 + a1 * mu2 + a2 * mu + a3;
}

template <const unsigned int exponent>
[[nodiscard]] constexpr int ipowExp(int base)
{
  return (exponent & 1U ? base : 1) * ipowExp<exponent / 2>(base * base);
}

[[nodiscard]] constexpr int ipow(int base, unsigned int exponent)
{
  if (exponent == 0 && base == 0)
  {
    throwRuntimeError("0^0 is undefined");
  }

  if (base == 2)
  {
    return static_cast<int>(1U << exponent);
  }

  int result = 1;
  int term = base;
  while (exponent != 0U)
  {
    if ((exponent & 1U) != 0U)
    {
      result *= term;
    }
    term *= term;
    exponent /= 2;
  }
  return result;
}

template <typename T>
[[nodiscard]] inline T makeRandomSeed()
{
  static std::random_device rd;
  static std::mt19937 gen(rd());
  static std::mutex globalRandomGeneratorMutex;

  const std::lock_guard lock(globalRandomGeneratorMutex);
  if constexpr (std::is_floating_point_v<T>)
  {
    std::uniform_real_distribution<T> distribution;
    return distribution(gen);
  }

  if constexpr (std::is_integral_v<T>)
  {
    std::uniform_int_distribution<T> distribution;
    return distribution(gen);
  }
}

}  // namespace impl

template <typename FloatType, std::size_t dimensionCount>
inline FloatType GradientNoise<FloatType, dimensionCount>::operator()(std::array<FloatType, dimensionCount> position)
{
  std::array<SeedType, dimensionCount> unitPosition;
  std::array<FloatType, dimensionCount> offsetPosition = position;

  for (std::size_t dimension = 0; dimension < dimensionCount; dimension++)
  {
    // The unit position is the same as the position, but it's the integer floor of all axis positions
    unitPosition.at(dimension) = static_cast<int>(floor(position.at(dimension)));

    // The offset position is the position within the node 'cell'
    offsetPosition.at(dimension) -= unitPosition.at(dimension);
  }

  // The pseudo-random nodes to be interpolated
  std::array<FloatType, impl::ipow(4, dimensionCount)> nodes;

  for (std::size_t node = 0; node < nodes.size(); node++)
  {
    std::array<SeedType, dimensionCount + 1> nodePosition;
    nodePosition[dimensionCount] = seed_ * 2;  // Use the private seed as an extra dimension

    for (std::size_t dimension = 0; dimension < dimensionCount; dimension++)
    {
      // Get the n-dimensional position of the node
      nodePosition.at(dimension) =
        (node / static_cast<int>(pow(4, static_cast<double>(dimension)))) % 4 + unitPosition.at(dimension);
    }

    std::seed_seq seq(nodePosition.begin(), nodePosition.end());  // Make a seed sequence from the node position
    std::array<SeedType, 1> nodeSeed {};
    seq.generate(nodeSeed.begin(), nodeSeed.end());

    EngineType engine(nodeSeed[0]);
    engine.discard(discardCount);                  // Escape from zero-land
    nodes.at(node) = DistType(-1.0, 1.0)(engine);  // Get node value
  }

  for (std::size_t dimension = 0; dimension < dimensionCount; dimension++)
  {
    const auto interpolatedNodeCount = (nodes.size() / 4) / static_cast<int>(pow(4, static_cast<double>(dimension)));

    for (std::size_t interpolatedNode = 0; interpolatedNode < interpolatedNodeCount; interpolatedNode++)
    {
      // The node that will be overwritten with the interpolated node (every fourth node is overwritten)
      const auto node = interpolatedNode * 4;

      // Overwrite every 4th node with an interpolation of the 3 preceding nodes including the overwritten node
      nodes.at(interpolatedNode) = ::sen::impl::cubicInterpolation(
        nodes.at(node), nodes.at(node + 1), nodes.at(node + 2), nodes.at(node + 3), offsetPosition.at(dimension));
    }
  }

  return nodes[0];  // The final noise value
}

}  // namespace sen

#endif  // SEN_CORE_BASE_GRADIENT_NOISE_H
