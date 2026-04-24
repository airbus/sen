// === classroom.cpp ===================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

// sen
#include "sen/core/base/compiler_macros.h"
#include "sen/core/base/numbers.h"
#include "sen/core/meta/class_type.h"
#include "sen/core/obj/object_list.h"
#include "sen/core/obj/object_source.h"
#include "sen/core/obj/subscription.h"
#include "sen/kernel/component_api.h"

// generated code
#include "stl/school/classroom.stl.h"
#include "stl/school/student.stl.h"

// package
#include "names.h"
#include "student.h"
#include "teacher.h"

// std
#include <cstddef>
#include <cstdint>
#include <memory>
#include <random>
#include <string>
#include <tuple>
#include <vector>

namespace school
{

class ClassroomImpl: public ClassroomBase
{
  SEN_NOCOPY_NOMOVE(ClassroomImpl)

public:
  using ClassroomBase::ClassroomBase;
  ~ClassroomImpl() override
  {
    if (bus_)
    {
      bus_->remove(students_);
      bus_->remove(teacher_);

      students_.clear();
      teacher_.reset();
      bus_.reset();
    }
  }

public:
  void registered(sen::kernel::RegistrationApi& api) override
  {
    // open the bus
    bus_ = api.getSource(getStudentsBus());

    // detect students (even if they are remote) and react to noise events when they join
    allStudents_ = api.selectAllFrom<StudentInterface>(getStudentsBus(),
                                                       [&](const auto& addedObjects)
                                                       {
                                                         for (auto* student: addedObjects)
                                                         {
                                                           // when someone makes some noise, someone else will hear it
                                                           auto cb = [&](const std::string& noise, float32_t volume)
                                                           {
                                                             std::uniform_int_distribution<std::size_t> dist {
                                                               0, students_.size() - 1};
                                                             students_.at(dist(rng_))->hearSomeNoise(noise, volume);
                                                           };

                                                           student->onMadeSomeNoise({this, std::move(cb)}).keep();
                                                         }
                                                       });

    // create the teacher
    if (getCreateTeacher())
    {
      auto [first, sur, full] = makeName();
      teacher_ = std::make_shared<TeacherImpl>(full, first, sur, allStudents_);
      setNextTeacherName(full);

      // publish the teacher
      bus_->add(teacher_);
    }

    if (getDefaultSize() != 0U)
    {
      addStudents(getDefaultSize());
    }
  }

protected:
  void addStudentsImpl(uint32_t count) override
  {
    // to store the new students
    std::vector<std::shared_ptr<StudentImpl>> newStudents;
    newStudents.reserve(count);

    // create the students
    for (uint32_t i = 0U; i < count; ++i)
    {
      auto [firstName, surName, fullName] = makeName();
      newStudents.emplace_back(std::make_shared<StudentImpl>(fullName, surName, firstName));
    }

    // publish the students
    bus_->add(newStudents);
    students_.insert(students_.end(), newStudents.begin(), newStudents.end());
  }

  void removeStudentsImpl(uint32_t count) override
  {
    // to store the students to be removed
    std::vector<std::shared_ptr<StudentImpl>> toRemove;
    toRemove.reserve(count);

    for (std::size_t i = 0; i < count && !students_.empty(); ++i)
    {
      toRemove.push_back(students_.back());
      students_.pop_back();
    }

    bus_->remove(toRemove);
  }

private:
  std::mt19937 rng_ {std::random_device {}()};
  std::shared_ptr<sen::ObjectSource> bus_;
  std::shared_ptr<TeacherImpl> teacher_;
  std::vector<std::shared_ptr<StudentImpl>> students_;
  std::shared_ptr<sen::Subscription<StudentInterface>> allStudents_;
};

SEN_EXPORT_CLASS(ClassroomImpl)

}  // namespace school
