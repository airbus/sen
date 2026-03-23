// === interest.h ======================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_CORE_OBJ_INTEREST_H
#define SEN_CORE_OBJ_INTEREST_H

// sen
#include "sen/core/base/compiler_macros.h"
#include "sen/core/base/strong_type.h"
#include "sen/core/lang/vm.h"
#include "sen/core/meta/class_type.h"
#include "sen/core/meta/type.h"
#include "sen/core/meta/type_registry.h"

// std
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <variant>

namespace sen
{

/// \addtogroup obj
/// @{

/// Unique identifier that represents the interest.
SEN_STRONG_TYPE(InterestId, uint32_t)

template <>
struct ShouldBePassedByValue<InterestId>: std::true_type
{
};

/// A condition set on an object's type.
using TypeCondition = std::variant<std::monostate, ConstTypeHandle<ClassType>, std::string>;

/// The name/address of a bus.
struct BusSpec
{
  std::string sessionName;  ///< Name of the session that owns the bus (e.g. `"session"`).
  std::string busName;      ///< Name of the bus within that session (e.g. `"objects"`).
};

/// Converts a BusSpec into a session.bus string representation
[[nodiscard]] std::string asString(const BusSpec& spec);

/// Bus conditions may be present, but are not mandatory
using BusCondition = std::optional<BusSpec>;

/// Gets the qualified type name from a type condition
[[nodiscard]] std::string_view extractQualifiedTypeName(const TypeCondition& condition);

/// Information about a variable used in the query
struct VarInfo
{
  const Property* property = nullptr;
  std::vector<uint16_t> fieldIndexes;
};

/// List of variables used in a query expression
using VarInfoList = std::vector<VarInfo>;

/// Declares interest on objects that satisfy a set of conditions.
class Interest: public std::enable_shared_from_this<Interest>
{
public:
  SEN_NOCOPY_NOMOVE(Interest)

public:
  /// Creates an `Interest` by parsing and compiling a query expression.
  /// @param query        STL filter expression.
  /// @param typeRegistry Registry used to resolve type names referenced in the query.
  /// @return Shared pointer to the newly created `Interest`.
  /// @throws std::exception if the query is syntactically or semantically invalid.
  [[nodiscard]] static std::shared_ptr<Interest> make(std::string_view query, const CustomTypeRegistry& typeRegistry);

  ~Interest() = default;

public:
  /// @return The type condition extracted from the query (class handle, name string, or empty).
  [[nodiscard]] const TypeCondition& getTypeCondition() const noexcept;

  /// @return Unique identifier assigned to this interest instance.
  [[nodiscard]] InterestId getId() const noexcept;

  /// @return Compiled VM bytecode that evaluates the query's filter expression.
  [[nodiscard]] const lang::Chunk& getQueryCode() const noexcept;

  /// @return The original query string as supplied to `make()`.
  [[nodiscard]] const std::string& getQueryString() const noexcept;

  /// @return Optional bus constraint embedded in the query, or `std::nullopt` if absent.
  [[nodiscard]] const BusCondition& getBusCondition() const noexcept;

  /// Returns (and caches) metadata about property variables referenced in the query expression.
  /// Thread-safe; result is computed lazily on first call for a given class type.
  /// @param classType Class type whose properties should be resolved against the query.
  /// @return List of `VarInfo` entries mapping query variables to class properties and field paths.
  /// @throws std::exception if a referenced property or field is not found, or the type is unknown.
  [[nodiscard]] const VarInfoList& getOrComputeVarInfoList(const ClassType* classType) const;

public:
  [[nodiscard]] bool operator==(const Interest& other) const noexcept;
  [[nodiscard]] bool operator!=(const Interest& other) const noexcept;

private:
  Interest(std::string_view query, const CustomTypeRegistry& typeRegistry);
  Interest(std::string_view query, TypeCondition typeCondition);

private:
  std::string queryString_;
  BusCondition busCondition_;
  TypeCondition type_;
  lang::Chunk code_;
  InterestId id_;
  mutable std::mutex varInfoListMutex_;
  mutable std::optional<VarInfoList> varInfoList_;
};

/// @}

}  // namespace sen

SEN_STRONG_TYPE_HASHABLE(sen::InterestId)

#endif  // SEN_CORE_OBJ_INTEREST_H
