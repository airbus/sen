// === component.cpp ===================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "sen/kernel/component.h"

// sen
#include "sen/core/base/duration.h"
#include "sen/core/meta/class_type.h"
#include "sen/core/meta/var.h"
#include "sen/core/obj/callback.h"
#include "sen/core/obj/interest.h"
#include "sen/core/obj/object.h"
#include "sen/core/obj/object_list.h"
#include "sen/kernel/component_api.h"

// std
#include <tuple>

/// This component simply calls a method on an remote proxy object that is being removed.
/// The idea is that this does not crash, but immediately returns an error.
struct MyComponent: public sen::kernel::Component
{
  sen::kernel::FuncResult run(sen::kernel::RunApi& api) override
  {
    auto bus = api.getSource("my.tutorial");  // get the source
    sen::ObjectList<sen::Object> objects;     // create a container

    std::ignore = objects.onRemoved(
      [&api](const auto& iterators)
      {
        for (auto itr = iterators.untypedBegin; itr != iterators.untypedEnd; ++itr)
        {
          const auto* meta = (*itr)->getClass().type();
          const auto methods = meta->getMethods(sen::ClassType::SearchMode::doNotIncludeParents);

          for (const auto& method: methods)
          {
            if (method->getArgs().empty())
            {
              (*itr)->invokeUntyped(
                method.get(),
                {},
                {api.getWorkQueue(), [](const sen::MethodResult<sen::Var>& result) { std::ignore = result; }});
              break;
            }
          }
        }
      });

    bus->addSubscriber(
      sen::Interest::make("SELECT * FROM my.tutorial", api.getTypes()), &objects, true);  // subscribe to all objects

    return api.execLoop(sen::Duration::fromHertz(1.0));
  }
};

SEN_COMPONENT(MyComponent)
