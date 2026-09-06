// === terrain_client_impl.cpp =========================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

// generated code
#include "netn/netn-metoc.xml.h"
#include "stl/terrain_client.stl.h"
#include "stl/terrain_server.stl.h"

// sen
#include "sen/core/base/compiler_macros.h"
#include "sen/core/meta/class_type.h"
#include "sen/core/obj/subscription.h"
#include "sen/kernel/component_api.h"

// std
#include <cstdio>
#include <iostream>
#include <memory>
#include <utility>

namespace terrain_server
{

class TerrainClientImpl: public TerrainClientBase
{
  SEN_NOCOPY_NOMOVE(TerrainClientImpl)

public:
  using TerrainClientBase::TerrainClientBase;
  ~TerrainClientImpl() override = default;

public:
  void registered(sen::kernel::RegistrationApi& api) override
  {
    sub_ = api.selectAllFrom<TerrainServerInterface>(getServerBus());  // declare an interest on all terrain servers
  }

  void update(sen::kernel::RunApi& api) override
  {
    for (auto server: sub_->list.getObjects())
    {
      std::puts("requesting hat/hot");
      server->requestHatHot(netn::GeoReferenceVariant(),
                            false,
                            {api.getWorkQueue(),
                             [](const auto& response)
                             {
                               if (response)
                               {
                                 std::cout << response.getValue() << "\n";
                               }
                             }});
    }
  }

private:
  std::shared_ptr<sen::Subscription<TerrainServerInterface>> sub_;
};

SEN_EXPORT_CLASS(TerrainClientImpl)

}  // namespace terrain_server
