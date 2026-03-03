// === shapes.cpp ======================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

// generated code
#include "stl/shapes.stl.h"

// sen
#include "sen/core/base/class_helpers.h"
#include "sen/core/base/compiler_macros.h"
#include "sen/core/meta/class_type.h"
#include "sen/kernel/component_api.h"

// std
#include <cmath>
#include <random>
#include <variant>

namespace shapes
{

const Vec2& getBounds()
{
  static auto bounds = Vec2 {50.0f, 50.0f};
  return bounds;
}

// A shape that bounces around in a box.
class ShapeImpl: public ShapeBase
{
  SEN_NOCOPY_NOMOVE(ShapeImpl)

public:
  using ShapeBase::ShapeBase;
  ~ShapeImpl() override = default;

protected:
  // Compute the half width and half height out of the geometry
  void registered(sen::kernel::RegistrationApi& /*api*/) override
  {
    halfSize_ = std::visit(
      sen::Overloaded {[](const Circle& g) { return Vec2 {g.radius, g.radius}; },
                       [](const Rectangle& g) { return Vec2 {g.width * 0.5f, g.height * 0.5f}; },
                       [f = sqrtf(3.0f) * 0.25f](const Triangle& g) { return Vec2 {g.side * f, g.side * f}; }},
      getGeometry());
  }

  void update(sen::kernel::RunApi& /*api*/) override
  {
    const auto& bounds = getBounds();

    if (!getFrozen())
    {
      auto newPos = getPosition();
      newPos.x = newPos.x + speed_.x;
      newPos.y = newPos.y + speed_.y;

      if (newPos.x <= halfSize_.x)
      {
        collidedWithWall(Wall::left);
        speed_.x = -speed_.x;
        newPos.x = halfSize_.x;
      }
      else if (auto xDiff = bounds.x - halfSize_.x; newPos.x >= xDiff)
      {
        collidedWithWall(Wall::right);
        speed_.x = -speed_.x;
        newPos.x = xDiff;
      }

      if (newPos.y <= halfSize_.y)
      {
        collidedWithWall(Wall::bottom);
        speed_.y = -speed_.y;
        newPos.y = halfSize_.y;
      }
      else if (auto yDiff = bounds.y - halfSize_.y; newPos.y >= yDiff)
      {
        collidedWithWall(Wall::top);
        speed_.y = -speed_.y;
        newPos.y = yDiff;
      }
      setNextPosition(newPos);
    }
  }

private:
  // Helper function to get a random number for the shape speed.
  [[nodiscard]] static Vec2 getRandomSpeed()
  {
    const auto& bounds = getBounds();

    using Dist = std::uniform_real_distribution<>;
    std::mt19937 e2(std::random_device {}());
    return Vec2 {Dist(-bounds.x * 0.05f, bounds.x * 0.05f)(e2), Dist(-bounds.y * 0.05f, bounds.y * 0.05f)(e2)};
  }

private:
  Vec2 speed_ = getRandomSpeed();
  Vec2 halfSize_ {};
};

SEN_EXPORT_CLASS(ShapeImpl)

}  // namespace shapes
