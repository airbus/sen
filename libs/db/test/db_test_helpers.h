

// === db_test_helpers.h ===============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_DB_TEST_HELPERS_H
#define SEN_DB_TEST_HELPERS_H

#include "stl/db_test_class.stl.h"

// sen
#include "sen/core/base/compiler_macros.h"
#include "sen/core/base/uuid.h"
#include "sen/kernel/component.h"
#include "sen/kernel/component_api.h"
#include "sen/kernel/test_kernel.h"

// std
#include <chrono>
#include <filesystem>
#include <memory>
#include <string>

namespace sen::db::test
{

class TempDir
{
public:
  TempDir(): path_(std::filesystem::temp_directory_path() / ("db_test_" + getRandomPathPostFix()))
  {
    std::filesystem::create_directories(path_);
  }

  // prevent object copy and movable type
  TempDir(const TempDir&) = delete;
  TempDir& operator=(const TempDir&) = delete;

  TempDir(TempDir&&) = delete;
  TempDir& operator=(TempDir&&) = delete;

  ~TempDir()
  {
    if (std::filesystem::exists(path_))
    {
      std::filesystem::remove_all(path_);
    }
  }

  [[nodiscard]] const std::filesystem::path& path() const { return path_; }

private:
  /// Returns a random post fix for temporary files paths.
  static std::string getRandomPathPostFix() { return sen::UuidRandomGenerator()().toString(); }

private:
  std::filesystem::path path_;
};

class TestObjImpl: public db_test::TestObjBase
{
public:
  SEN_NOCOPY_NOMOVE(TestObjImpl)
  using TestObjBase::TestObjBase;
  ~TestObjImpl() override = default;
  using TestObjBase::valueChanged;
};

struct SingleClassSetup
{
  std::shared_ptr<TestObjImpl> object;              // NOLINT(misc-non-private-member-variables-in-classes)
  sen::kernel::TestComponent component;             // NOLINT(misc-non-private-member-variables-in-classes)
  std::unique_ptr<sen::kernel::TestKernel> kernel;  // NOLINT(misc-non-private-member-variables-in-classes)

  SingleClassSetup()
  {
    object = std::make_shared<TestObjImpl>("testObj", sen::VarMap {});
    component.onInit(
      [this](sen::kernel::InitApi&& api) -> sen::kernel::PassResult
      {
        auto source = api.getSource("local.test");
        source->add(object);
        return sen::kernel::done();
      });
    component.onRun([](auto& api) { return api.execLoop(std::chrono::seconds(1), []() {}); });
    kernel = std::make_unique<sen::kernel::TestKernel>(&component);
    kernel->step();
  }
};

class OtherObjImpl: public db_test::OtherObjBase
{
public:
  SEN_NOCOPY_NOMOVE(OtherObjImpl)
  using OtherObjBase::OtherObjBase;
  ~OtherObjImpl() override = default;
};

struct DualClassSetup
{
  std::shared_ptr<TestObjImpl> testObject;          // NOLINT(misc-non-private-member-variables-in-classes)
  std::shared_ptr<OtherObjImpl> otherObject;        // NOLINT(misc-non-private-member-variables-in-classes)
  sen::kernel::TestComponent component;             // NOLINT(misc-non-private-member-variables-in-classes)
  std::unique_ptr<sen::kernel::TestKernel> kernel;  // NOLINT(misc-non-private-member-variables-in-classes)

  DualClassSetup()
  {
    testObject = std::make_shared<TestObjImpl>("testObj", sen::VarMap {});
    otherObject = std::make_shared<OtherObjImpl>("otherObj", sen::VarMap {});
    component.onInit(
      [this](sen::kernel::InitApi&& api) -> sen::kernel::PassResult
      {
        auto source = api.getSource("local.test");
        source->add(testObject);
        source->add(otherObject);
        return sen::kernel::done();
      });
    component.onRun([](auto& api) { return api.execLoop(std::chrono::seconds(1), []() {}); });
    kernel = std::make_unique<sen::kernel::TestKernel>(&component);
    kernel->step();
  }
};
}  // namespace sen::db::test

#endif  // SEN_DB_TEST_HELPERS_H
