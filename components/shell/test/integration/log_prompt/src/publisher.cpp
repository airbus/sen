// === publisher.cpp ===================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

// sen
#include "sen/core/meta/class_type.h"
#include "sen/core/meta/var.h"
#include "sen/kernel/component_api.h"

// generated code
#include "stl/log_prompt_test.stl.h"

// std
#include <string>
#include <utility>

namespace log_prompt_test
{

class PublisherImpl final: public PublisherBase
{
public:
  PublisherImpl(std::string name, const sen::VarMap& args): PublisherBase(std::move(name), args) {}

  void registered(sen::kernel::RegistrationApi& api) override { PublisherBase::registered(api); }
};

SEN_EXPORT_CLASS(PublisherImpl)

}  // namespace log_prompt_test
