// === plotter.h =======================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_COMPONENTS_EXPLORER_SRC_PLOTTER_H
#define SEN_COMPONENTS_EXPLORER_SRC_PLOTTER_H

#include "imgui.h"
#include "sen/core/base/duration.h"
#include "sen/core/base/timestamp.h"
#include "sen/core/meta/native_types.h"

class Plotter;
class ObjectState;

class Plot
{
  SEN_NOCOPY_NOMOVE(Plot)

public:
  Plot(std::string name, Plotter* owner, ObjectState* state, uint32_t elementId, int maxSize = 2000);
  ~Plot();

public:
  void deleteSelf();

  void addPoint(const sen::TimeStamp& time, float64_t value);

  void erase();

  [[nodiscard]] uint32_t getElementId() const noexcept { return elementId_; }

  [[nodiscard]] float64_t getLastTime() const noexcept { return lastTime_; }

  [[nodiscard]] int getSize() const noexcept { return data_.size(); }

  [[nodiscard]] const float64_t* xData() const noexcept { return &data_[0].x; }

  [[nodiscard]] const float64_t* yData() const noexcept { return &data_[0].y; }

  [[nodiscard]] const std::string& getName() const noexcept { return name_; }

  [[nodiscard]] int getOffset() const noexcept { return offset_; }

private:
  friend class Plotter;

  struct Vec2d
  {
    float64_t x;
    float64_t y;
  };

private:
  ImVector<Vec2d> data_;
  int maxSize_;
  int offset_;
  Plotter* owner_;
  std::string name_;
  ObjectState* state_;
  uint32_t elementId_;
  float64_t lastTime_ = 0.0;
};

class Plotter
{
  SEN_NOCOPY_NOMOVE(Plotter)

public:
  explicit Plotter(std::string name);
  ~Plotter() = default;

public:
  void update();
  void deletePlot(Plot* plot);
  [[nodiscard]] bool element();

private:
  std::string name_;
  std::vector<std::unique_ptr<Plot>> plots_;
  float32_t history_ = 10.0f;
  bool isOpen_ = true;
  bool autoFit_ = false;
};

//----------------------------------------------------------------------------------------------------------------------
// Inline implementation
//----------------------------------------------------------------------------------------------------------------------

inline void Plot::addPoint(const sen::TimeStamp& time, float64_t value)
{
  const auto x = time.sinceEpoch().toSeconds();
  if (x == lastTime_)
  {
    if (data_.empty())
    {
      data_.push_back({x, value});
    }
    else
    {
      data_.back() = {x, value};
    }
  }
  else
  {
    lastTime_ = x;

    if (data_.size() < maxSize_)
    {
      data_.push_back({x, value});
    }
    else
    {
      data_[offset_] = {x, value};
      offset_ = (offset_ + 1) % maxSize_;
    }
  }
}

inline void Plot::erase()
{
  if (!data_.empty())
  {
    data_.shrink(0);
    offset_ = 0;
  }
}

#endif  // SEN_COMPONENTS_EXPLORER_SRC_PLOTTER_H
