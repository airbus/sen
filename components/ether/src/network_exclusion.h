// === network_exclusion.h =============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_COMPONENTS_ETHER_SRC_NETWORK_EXCLUSION_H
#define SEN_COMPONENTS_ETHER_SRC_NETWORK_EXCLUSION_H

// sen
#include "sen/core/base/assert.h"
#include "sen/core/base/result.h"

// generated code
#include "stl/configuration.stl.h"

// asio
#include <asio/ip/address_v4.hpp>

// std
#include <algorithm>
#include <cstdint>
#include <iterator>
#include <optional>
#include <string>
#include <vector>

namespace sen::components::ether
{

/// Stores sorted and non-overlapping exclusion ranges.
/// Tag keeps different exclusion types separate
template <typename Value, typename Tag>
class ExclusionSet
{
public:
  /// Exclusion range
  struct Range
  {
    Value min;
    Value max;
  };

public:
  /// Adds a range and merges overlapping or adjacent ranges.
  void add(Value min, Value max)
  {
    SEN_ASSERT(min <= max);

    auto position = std::lower_bound(
      ranges_.begin(), ranges_.end(), min, [](const Range& range, Value value) { return hasGap(range.max, value); });

    while (position != ranges_.end() && !hasGap(max, position->min))
    {
      min = std::min(min, position->min);
      max = std::max(max, position->max);
      position = ranges_.erase(position);
    }

    ranges_.insert(position, {min, max});
  }

  /// Returns true if the value is excluded
  [[nodiscard]] bool isExcluded(Value value) const noexcept
  {
    const auto itr = std::upper_bound(
      ranges_.begin(), ranges_.end(), value, [](Value candidate, const Range& range) { return candidate < range.min; });
    return itr != ranges_.begin() && value <= std::prev(itr)->max;
  }

  /// Finds the first non-excluded value in a finite candidate sequence.
  template <typename Next>
  [[nodiscard]] std::optional<Value> nextUsable(Value start, uint64_t candidateCount, Next&& next) const
  {
    Value candidate = start;
    for (uint64_t attempt = 0; attempt < candidateCount; ++attempt)
    {
      if (!isExcluded(candidate))
      {
        return candidate;
      }
      candidate = next(candidate);
    }
    return std::nullopt;
  }

  /// Returns the sorted exclusion ranges
  [[nodiscard]] const std::vector<Range>& ranges() const noexcept { return ranges_; }

private:
  /// Returns true if there is a gap between two ranges
  [[nodiscard]] static bool hasGap(Value leftMax, Value rightMin) noexcept
  {
    if (leftMax >= rightMin)
    {
      return false;
    }

    auto nextValue = leftMax + 1;

    return nextValue < rightMin;
  }

private:
  std::vector<Range> ranges_;
};

struct MulticastExclusionTag;
struct BuiltInPortExclusionTag;
struct ConfiguredPortExclusionTag;
struct OsPortExclusionTag;

/// Excluded multicast addresses
using MulticastExclusions = ExclusionSet<uint32_t, MulticastExclusionTag>;
/// Built-in port exclusions
using BuiltInPortExclusions = ExclusionSet<uint16_t, BuiltInPortExclusionTag>;
/// User configured port exclusions
using ConfiguredPortExclusions = ExclusionSet<uint16_t, ConfiguredPortExclusionTag>;
/// Port exclusions reported by the operating system
using OsPortExclusions = ExclusionSet<uint16_t, OsPortExclusionTag>;

/// Keeps port exclusions separated by their source
struct PortExclusionSources
{
  BuiltInPortExclusions builtIn;
  ConfiguredPortExclusions configured;
  OsPortExclusions os;
};

/// Network exclusions used by ether.
/// Multicast address exclusions and port exclusions are independent.
struct NetworkExclusions
{
  MulticastExclusions multicast;
  PortExclusionSources ports;
};

/// Builds multicast and port exclusions.
/// Returns an error for invalid input.
[[nodiscard]] Result<NetworkExclusions, std::string> makeNetworkExclusions(const Configuration& config);

/// Returns true if the multicast range has a usable address.
[[nodiscard]] bool hasUsableMulticastAddress(const MulticastRange& range, const MulticastExclusions& exclusions);

/// Finds the first usable multicast address, starting at candidate.
/// Returns nullopt if all addresses are excluded.
[[nodiscard]] std::optional<asio::ip::address_v4> getUsableMulticastAddress(asio::ip::address_v4 candidate,
                                                                            const MulticastRange& range,
                                                                            const MulticastExclusions& exclusions);

/// Returns true if the port is excluded by any port source.
[[nodiscard]] inline bool isPortExcluded(uint16_t port, const PortExclusionSources& exclusions)
{
  return exclusions.builtIn.isExcluded(port) || exclusions.configured.isExcluded(port) ||
         exclusions.os.isExcluded(port);
}

}  // namespace sen::components::ether

#endif  // SEN_COMPONENTS_ETHER_SRC_NETWORK_EXCLUSION_H
