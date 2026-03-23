// === class_type.h ====================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_CORE_META_CLASS_TYPE_H
#define SEN_CORE_META_CLASS_TYPE_H

// sen
#include "sen/core/meta/custom_type.h"
#include "sen/core/meta/event.h"
#include "sen/core/meta/method.h"
#include "sen/core/meta/property.h"
#include "sen/core/meta/type.h"
#include "sen/core/meta/var.h"
#include "sen/core/obj/native_object.h"

// std
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace sen::impl
{
class NativeObjectProxy;
class RemoteObject;
struct RemoteObjectInfo;
}  // namespace sen::impl

namespace sen
{

/// \addtogroup types
/// @{

class ClassType;

/// An unbounded list of properties.
using PropertyList = std::vector<std::shared_ptr<const Property>>;

/// An unbounded list of methods.
using MethodList = std::vector<std::shared_ptr<const Method>>;

/// An unbounded list of events.
using EventList = std::vector<std::shared_ptr<const Event>>;

/// An unbounded list of raw class pointers.
using ClassList = std::vector<TypeHandle<const ClassType>>;

/// A function that creates remote proxy objects.
using RemoteProxyMaker = std::function<std::shared_ptr<impl::RemoteObject>(impl::RemoteObjectInfo&&)>;

/// A function that creates local proxy objects.
using LocalProxyMaker = std::function<std::shared_ptr<impl::NativeObjectProxy>(NativeObject*, const std::string&)>;

/// Descriptor used to construct a `ClassType` instance.
struct ClassSpec final
{
  std::string name;                             ///< Short type name (e.g. `"Vehicle"`).
  std::string qualifiedName;                    ///< Fully-qualified name (e.g. `"ns.Vehicle"`).
  std::string description;                      ///< Human-readable description of the class.
  std::vector<PropertySpec> properties;         ///< Property descriptors declared on this class.
  std::vector<MethodSpec> methods;              ///< Method descriptors declared on this class.
  std::vector<EventSpec> events;                ///< Event descriptors declared on this class.
  std::optional<MethodSpec> constructor;        ///< Optional constructor method descriptor.
  ClassList parents;                            ///< Direct parent class handles (single or multiple inheritance).
  bool isInterface = false;                     ///< `true` if this class is an interface (no implementation).
  bool isAbstract = false;                      ///< `true` if this class cannot be instantiated directly.
  RemoteProxyMaker remoteProxyMaker = nullptr;  ///< Factory for remote proxy objects; may be null.
  LocalProxyMaker localProxyMaker = nullptr;    ///< Factory for local proxy objects; may be null.
};

// Comparison operators
bool operator==(const ClassSpec& lhs, const ClassSpec& rhs) noexcept;
bool operator!=(const ClassSpec& lhs, const ClassSpec& rhs) noexcept;

/// Represents a class.
class ClassType final: public CustomType
{
  SEN_META_TYPE(ClassType)
  SEN_PRIVATE_TAG

public:  // special members
  /// The make function takes care of setting all members.
  ClassType(ClassSpec spec, Private notUsable);
  ~ClassType() override = default;

public:
  /// How to search class members
  enum class SearchMode
  {
    includeParents,       ///< Recursive lookup in parents, if any.
    doNotIncludeParents,  ///< Just the local members.
  };

public:
  /// Factory function that validates the spec and creates a class type.
  /// Throws std::exception if the spec is not valid.
  [[nodiscard]] static TypeHandle<ClassType> make(const ClassSpec& spec);

  /// Factory function that creates an empty child class to hold the user implementation.
  [[nodiscard]] static TypeHandle<ClassType> makeImplementation(std::string_view name,
                                                                ConstTypeHandle<ClassType> parent,
                                                                const char* packagePath);

public:
  /// Gets the properties of this class.
  /// @param mode Whether to include properties inherited from parent classes.
  /// @return List of shared pointers to the matching `Property` objects.
  [[nodiscard]] PropertyList getProperties(SearchMode mode) const;

  /// Gets the methods of this class.
  /// @param mode Whether to include methods inherited from parent classes.
  /// @return List of shared pointers to the matching `Method` objects.
  [[nodiscard]] MethodList getMethods(SearchMode mode) const;

  /// Gets the events of this class.
  /// @param mode Whether to include events inherited from parent classes.
  /// @return List of shared pointers to the matching `Event` objects.
  [[nodiscard]] EventList getEvents(SearchMode mode) const;

  /// Get the parents of this class (if any).
  [[nodiscard]] const ClassList& getParents() const noexcept;

  /// Get the constructor method (if any).
  [[nodiscard]] const Method* getConstructor() const noexcept;

  /// Search for a property using a name.
  /// @returns nullptr if not found.
  [[nodiscard]] const Property* searchPropertyByName(std::string_view name,
                                                     SearchMode mode = SearchMode::includeParents) const;

  /// Search for a property using a hash.
  /// @returns nullptr if not found.
  [[nodiscard]] const Property* searchPropertyById(MemberHash id, SearchMode mode = SearchMode::includeParents) const;

  /// Search for a method using a name.
  /// @returns nullptr if not found.
  [[nodiscard]] const Method* searchMethodByName(std::string_view name,
                                                 SearchMode mode = SearchMode::includeParents) const;

  /// Search for a method using an id.
  /// @returns nullptr if not found.
  [[nodiscard]] const Method* searchMethodById(MemberHash id, SearchMode mode = SearchMode::includeParents) const;

  /// Search for an event using a name.
  /// @returns nullptr if not found.
  [[nodiscard]] const Event* searchEventByName(std::string_view name,
                                               SearchMode mode = SearchMode::includeParents) const;

  /// Search for an event using a hash.
  /// @returns nullptr if not found.
  [[nodiscard]] const Event* searchEventById(MemberHash id, SearchMode mode = SearchMode::includeParents) const;

  /// Returns true if this class is the same as other of inherits from it.
  [[nodiscard]] bool isSameOrInheritsFrom(const ClassType& other) const noexcept;

  /// Returns true if this class is the same as other of inherits from it.
  [[nodiscard]] bool isSameOrInheritsFrom(const std::string& otherClassName) const noexcept;

  /// True if this class represents an interface.
  [[nodiscard]] bool isInterface() const noexcept;

  /// True if this class should not be instantiated.
  [[nodiscard]] bool isAbstract() const noexcept;

  /// The unique class id (in this hierarchy).
  [[nodiscard]] MemberHash getId() const noexcept;

  /// Creates a remote proxy for this class.
  /// @param info Ownership/identity information for the new remote object.
  /// @return Shared pointer to the proxy, or `nullptr` if `remoteProxyMaker` is not set.
  [[nodiscard]] std::shared_ptr<impl::RemoteObject> createRemoteProxy(impl::RemoteObjectInfo&& info) const;

  /// Creates a local proxy for this class.
  /// @param object      The native object to wrap.
  /// @param localPrefix Prefix prepended to the proxy's name.
  /// @return Shared pointer to the proxy, or `nullptr` if `localProxyMaker` is not set.
  [[nodiscard]] std::shared_ptr<impl::NativeObjectProxy> createLocalProxy(NativeObject* object,
                                                                          const std::string& localPrefix) const;

  /// True if this class is able to instantiate objects (including proxies) that
  /// contain the real types. This must be the case for most applications.
  [[nodiscard]] bool hasProxyMakers() const noexcept;

public:
  /// Computes the hash of a method by name.
  [[nodiscard]] static MemberHash computeMethodHash(MemberHash classId, const std::string& name);

  /// Computes the hash of an event by name.
  [[nodiscard]] static MemberHash computeEventHash(MemberHash classId, const std::string& name);

  /// Computes the type getter function of a class by name.
  [[nodiscard]] static std::string computeTypeGetterFuncName(const std::string& qualName);

  /// Computes the instance maker function of a class by name.
  [[nodiscard]] static std::string computeInstanceMakerFuncName(const std::string& qualName);

public:
  /// Allow injecting a local proxy maker function. Only use this if you know what you are doing.
  void setLocalProxyMaker(LocalProxyMaker maker) const noexcept;

public:  // Type
  [[nodiscard]] std::string_view getName() const noexcept override;
  [[nodiscard]] std::string_view getQualifiedName() const noexcept override;
  [[nodiscard]] std::string_view getDescription() const noexcept override;
  [[nodiscard]] bool equals(const Type& other) const noexcept override;
  [[nodiscard]] bool isBounded() const noexcept override;

private:
  ClassSpec spec_;
  MemberHash id_;
  std::shared_ptr<const Method> constructor_;
  MethodList methods_;
  std::unordered_map<MemberHash, Method*> idToMethodMap_;
  std::unordered_map<std::string, Method*> nameToMethodMap_;
  EventList events_;
  std::unordered_map<MemberHash, Event*> idToEventMap_;
  std::unordered_map<std::string, Event*> nameToEventMap_;
  PropertyList properties_;
  std::unordered_map<MemberHash, Property*> idToPropertyMap_;
  std::unordered_map<std::string, Property*> nameToPropertyMap_;
  mutable LocalProxyMaker localProxyMaker_;  // allows customization of spec
};

/// A function that creates a native object.
using InstanceStorageType = std::shared_ptr<::sen::NativeObject>;
using InstanceMakerFunc = std::add_pointer_t<void(const std::string&, const VarMap&, InstanceStorageType&)>;

/// A list of types, together with a maker function. Used by package loading.
// using ExportedTypesList = std::vector<std::pair<const Type*, InstanceMakerFunc>>;
using ExportedTypesList = std::vector<std::pair<ConstTypeHandle<Type>, InstanceMakerFunc>>;

/// @}

//----------------------------------------------------------------------------------------------------------------------
// Inline implementation
//----------------------------------------------------------------------------------------------------------------------

/// Hash for the ClassSpec
template <>
struct impl::hash<ClassSpec>
{
  u32 operator()(const ClassSpec& spec) const noexcept
  {
    auto result = hashCombine(hashSeed, spec.name, spec.qualifiedName);

    for (const auto& prop: spec.properties)
    {
      result = hashCombine(result, prop);
    }

    for (const auto& method: spec.methods)
    {
      result = hashCombine(result, method);
    }

    for (const auto& event: spec.events)
    {
      result = hashCombine(result, event);
    }

    result = hashCombine(
      result, spec.constructor.has_value() ? impl::hash<MethodSpec>()(spec.constructor.value()) : nonPresentTypeHash);

    for (const auto& parent: spec.parents)
    {
      result = hashCombine(result, parent->getHash());
    }

    return result;
  }
};

}  // namespace sen

/// Use this macro to make your class loadable by sen
/// from a shared library. There are only 2 requirements
/// for using this functionality:
///
///   1. You need to define this macro in the same namespace
///      where the class is defined.
///
///   2. Your class must have a constructor taking
///      a name string and a VarMap with the arguments.
///
/// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define SEN_EXPORT_CLASS(classname)                                                                                    \
  [[nodiscard]] const ::sen::Type* senExport##classname(const char* packagePath)                                       \
  {                                                                                                                    \
    static ::sen::ConstTypeHandle<::sen::ClassType> val =                                                              \
      ::sen::ClassType::makeImplementation(#classname, classname::meta(), packagePath);                                \
                                                                                                                       \
    return val.type();                                                                                                 \
  }                                                                                                                    \
  void make##classname(const std::string& name, const ::sen::VarMap& args, ::sen::InstanceStorageType& outputStorage)  \
  {                                                                                                                    \
    outputStorage = std::make_shared<classname>(name, args);                                                           \
  }

#endif  // SEN_CORE_META_CLASS_TYPE_H
