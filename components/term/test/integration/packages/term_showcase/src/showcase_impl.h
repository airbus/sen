// === showcase_impl.h =================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef TEST_UTIL_TERM_SHOWCASE_SRC_SHOWCASE_IMPL_H
#define TEST_UTIL_TERM_SHOWCASE_SRC_SHOWCASE_IMPL_H

// local
#include "worker_impl.h"

// sen
#include "sen/core/meta/var.h"
#include "sen/core/obj/object_source.h"
#include "sen/kernel/component_api.h"

// generated code
#include "stl/term_showcase/term_showcase.stl.h"

// std
#include <deque>
#include <memory>
#include <string>

namespace term_showcase
{

/// Concrete Showcase object. Each method returns a short human-readable string describing what
/// was received, so the user running the term can visually confirm values round-tripped through
/// the form correctly.
///
/// Also acts as a coordinator: when `spawning` is true, periodically creates and removes Worker
/// objects on the same bus so the term's watch pane can exercise disconnected/reconnected states.
class ShowcaseImpl: public ShowcaseBase
{
  SEN_NOCOPY_NOMOVE(ShowcaseImpl)

public:
  ShowcaseImpl(const std::string& name, const sen::VarMap& args);
  ~ShowcaseImpl() override = default;

  void registered(sen::kernel::RegistrationApi& api) override;
  void update(sen::kernel::RunApi& runApi) override;

protected:
  std::string echoImpl(const std::string& message) const override;
  i32 addImpl(i32 a, i32 b) const override;
  std::string waitImpl(sen::Duration duration) const override;
  std::string configureImpl(bool enabled, const Severity& level) const override;
  std::string moveToImpl(const Point& target) const override;
  std::string drawRectImpl(const Rectangle& area) const override;
  i32 sumIntsImpl(const IntList& values) const override;
  Point centroidImpl(const PointList& points) const override;
  std::string performImpl(const Action& action) const override;
  std::string setRadiusImpl(const MaybeFloat& radius) const override;
  std::string setPositionImpl(const MaybePoint& position) const override;
  std::string setLengthImpl(Length length) const override;
  std::string setFractionImpl(Fraction fraction) const override;

private:
  std::shared_ptr<sen::ObjectSource> source_;
  std::deque<std::shared_ptr<WorkerImpl>> workers_;
  std::deque<std::string> freeNames_;  // recycled worker names so watches can reconnect
};

}  // namespace term_showcase

#endif  // TEST_UTIL_TERM_SHOWCASE_SRC_SHOWCASE_IMPL_H
