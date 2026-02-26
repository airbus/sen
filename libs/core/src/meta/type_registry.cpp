// === type_registry.cpp ===============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "sen/core/meta/type_registry.h"

// sen
#include "sen/core/base/assert.h"
#include "sen/core/base/compiler_macros.h"
#include "sen/core/base/numbers.h"
#include "sen/core/meta/alias_type.h"
#include "sen/core/meta/class_type.h"
#include "sen/core/meta/detail/types_fwd.h"
#include "sen/core/meta/enum_type.h"
#include "sen/core/meta/native_types.h"
#include "sen/core/meta/optional_type.h"
#include "sen/core/meta/quantity_type.h"
#include "sen/core/meta/sequence_type.h"
#include "sen/core/meta/struct_type.h"
#include "sen/core/meta/type.h"
#include "sen/core/meta/type_visitor.h"
#include "sen/core/meta/var.h"
#include "sen/core/meta/variant_type.h"
#include "sen/core/obj/native_object.h"

// std
#include <algorithm>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <string>
#include <tuple>

namespace sen
{

//--------------------------------------------------------------------------------------------------------------
// Free functions
//--------------------------------------------------------------------------------------------------------------

void iterateOverDependentTypes(const Type& type, std::function<void(ConstTypeHandle<> type)>& callback)
{
  class Visitor: public TypeVisitor
  {
  public:
    SEN_NOCOPY_NOMOVE(Visitor)

  public:
    using CallbackType = std::function<void(ConstTypeHandle<> type)>;

    explicit Visitor(CallbackType& callback) noexcept: callback_(callback) {}

    ~Visitor() override = default;

  public:
    void apply(const Type& type) override
    {
      if (type.isCustomType())
      {
        std::string err;
        err.append("unhandled custom type '");
        err.append(type.getName());
        err.append("' in type registry");
        throw std::logic_error(err);
      }
    }

    void apply(const EnumType& type) override
    {
      std::ignore = type;
      // no dependent types for enums
    }

    void apply(const StructType& type) override
    {
      addParentTypes(type);
      const auto fields = type.getFields();
      for (const auto& elem: fields)
      {
        typeDiscovered(elem.type);
      }
    }

    void apply(const VariantType& type) override
    {
      const auto fields = type.getFields();
      for (const auto& elem: fields)
      {
        typeDiscovered(elem.type);
      }
    }

    void apply(const SequenceType& type) override { typeDiscovered(type.getElementType()); }

    void apply(const ClassType& type) override
    {
      addParentTypes(type);
      addPropertyTypes(type);
      addEventTypes(type);
      addMethodTypes(type);
    }

    void apply(const TimestampType& type) override
    {
      std::ignore = type;  // do not count timestamps as custom types
    }

    void apply(const DurationType& type) override
    {
      std::ignore = type;  // do not count durations as custom types
    }

    void apply(const QuantityType& type) override { typeDiscovered(type.getElementType()); }

    void apply(const AliasType& type) override { typeDiscovered(type.getAliasedType()); }

    void apply(const OptionalType& type) override { typeDiscovered(type.getType()); }

  private:
    void addParentTypes(const ClassType& type)
    {
      for (const auto& parent: type.getParents())
      {
        typeDiscovered(parent);
      }
    }

    void addParentTypes(const StructType& type)
    {
      auto parent = type.getParent();
      while (parent)
      {
        typeDiscovered(parent.value());
        parent = parent.value()->getParent();
      }
    }

    void addPropertyTypes(const ClassType& type)
    {
      const auto props = type.getProperties(ClassType::SearchMode::doNotIncludeParents);
      for (const auto& prop: props)
      {
        typeDiscovered(prop->getType());
      }
    }

    void addEventTypes(const ClassType& type)
    {
      const auto events = type.getEvents(ClassType::SearchMode::doNotIncludeParents);
      for (const auto& ev: events)
      {
        for (const auto& arg: ev->getArgs())
        {
          typeDiscovered(arg.type);
        }
      }
    }

    void addMethodTypes(const ClassType& type)
    {
      const auto methods = type.getMethods(ClassType::SearchMode::doNotIncludeParents);
      for (const auto& method: methods)
      {
        // arguments
        for (const auto& arg: method->getArgs())
        {
          typeDiscovered(arg.type);
        }

        // return type
        if (!method->getReturnType()->isVoidType())
        {
          typeDiscovered(method->getReturnType());
        }
      }
    }

    void typeDiscovered(ConstTypeHandle<> type)
    {
      // important: iterate first and then do the callback, so that calls go
      // from leaves (independent types) to nodes (types that have dependencies)
      Visitor visitor(callback_);
      type->accept(visitor);

      callback_(type);
    }

  private:
    CallbackType& callback_;
  };

  Visitor visitor(callback);
  type.accept(visitor);
}

//--------------------------------------------------------------------------------------------------------------
// CustomTypeRegistry
//--------------------------------------------------------------------------------------------------------------

void CustomTypeRegistry::add(ConstTypeHandle<> type)
{
  if (!type->isCustomType())
  {
    return;
  }

  // ignore built-in "custom" types
  if (type->isTimestampType() || type->isDurationType())
  {
    return;
  }

  const std::string qualifiedName(type->asCustomType()->getQualifiedName());

  {
    std::scoped_lock<std::recursive_mutex> usageLock(usageMutex_);

    // do nothing if already added
    if (types_.find(qualifiedName) != types_.end())
    {
      return;
    }

    std::function<void(ConstTypeHandle<> type)> callback = [this](ConstTypeHandle<Type> dependent)
    {
      // add the type, if custom and not present
      if (const auto* custom = dependent->asCustomType();
          dependent->isCustomType() && types_.find(std::string(custom->getQualifiedName())) == types_.end())
      {
        uncheckedAdd(dynamicTypeHandleCast<const CustomType>(dependent).value());
      }
    };

    // add the dependent types
    iterateOverDependentTypes(*type, callback);

    // add the type
    uncheckedAdd(dynamicTypeHandleCast<const CustomType>(type).value());
  }
}

void CustomTypeRegistry::addInstanceMaker(const ClassType& type, InstanceMakerFunc maker)  // NOSONAR
{
  std::scoped_lock<std::recursive_mutex> usageLock(usageMutex_);
  instanceMakers_.insert({std::string(type.getQualifiedName()), maker});
}

void CustomTypeRegistry::uncheckedAdd(ConstTypeHandle<CustomType> type)
{
  auto inserted = types_.try_emplace(std::string(type->getQualifiedName()), type);
  SEN_ASSERT(inserted.second && "Type was reinserted.");
  typesByHash_.try_emplace(type->getHash().get(), inserted.first->second.type());
  typesInDependencyOrder_.push_back(inserted.first->second.type());
}

std::optional<ConstTypeHandle<>> CustomTypeRegistry::get(const std::string& qualifiedName) const
{
  std::scoped_lock<std::recursive_mutex> usageLock(usageMutex_);
  static auto native = getNativeTypes();

  auto itr = types_.find(qualifiedName);
  if (itr == types_.end())
  {
    // look in the native types
    for (const auto& elem: native)
    {
      if (elem->getName() == qualifiedName)
      {
        return elem;
      }
    }

    return std::nullopt;
  }

  return itr->second;
}

std::optional<ConstTypeHandle<>> CustomTypeRegistry::get(u32 hash) const
{
  std::scoped_lock<std::recursive_mutex> usageLock(usageMutex_);

  if (const auto itr = typesByHash_.find(hash); itr != typesByHash_.end())
  {
    return makeNonOwningTypeHandle(itr->second);
  }

  // look in the native types
  static auto native = getNativeTypes();
  auto sameHash = [hash](ConstTypeHandle<> elem) { return elem->getHash() == hash; };

  const auto itr = std::find_if(native.begin(), native.end(), sameHash);
  if (itr != native.end())
  {
    return *itr;
  }

  return std::nullopt;
}

CustomTypeRegistry::CustomTypeMap CustomTypeRegistry::getAll() const
{
  CustomTypeMap result;

  {
    std::scoped_lock<std::recursive_mutex> usageLock(usageMutex_);
    result = types_;
  }

  return result;
}

CustomTypeRegistry::CustomTypeList CustomTypeRegistry::getAllInOrder() const
{
  CustomTypeList result;

  {
    std::scoped_lock<std::recursive_mutex> usageLock(usageMutex_);
    result = typesInDependencyOrder_;
  }

  return result;
}

std::shared_ptr<NativeObject> CustomTypeRegistry::makeInstance(const ClassType* type,
                                                               const std::string& name,
                                                               const VarMap& properties) const
{
  if (type->isAbstract())
  {
    std::string err;
    err.append("class '");
    err.append(type->getQualifiedName());
    err.append(" is marked as abstract");
    throwRuntimeError(err);
  }

  std::scoped_lock<std::recursive_mutex> usageLock(usageMutex_);
  auto maker = instanceMakers_.find(std::string(type->getQualifiedName()));
  if (maker == instanceMakers_.end())
  {
    std::string err;
    err.append("factory function for object '");
    err.append(name);
    err.append("' of class '");
    err.append(type->getQualifiedName());
    err.append(" is not found");
    throwRuntimeError(err);
  }

  InstanceStorageType createdNativeObject;
  (*maker->second)(name, properties, createdNativeObject);

  if (createdNativeObject == nullptr)
  {
    std::string err;
    err.append("error calling the factory function for object '");
    err.append(name);
    err.append("' of class '");
    err.append(type->getQualifiedName());
    throwRuntimeError(err);
  }

  return createdNativeObject;
}

}  // namespace sen
