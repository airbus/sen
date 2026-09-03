// === publisher.cpp ===================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

// sen
#include "sen/core/meta/class_type.h"
#include "sen/core/meta/var.h"
#include "sen/core/obj/object_source.h"
#include "sen/kernel/component_api.h"

// generated code
#include "stl/ls_command_test.stl.h"

// std
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace ls_command_test
{

class DummyObjectImpl final: public DummyObjectBase
{
public:
  DummyObjectImpl(std::string name, const sen::VarMap& args): DummyObjectBase(std::move(name), args) {}
};

class PublisherImpl final: public PublisherBase
{
public:
  PublisherImpl(std::string name, const sen::VarMap& args): PublisherBase(std::move(name), args) {}

  void registered(sen::kernel::RegistrationApi& api) override
  {
    PublisherBase::registered(api);

    env_ = api.getSource(getBus());

    {
      auto tempBus = api.getSource("other_session.other_bus");
      auto tempObj = std::make_shared<DummyObjectImpl>("other_session.other_bus.dummy_object", sen::VarMap {});

      objs_.push_back(tempObj);
      tempBus->add(tempObj);
    }
  }

private:
  std::shared_ptr<sen::ObjectSource> env_;
  std::vector<std::shared_ptr<DummyObjectImpl>> objs_;
};

SEN_EXPORT_CLASS(DummyObjectImpl)
SEN_EXPORT_CLASS(PublisherImpl)

}  // namespace ls_command_test
