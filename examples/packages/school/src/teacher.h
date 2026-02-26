// === teacher.h =======================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_EXAMPLES_PACKAGES_SCHOOL_SRC_TEACHER_H
#define SEN_EXAMPLES_PACKAGES_SCHOOL_SRC_TEACHER_H

#include "person.h"

// sen
#include "sen/core/base/compiler_macros.h"
#include "sen/core/base/gradient_noise.h"
#include "sen/core/base/numbers.h"
#include "sen/core/base/timestamp.h"
#include "sen/core/obj/subscription.h"
#include "sen/kernel/component_api.h"

// generated code
#include "stl/school/student.stl.h"
#include "stl/school/teacher.stl.h"

// std
#include <memory>
#include <string>

namespace school
{

class TeacherImpl: public TeacherBase<PersonImpl>
{
  SEN_NOCOPY_NOMOVE(TeacherImpl)

public:
  using Parent = TeacherBase<PersonImpl>;

public:
  TeacherImpl(std::string name,
              std::string surName,
              std::string firstName,
              std::shared_ptr<sen::Subscription<StudentInterface>> students);
  ~TeacherImpl() override = default;

public:
  void update(sen::kernel::RunApi& runApi) override;

protected:
  [[nodiscard]] std::string askImpl(const std::string& question) override;
  void assignTasksImpl() override;

private:
  void waitForStudents(const sen::TimeStamp& time, const WaitingForStudents& activity);
  void impartClass(const sen::TimeStamp& time, const ImpartingClass& activity);

private:
  sen::GradientNoise<float64_t, 1U> stressLevelNoise_;
  std::shared_ptr<sen::Subscription<StudentInterface>> students_;
};

}  // namespace school

#endif  // SEN_EXAMPLES_PACKAGES_SCHOOL_SRC_TEACHER_H
