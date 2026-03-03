// === student.h =======================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_EXAMPLES_PACKAGES_SCHOOL_SRC_STUDENT_H
#define SEN_EXAMPLES_PACKAGES_SCHOOL_SRC_STUDENT_H

#include "person.h"

// generated code
#include "stl/school/student.stl.h"

// sen
#include "sen/core/base/compiler_macros.h"
#include "sen/core/base/gradient_noise.h"
#include "sen/core/base/numbers.h"
#include "sen/core/base/timestamp.h"
#include "sen/kernel/component_api.h"

// std
#include <string>

namespace school
{

class StudentImpl: public StudentBase<PersonImpl>
{
  SEN_NOCOPY_NOMOVE(StudentImpl)

public:
  using Parent = StudentBase<PersonImpl>;

public:
  StudentImpl(std::string name, std::string surName, std::string firstName);
  ~StudentImpl() override;

public:
  void update(sen::kernel::RunApi& runApi) override;

public:
  void hearSomeNoise(const std::string& noise, float32_t volume);

protected:
  [[nodiscard]] std::string askImpl(const std::string& question) override;
  [[nodiscard]] bool startDoingTaskImpl(const std::string& taskName, float32_t difficulty) override;

private:
  void doNothing(const sen::TimeStamp& time, const DoingNothing& activity);
  void doSomething(const sen::TimeStamp& time, const DoingSomething& activity);
  void doSleep(const sen::TimeStamp& time, const Sleeping& activity);

private:
  sen::GradientNoise<float64_t, 1U> focusLevelNoise_;
  sen::GradientNoise<float64_t, 1U> snortingLevelNoise_;
};

}  // namespace school

#endif  // SEN_EXAMPLES_PACKAGES_SCHOOL_SRC_STUDENT_H
