// === teacher.cpp =====================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "teacher.h"

#include "person.h"

// sen
#include "sen/core/base/class_helpers.h"
#include "sen/core/base/hash32.h"
#include "sen/core/base/numbers.h"
#include "sen/core/base/timestamp.h"
#include "sen/core/obj/subscription.h"
#include "sen/kernel/component_api.h"

// generated code
#include "stl/school/student.stl.h"
#include "stl/school/teacher.stl.h"

// std
#include <array>
#include <chrono>
#include <limits>
#include <memory>
#include <random>
#include <string>
#include <utility>
#include <variant>

namespace school
{

int randomPositiveInteger()
{
  static std::mt19937 rng;
  static std::uniform_int_distribution<int> dist(0, std::numeric_limits<int>::max());
  return dist(rng);
}

TeacherImpl::TeacherImpl(std::string name,
                         std::string surName,
                         std::string firstName,
                         std::shared_ptr<sen::Subscription<StudentInterface>> students)
  : Parent(name, std::move(surName), std::move(firstName)), students_(std::move(students))
{
  stressLevelNoise_.seed(sen::hashCombine(sen::crc32("stressLevel"), sen::crc32(name), randomPositiveInteger()));
  setNextStatus(WaitingForStudents {});
}

void TeacherImpl::update(sen::kernel::RunApi& runApi)
{
  PersonImpl::update(runApi);

  // signal if we are super stressed
  if (getStressLevel() > 0.9f)
  {
    stressLevelPeaked(getStressLevel());
  }

  // update our status
  std::visit(sen::Overloaded {[&](const WaitingForStudents& activity) { waitForStudents(runApi.getTime(), activity); },
                              [&](const ImpartingClass& activity) { impartClass(runApi.getTime(), activity); }},
             getStatus());
}

std::string TeacherImpl::askImpl(const std::string& question)
{
  std::string result = "about your question '";
  result.append(question);
  result.append("': ");

  if (!std::holds_alternative<WaitingForStudents>(getStatus()))
  {
    result.append("let's wait until we start the class");
  }
  else
  {
    static std::array<const char*, 6> answers = {
      "yes", "no", "I have no clue", "maybe", "I have no time for that", "how do you dare?"};

    result.append(answers.at(randomPositiveInteger() % answers.size()));
  }

  return result;
}

void TeacherImpl::assignTasksImpl()
{
  static std::array<const char*, 5> taskNames = {
    "math problem", "history research", "physics problem", "read article", "write a report"};

  static std::array<float32_t, 5> difficultyLevels = {0.01f, 0.02f, 0.03f, 0.1f, 0.2f};

  for (auto& student: students_->list.getObjects())
  {
    student->startDoingTask(taskNames.at(randomPositiveInteger() % taskNames.size()),
                            difficultyLevels.at(randomPositiveInteger() % difficultyLevels.size()));
  }
}

void TeacherImpl::waitForStudents(const sen::TimeStamp& time, const WaitingForStudents& activity)
{
  if (activity.since.sinceEpoch().getNanoseconds() == 0)
  {
    setNextStatus(WaitingForStudents {time});
    return;
  }

  // compute our stress level
  setNextStressLevel(0.1f);

  // check if we have to start the class
  if ((time - activity.since) > std::chrono::seconds(3))
  {
    static std::array<const char*, 5> classNames = {"math", "history", "physics", "literature", "biology"};
    std::string className = classNames.at(randomPositiveInteger() % classNames.size());

    setNextStatus(ImpartingClass {time, className});
    saidSomething("ok guys, let's start with our " + className + " class");

    assignTasksImpl();
  }
  else
  {
    // wait for students
    if (students_->list.getUntypedObjects().size() < 2 && randomPositiveInteger() % 5 == 0)
    {
      saidSomething("people are not showing up...");
    }
  }
}

void TeacherImpl::impartClass(const sen::TimeStamp& time, const ImpartingClass& activity)
{
  // check if we have to finish the class
  if ((time - activity.since) > std::chrono::seconds(10))
  {
    saidSomething("I think we can call it a day.");
    setNextStatus(WaitingForStudents {time});
  }
  else
  {
    // imparting class

    // compute our stress level
    setNextStressLevel((static_cast<float32_t>(stressLevelNoise_({time.sinceEpoch().toSeconds()})) + 1.0f) * 0.5f);

    if (randomPositiveInteger() % 20 == 0)
    {
      // ask some student
      const auto& students = students_->list.getObjects();
      if (!students.empty())
      {
        students.front()->ask("what do you think?");
      }
    }
  }
}

}  // namespace school
