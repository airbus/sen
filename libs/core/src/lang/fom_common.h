// === fom_common.h ====================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_LIBS_CORE_SRC_LANG_FOM_COMMON_H
#define SEN_LIBS_CORE_SRC_LANG_FOM_COMMON_H

// sen
#include "sen/core/meta/callable.h"
#include "sen/core/meta/class_type.h"
#include "sen/core/meta/type.h"

// xml parser
#include "pugixml.hpp"

// std
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace sen::lang::fom
{

/// The largest maximum a bounded sequence is given. Above this an array becomes unbounded.
constexpr std::size_t maxStaticVectorSize = 1024 * 1024;

//--------------------------------------------------------------------------------------------------------------
// Names
//--------------------------------------------------------------------------------------------------------------

/// True if str begins with subStr.
[[nodiscard]] bool startsWith(const std::string& str, const std::string subStr) noexcept;

/// True if s is not empty and holds digits only.
[[nodiscard]] bool isNumber(const std::string& s);

/// Reads a decimal unsigned value, or nothing if the text is not one or does not fit in 64 bits.
/// Surrounding whitespace and a leading `+` are ignored; any other trailing character is a
/// failure, so a half-numeric value is not read as its numeric prefix.
[[nodiscard]] std::optional<std::uint64_t> parseUnsigned(std::string_view text);

/// Splits input on any of the delimiters. Empty pieces are kept.
[[nodiscard]] std::vector<std::string_view> splitString(std::string_view input, std::string_view delimiters);

/// A copy of original with every ASCII upper case letter lowered.
[[nodiscard]] std::string toLower(const std::string& original);

/// An HLA name as a sen member name: words split on `_` and `-`, lower camel case, and a
/// `Val` suffix where the result would be a C++ keyword.
[[nodiscard]] std::string toLowerCamelCase(const std::string& original);

/// True if strNeedle appears anywhere in strHaystack, ignoring case.
[[nodiscard]] bool findStringIgnoreCase(const std::string& strHaystack, const std::string& strNeedle);

/// An HLA attribute name as a sen property name. Five names belong to the Object class, so an
/// attribute that would take one of them gets `NonSen` appended.
[[nodiscard]] std::string toSenPropertyName(const std::string& original);

/// Semantics text as a description: newlines become spaces and quotes become apostrophes.
[[nodiscard]] std::string formatSemantics(std::string str);

/// Reads `true` or `false`, ignoring case. An empty string is false; anything else throws.
[[nodiscard]] bool isTrue(std::string_view str, std::string_view attribute);

//--------------------------------------------------------------------------------------------------------------
// Nodes
//--------------------------------------------------------------------------------------------------------------

/// Collects the object class nodes that declare a class, descending through nested ones. A node
/// declares one if it carries semantics or an attribute; an extension carries no semantics.
void collectClassesNodes(pugi::xml_node node, std::vector<pugi::xml_node>& result);

/// The same for interaction classes, on semantics or at least one parameter.
void collectInteractionNodes(pugi::xml_node node, std::vector<pugi::xml_node>& result);

/// The path of an interaction's PARENT, without the interaction root, which is what walking up
/// the hierarchy needs. Empty for an interaction directly under the root. For the interaction's
/// own path, which is its identity, use interactionClassPath.
[[nodiscard]] std::string computeInteractionPath(const pugi::xpath_node& interactionNode);

/// The nested path of an interaction class node, without the interaction root, joined with dots.
/// An interaction is identified by its path, so two sharing a leaf name in different branches stay
/// apart, and one named by two modules is recognized as one interaction.
[[nodiscard]] std::string interactionClassPath(pugi::xml_node node);

/// The nested path of an object class node, without the HLA root, joined with dots. A class is
/// identified by its path, so two classes sharing a leaf name in different branches stay apart.
[[nodiscard]] std::string objectClassPath(pugi::xml_node node);

//--------------------------------------------------------------------------------------------------------------
// Types
//--------------------------------------------------------------------------------------------------------------

/// The path of a class in the FOM hierarchy, up to but not including root.
[[nodiscard]] std::string computeClassPath(const ClassType* parent, const std::string& child, const ClassType* root);

/// The static read-write properties of a class, parents first: these are its constructor arguments.
void collectConstructorArgs(std::vector<Arg>& list, const ClassType& type);

/// The same from a spec that has not been made into a type yet.
void collectConstructorArgs(std::vector<Arg>& list, const ClassSpec& spec);

/// argsToPrepend followed by currentArgs.
[[nodiscard]] std::vector<Arg> prependArgs(const std::vector<Arg>& currentArgs, const std::vector<Arg>& argsToPrepend);

/// The transport mode for a property, from the FOM transportation. Nothing for a value the DIF
/// does not define: the caller knows the file and the member, so it writes the message.
[[nodiscard]] std::optional<TransportMode> getTransportMode(const std::string& hlaTransport);

/// The same for a method: best effort is unicast there, because a call goes to one object.
[[nodiscard]] std::optional<TransportMode> getMethodTransportMode(const std::string& hlaTransport);

/// The same for an event.
[[nodiscard]] std::optional<TransportMode> getEventTransportMode(const std::string& hlaTransport);

}  // namespace sen::lang::fom

#endif  // SEN_LIBS_CORE_SRC_LANG_FOM_COMMON_H
