// === component.cpp ===================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "sen/kernel/component.h"

// sen
#include "sen/core/base/assert.h"
#include "sen/core/base/compiler_macros.h"
#include "sen/core/base/duration.h"
#include "sen/core/meta/class_type.h"
#include "sen/core/meta/var.h"
#include "sen/core/obj/interest.h"
#include "sen/core/obj/subscription.h"
#include "sen/kernel/component_api.h"

// generated code
#include "stl/query_test.stl.h"

// std
#include <memory>
#include <string>
#include <tuple>

namespace query_test
{

class QueryTestClassImpl: public QueryTestClassBase
{
public:
  SEN_NOCOPY_NOMOVE(QueryTestClassImpl)
  QueryTestClassImpl(const std::string& name, const sen::VarMap& args): QueryTestClassBase(name, args) {}
  ~QueryTestClassImpl() override = default;
};

SEN_EXPORT_CLASS(QueryTestClassImpl)

}  // namespace query_test

namespace
{

class QueryTestComponent: public sen::kernel::Component
{
public:
  sen::kernel::FuncResult run(sen::kernel::RunApi& api) override
  {
    obj_ = std::make_shared<query_test::QueryTestClassImpl>("object1", sen::VarMap {});
    obj_->setNextCurrentStatus(query_test::Status::idle);

    // --8<-- [start:subscribe]
    const auto bus = api.getSource("se.env");
    bus->add(obj_);

    std::ignore = enumSub_.list.onAdded([this](const auto& /*iterators*/) { matchCount_++; });

    const auto interest = sen::Interest::make(
      R"(SELECT query_test.QueryTestClass FROM se.env WHERE currentStatus = "error")", api.getTypes());
    enumSub_.attachTo(bus, enumInterest, true);
    // --8<-- [end:subscribe]

    // The change has to cross the bus and fire the subscription callback, which one cycle
    // does not bound on a loaded machine. Wait for the match and bound the wait instead.
    // The bound stops applying once the stop is requested: the loop keeps ticking while the
    // kernel shuts down, and a slow shutdown was failing this test rather than the query.
    constexpr int maxTicks = 200;  // ~2 s at 100 Hz

    return api.execLoop(sen::Duration::fromHertz(100.0),
                        [this, &api]
                        {
                          tick_++;

                          if (tick_ == 1)
                          {
                            obj_->setNextCurrentStatus(query_test::Status::error);
                          }
                          else if (matchCount_ > 0 && !stopRequested_)
                          {
                            if (matchCount_ != 1)
                            {
                              sen::throwRuntimeError("Enum query matched the property change more than once");
                            }

                            stopRequested_ = true;
                            enumSub_.release(false);
                            api.requestKernelStop(0);
                          }
                          else if (tick_ > maxTicks && !stopRequested_)
                          {
                            sen::throwRuntimeError("Enum query never matched the property change");
                          }
                        });
  }

private:
  std::shared_ptr<query_test::QueryTestClassImpl> obj_;
  sen::Subscription<query_test::QueryTestClassInterface> enumSub_;
  int matchCount_ = 0;
  bool stopRequested_ = false;
  int tick_ = 0;
};

}  // namespace

SEN_COMPONENT(QueryTestComponent)
