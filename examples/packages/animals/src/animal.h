// === animal.h ========================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_EXAMPLES_PACKAGES_ANIMALS_SRC_ANIMAL_H
#define SEN_EXAMPLES_PACKAGES_ANIMALS_SRC_ANIMAL_H

// sen
#include "sen/core/base/compiler_macros.h"

// generated code
#include "stl/animal.stl.h"

namespace animals
{

class AnimalImpl: public AnimalBase
{
public:
  SEN_NOCOPY_NOMOVE(AnimalImpl)

public:
  using AnimalBase::AnimalBase;
  ~AnimalImpl() override = default;

protected:
  void advanceImpl(MetersU32 a, MetersU32 b) override
  {
    const auto& currentPosition = getPosition();
    setNextPosition(Vec2 {currentPosition.x + a, currentPosition.y + b});
  }
};

}  // namespace animals

#endif  // SEN_EXAMPLES_PACKAGES_ANIMALS_SRC_ANIMAL_H
