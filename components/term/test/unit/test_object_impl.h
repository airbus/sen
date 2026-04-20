// === test_object_impl.h ==============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_COMPONENTS_TERM_TEST_UNIT_TEST_OBJECT_IMPL_H
#define SEN_COMPONENTS_TERM_TEST_UNIT_TEST_OBJECT_IMPL_H

// sen
#include "sen/core/base/duration.h"
#include "sen/core/meta/class_type.h"
#include "sen/core/meta/method.h"
#include "sen/core/meta/property.h"

// generated code
#include "stl/test_object.stl.h"

// std
#include <memory>
#include <string>
#include <string_view>

namespace sen::components::term::test
{

/// Concrete TestObject implementation shared by all test files that need a live object.
/// Methods have trivial-but-correct implementations so invocation tests can assert on results.
class TestObjectImpl final: public ::term::test::TestObjectBase
{
public:
  using TestObjectBase::TestObjectBase;

protected:
  std::string pingImpl() const override { return "pong"; }
  i32 addImpl(i32 a, i32 b) const override { return a + b; }
  std::string echoImpl(const std::string& message) const override { return message; }
  ::term::test::Point movePointImpl(const ::term::test::Point& p) const override
  {
    return ::term::test::Point {p.x + 1, p.y + 1};
  }
  void configureImpl(bool /*enabled*/, const ::term::test::Colour& /*tint*/) const override {}
  i32 sumIntsImpl(const ::term::test::IntSeq& values) const override
  {
    i32 s = 0;
    for (auto v: values)
      s += v;
    return s;
  }
  i32 sumBoundedImpl(const ::term::test::BoundedInts& values) const override
  {
    i32 s = 0;
    for (auto v: values)
      s += v;
    return s;
  }
  i32 sumTripleImpl(const ::term::test::TripleInts& values) const override
  {
    i32 s = 0;
    for (auto v: values)
      s += v;
    return s;
  }
  ::term::test::Point centroidImpl(const ::term::test::PointSeq& points) const override
  {
    if (points.empty())
      return ::term::test::Point {0, 0};
    i32 sx = 0, sy = 0;
    for (const auto& p: points)
    {
      sx += p.x;
      sy += p.y;
    }
    return ::term::test::Point {sx / static_cast<i32>(points.size()), sy / static_cast<i32>(points.size())};
  }
  std::string describeImpl(const ::term::test::Shape& /*s*/) const override { return "shape"; }
  std::string anchorImpl(const ::term::test::MaybePoint& /*p*/) const override { return "anchor"; }
  i32 limitImpl(const ::term::test::MaybeInt& v) const override { return v.value_or(0); }
  std::string waitImpl(sen::Duration /*duration*/) const override { return "waited"; }
  std::string setLengthImpl(::term::test::Meters /*length*/) const override { return "length set"; }
  std::string setRatioImpl(::term::test::Ratio /*ratio*/) const override { return "ratio set"; }
  void resetImpl() override { setNextCounter(0); }
};

/// Create a TestObjectImpl with the given local name and empty config.
inline std::shared_ptr<Object> makeTestObject(std::string_view localName)
{
  return std::make_shared<TestObjectImpl>(std::string(localName), VarMap {});
}

/// Find a method on TestObjectInterface by name (searches declared methods, then property
/// getters/setters, since searchMethodByName only returns declared methods).
inline const Method* findTestMethod(std::string_view name)
{
  const auto& cls = *::term::test::TestObjectInterface::meta();
  if (auto* m = cls.searchMethodByName(name); m != nullptr)
  {
    return m;
  }
  for (const auto& prop: cls.getProperties(ClassType::SearchMode::includeParents))
  {
    if (prop->getGetterMethod().getName() == name)
    {
      return &prop->getGetterMethod();
    }
    if (prop->getSetterMethod().getName() == name)
    {
      return &prop->getSetterMethod();
    }
  }
  return nullptr;
}

/// Find a property on TestObjectInterface by name.
inline const Property* findTestProperty(std::string_view name)
{
  const auto& cls = *::term::test::TestObjectInterface::meta();
  for (const auto& prop: cls.getProperties(ClassType::SearchMode::includeParents))
  {
    if (prop->getName() == name)
    {
      return prop.get();
    }
  }
  return nullptr;
}

}  // namespace sen::components::term::test

#endif
