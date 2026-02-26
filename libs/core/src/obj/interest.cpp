// === interest.cpp ====================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "sen/core/obj/interest.h"

// sen
#include "sen/core/base/assert.h"
#include "sen/core/base/hash32.h"
#include "sen/core/io/util.h"
#include "sen/core/lang/vm.h"
#include "sen/core/meta/class_type.h"
#include "sen/core/meta/struct_type.h"
#include "sen/core/meta/type.h"
#include "sen/core/meta/type_registry.h"
#include "sen/core/meta/variant_type.h"

// std
#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <utility>
#include <variant>

namespace sen
{

//--------------------------------------------------------------------------------------------------------------
// Helpers
//--------------------------------------------------------------------------------------------------------------

namespace
{

[[nodiscard]] VarInfo findFieldIndexes(const ClassType* classType, const std::string& member)
{
  auto tokens = impl::split(member, '.');
  const auto& propName = tokens.at(0);

  VarInfo result;
  result.property = classType->searchPropertyByName(propName);
  if (result.property == nullptr)
  {
    std::string err;
    err.append("did not find any property named '");
    err.append(propName);
    err.append("' in query expression for class '");
    err.append(std::string(classType->getQualifiedName()));
    err.append("'");
    throwRuntimeError(err);
  }

  if (tokens.size() == 1)
  {
    return result;
  }

  auto currentType = result.property->getType();
  for (std::size_t tokenIndex = 1U; tokenIndex < tokens.size(); ++tokenIndex)
  {
    const auto& currentFieldName = tokens.at(tokenIndex);

    const auto* structType = currentType->asStructType();
    const auto* variantType = currentType->asVariantType();
    if (structType != nullptr)
    {
      const auto* field = structType->getFieldFromName(currentFieldName);
      if (field == nullptr)
      {
        std::string err;
        err.append("structure '");
        err.append(structType->getQualifiedName());
        err.append("' does not have any field named '");
        err.append(currentFieldName);
        err.append("'");
        throwRuntimeError(err);
      }

      const auto& structFields = structType->getFields();
      for (std::size_t fieldIndex = 0; fieldIndex < structFields.size(); ++fieldIndex)
      {
        if (field == &structFields[fieldIndex])
        {
          result.fieldIndexes.push_back(static_cast<uint16_t>(fieldIndex));
          currentType = field->type;
          break;
        }
      }
    }
    else if (variantType != nullptr)
    {
      const auto& variantFields = variantType->getFields();
      bool fieldFound = false;
      for (std::size_t fieldIndex = 0; fieldIndex < variantFields.size(); ++fieldIndex)
      {
        const auto& variantField = variantFields[fieldIndex];
        if (variantField.type->getName() == currentFieldName)
        {
          fieldFound = true;
          result.fieldIndexes.push_back(static_cast<uint16_t>(fieldIndex));
          currentType = variantField.type;
          break;
        }
      }

      if (!fieldFound)
      {
        std::string err;
        err.append("variant '");
        err.append(variantType->getQualifiedName());
        err.append("' does not have any field with the type '");
        err.append(currentFieldName);
        err.append("'");
        throwRuntimeError(err);
      }
    }
    else
    {
      std::string err;
      err.append("type '");
      err.append(currentType->getName());
      err.append("' is not a structure or variant, and the field '");
      err.append(currentFieldName);
      err.append("' cannot be addressed in query expression");
      throwRuntimeError(err);
    }
  }

  return result;
}

}  // namespace

std::string_view extractQualifiedTypeName(const TypeCondition& condition)
{
  if (std::holds_alternative<std::string>(condition))
  {
    return std::string_view {std::get<std::string>(condition)};
  }

  if (std::holds_alternative<ConstTypeHandle<ClassType>>(condition))
  {
    return std::get<ConstTypeHandle<ClassType>>(condition)->getQualifiedName();
  }

  return {};
}

bool operator==(const TypeCondition& lhs, const TypeCondition& rhs) noexcept
{
  return extractQualifiedTypeName(lhs) == extractQualifiedTypeName(rhs);
}

bool operator!=(const TypeCondition& lhs, const TypeCondition& rhs) noexcept { return !(lhs == rhs); }

std::string asString(const BusSpec& spec)
{
  std::string result;
  result.append(spec.sessionName);
  result.append(".");
  result.append(spec.busName);
  return result;
}

//--------------------------------------------------------------------------------------------------------------
// Interest
//--------------------------------------------------------------------------------------------------------------

Interest::Interest(std::string_view query, const CustomTypeRegistry& typeRegistry)
  : queryString_(query), id_(crc32(queryString_))
{
  lang::VM vm;
  auto statement = vm.parse(queryString_);

  if (statement.type.has_value())
  {
    type_ = statement.type.value().qualifiedName;

    const auto type = typeRegistry.get(statement.type.value().qualifiedName);
    if (type)
    {
      if (!type.value()->isClassType())
      {
        std::string err;
        err.append("found a type named '");
        err.append(statement.type.value().qualifiedName);
        err.append("' but it is not a class (query '");
        err.append(query);
        err.append("')");
        throwRuntimeError(err);
      }
      type_ = dynamicTypeHandleCast<const ClassType>(std::move(type).value()).value();
    }
  }

  auto compileResult = vm.compile(statement);
  if (compileResult.isError())
  {
    throwRuntimeError(compileResult.getError().what);
  }

  if (!statement.bus.session.lexeme.empty())
  {
    busCondition_ = BusSpec {statement.bus.session.lexeme, statement.bus.bus.lexeme};
  }

  code_ = std::move(compileResult).getValue();
}

Interest::Interest(std::string_view query, TypeCondition typeCondition)
  : queryString_(query), type_(std::move(typeCondition)), id_(crc32(queryString_))
{
  lang::VM vm;
  auto statement = vm.parse(queryString_);
  auto compileResult = vm.compile(statement);
  if (compileResult.isError())
  {
    throwRuntimeError(compileResult.getError().what);
  }

  if (!statement.bus.session.lexeme.empty())
  {
    busCondition_ = BusSpec {statement.bus.session.lexeme, statement.bus.bus.lexeme};
  }

  if (statement.type.has_value())
  {
    const auto codeTypeName = extractQualifiedTypeName(typeCondition);
    const auto queryTypeName = statement.type.value().qualifiedName;
    if (codeTypeName != queryTypeName)
    {
      std::string err;
      err.append("inconsistent type condition; code indicates ");
      err.append(codeTypeName);
      err.append("' but the query string indicates '");
      err.append(queryTypeName);
      err.append("'");
      throwRuntimeError(err);
    }
  }

  code_ = std::move(compileResult).getValue();
}

std::shared_ptr<Interest> Interest::make(std::string_view query, const CustomTypeRegistry& typeRegistry)
{
  return std::shared_ptr<Interest>(new Interest(query, typeRegistry));  // NOSONAR
}

const VarInfoList& Interest::getOrComputeVarInfoList(const ClassType* classType) const
{
  std::lock_guard lock(varInfoListMutex_);
  if (varInfoList_.has_value())
  {
    return varInfoList_.value();
  }

  // if there's no code, set an empty list
  if (!code_.isValid())
  {
    varInfoList_ = VarInfoList {};
    return varInfoList_.value();
  }

  VarInfoList list;
  list.reserve(code_.getVariables().size());

  for (const auto& var: code_.getVariables())
  {
    // ignore built-in variables
    if (var == "name" || var == "id")
    {
      list.emplace_back();
    }
    else
    {
      list.push_back(findFieldIndexes(classType, var));
    }
  }

  varInfoList_.emplace(std::move(list));
  return varInfoList_.value();
}

bool Interest::operator==(const Interest& other) const noexcept { return queryString_ == other.queryString_; }

bool Interest::operator!=(const Interest& other) const noexcept { return !(*this == other); }

const lang::Chunk& Interest::getQueryCode() const noexcept { return code_; }

const TypeCondition& Interest::getTypeCondition() const noexcept { return type_; }

const std::string& Interest::getQueryString() const noexcept { return queryString_; }

InterestId Interest::getId() const noexcept { return id_; }

const BusCondition& Interest::getBusCondition() const noexcept { return busCondition_; }

}  // namespace sen
