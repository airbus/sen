// === student.cpp =====================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "student.h"

#include "person.h"

// generated code
#include "stl/school/student.stl.h"

// sen
#include "sen/core/base/class_helpers.h"
#include "sen/core/base/hash32.h"
#include "sen/core/base/numbers.h"
#include "sen/core/base/timestamp.h"
#include "sen/kernel/component_api.h"

// std
#include <array>
#include <chrono>
#include <string>
#include <utility>
#include <variant>

namespace school
{

StudentImpl::StudentImpl(std::string name, std::string surName, std::string firstName)
  : Parent(std::move(name), std::move(surName), std::move(firstName))
{
  focusLevelNoise_.seed(sen::hashCombine(sen::crc32("focusLevel"), sen::crc32(name), rand()));        // NOLINT
  snortingLevelNoise_.seed(sen::hashCombine(sen::crc32("snortingLevel"), sen::crc32(name), rand()));  // NOLINT
  setNextStatus(DoingNothing {});
}

StudentImpl::~StudentImpl() = default;

void StudentImpl::update(sen::kernel::RunApi& runApi)
{
  PersonImpl::update(runApi);

  // update our status
  std::visit(sen::Overloaded {[&](const DoingNothing& activity) { doNothing(runApi.getTime(), activity); },
                              [&](const DoingSomething& activity) { doSomething(runApi.getTime(), activity); },
                              [&](const Sleeping& activity) { doSleep(runApi.getTime(), activity); }},
             getStatus());
}

std::string StudentImpl::askImpl(const std::string& question)
{
  std::ignore = question;  // this is the typical behavior in schools and corporate environments

  // answer depending on what we were doing
  return std::visit(sen::Overloaded {[&](const DoingNothing& /*activity*/) -> std::string
                                     {
                                       std::array<const char*, 3> answers = {
                                         "Yes",
                                         "No",
                                         "I don't know...",
                                       };
                                       return answers.at(rand() % answers.size());  // NOLINT
                                     },
                                     [&](const DoingSomething& /*activity*/) -> std::string
                                     {
                                       std::array<const char*, 3> answers = {
                                         "I'm busy!",
                                         "Can't say right now",
                                         "Hold on, I need to finish this first",
                                       };
                                       return answers.at(rand() % answers.size());  // NOLINT
                                     },
                                     [&](const Sleeping& /*activity*/) -> std::string
                                     {
                                       setNextStatus(DoingNothing {getLastCommitTime()});  // wake up
                                       std::array<const char*, 3> answers = {
                                         "mhhhh??",
                                         "zfghhh... ??",
                                         ".....zZzZzZz",
                                       };
                                       return answers.at(rand() % answers.size());  // NOLINT
                                     }},
                    getStatus());
}

bool StudentImpl::startDoingTaskImpl(const std::string& taskName, float32_t difficulty)
{
  // reject the task if we are already doing something
  if (std::holds_alternative<DoingSomething>(getStatus()))
  {
    return false;
  }

  setNextStatus(DoingSomething {this->getLastCommitTime(), taskName, difficulty, 0.0f});
  return true;
}

void StudentImpl::doNothing(const sen::TimeStamp& time, const DoingNothing& activity)
{
  setNextFocusLevel((static_cast<float32_t>(focusLevelNoise_({time.sinceEpoch().toSeconds()})) + 1.0f) * 0.5f * 0.1f);

  // ensure we mark the time since we do nothing
  if (activity.since.sinceEpoch().getNanoseconds() == 0)
  {
    setNextStatus(DoingNothing {time});
  }
  else if (time - activity.since > sen::Duration(std::chrono::seconds(5)))
  {
    // go to sleep after some time of doing nothing
    setNextStatus(Sleeping {time});
    setNextFocusLevel(0.0f);
  }
  else if (rand() % 10 == 0)  // NOLINT
  {
    // otherwise, say random things
    static std::array<const char*, 4> phrases = {"I'm bored", "lalala...", "hey, the teacher is dumb", "this sucks"};
    saidSomething(phrases.at(rand() % phrases.size()));  // NOLINT
  }
}

void StudentImpl::doSomething(const sen::TimeStamp& time, const DoingSomething& activity)
{
  auto progress = activity.progress + activity.difficulty;

  setNextFocusLevel((static_cast<float32_t>(focusLevelNoise_({time.sinceEpoch().toSeconds()})) + 1.0f) * 0.5f);

  if (progress >= 1.0f)
  {
    setNextStatus(DoingNothing {time});
    saidSomething("done with " + activity.taskName);
  }
  else
  {
    DoingSomething newValue = activity;
    newValue.progress = progress;
    setNextStatus(std::move(newValue));

    if (rand() % 7 == 0)  // NOLINT
    {
      saidSomething("this " + activity.taskName + " thing is not easy...");
    }
  }
}

void StudentImpl::doSleep(const sen::TimeStamp& time, const Sleeping& activity)
{
  auto t = time.sinceEpoch().toSeconds();

  if (time - activity.since > std::chrono::seconds(3))
  {
    // wake up due own snorting
    setNextStatus(DoingNothing {time});
    saidSomething("was I sleeping?");
    return;
  }

  auto snortingVolume = snortingLevelNoise_({t});
  if (snortingVolume < 0.9)
  {
    Sleeping nextStatus = activity;
    nextStatus.snortingVolume = static_cast<float32_t>(snortingVolume);
    setNextStatus(nextStatus);
  }
  else
  {
    static std::array<const char*, 4> noises = {"hrghh..hfmmf", "mmhmm.ghfnf", "mmmh?...zzzZzz", "grr..ffwg"};
    static std::array<float32_t, 5> volumes = {0.1f, 0.7f, 0.33f, 1.3f, 0.41f};

    // wake up due own snorting
    setNextStatus(DoingNothing {time});

    // make some noise
    madeSomeNoise(noises.at(rand() % noises.size()), volumes.at(rand() % volumes.size()));  // NOLINT
  }
}

void StudentImpl::hearSomeNoise(const std::string& noise, float32_t volume)
{
  if (rand() % 2 == 0)  // NOLINT
  {
    std::string reason;
    reason.append("heard a noise '");
    reason.append(noise);
    reason.append("' with volume ");
    reason.append(std::to_string(volume));

    gotDistracted(reason);
  }
}

}  // namespace school
