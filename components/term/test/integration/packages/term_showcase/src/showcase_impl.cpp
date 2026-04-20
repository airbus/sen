// === showcase_impl.cpp ===============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "showcase_impl.h"

// std
#include <cmath>
#include <sstream>
#include <variant>

namespace term_showcase
{

namespace
{

/// Round an f32 to a short-ish decimal representation so the watch pane shows "21.34" instead
/// of "21.339999...". Tiny helper, kept local to this file.
float roundedTemperature(float v) { return std::round(v * 100.0F) / 100.0F; }

}  // namespace

ShowcaseImpl::ShowcaseImpl(const std::string& name, const sen::VarMap& args): ShowcaseBase(name, args)
{
  setNextTicks(0U);
  setNextTemperature(20.0F);
  setNextStatus(Severity::info);
  setNextEnabled(false);
  setNextMessage("idle");
  setNextPosition(Point {0, 0});
  setNextTrail(PointList {});
  setNextCurrent(Action {Point {0, 0}});
  setNextTarget(MaybePoint {});
  setNextLength(Length {0U});
  setNextCounter(0);
  setNextWorkerCount(0U);
  setNextSpawning(true);
}

void ShowcaseImpl::registered(sen::kernel::RegistrationApi& api)
{
  // Grab the source for the bus this object lives on. Workers added to it will appear as
  // siblings of the showcase object (e.g. local.demo.worker_0 alongside local.demo.showcase).
  source_ = api.getSource("local.demo");
}

void ShowcaseImpl::update(sen::kernel::RunApi& /*runApi*/)
{
  // Each `if (t % N == 0)` gate schedules a property for update every N cycles, so fast and
  // slow properties coexist in the same pane. The cadences are chosen to look visually
  // distinct: the ticks counter races, temperature oscillates, status cycles through the enum,
  // enabled flips slowly, etc.
  const auto t = getTicks() + 1U;
  setNextTicks(t);

  // Temperature: every 10 cycles. Sine-ish oscillation around 20.0 ± 5.0.
  if (t % 10U == 0U)
  {
    const auto phase = static_cast<float>(t) * 0.05F;
    setNextTemperature(roundedTemperature(20.0F + 5.0F * std::sin(phase)));
  }

  // Status: every 30 cycles, cycle through the enum in order.
  if (t % 30U == 0U)
  {
    constexpr Severity rotation[] = {Severity::debug, Severity::info, Severity::warning, Severity::error};
    const auto idx = (t / 30U) % 4U;
    setNextStatus(rotation[idx]);
  }

  // Enabled: every 90 cycles, toggle.
  if (t % 90U == 0U)
  {
    setNextEnabled(!getEnabled());
  }

  // Message: every 60 cycles, pick a short line from a rotation.
  if (t % 60U == 0U)
  {
    constexpr const char* rotation[] = {"idle", "busy", "syncing", "paused"};
    const auto idx = (t / 60U) % 4U;
    setNextMessage(rotation[idx]);
  }

  // Position: every 20 cycles, walk around a circle of radius 50 centred at (0, 0).
  if (t % 20U == 0U)
  {
    const auto angle = static_cast<float>(t) * 0.02F;
    setNextPosition(Point {static_cast<i32>(50.0F * std::cos(angle)), static_cast<i32>(50.0F * std::sin(angle))});
  }

  // Trail: every 50 cycles, append the current position and trim to the last 8 samples.
  if (t % 50U == 0U)
  {
    auto trail = getTrail();
    trail.push_back(getPosition());
    constexpr size_t maxTrail = 8U;
    if (trail.size() > maxTrail)
    {
      trail.erase(trail.begin(), trail.begin() + static_cast<ptrdiff_t>(trail.size() - maxTrail));
    }
    setNextTrail(std::move(trail));
  }

  // Current action: every 120 cycles, cycle through the variant alternatives.
  if (t % 120U == 0U)
  {
    const auto idx = (t / 120U) % 4U;
    switch (idx)
    {
      case 0U:
        setNextCurrent(Action {getPosition()});
        break;
      case 1U:
        setNextCurrent(Action {Rectangle {Point {0, 0}, getPosition()}});
        break;
      case 2U:
        setNextCurrent(Action {std::string("ping")});
        break;
      case 3U:
        setNextCurrent(Action {static_cast<i32>(t)});
        break;
      default:
        break;
    }
  }

  // Target: every 180 cycles, toggle between (none) and (position).
  if (t % 180U == 0U)
  {
    const auto phase = (t / 180U) % 2U;
    setNextTarget(phase == 0U ? MaybePoint {} : MaybePoint {getPosition()});
  }

  // Length: every 40 cycles, sweep 0→1000 meters modulo the range.
  if (t % 40U == 0U)
  {
    const auto step = (t / 40U) % 26U;  // 0..25 → 0..1000 in jumps of 40
    setNextLength(Length {static_cast<u16>(step * 40U)});
  }

  // ----- Events -----

  // tick: every 60 cycles (~2 s). No arguments.
  if (t % 60U == 0U)
  {
    tick();
  }

  // thresholdCrossed: when temperature crosses 20.0 in either direction.
  {
    static bool wasAbove = false;
    const bool isAbove = getTemperature() > 20.0F;
    if (isAbove != wasAbove)
    {
      thresholdCrossed(getTemperature(), isAbove ? "up" : "down");
      wasAbove = isAbove;
    }
  }

  // positionChanged: every 20 cycles when the position updates.
  if (t % 20U == 0U)
  {
    auto oldPos = getPosition();
    auto angle = static_cast<float>(t) * 0.02F;
    auto newPos = Point {static_cast<i32>(50.0F * std::cos(angle)), static_cast<i32>(50.0F * std::sin(angle))};
    if (oldPos.x != newPos.x || oldPos.y != newPos.y)
    {
      positionChanged(oldPos, newPos);
    }
  }

  // actionPerformed: every 120 cycles when the variant cycles.
  if (t % 120U == 0U && t > 0U)
  {
    actionPerformed(getCurrent());
  }

  // ----- Worker coordination -----

  if (getSpawning() && source_)
  {
    constexpr u32 spawnInterval = 150U;   // ~5 s at 30 Hz
    constexpr u32 removeInterval = 300U;  // ~10 s at 30 Hz
    constexpr u32 maxWorkers = 3U;

    // Spawn a new worker periodically, up to the cap. Reuse names from the free pool so that
    // a watch on "worker_0" reconnects when the object is respawned with the same name.
    if (t % spawnInterval == 0U && workers_.size() < maxWorkers)
    {
      std::string name;
      if (!freeNames_.empty())
      {
        name = std::move(freeNames_.front());
        freeNames_.pop_front();
      }
      else
      {
        name = "worker_" + std::to_string(workers_.size() + freeNames_.size());
      }
      auto worker = std::make_shared<WorkerImpl>(name, sen::VarMap {});
      source_->add(worker);
      workers_.push_back(std::move(worker));
      setNextWorkerCount(static_cast<u32>(workers_.size()));
    }

    // Remove the oldest worker periodically. Its name goes back into the free pool so the
    // next spawn cycle can reuse it, exercising the watch reconnection path.
    if (t % removeInterval == 0U && !workers_.empty())
    {
      freeNames_.push_back(workers_.front()->getName());
      source_->remove(workers_.front());
      workers_.pop_front();
      setNextWorkerCount(static_cast<u32>(workers_.size()));
    }
  }
}

std::string ShowcaseImpl::echoImpl(const std::string& message) const { return std::string("echo: ") + message; }

i32 ShowcaseImpl::addImpl(i32 a, i32 b) const { return a + b; }

std::string ShowcaseImpl::waitImpl(sen::Duration duration) const
{
  std::ostringstream oss;
  oss << "waited " << duration.toSeconds() << "s";
  return oss.str();
}

std::string ShowcaseImpl::configureImpl(bool enabled, const Severity& level) const
{
  std::ostringstream oss;
  oss << "enabled=" << (enabled ? "true" : "false") << " level=";
  switch (level)
  {
    case Severity::debug:
      oss << "debug";
      break;
    case Severity::info:
      oss << "info";
      break;
    case Severity::warning:
      oss << "warning";
      break;
    case Severity::error:
      oss << "error";
      break;
  }
  return oss.str();
}

std::string ShowcaseImpl::moveToImpl(const Point& target) const
{
  std::ostringstream oss;
  oss << "moved to (" << target.x << ", " << target.y << ")";
  return oss.str();
}

std::string ShowcaseImpl::drawRectImpl(const Rectangle& area) const
{
  std::ostringstream oss;
  oss << "drew rect (" << area.topLeft.x << "," << area.topLeft.y << ")-(" << area.bottomRight.x << ","
      << area.bottomRight.y << ")";
  return oss.str();
}

i32 ShowcaseImpl::sumIntsImpl(const IntList& values) const
{
  i32 total = 0;
  for (auto v: values)
  {
    total += v;
  }
  return total;
}

Point ShowcaseImpl::centroidImpl(const PointList& points) const
{
  if (points.empty())
  {
    return Point {0, 0};
  }
  i32 sx = 0;
  i32 sy = 0;
  for (const auto& p: points)
  {
    sx += p.x;
    sy += p.y;
  }
  return Point {sx / static_cast<i32>(points.size()), sy / static_cast<i32>(points.size())};
}

std::string ShowcaseImpl::performImpl(const Action& action) const
{
  // std::visit dispatches on the actual alternative currently held by the variant.
  return std::visit(
    [](const auto& v) -> std::string
    {
      std::ostringstream oss;
      using T = std::decay_t<decltype(v)>;
      if constexpr (std::is_same_v<T, Point>)
      {
        oss << "move to (" << v.x << ", " << v.y << ")";
      }
      else if constexpr (std::is_same_v<T, Rectangle>)
      {
        oss << "draw rect (" << v.topLeft.x << "," << v.topLeft.y << ")-(" << v.bottomRight.x << "," << v.bottomRight.y
            << ")";
      }
      else if constexpr (std::is_same_v<T, std::string>)
      {
        oss << "log \"" << v << "\"";
      }
      else if constexpr (std::is_same_v<T, i32>)
      {
        oss << "wait for " << v << " cycles";
      }
      return oss.str();
    },
    action);
}

std::string ShowcaseImpl::setRadiusImpl(const MaybeFloat& radius) const
{
  if (!radius.has_value())
  {
    return "radius cleared";
  }
  std::ostringstream oss;
  oss << "radius = " << *radius;
  return oss.str();
}

std::string ShowcaseImpl::setPositionImpl(const MaybePoint& position) const
{
  if (!position.has_value())
  {
    return "position cleared";
  }
  std::ostringstream oss;
  oss << "position = (" << position->x << ", " << position->y << ")";
  return oss.str();
}

std::string ShowcaseImpl::setLengthImpl(Length length) const
{
  std::ostringstream oss;
  oss << "length = " << length.get() << " m";
  return oss.str();
}

std::string ShowcaseImpl::setFractionImpl(Fraction fraction) const
{
  std::ostringstream oss;
  oss << "fraction = " << fraction.get();
  return oss.str();
}

SEN_EXPORT_CLASS(ShowcaseImpl)

}  // namespace term_showcase
