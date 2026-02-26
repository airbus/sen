// === creator_monkey.h ================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_TEST_UTIL_CHAOS_MONKEY_SRC_CREATOR_MONKEY_H
#define SEN_TEST_UTIL_CHAOS_MONKEY_SRC_CREATOR_MONKEY_H

// sen
#include "sen/core/meta/class_type.h"
#include "sen/core/meta/type.h"
#include "sen/core/obj/native_object.h"
#include "sen/kernel/component_api.h"

// generated code
#include "stl/creator_monkeys/chaos_monkeys.stl.h"

// std
#include <cstddef>
#include <memory>
#include <random>
#include <unordered_set>
#include <vector>

namespace creator_monkeys
{

class MonkeyTestClassImpl final: public MonkeyTestClassBase
{
public:
  SEN_NOCOPY_NOMOVE(MonkeyTestClassImpl)

public:
  using MonkeyTestClassBase::MonkeyTestClassBase;
  ~MonkeyTestClassImpl() override = default;

protected:  // NativeObject
  void registered(sen::kernel::RegistrationApi& api) override;
  void update(sen::kernel::RunApi& runApi) override;

protected:  // TestClassBase
  i32 testMethodImpl(i32 arg1, const std::string& arg2) override;
  void emitTestEventImpl() override;

private:
  void doTask(Task task, sen::TimeStamp time);
};

class MonkeyTestClass2Impl final: public MonkeyTestClass2Base
{
public:
  SEN_NOCOPY_NOMOVE(MonkeyTestClass2Impl)

public:
  using MonkeyTestClass2Base::MonkeyTestClass2Base;
  ~MonkeyTestClass2Impl() override = default;

protected:  // NativeObject
  void registered(sen::kernel::RegistrationApi& api) override;
  void update(sen::kernel::RunApi& runApi) override;

protected:  // TestClass2Base
  f64 testMethodImpl(f64 arg1) override;
  void testMethod2Impl(u16 arg1) override;
  void emitTestEventImpl() override;

private:
  void doTask(Task task, sen::TimeStamp time);
};

class CreatorMonkeyImpl final: public CreatorMonkeyBase
{
public:
  SEN_NOCOPY_NOMOVE(CreatorMonkeyImpl)

public:
  using CreatorMonkeyBase::CreatorMonkeyBase;
  ~CreatorMonkeyImpl() override = default;

private:  // CreatorMonkey
  void addObject(sen::kernel::RunApi& api, std::size_t count, const sen::ClassType* type);
  void deleteObject(std::size_t count);
  void registerObject(sen::kernel::RegistrationApi& api, std::string className);

public:
  void update(sen::kernel::RunApi& runApi) override;

protected:  // NativeObject
  void registered(sen::kernel::RegistrationApi& api) override;

private:
  std::unordered_set<std::shared_ptr<sen::NativeObject>> objects_;
  sen::TimeStamp startTime_;
  uint64_t frameCount_ = 0U;
  uint64_t nextObject_ = 0U;
  std::vector<sen::ConstTypeHandle<sen::ClassType>> meta_;
  std::shared_ptr<sen::ObjectSource> bus_;
  std::string instancesPrefix_;
  std::random_device rd_;
};

}  // namespace creator_monkeys

#endif  // SEN_TEST_UTIL_CHAOS_MONKEY_SRC_CREATOR_MONKEY_H
