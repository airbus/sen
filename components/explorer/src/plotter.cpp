// === plotter.cpp =====================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "plotter.h"

// component
#include "object_state.h"
#include "property_data.h"

// sen
#include "sen/core/base/numbers.h"

// imgui
#include "imgui.h"

// implot
#include "implot.h"
//
// std
#include <algorithm>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>

//--------------------------------------------------------------------------------------------------------------
// Plot
//--------------------------------------------------------------------------------------------------------------

Plot::Plot(std::string name, Plotter* owner, ObjectState* state, uint32_t elementId, int maxSize)
  : maxSize_(maxSize), offset_(0U), owner_(owner), name_(std::move(name)), state_(state), elementId_(elementId)
{
  data_.reserve(maxSize);
  state_->startObservingElement(elementId_, this);
}

Plot::~Plot()
{
  if (state_)
  {
    state_->stopObservingElement(elementId_, this);
  }
}

void Plot::deleteSelf()
{
  state_ = nullptr;
  owner_->deletePlot(this);
}

//--------------------------------------------------------------------------------------------------------------
// Plotter
//--------------------------------------------------------------------------------------------------------------

Plotter::Plotter(std::string name): name_(std::move(name)) {}

void Plotter::deletePlot(Plot* plot)
{
  for (auto itr = plots_.begin(); itr != plots_.end(); ++itr)
  {
    if (itr->get() == plot)
    {
      plots_.erase(itr);
      return;
    }
  }
}

bool Plotter::element()
{
  ImGui::PushID(this);
  ImGui::Checkbox(name_.c_str(), &isOpen_);
  ImGui::SameLine();
  bool close = ImGui::SmallButton("x");
  ImGui::PopID();
  return close;
}

void Plotter::update()  // NOLINT(readability-function-size)
{
  if (!isOpen_)
  {
    return;
  }

  ImGui::SetNextWindowSize(ImVec2(430, 450), ImGuiCond_FirstUseEver);
  if (ImGui::Begin(name_.c_str(), &isOpen_))
  {
    ImGui::PushID(this);
    {
      ImGui::SliderFloat("History", &history_, 1, 30, "%.1f s");

      ImGui::SameLine();
      ImGui::Checkbox("Auto fit", &autoFit_);
      if (autoFit_)
      {
        ImPlot::SetNextAxisToFit(ImAxis_Y1);
      }

      if (ImPlot::BeginPlot(name_.c_str(), ImVec2(-1, -1), ImPlotFlags_NoTitle))
      {
        float64_t t = 0.0;
        for (const auto& plot: plots_)
        {
          t = std::max(t, plot->getLastTime());
        }

        ImPlot::SetupAxes(nullptr, nullptr, ImPlotAxisFlags_NoTickLabels, ImPlotAxisFlags_Opposite);
        ImPlot::SetupAxisScale(ImAxis_X1, ImPlotScale_Time);
        ImPlot::SetupAxisLimits(ImAxis_X1, t - static_cast<float64_t>(history_), t, ImGuiCond_Always);
        ImPlot::SetupAxisLimits(ImAxis_Y1, 0, 1);
        ImPlot::SetNextFillStyle(IMPLOT_AUTO_COL, 0.5f);  // NOLINT

        for (const auto& plot: plots_)
        {
          ImPlot::PlotLine(plot->getName().c_str(),
                           plot->xData(),
                           plot->yData(),
                           plot->getSize(),
                           0,
                           plot->getOffset(),
                           2 * sizeof(float64_t));
        }

        ImPlot::EndPlot();

        if (ImGui::BeginDragDropTarget())
        {
          if (const auto* payload = ImGui::AcceptDragDropPayload(PropStateDrag::getTypeString()); payload)
          {
            const auto* dragData = reinterpret_cast<const PropStateDrag*>(payload->Data);  // NOLINT NOSONAR

            bool alreadyPresent = false;
            for (const auto& plot: plots_)
            {
              if (plot->getElementId() == dragData->elementHash)
              {
                alreadyPresent = true;
                break;
              }
            }

            if (!alreadyPresent)
            {
              plots_.push_back(
                std::make_unique<Plot>(dragData->elementLabel, this, dragData->state, dragData->elementHash));
            }
          }
          ImGui::EndDragDropTarget();
        }
      }
    }
    ImGui::PopID();
  }

  ImGui::End();
}
