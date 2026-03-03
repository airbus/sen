// === person.h ========================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_EXAMPLES_PACKAGES_SCHOOL_SRC_PERSON_H
#define SEN_EXAMPLES_PACKAGES_SCHOOL_SRC_PERSON_H

// sen
#include "sen/core/base/compiler_macros.h"
#include "sen/core/base/gradient_noise.h"
#include "sen/core/base/hash32.h"
#include "sen/core/base/numbers.h"
#include "sen/kernel/component_api.h"

// generated code
#include "stl/school/person.stl.h"

// std
#include <string>
#include <utility>

namespace school
{

class PersonImpl: public PersonBase
{
  SEN_NOCOPY_NOMOVE(PersonImpl)

public:
  PersonImpl(std::string name, std::string surName, std::string firstName)
    : PersonBase(std::move(name), std::move(surName), std::move(firstName))
  {
    noise_.seed(sen::hashCombine(sen::crc32("brainActivity"), sen::crc32(name), rand()));  // NOLINT
  }

  ~PersonImpl() override = default;

public:
  void update(sen::kernel::RunApi& runApi) override
  {
    setNextBrainActivity((static_cast<float32_t>(noise_({runApi.getTime().sinceEpoch().toSeconds()})) + 1.0f) * 0.5f);
  }

private:
  sen::GradientNoise<float64_t, 1U> noise_;
};

}  // namespace school

#endif  // SEN_EXAMPLES_PACKAGES_SCHOOL_SRC_PERSON_H
