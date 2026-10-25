// === generic_local_object_against_generated_test.cpp =================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

/// Checks that a GenericLocalObject and a generated object with the same class and values give the
/// same query results and write the same bytes to the bus.

// test helpers
#include "generic_local_object_helpers.h"

// sen
#include "sen/core/base/duration.h"
#include "sen/core/base/span.h"
#include "sen/core/base/timestamp.h"
#include "sen/core/lang/vm.h"
#include "sen/core/meta/class_type.h"
#include "sen/core/meta/property.h"
#include "sen/core/meta/struct_type.h"
#include "sen/core/meta/type.h"
#include "sen/core/meta/var.h"
#include "sen/core/meta/variant_type.h"
#include "sen/core/obj/detail/native_object_impl.h"
#include "sen/core/obj/native_object.h"
#include "sen/kernel/detail/generic_local_object.h"

// generated code
#include "bus/stl/rich.stl.h"

// google test
#include <gtest/gtest.h>

// std
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace
{

using sen::Var;
using sen::VarList;
using sen::VarMap;
using sen::lang::Value;

struct Rich final: rich::RichBase
{
  explicit Rich(std::string name): RichBase(std::move(name)) {}
  using RichBase::commit;
};

/// The field indexes of a query path, found as Interest's findFieldIndexes finds them.
std::vector<uint16_t> indexesOf(const sen::Property& property, const std::vector<std::string>& path)
{
  std::vector<uint16_t> indexes;
  const sen::Type* type = property.getType().type();
  for (std::size_t t = 1U; t < path.size(); ++t)
  {
    if (const auto* structType = type->asStructType(); structType != nullptr)
    {
      const auto fields = structType->getFields();
      for (std::size_t k = 0U; k < fields.size(); ++k)
      {
        if (fields[k].name == path[t])
        {
          indexes.push_back(static_cast<uint16_t>(k));
          type = fields[k].type.type();
          break;
        }
      }
    }
    else if (const auto* variant = type->asVariantType(); variant != nullptr)
    {
      const auto alternatives = variant->getFields();
      for (std::size_t k = 0U; k < alternatives.size(); ++k)
      {
        if (alternatives[k].type->getName() == path[t])
        {
          indexes.push_back(static_cast<uint16_t>(k));
          type = alternatives[k].type.type();
          break;
        }
      }
    }
  }
  return indexes;
}

std::string show(const Value& value)
{
  std::ostringstream out;
  out << "#" << value.index() << " ";
  std::visit(
    [&out](const auto& x)
    {
      using T = std::decay_t<decltype(x)>;
      if constexpr (std::is_same_v<T, sen::lang::VariantAccessError>)
      {
        out << "no value";
      }
      else if constexpr (std::is_same_v<T, std::string>)
      {
        out << '"' << x << '"';
      }
      else if constexpr (std::is_same_v<T, uint8_t> || std::is_same_v<T, bool>)
      {
        out << static_cast<int>(x);
      }
      else
      {
        out << x;
      }
    },
    value);
  return out.str();
}

bool same(const Value& a, const Value& b)
{
  if (a.index() != b.index())
  {
    return false;
  }
  return std::visit(
    [&b](const auto& x)
    {
      using T = std::decay_t<decltype(x)>;
      if constexpr (std::is_same_v<T, sen::lang::VariantAccessError>)
      {
        return true;
      }
      else
      {
        return x == std::get<T>(b);
      }
    },
    a);
}

/// The result of one field getter: its value, or the message it threw when made or called.
struct Read
{
  std::optional<Value> value;  // NOLINT(misc-non-private-member-variables-in-classes)
  std::string threw;           // NOLINT(misc-non-private-member-variables-in-classes)
};

Read read(const sen::NativeObject& object, const sen::Property& property, std::vector<uint16_t> indexes)
{
  Read result;
  try
  {
    const auto getter = (object.*generic_test::Access::fieldGetter())(
      property.getId(), sen::Span<uint16_t>(indexes.data(), indexes.size()));
    try
    {
      result.value = getter.getterFunc();
    }
    catch (const std::bad_variant_access&)
    {
      result.value = sen::lang::VariantAccessError {};  // as the VM converts it
    }
  }
  catch (const std::exception& err)
  {
    result.threw = err.what();
  }
  return result;
}

Var keyed(uint32_t key, Var value) { return sen::KeyedVar {key, std::make_shared<Var>(std::move(value))}; }

Var record(uint16_t tag,
           std::string label,
           Var maybe,
           VarList pilots,
           VarList bytes,
           uint32_t force,
           sen::Duration elapsed,
           double height,
           int32_t count,
           bool ok)
{
  return VarMap {{"tag", tag},
                 {"label", std::move(label)},
                 {"maybe", std::move(maybe)},
                 {"pilots", std::move(pilots)},
                 {"bytes", std::move(bytes)},
                 {"force", force},
                 {"elapsed", elapsed},
                 {"height", height},
                 {"count", count},
                 {"ok", ok}};
}

VarList bytesOf(std::size_t count, uint8_t value)
{
  VarList list;
  for (std::size_t k = 0U; k < count; ++k)
  {
    list.emplace_back(static_cast<uint8_t>(value + k));
  }
  return list;
}

using Setting = std::vector<std::pair<std::string, Var>>;

std::vector<Setting> settings()
{
  using std::chrono::milliseconds;
  // strings spelled out: a Var made from a character pointer would be a bool
  const auto none = Var {std::monostate {}};
  return {
    // the optional empty, and the payload holding a Record whose optional is full
    {{"record",
      record(9U,
             std::string {"hdr"},
             none,
             {std::string {"ab"}, std::string {"c"}},
             bytesOf(3U, 1U),
             2U,
             sen::Duration {milliseconds {1500}},
             120.25,
             -7,
             true)},
     {"payload",
      keyed(0U,
            record(1U,
                   std::string {""},
                   true,
                   {std::string {""}, std::string {"xyz"}},
                   {},
                   0U,
                   sen::Duration {},
                   -3.5,
                   42,
                   false))},
     {"height", 33.5},
     {"force", uint32_t {1U}},
     {"elapsed", sen::Duration {milliseconds {250}}},
     {"label", std::string {"rich"}}},
    // the optional full, longer strings, and the payload's other alternative
    {{"record",
      record(65535U,
             std::string {"a longer label than the small-string buffer"},
             false,
             {std::string {"a pilot with a long name, past any small-string buffer"}, std::string {""}},
             bytesOf(20U, 9U),
             1U,
             sen::Duration {milliseconds {-20}},
             0.0,
             123456,
             false)},
     {"payload", keyed(1U, VarMap {{"lat", 12.5}, {"lon", 3.25}})},
     {"height", -1.0},
     {"force", uint32_t {0U}},
     {"elapsed", sen::Duration {}},
     {"label", std::string {""}}},
  };
}

/// Every path a query can name on Rich that reaches a value.
const std::vector<std::vector<std::string>>& paths()
{
  static const std::vector<std::vector<std::string>> all {
    {"record", "count"},
    {"record", "ok"},
    {"record", "force"},
    {"record", "elapsed"},
    {"record", "height"},
    {"record", "maybe"},  // a generated class cannot read it
    {"payload", "Record", "count"},
    {"payload", "Record", "ok"},
    {"payload", "Record", "force"},
    {"payload", "Record", "elapsed"},
    {"payload", "Record", "height"},
    {"payload", "Record", "maybe"},  // the same
    {"payload", "FixedPos", "lat"},
    {"payload", "FixedPos", "lon"},
    {"height"},
    {"force"},
    {"elapsed"},
    {"label"},
  };
  return all;
}

/// Sets the same values on both objects, for the next commit.
void give(Rich& generated, sen::kernel::impl::GenericLocalObject& generic, const Setting& setting)
{
  const auto meta = rich::RichInterface::meta();
  for (const auto& [name, value]: setting)
  {
    const auto* property = meta->searchPropertyByName(name);
    generated.setNextPropertyUntyped(property, value);
    generic.setNextPropertyUntyped(property, value);
  }
}

}  // namespace

/// @test
/// Every query path reads the same from both, except an optional field, which only the generic
/// object can read.
TEST(GenericLocalObjectAgainstGenerated, readsEveryQueryPathAsTheGeneratedGettersRead)
{
  const auto meta = rich::RichInterface::meta();
  std::size_t compared = 0U;
  std::size_t expected = 0U;
  std::vector<std::string> differences;

  std::size_t n = 0U;
  for (const auto& setting: settings())
  {
    ++n;
    Rich generated("generated" + std::to_string(n));
    sen::kernel::impl::GenericLocalObject generic("generic" + std::to_string(n), generic_test::runtimeCopyOf(*meta));
    give(generated, generic, setting);

    const sen::TimeStamp time {sen::Duration {std::chrono::milliseconds {10}}};
    generated.commit(time);
    generic.commit(time);

    for (const auto& path: paths())
    {
      const auto* property = meta->searchPropertyByName(path.front());
      const auto indexes = indexesOf(*property, path);
      const auto fromGenerated = read(generated, *property, indexes);
      const auto fromGeneric = read(generic, *property, indexes);

      std::string where = "setting " + std::to_string(n) + ", ";
      for (std::size_t t = 0U; t < path.size(); ++t)
      {
        where.append(t == 0U ? "" : ".").append(path[t]);
      }

      ++compared;
      if (path.back() == "maybe")
      {
        if (!fromGenerated.value && fromGeneric.value)
        {
          ++expected;  // only the GenericLocalObject can read an optional field
        }
        else
        {
          differences.push_back(where + ": expected the generated class to refuse and the reader to answer");
        }
        continue;
      }

      if (fromGenerated.value && fromGeneric.value)
      {
        if (!same(*fromGenerated.value, *fromGeneric.value))
        {
          differences.push_back(where + ": generated " + show(*fromGenerated.value) + ", generic " +
                                show(*fromGeneric.value));
        }
        continue;
      }

      if (!fromGenerated.value && !fromGeneric.value)
      {
        continue;  // neither can read it
      }

      differences.push_back(
        where + ": generated " + (fromGenerated.value ? show(*fromGenerated.value) : "threw " + fromGenerated.threw) +
        ", generic " + (fromGeneric.value ? show(*fromGeneric.value) : "threw " + fromGeneric.threw));
    }
  }

  for (const auto& difference: differences)
  {
    ADD_FAILURE() << difference;
  }
  EXPECT_EQ(compared, 36U);
  EXPECT_EQ(expected, 4U);
}

/// @test
/// Given the same values, writes the same records as the generated object, byte for byte.
TEST(GenericLocalObjectAgainstGenerated, writesTheRecordsTheGeneratedClassWrites)
{
  using generic_test::Access;
  using generic_test::collectAll;
  using generic_test::collectChanges;
  using generic_test::collectRecords;

  const auto meta = rich::RichInterface::meta();
  Rich generated("generated");
  sen::kernel::impl::GenericLocalObject generic("generic", generic_test::runtimeCopyOf(*meta));

  auto expectSameChanges = [&generated, &generic](const std::string& when)
  {
    const auto fromGenerated = collectChanges(generated);
    const auto fromGeneric = collectChanges(generic);
    EXPECT_EQ(fromGenerated.confirmed, fromGeneric.confirmed) << when;
    EXPECT_EQ(fromGenerated.unicast, fromGeneric.unicast) << when;
    EXPECT_EQ(fromGenerated.multicast, fromGeneric.multicast) << when;
  };

  int64_t ms = 0;
  auto commitBoth = [&generated, &generic, &ms]()
  {
    ms += 10;
    const sen::TimeStamp time {sen::Duration {std::chrono::milliseconds {ms}}};
    generated.commit(time);
    generic.commit(time);
  };

  std::size_t n = 0U;
  for (const auto& setting: settings())
  {
    ++n;
    const auto when = "setting " + std::to_string(n);
    give(generated, generic, setting);
    commitBoth();

    EXPECT_EQ(collectAll(generated), collectAll(generic)) << when;
    EXPECT_EQ(collectRecords(generated, Access::writeStatic()), collectRecords(generic, Access::writeStatic())) << when;
    EXPECT_EQ(collectRecords(generated, Access::writeDynamic()), collectRecords(generic, Access::writeDynamic()))
      << when;
    expectSameChanges(when);
  }

  // one property changed: only it is written as a change
  give(generated, generic, {{"height", 77.0}});
  commitBoth();
  const auto heightOnly = collectChanges(generic);
  EXPECT_EQ(heightOnly.multicast.size(), 1U);
  EXPECT_TRUE(heightOnly.confirmed.empty());
  expectSameChanges("height changed");

  // an idle cycle: nothing is written as a change
  commitBoth();
  const auto idle = collectChanges(generic);
  EXPECT_TRUE(idle.confirmed.empty() && idle.unicast.empty() && idle.multicast.empty());
  expectSameChanges("an idle cycle");
}
