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
#include "sen/core/obj/object_list.h"
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

    const auto bus = api.getSource("se.env");
    bus->add(obj_);

    std::ignore = enumList_.onAdded([this](const auto& /*iterators*/) { enumHits_++; });

    const auto enumInterest = sen::Interest::make(
      R"(SELECT query_test.QueryTestClass FROM se.env WHERE currentStatus = "error")", api.getTypes());
    bus->addSubscriber(enumInterest, &enumList_, true);

    return api.execLoop(sen::Duration::fromHertz(100.0),
                        [this, &api]
                        {
                          tick_++;

                          if (tick_ == 1)
                          {
                            obj_->setNextCurrentStatus(query_test::Status::error);
                          }
                          else if (tick_ == 2)
                          {
                            if (enumHits_ != 1)
                            {
                              sen::throwRuntimeError("Enum query failed to match property change");
                            }

                            api.requestKernelStop(0);
                          }
                          else if (tick_ > 10)
                          {
                            sen::throwRuntimeError("Test timeout");
                          }
                        });
  }

private:
  std::shared_ptr<query_test::QueryTestClassImpl> obj_;
  sen::ObjectList<query_test::QueryTestClassInterface> enumList_;
  int enumHits_ = 0;
  int tick_ = 0;
};

}  // namespace

SEN_COMPONENT(QueryTestComponent)
