// === json_type_storage.h =============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_APPS_CLI_GEN_SRC_CPP_JSON_TYPE_STORAGE_H
#define SEN_APPS_CLI_GEN_SRC_CPP_JSON_TYPE_STORAGE_H

// sen
#include "sen/core/lang/stl_resolver.h"
#include "sen/core/meta/alias_type.h"
#include "sen/core/meta/class_type.h"
#include "sen/core/meta/custom_type.h"
#include "sen/core/meta/enum_type.h"
#include "sen/core/meta/optional_type.h"
#include "sen/core/meta/quantity_type.h"
#include "sen/core/meta/sequence_type.h"
#include "sen/core/meta/struct_type.h"
#include "sen/core/meta/time_types.h"
#include "sen/core/meta/variant_type.h"

// inja
#include <inja/json.hpp>

// Stores all the type information in Json format so
// that it can be easily used by the template engine
class JsonTypeStorage final
{
  SEN_NOCOPY_NOMOVE(JsonTypeStorage)

public:
  // Takes the type set that we will
  // use to populate all the Json containers.
  explicit JsonTypeStorage(const sen::lang::TypeSet& typeSet, bool publicSymbols = false) noexcept;
  ~JsonTypeStorage() noexcept = default;

public:
  /// Gets (or creates) the Json representation of a particular
  /// sen meta type. Note that this will not compile if there
  /// is no private toJson<T>(type) function implemented.
  template <typename T>
  const inja::json& getOrCreate(const T& type);

  /// Same as getOrCreate, but ensuring that the info is available
  [[nodiscard]] inline const inja::json& get(const sen::CustomType& type) const;

  // The type set we passed to the constructor.
  [[nodiscard]] const sen::lang::TypeSet& getTypeSet() const noexcept { return typeSet_; }

  /// The type name
  [[nodiscard]] std::tuple<std::string, std::string, std::string> cppTypeName(const sen::Type& type) const;

private:
  template <typename T>
  inja::json toJsonList(const sen::Span<T>& list) const;

private:
  [[nodiscard]] inja::json toJson(const sen::StructField& field) const;
  [[nodiscard]] inja::json toJson(const sen::Enumerator& enumerator) const;
  [[nodiscard]] inja::json toJson(const sen::VariantField& field) const;
  [[nodiscard]] inja::json toJson(const sen::StructType& type) const;
  [[nodiscard]] inja::json toJson(const sen::EnumType& type) const;
  [[nodiscard]] inja::json toJson(const sen::VariantType& type) const;
  [[nodiscard]] inja::json toJson(const sen::SequenceType& type) const;
  [[nodiscard]] inja::json toJson(const sen::AliasType& type) const;
  [[nodiscard]] inja::json toJson(const sen::OptionalType& type) const;
  [[nodiscard]] inja::json toJson(const sen::QuantityType& type) const;
  [[nodiscard]] inja::json toJson(const sen::ClassType& type) const;
  [[nodiscard]] inja::json toJson(const sen::Arg& arg) const;
  [[nodiscard]] inja::json toJson(std::shared_ptr<const sen::Property> prop) const;
  [[nodiscard]] inja::json toJson(std::shared_ptr<const sen::Method> method) const;
  [[nodiscard]] inja::json toJson(const sen::Method& method) const;
  [[nodiscard]] inja::json toJson(std::shared_ptr<const sen::Event> ev) const;

private:
  [[nodiscard]] inja::json interfaceData(const sen::ClassType& type) const;

private:
  std::map<std::string, inja::json> map_;
  const sen::lang::TypeSet& typeSet_;
  std::string nameSpace_;
  bool publicSymbols_;
};

//----------------------------------------------------------------------------------------------------------------------
// Inline implementation
//----------------------------------------------------------------------------------------------------------------------

template <typename T>
const inja::json& JsonTypeStorage::getOrCreate(const T& type)
{
  const std::string qualName(type.getQualifiedName());
  auto itr = map_.find(qualName);
  if (itr != map_.end())
  {
    return itr->second;
  }

  auto [insertItr, inserted] = map_.insert({qualName, toJson(type)});
  SEN_ENSURE(inserted);
  return insertItr->second;
}

const inja::json& JsonTypeStorage::get(const sen::CustomType& type) const
{
  const std::string qualName(type.getQualifiedName());
  auto itr = map_.find(qualName);
  if (itr == map_.end())
  {
    std::string err;
    err.append("type ");
    err.append(qualName);
    err.append(" not found in the computed info");
    sen::throwRuntimeError(err);
  }

  return itr->second;
}

#endif  // SEN_APPS_CLI_GEN_SRC_CPP_JSON_TYPE_STORAGE_H
