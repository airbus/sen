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

/// The name/address of a bus
struct BusSpec
{
  std::string sessionName;
  std::string busName;
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
  SEN_PRIVATE_TAG

public:
  /// Make an interest from a query.
  /// Throws std::exception if not well formed.
  [[nodiscard]] static std::shared_ptr<Interest> make(std::string_view query, const CustomTypeRegistry& typeRegistry);

  ~Interest() = default;

public:
  /// The condition related to the type.
  [[nodiscard]] const TypeCondition& getTypeCondition() const noexcept;

  /// The unique ID of this interest.
  [[nodiscard]] InterestId getId() const noexcept;

  /// The code that can evaluate the condition.
  [[nodiscard]] const lang::Chunk& getQueryCode() const noexcept;

  /// The user-defined string that encodes the criteria.
  [[nodiscard]] const std::string& getQueryString() const noexcept;

  /// The source of this interest (if any).
  [[nodiscard]] const BusCondition& getBusCondition() const noexcept;

  /// Compute the information about the variables used in the query expression (if any).
  /// Throws in case the type is not known, or if there's an error in the expression
  /// (property or field not present, wrong type used, etc.).
  [[nodiscard]] const VarInfoList& getOrComputeVarInfoList(const ClassType* classType) const;

public:
  [[nodiscard]] bool operator==(const Interest& other) const noexcept;
  [[nodiscard]] bool operator!=(const Interest& other) const noexcept;

public:
  Interest(std::string_view query, const CustomTypeRegistry& typeRegistry, Private notUsable);

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
