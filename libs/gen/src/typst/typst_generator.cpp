// === typst_generator.cpp =============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "../common/util.h"
#include "sen/gen/typst.h"

// generated code
#include "typst_app/document.h"
#include "typst_app/style.h"

// sen
#include "sen/core/base/hash32.h"
#include "sen/core/lang/stl_resolver.h"
#include "sen/core/meta/alias_type.h"
#include "sen/core/meta/class_type.h"
#include "sen/core/meta/custom_type.h"
#include "sen/core/meta/enum_type.h"
#include "sen/core/meta/optional_type.h"
#include "sen/core/meta/property.h"
#include "sen/core/meta/quantity_type.h"
#include "sen/core/meta/sequence_type.h"
#include "sen/core/meta/struct_type.h"
#include "sen/core/meta/type.h"
#include "sen/core/meta/variant_type.h"

// std
#include <algorithm>
#include <array>
#include <cctype>
#include <cstddef>
#include <cstring>
#include <functional>
#include <map>
#include <memory>
#include <set>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace sen::gen
{

namespace
{

// What a kind is called on the page. Both forms: a chip on one type says "class",
// a section holding many says "Object classes".
struct KindName
{
  std::string_view id;
  std::string_view one;
  std::string_view section;
};

// Section order is the order a reader wants them: the things with behaviour, then
// the things that describe data, then the wrappers.
constexpr std::array<KindName, 8> kindNames {{{"classes", "class", "Object classes"},
                                              {"structures", "structure", "Fixed records"},
                                              {"enumerations", "enumeration", "Enumerations"},
                                              {"variants", "variant", "Variant records"},
                                              {"sequences", "sequence", "Arrays"},
                                              {"quantities", "quantity", "Quantities"},
                                              {"aliases", "alias", "Aliases"},
                                              {"optionals", "optional", "Optionals"}}};

// Model text is data, not markup: a leading `=` is a heading, `//` a comment, `~` a
// non-breaking space. A string literal is read verbatim, so only the two characters
// that could end one need escaping.
[[nodiscard]] std::string literal(std::string_view text)
{
  std::string result {"\""};
  result.reserve(text.size() + text.size() / 8U + 2U);
  for (const auto character: sen::gen::detail::collapseWhitespace(text))
  {
    if (character == '\\' || character == '"')
    {
      result.push_back('\\');
    }
    result.push_back(character);
  }
  result.push_back('"');
  return result;
}

// An identifier is one long word and will not wrap. Offering a break at each camelCase
// boundary and after each dot lets it fold inside its column instead of widening the
// table or running off the page. U+200B is invisible and copies as nothing.
[[nodiscard]] std::string breakable(std::string_view name)
{
  static constexpr std::string_view zeroWidthSpace {"​"};
  std::string result;
  result.reserve(name.size() + name.size() / 4U);
  for (std::size_t i = 0U; i < name.size(); ++i)
  {
    const auto character = name[i];
    const bool boundary = i > 0U && std::isupper(static_cast<unsigned char>(character)) != 0 &&
                          std::islower(static_cast<unsigned char>(name[i - 1U])) != 0;
    if (boundary)
    {
      result.append(zeroWidthSpace);
    }
    result.push_back(character);
    if (character == '.')
    {
      result.append(zeroWidthSpace);
    }
  }
  return result;
}

// Typst labels cannot hold a dot, and every qualified name has them.
[[nodiscard]] std::string labelOf(std::string_view qualified)
{
  std::string result {"t-"};
  result.reserve(qualified.size() + 2U);
  for (const auto character: qualified)
  {
    result.push_back(character == '.' ? '-' : character);
  }
  return result;
}

// One entry the document will render, flattened out of the model so the emitting code
// reads as layout rather than as traversal.
struct Entry
{
  std::string qualified;
  std::string package;
  std::string kind;
  std::string description;
  std::vector<std::string> parents;  ///< direct parents, base first
  std::vector<std::string> usedBy;   ///< qualified names of types that name this one
  const sen::CustomType* type {nullptr};
};

// A member's type as the document names it. Optionals are unwrapped, because the page
// says "optional" in the facts line rather than in every row.
[[nodiscard]] std::string typeNameOf(const sen::Type& type)
{
  // The optional is a wrapper around the type that matters, and every optional is also a
  // custom type, so it has to be unwrapped before that test rather than after it.
  if (type.isOptionalType())
  {
    return typeNameOf(*type.asOptionalType()->getType());
  }
  if (type.isCustomType())
  {
    return std::string {type.asCustomType()->getQualifiedName()};
  }
  return std::string {type.getName()};
}

// Flags as codes with a legend, the way an interface control document writes them.
// Spelling them out costs more column width than the descriptions can spare.
[[nodiscard]] std::string flagCodes(const sen::Property& prop)
{
  const auto category = prop.getCategory();

  // Only dynamicRW has a public setter. staticRW can be set from code and configuration,
  // which is not the same thing.
  const bool writable = category == sen::PropertyCategory::dynamicRW;
  const bool dynamic = category == sen::PropertyCategory::dynamicRO || category == sen::PropertyCategory::dynamicRW;

  std::string codes {writable ? "RW" : "RO"};
  codes += dynamic ? " D" : " S";
  codes += prop.getTransportMode() == sen::TransportMode::confirmed ? " C" : " BE";
  return codes;
}

// A unit abbreviation is written for a machine: "m_per_s_sq". Typeset it the way the
// quantity is actually written down, upright as SI asks and inline rather than as a
// stacked fraction, which would be too tall for a line of declaration.
[[nodiscard]] std::string unitMath(std::string_view abbreviation)
{
  const auto symbol = [](std::string_view token) -> std::string
  {
    // Micro and degree have no ASCII spelling, so the abbreviation approximates them.
    // Spelt as UTF-8 bytes: MSVC encodes a \u escape into its own code page instead,
    // which puts a byte the document cannot carry into the output.
    if (token == "um" || token == "us")
    {
      return "\"\xc2\xb5" + std::string {token.substr(1U)} + "\"";
    }
    if (token == "deg")
    {
      return "\"\xc2\xb0\"";
    }
    if (token == "degC")
    {
      return "\"\xc2\xb0"
             "C\"";
    }
    return "\"" + std::string {token} + "\"";
  };

  std::string body {abbreviation};
  std::string exponent;
  for (const auto& [suffix, power]: {std::pair {"_sq", "2"}, std::pair {"_cubed", "3"}})
  {
    if (body.size() > std::strlen(suffix) &&
        body.compare(body.size() - std::strlen(suffix), std::strlen(suffix), suffix) == 0)
    {
      body.resize(body.size() - std::strlen(suffix));
      exponent = std::string {"^"} + power;
    }
  }

  const auto per = body.find("_per_");
  if (per == std::string::npos)
  {
    return symbol(body) + exponent;
  }
  return symbol(body.substr(0U, per)) + "\\/" + symbol(body.substr(per + 5U)) + exponent;
}

[[nodiscard]] std::string limits(const sen::QuantityType& type)
{
  const auto low = type.getMinValue();
  const auto high = type.getMaxValue();
  std::ostringstream out;
  if (low.has_value())
  {
    out << "min " << *low;
  }
  if (high.has_value())
  {
    out << (low.has_value() ? ", " : "") << "max " << *high;
  }
  return out.str();
}

// The codes flagCodes writes, and what each one tells a reader. Kept beside it so the
// legend cannot come to disagree with the column it explains.
constexpr std::array<std::pair<std::string_view, std::string_view>, 6> flagLegend {
  {{"RO", "Read-only: the value is published by whoever owns it and cannot be set from outside."},
   {"RW", "Read-write: the value can be set as well as read."},
   {"D", "Dynamic: the value changes over the life of an instance."},
   {"S", "Static: the value is fixed once an instance exists."},
   {"C", "Confirmed: delivery is acknowledged, and the update is retried until it arrives."},
   {"BE", "Best effort: the update is sent once and may be lost."}}};

// Every type this one names: its members, its arguments and returns, whatever it wraps.
// One walk, because the callers that need it would otherwise each carry their own and
// the model would grow a kind that only some of them learned about.
void forEachReferencedType(const sen::CustomType& type, const std::function<void(const sen::Type&)>& visit)
{
  if (const auto* asClass = type.isClassType() ? type.asClassType() : nullptr; asClass != nullptr)
  {
    constexpr auto ownOnly = sen::ClassType::SearchMode::doNotIncludeParents;
    for (const auto& prop: asClass->getProperties(ownOnly))
    {
      visit(*prop->getType());
    }
    for (const auto& method: asClass->getMethods(ownOnly))
    {
      for (const auto& argument: method->getArgs())
      {
        visit(*argument.type);
      }
      visit(*method->getReturnType());
    }
    for (const auto& event: asClass->getEvents(ownOnly))
    {
      for (const auto& argument: event->getArgs())
      {
        visit(*argument.type);
      }
    }
  }
  else if (const auto* asStruct = type.isStructType() ? type.asStructType() : nullptr; asStruct != nullptr)
  {
    for (const auto& field: asStruct->getFields())
    {
      visit(*field.type);
    }
  }
  else if (const auto* asVariant = type.isVariantType() ? type.asVariantType() : nullptr; asVariant != nullptr)
  {
    for (const auto& alternative: asVariant->getFields())
    {
      visit(*alternative.type);
    }
  }
  else if (const auto* asSequence = type.isSequenceType() ? type.asSequenceType() : nullptr; asSequence != nullptr)
  {
    visit(*asSequence->getElementType());
  }
  else if (const auto* asAlias = type.isAliasType() ? type.asAliasType() : nullptr; asAlias != nullptr)
  {
    visit(*asAlias->getAliasedType());
  }
  else if (const auto* asQuantity = type.isQuantityType() ? type.asQuantityType() : nullptr; asQuantity != nullptr)
  {
    visit(*asQuantity->getElementType());
  }
}

// An optional is a wrapper around the type that matters; nothing wants the wrapper.
[[nodiscard]] const sen::Type& unwrapped(const sen::Type& type)
{
  return type.isOptionalType() ? *type.asOptionalType()->getType() : type;
}

[[nodiscard]] std::string join(const std::vector<std::string>& parts, const std::string& with)
{
  std::string out;
  for (const auto& part: parts)
  {
    out += out.empty() ? part : with + part;
  }
  return out;
}

[[nodiscard]] std::vector<std::string> segmentsOf(const std::string& package)
{
  std::vector<std::string> segments;
  for (std::size_t at = 0U; at <= package.size();)
  {
    const auto dot = std::min(package.find('.', at), package.size());
    segments.push_back(package.substr(at, dot - at));
    at = dot + 1U;
  }
  return segments;
}

// A leading segment every package shares carries no information: "sen" here, nothing in a
// FOM. Dropping it gives Sen a tree and leaves flat packages flat. Never drop so much that
// a package is left with no name of its own.
[[nodiscard]] std::size_t sharedPrefix(const std::vector<std::vector<std::string>>& packages)
{
  if (packages.size() < 2U)
  {
    return 0U;
  }
  std::size_t shared = 0U;
  const auto shortest =
    std::min_element(packages.begin(), packages.end(), [](const auto& a, const auto& b) { return a.size() < b.size(); })
      ->size();
  while (
    shared + 1U < shortest &&
    std::all_of(packages.begin(), packages.end(), [&](const auto& p) { return p[shared] == packages.front()[shared]; }))
  {
    ++shared;
  }
  return shared;
}

/// "1 package", not "1 packages". A document that cannot count its own contents reads as
/// generated, which is the impression a reference has to avoid.
[[nodiscard]] std::string plural(std::size_t count, std::string_view noun, std::string_view many = {})
{
  const std::string prefix {std::to_string(count) + " "};
  if (count == 1U)
  {
    return prefix + std::string {noun};
  }
  return prefix + (many.empty() ? std::string {noun} + "s" : std::string {many});
}

// Whether the document covers this package. The empty one holds the built-ins, which are
// referenced rather than documented.
[[nodiscard]] bool documented(const std::string& package, const TypstOptions& options)
{
  if (package.empty())
  {
    return false;
  }
  const auto& include = options.includePackages;
  if (!include.empty() && std::find(include.begin(), include.end(), package) == include.end())
  {
    return false;
  }
  const auto& exclude = options.excludePackages;
  return std::find(exclude.begin(), exclude.end(), package) == exclude.end();
}

[[nodiscard]] std::string qualifiedName(const sen::lang::TypeSet& set, const sen::CustomType& type)
{
  const auto package = sen::gen::detail::computePackageName(set);
  const std::string name {type.getName()};
  if (package.empty())
  {
    return name;
  }
  std::string qualified {package};
  qualified.append(".").append(name);
  return qualified;
}

}  // namespace

class TypstGenerator::Impl
{
  /// package -> kind -> qualified names, ordered so the document is byte-identical
  /// from one run to the next.
  using PackageIndex = std::map<std::string, std::map<std::string, std::vector<std::string>>>;

public:
  [[nodiscard]] FileContents generate(const sen::lang::TypeSetContext& typeSets, const TypstOptions& options);

private:
  /// What the document renders, read out of the model before any of it is written.
  struct Document
  {
    std::map<std::string, Entry> entries;
    std::map<std::string, std::string> builtIns;
    PackageIndex byPackage;
  };

  [[nodiscard]] Document read(const sen::lang::TypeSetContext& typeSets, const TypstOptions& options);

  // What the document contains. A reference may only point at a type in here: Typst
  // treats a link to an absent label as an error rather than a dangling link, so the
  // model is not the document and the difference has to be tracked.
  std::set<std::string> included_;

  // The leading segments every package shares, as a dotted prefix. Carries no information,
  // so it is dropped wherever a name is shown.
  std::string sharedRoot_;

  /// A link, if the document contains the target; plain text otherwise, since Typst treats
  /// a link to an absent label as an error. `here` is the package doing the printing: a
  /// name from it reads unqualified, one from elsewhere keeps its package.
  [[nodiscard]] std::string reference(const std::string& qualified, const std::string& here = {}) const;

  /// The same, for use inside a `#mono[..]` span: there a table-cell argument would be a
  /// syntax error, so the link needs its leading hash and bare text needs no brackets.
  [[nodiscard]] std::string inlineReference(const std::string& qualified, const std::string& here) const;

  /// The name as it should read where it is printed: unqualified inside its own
  /// package, since the section heading already said which package that is.
  [[nodiscard]] static std::string local(const std::string& qualified, const std::string& package);

  /// The tables for one type: what it carries, and what it can be asked to do.
  void emitBody(std::ostringstream& out, const sen::CustomType& type, const std::string& package) const;

  /// Orientation before detail: what the packages are, and how the class tree runs
  /// across them. A per-package tree cannot show that one package's class descends
  /// from another's, so there is one tree and it is here.
  void emitOverview(std::ostringstream& out,
                    const std::map<std::string, Entry>& entries,
                    const PackageIndex& byPackage,
                    bool withHierarchy) const;
  void emitHierarchy(std::ostringstream& out, const std::map<std::string, Entry>& entries) const;
  void emitMembers(std::ostringstream& out, const sen::ClassType& type, const std::string& here) const;
  void emitFields(std::ostringstream& out, const sen::StructType& type, const std::string& here) const;
  void emitEnumerators(std::ostringstream& out, const sen::EnumType& type) const;
  void emitAlternatives(std::ostringstream& out, const sen::VariantType& type, const std::string& here) const;

  /// What the codes in the Flags column mean.
  static void emitFlagLegend(std::ostringstream& out);

  /// Optionals, arrays, aliases and quantities: what is wrapped, and the bounds on it.
  void emitWrapping(std::ostringstream& out, const sen::CustomType& type, const std::string& here) const;
  void emitCallables(std::ostringstream& out, const sen::ClassType& type, const std::string& here) const;
  void emitEvents(std::ostringstream& out, const sen::ClassType& type, const std::string& here) const;
};

void TypstGenerator::Impl::emitMembers(std::ostringstream& out,
                                       const sen::ClassType& type,
                                       const std::string& here) const
{
  const auto properties = type.getProperties(sen::ClassType::SearchMode::doNotIncludeParents);
  if (properties.empty())
  {
    return;
  }
  out << "#sen-table(\n  columns: (COL-NAME, COL-TYPE, COL-FLAGS, 1fr),\n"
         "  table.header([*Name*], [*Type*], [*Flags*], [*Description*]),\n";
  for (const auto& prop: properties)
  {
    out << "  [#mono(" << literal(breakable(std::string {prop->getName()})) << ")], "
        << reference(typeNameOf(*prop->getType()), here) << ", [" << flagCodes(*prop) << "], "
        << literal(prop->getDescription()) << ",\n";
  }
  out << ")\n";
}

void TypstGenerator::Impl::emitFields(std::ostringstream& out,
                                      const sen::StructType& type,
                                      const std::string& here) const
{
  if (const auto parent = type.getParent(); parent.has_value())
  {
    out << "#facts[Extends " << inlineReference(std::string {(*parent)->getQualifiedName()}, here)
        << ", and carries its fields as well.]\n";
  }

  const auto fields = type.getFields();
  if (fields.empty())
  {
    return;
  }
  out << "#sen-table(\n  columns: (COL-NAME, COL-TYPE, 1fr),\n"
         "  table.header([*Name*], [*Type*], [*Description*]),\n";
  for (const auto& field: fields)
  {
    out << "  [#mono(" << literal(breakable(field.name)) << ")], " << reference(typeNameOf(*field.type), here) << ", "
        << literal(field.description) << ",\n";
  }
  out << ")\n";
}

void TypstGenerator::Impl::emitEnumerators(std::ostringstream& out, const sen::EnumType& type) const
{
  out << "#facts[Held as #mono(" << literal(type.getStorageType().getName()) << ").]\n";

  const auto enumerators = type.getEnums();
  if (enumerators.empty())
  {
    return;
  }
  // The column count follows the length most names have, not the longest: one outlier
  // would drop a long enumeration into a single column. Names carry break
  // opportunities, so the rare long one wraps.
  std::vector<std::size_t> lengths;
  lengths.reserve(enumerators.size());
  for (const auto& enumerator: enumerators)
  {
    lengths.push_back(enumerator.name.size());
  }
  const auto ninth = lengths.begin() + static_cast<std::ptrdiff_t>(lengths.size() * 9U / 10U);
  std::nth_element(lengths.begin(), ninth, lengths.end());
  const auto typical = *ninth;
  const auto columns = typical <= 24U ? 3U : typical <= 38U ? 2U : 1U;

  out << "#enum-columns(" << columns << ",\n";
  for (const auto& enumerator: enumerators)
  {
    out << "  (" << literal(breakable(enumerator.name)) << ", [" << enumerator.key << "]),\n";
  }
  out << ")\n";
}

void TypstGenerator::Impl::emitAlternatives(std::ostringstream& out,
                                            const sen::VariantType& type,
                                            const std::string& here) const
{
  const auto alternatives = type.getFields();
  if (alternatives.empty())
  {
    return;
  }
  out << "#sen-table(\n  columns: (COL-NAME, 1fr),\n  table.header([*Type*], [*Description*]),\n";
  for (const auto& alternative: alternatives)
  {
    out << "  " << reference(typeNameOf(*alternative.type), here) << ", " << literal(alternative.description) << ",\n";
  }
  out << ")\n";
}

void TypstGenerator::Impl::emitCallables(std::ostringstream& out,
                                         const sen::ClassType& type,
                                         const std::string& here) const
{
  constexpr auto ownOnly = sen::ClassType::SearchMode::doNotIncludeParents;

  const auto methods = type.getMethods(ownOnly);
  if (!methods.empty())
  {
    // The signature column takes width in proportion to what it holds. A fixed share
    // starves the descriptions where signatures are short and overflows where they are
    // long, and one model has both.
    std::size_t longest = 0U;
    for (const auto& method: methods)
    {
      std::size_t width = 2U;
      for (const auto& argument: method->getArgs())
      {
        width += argument.name.size() + typeNameOf(*argument.type).size() + 4U;
      }
      longest = std::max(longest, width);
    }
    const auto share = std::clamp(static_cast<double>(longest) / 90.0, 0.55, 1.4);

    out << "#sen-table(\n  columns: (COL-NAME, " << share
        << "fr, 1fr),\n"
           "  table.header([*Method*], [*Signature*], [*Description*]),\n";
    for (const auto& method: methods)
    {
      std::string signature {"("};
      bool first = true;
      for (const auto& argument: method->getArgs())
      {
        if (!first)
        {
          signature += ", ";
        }
        first = false;
        signature += "#" + literal(breakable(argument.name)) + ": " + inlineReference(typeNameOf(*argument.type), here);
      }
      signature += ")";
      // A void return is the absence of a type, not a null handle.
      if (const auto& returned = *method->getReturnType(); !returned.isVoidType())
      {
        signature += " " + std::string {"\\u{2192}"} + " " + inlineReference(typeNameOf(returned), here);
      }
      out << "  [#mono(" << literal(breakable(std::string {method->getName()})) << ")], [#mono[" << signature << "]], "
          << literal(method->getDescription()) << ",\n";
    }
    out << ")\n";
  }
}

void TypstGenerator::Impl::emitFlagLegend(std::ostringstream& out)
{
  out << "= Reading the tables\n\n#prose[Every property carries three flags, in this order.]\n\n"
         "#sen-table(\n  columns: (COL-FLAGS, 1fr),\n  table.header([*Flag*], [*Meaning*]),\n";
  for (const auto& [code, meaning]: flagLegend)
  {
    out << "  [#mono[" << code << "]], [" << meaning << "],\n";
  }
  out << ")\n\n";
}

// What a class announces, as against what it can be asked. A class can carry nothing but
// events, and without this it reaches the page as a heading with no content under it.
void TypstGenerator::Impl::emitEvents(std::ostringstream& out,
                                      const sen::ClassType& type,
                                      const std::string& here) const
{
  const auto events = type.getEvents(sen::ClassType::SearchMode::doNotIncludeParents);
  if (events.empty())
  {
    return;
  }

  out << "#sen-table(\n  columns: (COL-NAME, 1fr, 1fr),\n"
         "  table.header([*Event*], [*Payload*], [*Description*]),\n";
  for (const auto& event: events)
  {
    std::string payload {"("};
    bool first = true;
    for (const auto& argument: event->getArgs())
    {
      payload += (first ? "" : ", ") + ("#" + literal(breakable(argument.name))) + ": " +
                 inlineReference(typeNameOf(*argument.type), here);
      first = false;
    }
    payload += ")";
    out << "  [#mono(" << literal(breakable(std::string {event->getName()})) << ")], [#mono[" << payload << "]], "
        << literal(event->getDescription()) << ",\n";
  }
  out << ")\n";
}

void TypstGenerator::Impl::emitBody(std::ostringstream& out,
                                    const sen::CustomType& type,
                                    const std::string& package) const
{
  if (const auto* asClass = type.isClassType() ? type.asClassType() : nullptr; asClass != nullptr)
  {
    emitMembers(out, *asClass, package);
    emitCallables(out, *asClass, package);
    emitEvents(out, *asClass, package);
  }
  else if (const auto* asStruct = type.isStructType() ? type.asStructType() : nullptr; asStruct != nullptr)
  {
    emitFields(out, *asStruct, package);
  }
  else if (const auto* asEnum = type.isEnumType() ? type.asEnumType() : nullptr; asEnum != nullptr)
  {
    emitEnumerators(out, *asEnum);
  }
  else if (const auto* asVariant = type.isVariantType() ? type.asVariantType() : nullptr; asVariant != nullptr)
  {
    emitAlternatives(out, *asVariant, package);
  }
  else
  {
    emitWrapping(out, type, package);
  }
}

// A wrapper type has no rows, only the type it wraps and the bounds put on it. Without
// this it reaches the page as a heading and nothing else, which says less than the name.
void TypstGenerator::Impl::emitWrapping(std::ostringstream& out,
                                        const sen::CustomType& type,
                                        const std::string& here) const
{
  const auto named = [this, &here](const sen::Type& wrapped) { return inlineReference(typeNameOf(wrapped), here); };
  const auto own = literal(breakable(local(std::string {type.getQualifiedName()}, here)));

  // A wrapper type is one line of the language. Writing that line says what it is and
  // what it wraps at once, and reads the same way as the html explorer shows it.
  if (const auto* asOptional = type.isOptionalType() ? type.asOptionalType() : nullptr; asOptional != nullptr)
  {
    out << "#declared[#kw[optional]\\<" << named(*asOptional->getType()) << "\\> #" << own << "#\";\"]\n";
  }
  else if (const auto* asSequence = type.isSequenceType() ? type.asSequenceType() : nullptr; asSequence != nullptr)
  {
    // The language has two keywords: a fixed array always holds its bound, a sequence
    // holds up to it. Saying "at most" for an array contradicts the model.
    out << "#declared[#kw[" << (asSequence->hasFixedSize() ? "array" : "sequence") << "]\\<"
        << named(*asSequence->getElementType());
    if (const auto bound = asSequence->getMaxSize(); bound.has_value())
    {
      out << ", #lit[" << *bound << "]";
    }
    out << "\\> #" << own << "#\";\"]\n";
  }
  else if (const auto* asAlias = type.isAliasType() ? type.asAliasType() : nullptr; asAlias != nullptr)
  {
    out << "#declared[#kw[alias] #" << own << " " << named(*asAlias->getAliasedType()) << "#\";\"]\n";
  }
  else if (const auto* asQuantity = type.isQuantityType() ? type.asQuantityType() : nullptr; asQuantity != nullptr)
  {
    out << "#declared[#kw[quantity]\\<" << named(*asQuantity->getElementType());
    if (const auto unit = asQuantity->getUnit(); unit.has_value())
    {
      if (const auto symbol = (*unit)->getAbbreviation(); !symbol.empty())
      {
        out << ", #lit($" << unitMath(symbol) << "$)";
      }
    }
    out << "\\> #" << own << "#\";\"";
    if (const auto note = limits(*asQuantity); !note.empty())
    {
      out << "#text(fill: luma(140), " << literal("  // " + note) << ")";
    }
    out << "]\n";
  }
}

std::string TypstGenerator::Impl::reference(const std::string& qualified, const std::string& here) const
{
  const auto text = literal(breakable(local(qualified, here)));
  return included_.count(qualified) != 0U ? "link(<" + labelOf(qualified) + ">)[#" + text + "]" : text;
}

std::string TypstGenerator::Impl::inlineReference(const std::string& qualified, const std::string& here) const
{
  const auto text = literal(breakable(local(qualified, here)));
  return included_.count(qualified) != 0U ? "#link(<" + labelOf(qualified) + ">)[#" + text + "]" : "#" + text;
}

std::string TypstGenerator::Impl::local(const std::string& qualified, const std::string& package)
{
  const auto prefix = package + ".";
  return qualified.compare(0U, prefix.size(), prefix) == 0 ? qualified.substr(prefix.size()) : qualified;
}

void TypstGenerator::Impl::emitHierarchy(std::ostringstream& out, const std::map<std::string, Entry>& entries) const
{
  std::map<std::string, std::vector<std::string>> children;
  std::vector<std::string> roots;
  for (const auto& [name, entry]: entries)
  {
    if (entry.kind != "classes")
    {
      continue;
    }
    const auto parent = entry.parents.empty() ? std::string {} : entry.parents.front();
    if (!parent.empty() && entries.count(parent) != 0U)
    {
      children[parent].push_back(name);
    }
    else
    {
      roots.push_back(name);
    }
  }
  if (roots.empty())
  {
    return;  // nothing inherits: a list of unconnected names is not a hierarchy
  }

  out << "*Class hierarchy across all packages*\n\n#hierarchy(\n";

  // Depth-first, carrying the prefix that draws the connectors. Branch characters
  // alone show that a name is indented; the continuation bars show what it hangs
  // from, which is the part a reader actually needs.
  const std::function<void(const std::string&, const std::string&, bool, bool)> walk =
    [&](const std::string& name, const std::string& prefix, const bool last, const bool top)
  {
    const auto branch = top ? std::string {} : prefix + (last ? "└── " : "├── ");
    const auto below = top ? std::string {} : prefix + (last ? "    " : "│   ");
    out << "  (\"" << branch << "\", " << reference(name, sharedRoot_) << ", <" << labelOf(name) << ">),\n";
    const auto found = children.find(name);
    if (found != children.end())
    {
      for (std::size_t i = 0U; i < found->second.size(); ++i)
      {
        walk(found->second[i], below, i + 1U == found->second.size(), false);
      }
    }
  };
  for (const auto& root: roots)
  {
    walk(root, "", true, true);
  }
  out << ")\n\n";
}

void TypstGenerator::Impl::emitOverview(std::ostringstream& out,
                                        const std::map<std::string, Entry>& entries,
                                        const PackageIndex& byPackage,
                                        bool withHierarchy) const
{
  out << "= Model overview\n\n";

  // Derive what is true of this model rather than assert the shape of another: a FOM
  // has one root and a hand-written API may have a dozen.
  std::size_t classes = 0U;
  std::size_t roots = 0U;
  std::size_t inheritFromOutside = 0U;
  for (const auto& [name, entry]: entries)
  {
    if (entry.kind != "classes")
    {
      continue;
    }
    ++classes;
    if (entry.parents.empty())
    {
      ++roots;
    }
    // A class whose parent the document leaves out is not a root: counting it as one
    // would claim the model has hierarchies it does not have.
    else if (std::none_of(entry.parents.begin(),
                          entry.parents.end(),
                          [&entries](const auto& parent) { return entries.count(parent) != 0U; }))
    {
      ++inheritFromOutside;
    }
  }

  out << "#prose("
      << literal(
           [&]
           {
             std::string said {plural(entries.size(), "type") + " in " + plural(byPackage.size(), "package") + "."};
             if (roots == 1U && inheritFromOutside == 0U)
             {
               said += " Every class descends from a single root.";
             }
             else if (roots > 1U)
             {
               said += " " + plural(classes, "class", "classes") + " in " +
                       plural(roots, "independent hierarchy", "independent hierarchies") + ".";
             }
             if (inheritFromOutside > 0U)
             {
               said += " " + plural(inheritFromOutside, "class", "classes") + " inherit from outside the document.";
             }
             return said;
           }())
      << ")\n\n";

  // Only kinds this model uses get a column: two empty columns on every row is width
  // the package names need, and dotted names then collide with the counts beside them.
  std::vector<const KindName*> used;
  for (const auto& kindName: kindNames)
  {
    const auto id = std::string {kindName.id};
    const bool anywhere =
      std::any_of(byPackage.begin(), byPackage.end(), [&id](const auto& pair) { return pair.second.count(id) != 0U; });
    if (anywhere)
    {
      used.push_back(&kindName);
    }
  }

  out << "*Packages*\n\n#packages(" << used.size() + 2U << ",\n  ([*Package*], [*Types*]";
  for (const auto* kindName: used)
  {
    auto label = std::string {kindName->section};
    out << ", [*" << label << "*]";
  }
  out << "),\n";
  // The rows follow the package tree. A node the model never declares still gets a row,
  // greyed and without counts, because the packages under it need something to hang from.
  const auto shared = sharedRoot_.empty() ? 0U : segmentsOf(sharedRoot_).size();

  std::set<std::vector<std::string>> rows;
  std::vector<std::vector<std::string>> split;
  for (const auto& [package, kinds]: byPackage)
  {
    split.push_back(segmentsOf(package));
  }
  for (const auto& segments: split)
  {
    for (auto depth = shared + 1U; depth <= segments.size(); ++depth)
    {
      rows.emplace(segments.begin(), segments.begin() + static_cast<std::ptrdiff_t>(depth));
    }
  }

  for (const auto& path: rows)
  {
    const auto package = join(path, ".");
    const auto found = byPackage.find(package);
    const auto indent = (path.size() - shared - 1U) * 10U;
    out << "  ([#h(" << indent << "pt)";
    if (found == byPackage.end())
    {
      out << "#text(fill: luma(120))[#" << literal(path.back()) << "]], [—]";
      for (std::size_t i = 0U; i < used.size(); ++i)
      {
        out << ", [—]";
      }
      out << "),\n";
      continue;
    }

    std::size_t total = 0U;
    for (const auto& [kind, names]: found->second)
    {
      total += names.size();
    }
    out << "#" << literal(path.back()) << "], [" << total << "]";
    for (const auto* kindName: used)
    {
      const auto kind = found->second.find(std::string {kindName->id});
      out << ", [" << (kind == found->second.end() ? std::string {"—"} : std::to_string(kind->second.size())) << "]";
    }
    out << "),\n";
  }
  out << ")\n\n";

  if (withHierarchy)
  {
    emitHierarchy(out, entries);
  }
}

namespace
{

// Which style module the document imports. A caller replacing it is naming a file we do
// not write, so both the skeleton and the reference have to point at theirs, not at ours.
[[nodiscard]] std::string styleImport(const TypstOptions& options)
{
  return options.style.empty() ? "style.typ" : options.style.generic_string();
}

// A caller supplies a title page by writing a Typst file, which this library never
// reads. An unset point becomes a comment naming what could go there.
[[nodiscard]] std::string fillSkeleton(std::string skeleton, const TypstOptions& options)
{
  const auto substitute = [&skeleton](std::string_view placeholder, const std::string& replacement)
  {
    const auto at = skeleton.find(placeholder);
    if (at != std::string::npos)
    {
      skeleton.replace(at, placeholder.size(), replacement);
    }
  };

  const auto includeOrHint = [](const std::filesystem::path& path, std::string_view what)
  {
    return path.empty() ? "// " + std::string {what} + ": supply a .typ file and include it here"
                        : "#include " + literal(path.generic_string());
  };

  substitute("{{TITLE}}", literal(options.title));
  substitute("{{STYLE}}", literal(styleImport(options)));
  substitute("{{FRONT_MATTER}}", includeOrHint(options.frontMatter, "your title page"));
  substitute("{{BEFORE_REFERENCE}}", includeOrHint(options.beforeReference, "your scope and conventions"));
  substitute("{{AFTER_REFERENCE}}", includeOrHint(options.afterReference, "your appendices"));
  return skeleton;
}

}  // namespace

// What the document renders, read out of the model before any of it is written.
TypstGenerator::Impl::Document TypstGenerator::Impl::read(const sen::lang::TypeSetContext& typeSets,
                                                          const TypstOptions& options)
{
  // Every type in the model, in one flat map keyed by qualified name.
  std::map<std::string, Entry> entries;
  std::map<std::string, std::string> builtIns;

  // Qualified name -> the types that name it. Built while the model is walked, because a
  // type does not know what refers to it until everything has been read.
  std::map<std::string, std::set<std::string>> uses;

  // Types the model names that no type set declares: the kernel's own, sen.Duration and
  // sen.TimeStamp among them. They are not part of the model and still have to lead
  // somewhere, so they join the built-ins rather than reaching the page as bare text.
  std::map<std::string, const sen::CustomType*> referencedButUndeclared;
  included_.clear();
  sharedRoot_.clear();

  std::map<std::string, const sen::CustomType*> declared;
  for (const auto* set: sen::gen::detail::collectAllTypeSets(typeSets))
  {
    for (const auto& handle: set->types)
    {
      declared.emplace(qualifiedName(*set, *handle), &*handle);
    }
  }

  for (const auto* set: sen::gen::detail::collectAllTypeSets(typeSets))
  {
    const auto package = sen::gen::detail::computePackageName(*set);
    if (!documented(package, options))
    {
      continue;
    }

    for (const auto& handle: set->types)
    {
      const auto& type = *handle;
      Entry entry;
      entry.qualified = qualifiedName(*set, type);
      entry.package = package;
      entry.kind = sen::gen::detail::kindOf(type);
      entry.description = type.getDescription();
      entry.type = &type;
      if (const auto* asClass = type.isClassType() ? type.asClassType() : nullptr; asClass != nullptr)
      {
        // Direct parents, in declaration order: a base first, then anything implemented.
        for (const auto& parent: asClass->getParents())
        {
          entry.parents.emplace_back(parent->getQualifiedName());
        }
      }
      forEachReferencedType(type,
                            [&](const sen::Type& referenced)
                            {
                              const auto& bare = unwrapped(referenced);
                              if (const auto* custom = bare.isCustomType() ? bare.asCustomType() : nullptr;
                                  custom != nullptr)
                              {
                                const std::string name {custom->getQualifiedName()};
                                uses[name].insert(entry.qualified);
                                if (declared.count(name) == 0U)
                                {
                                  referencedButUndeclared.emplace(name, custom);
                                }
                              }
                              // void is what a method returns when it returns nothing, not
                              // a type anything holds.
                              else if (!bare.isVoidType())
                              {
                                builtIns.emplace(bare.getName(), bare.getDescription());
                              }
                            });
      entries.emplace(entry.qualified, std::move(entry));
    }
  }

  for (auto& [name, entry]: entries)
  {
    if (const auto found = uses.find(name); found != uses.end())
    {
      entry.usedBy.assign(found->second.begin(), found->second.end());
    }
  }

  for (const auto& [name, entry]: entries)
  {
    included_.insert(name);
  }
  if (options.builtIns)
  {
    for (const auto& [name, type]: referencedButUndeclared)
    {
      builtIns.emplace(name, type->getDescription());
    }
    for (const auto& [name, description]: builtIns)
    {
      included_.insert(name);
    }
  }

  // Packages, and the types within each grouped by kind. Both maps are ordered, so
  // the document is byte-identical from one run to the next.
  std::map<std::string, std::map<std::string, std::vector<std::string>>> byPackage;
  for (const auto& [name, entry]: entries)
  {
    byPackage[entry.package][entry.kind].push_back(name);
  }

  std::vector<std::vector<std::string>> packageSegments;
  packageSegments.reserve(byPackage.size());
  for (const auto& [package, kinds]: byPackage)
  {
    packageSegments.push_back(segmentsOf(package));
  }
  if (const auto shared = sharedPrefix(packageSegments); shared > 0U)
  {
    sharedRoot_ = join(
      {packageSegments.front().begin(), packageSegments.front().begin() + static_cast<std::ptrdiff_t>(shared)}, ".");
  }

  return {std::move(entries), std::move(builtIns), std::move(byPackage)};
}

TypstGenerator::FileContents TypstGenerator::Impl::generate(const sen::lang::TypeSetContext& typeSets,
                                                            const TypstOptions& options)
{
  auto [entries, builtIns, byPackage] = read(typeSets, options);

  std::ostringstream out;
  out << "#import " << literal(styleImport(options))
      << ": sen-table, facts, declared, kw, lit, summary, chip, section, prose, "
         "index-of, hierarchy, packages, enum-columns, mono, COL-NAME, COL-TYPE, COL-FLAGS\n\n";

  if (options.overview)
  {
    emitOverview(out, entries, byPackage, options.hierarchy);
  }

  // The legend explains the Flags column, which only classes have. A model of nothing but
  // records would otherwise open with a page explaining something it never shows.
  const bool anyClasses = std::any_of(
    byPackage.begin(), byPackage.end(), [](const auto& pair) { return pair.second.count("classes") != 0U; });
  if (options.flagLegend && anyClasses)
  {
    emitFlagLegend(out);
  }

  // The heading follows the tree: a group opens a section, the packages under it are
  // sub-headings carrying only what the group has not already said.
  const auto sharedSegments = sharedRoot_.empty() ? 0U : segmentsOf(sharedRoot_).size();
  std::string openGroup;

  for (const auto& [package, kinds]: byPackage)
  {
    const auto segments = segmentsOf(package);
    const auto group =
      join({segments.begin(), segments.begin() + static_cast<std::ptrdiff_t>(sharedSegments + 1U)}, ".");
    if (group != openGroup)
    {
      out << "= #"
          << literal(join({segments.begin() + static_cast<std::ptrdiff_t>(sharedSegments),
                           segments.begin() + static_cast<std::ptrdiff_t>(sharedSegments + 1U)},
                          "."))
          << "\n\n";
      openGroup = group;
    }
    if (package != group)
    {
      out << "== #"
          << literal(join({segments.begin() + static_cast<std::ptrdiff_t>(sharedSegments + 1U), segments.end()}, "."))
          << "\n\n";
    }

    std::size_t total = 0U;
    for (const auto& [kind, names]: kinds)
    {
      total += names.size();
    }
    out << "#prose[" << total << " types.]\n\n";

    for (const auto& kindName: kindNames)
    {
      const auto found = kinds.find(std::string {kindName.id});
      if (found == kinds.end())
      {
        continue;
      }

      out << "#section(\"" << kindName.section << "\", \"" << kindName.id << "\")\n\n";

      if (options.summaries)
      {
        out << "#summary(\n";
        for (const auto& name: found->second)
        {
          out << "  (" << reference(name, package) << ", " << literal(entries.at(name).description) << ", <"
              << labelOf(name) << ">),\n";
        }
        out << ")\n\n";
      }

      for (const auto& name: found->second)
      {
        const auto& entry = entries.at(name);
        out << "==== #" << literal(local(name, package)) << " #chip(\"" << entry.kind << "\", \"" << kindName.one
            << "\") <" << labelOf(name) << ">\n";
        if (!entry.description.empty())
        {
          out << "#prose(" << literal(entry.description) << ")\n";
        }
        emitBody(out, *entry.type, package);
        if (options.usedBy && !entry.usedBy.empty())
        {
          out << "#facts[Named by ";
          for (std::size_t i = 0U; i < entry.usedBy.size(); ++i)
          {
            out << (i == 0U ? "" : ", ") << inlineReference(entry.usedBy[i], package);
          }
          out << ".]\n";
        }
        out << "\n";
      }
    }
  }

  if (options.builtIns && !builtIns.empty())
  {
    out << "= Built-in types\n\n#prose[The types the language provides. Every one is written the same way "
           "on the wire wherever it appears.]\n\n#sen-table(\n  columns: (COL-NAME, 1fr),\n"
           "  table.header([*Type*], [*Meaning*]),\n";
    for (const auto& [name, description]: builtIns)
    {
      out << "  [#mono(" << literal(name) << ") <" << labelOf(name) << ">], " << literal(description) << ",\n";
    }
    out << ")\n\n";
  }

  if (options.index)
  {
    out << "= Index\n\n#index-of(\n";
    for (const auto& [name, entry]: entries)
    {
      out << "  (" << reference(name, sharedRoot_) << ", <" << labelOf(name) << ">),\n";
    }
    out << ")\n";
  }

  FileContents files;
  files["reference.typ"] = out.str();

  // The style is emitted unless the caller brings their own: every visual decision
  // lives in that one module, so replacing it is how the document is restyled without
  // anybody editing generated output.
  if (options.style.empty())
  {
    files["style.typ"] = sen::decompressSymbolToString(style, styleSize);
  }

  files["document.typ"] = fillSkeleton(sen::decompressSymbolToString(document, documentSize), options);
  return files;
}

TypstGenerator::TypstGenerator(): pimpl_(std::make_unique<Impl>()) {}
TypstGenerator::~TypstGenerator() = default;

TypstGenerator::FileContents TypstGenerator::generate(const sen::lang::TypeSetContext& typeSets,
                                                      const TypstOptions& options)
{
  return pimpl_->generate(typeSets, options);
}

}  // namespace sen::gen
