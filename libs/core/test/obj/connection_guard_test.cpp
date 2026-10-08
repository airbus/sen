// === connection_guard_test.cpp =======================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

// sen
#include "sen/core/base/compiler_macros.h"
#include "sen/core/base/timestamp.h"
#include "sen/core/io/output_stream.h"
#include "sen/core/meta/event.h"
#include "sen/core/meta/method.h"
#include "sen/core/meta/type.h"
#include "sen/core/meta/var.h"
#include "sen/core/obj/callback.h"
#include "sen/core/obj/connection_guard.h"
#include "sen/core/obj/object.h"

// generated code
#include "stl/example_class.stl.h"

// google test
#include <gtest/gtest.h>

// std
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <utility>

namespace
{

class MockConnectionObject final: public sen::Object
{
  SEN_NOCOPY_NOMOVE(MockConnectionObject)

public:
  MockConnectionObject() = default;
  ~MockConnectionObject() override = default;

  [[nodiscard]] const std::string& getName() const noexcept override
  {
    static const std::string name = "MockObject";
    return name;
  }

  [[nodiscard]] sen::ObjectId getId() const noexcept override { return sen::ObjectId {1U}; }

  [[nodiscard]] sen::ConstTypeHandle<sen::ClassType> getClass() const noexcept override
  {
    return example_class::ExampleClassInterface::meta();
  }

  [[nodiscard]] const std::string& getLocalName() const noexcept override
  {
    static const std::string localName = "LocalMockObject";
    return localName;
  }

  [[nodiscard]] sen::TimeStamp getLastCommitTime() const noexcept override { return sen::TimeStamp {0}; }

  [[nodiscard]] sen::Var getPropertyUntyped(const sen::Property* /*prop*/) const override { return {}; }

  void invokeUntyped(const sen::Method* /*method*/,
                     const sen::VarList& /*args*/,
                     sen::MethodCallback<sen::Var>&& /*onDone*/) override
  {
  }

  [[nodiscard]] sen::ConnectionGuard onPropertyChangedUntyped(const sen::Property* /*prop*/,
                                                              sen::EventCallback<sen::VarList>&& /*callback*/) override
  {
    return {};
  }

  [[nodiscard]] sen::ConnectionGuard onEventUntyped(const sen::Event* /*ev*/,
                                                    sen::EventCallback<sen::VarList>&& /*callback*/) override
  {
    return {};
  }

  void invokeAllPropertyCallbacks() override {}

  void senImplEventEmitted(sen::MemberHash /*id*/,
                           std::function<sen::VarList()>&& /*argsGetter*/,
                           const sen::EventInfo& /*info*/) override
  {
  }

  void senImplWriteAllPropertiesToStream(sen::OutputStream& /*out*/) const override {}
  void senImplWriteStaticPropertiesToStream(sen::OutputStream& /*out*/) const override {}
  void senImplWriteDynamicPropertiesToStream(sen::OutputStream& /*out*/) const override {}

  void senImplRemoveTypedConnection(const sen::ConnId id) override
  {
    typedRemovedId = id.get();
    typedRemovedCalled = true;
  }

  void senImplRemoveUntypedConnection(const sen::ConnId id, const sen::MemberHash memberHash) override
  {
    untypedRemovedId = id.get();
    untypedRemovedHash = memberHash;
    untypedRemovedCalled = true;
  }

  sen::ConnectionGuard makeGuard(const uint32_t id, const sen::MemberHash hash, const bool typed)
  {
    return senImplMakeConnectionGuard(sen::ConnId {id}, hash, typed);
  }

  uint32_t typedRemovedId = 0U;            // NOLINT(misc-non-private-member-variables-in-classes)
  bool typedRemovedCalled = false;         // NOLINT(misc-non-private-member-variables-in-classes)
  uint32_t untypedRemovedId = 0U;          // NOLINT(misc-non-private-member-variables-in-classes)
  sen::MemberHash untypedRemovedHash = 0;  // NOLINT(misc-non-private-member-variables-in-classes)
  bool untypedRemovedCalled = false;       // NOLINT(misc-non-private-member-variables-in-classes)
};

}  // namespace

/// @test
/// Constructs a default connection guard that is safe to destroy without any connection to
/// remove.
/// @requirements(SEN-362)
TEST(ConnectionGuard, DefaultConstructor)
{
  sen::ConnectionGuard guard;
  SUCCEED();
}

/// @test
/// Requests typed connection removal from the owning object when the guard leaves scope,
/// passing the guarded connection id, and not before.
/// @requirements(SEN-362)
TEST(ConnectionGuard, TypedDestruction)
{
  const auto obj = std::make_shared<MockConnectionObject>();

  {
    auto guard = obj->makeGuard(42U, sen::MemberHash {0}, true);
    EXPECT_FALSE(obj->typedRemovedCalled);
  }

  EXPECT_TRUE(obj->typedRemovedCalled);
  EXPECT_EQ(obj->typedRemovedId, 42U);
}

/// @test
/// Requests untyped connection removal when the guard leaves scope, handing the object both
/// the connection id and the member hash, and not before.
/// @requirements(SEN-362)
TEST(ConnectionGuard, UntypedDestruction)
{
  const auto obj = std::make_shared<MockConnectionObject>();

  {
    auto guard = obj->makeGuard(84U, sen::MemberHash {123U}, false);
    EXPECT_FALSE(obj->untypedRemovedCalled);
  }

  EXPECT_TRUE(obj->untypedRemovedCalled);
  EXPECT_EQ(obj->untypedRemovedId, 84U);
  EXPECT_EQ(obj->untypedRemovedHash.get(), 123U);
}

/// @test
/// Skips connection removal at scope exit after keep() is called on the guard.
/// @requirements(SEN-362)
TEST(ConnectionGuard, KeepPreventsDestruction)
{
  const auto obj = std::make_shared<MockConnectionObject>();

  {
    auto guard = obj->makeGuard(42U, sen::MemberHash {0}, true);
    guard.keep();
  }

  EXPECT_FALSE(obj->typedRemovedCalled);
}

/// @test
/// A move construction does not itself remove the connection, removal fires after the
/// move-constructed guard and the moved-from one leave their shared scope.
/// @requirements(SEN-362)
TEST(ConnectionGuard, MoveConstructor)
{
  const auto obj = std::make_shared<MockConnectionObject>();

  {
    {
      auto guard1 = obj->makeGuard(42U, sen::MemberHash {0}, true);
      sen::ConnectionGuard guard2(std::move(guard1));
      EXPECT_FALSE(obj->typedRemovedCalled);
    }

    EXPECT_TRUE(obj->typedRemovedCalled);
    EXPECT_EQ(obj->typedRemovedId, 42U);

    obj->typedRemovedCalled = false;
  }

  EXPECT_FALSE(obj->typedRemovedCalled);
}

/// @test
/// Releases the previously held connection at move assignment, then removes the newly assumed
/// connection only when the assigned guard itself is destroyed, the moved-from guard removes
/// nothing.
/// @requirements(SEN-362)
TEST(ConnectionGuard, MoveAssignment)
{
  const auto obj = std::make_shared<MockConnectionObject>();

  std::optional<sen::ConnectionGuard> guardOpt;
  guardOpt.emplace(obj->makeGuard(1U, sen::MemberHash {0}, true));

  EXPECT_FALSE(obj->typedRemovedCalled);

  {
    sen::ConnectionGuard guard2 = obj->makeGuard(2U, sen::MemberHash {0}, true);

    *guardOpt = std::move(guard2);

    EXPECT_TRUE(obj->typedRemovedCalled);
    EXPECT_EQ(obj->typedRemovedId, 1U);

    obj->typedRemovedCalled = false;
  }

  EXPECT_FALSE(obj->typedRemovedCalled);

  guardOpt.reset();

  EXPECT_TRUE(obj->typedRemovedCalled);
  EXPECT_EQ(obj->typedRemovedId, 2U);
}

/// @test
/// Destroys without crashing when the object behind the guard's weak reference has already
/// been destroyed.
/// @requirements(SEN-362)
TEST(ConnectionGuard, ObjectDestroyedFirst)
{
  auto obj = std::make_shared<MockConnectionObject>();
  auto guard = obj->makeGuard(42U, sen::MemberHash {0}, true);

  obj.reset();

  SUCCEED();
}
