// === app_renderers.cpp ===============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "app_renderers.h"

// local
#include "styles.h"
#include "unicode.h"

// sen
#include "sen/core/base/checked_conversions.h"
#include "sen/core/meta/alias_type.h"
#include "sen/core/meta/enum_type.h"
#include "sen/core/meta/sequence_type.h"
#include "sen/core/meta/type.h"

// ftxui
#include <ftxui/dom/elements.hpp>

// std
#include <cstddef>
#include <functional>
#include <string>
#include <vector>

namespace sen::components::term
{

using sen::std_util::ignoredLossyConversion;

//--------------------------------------------------------------------------------------------------------------
// Form rendering helpers
//--------------------------------------------------------------------------------------------------------------

void collectFormColumnWidths(const ArgFormField& f, std::size_t& nameWidth)
{
  nameWidth = std::max(nameWidth, f.name.size());
  for (const auto& c: f.children)
  {
    collectFormColumnWidths(c, nameWidth);
  }
}

ftxui::Elements focusedHintElements(const ArgFormField& leaf)
{
  ftxui::Elements cols;
  const bool hasType = !leaf.typeName.empty();

  std::string desc;
  if (leaf.type.has_value())
  {
    if (const auto* enumType = leaf.type.value()->asEnumType(); enumType != nullptr)
    {
      if (const auto* entry = enumType->getEnumFromName(leaf.text); entry != nullptr && !entry->description.empty())
      {
        desc = entry->description;
      }
    }
  }
  if (desc.empty())
  {
    desc = std::string(effectiveDescription(leaf.description, leaf.type.value()));
  }
  const bool hasDesc = !desc.empty();
  const bool isTimeValued =
    leaf.type.has_value() && (leaf.type.value()->isDurationType() || leaf.type.value()->isTimestampType());

  if (!hasType && !hasDesc && !isTimeValued)
  {
    return cols;
  }

  if (hasType)
  {
    cols.push_back(ftxui::text(leaf.typeName) | styles::typeName());
  }
  if (hasDesc)
  {
    if (hasType)
    {
      cols.push_back(ftxui::text("  " + std::string(unicode::middleDot) + "  ") | styles::mutedText());
    }
    cols.push_back(ftxui::text(desc) | styles::mutedText());
  }
  if (isTimeValued)
  {
    if (hasType || hasDesc)
    {
      cols.push_back(ftxui::text("  " + std::string(unicode::middleDot) + "  ") | styles::mutedText());
    }
    cols.push_back(ftxui::text("accepts unit suffix, e.g. 500ms, 1.5 s, 2 min, 1h") | styles::mutedText());
  }
  return cols;
}

const ArgFormField& hintSourceFor(Span<const ArgFormField> topLevel, const ArgFormField* leaf)
{
  const ArgFormField* result = leaf;
  std::function<bool(const ArgFormField&)> walk = [&](const ArgFormField& node) -> bool
  {
    if (node.kind == FieldKind::quantityGroup)
    {
      for (const auto& c: node.children)
      {
        if (&c == leaf)
        {
          result = &node;
          return true;
        }
      }
    }
    for (const auto& c: node.children)
    {
      if (walk(c))
      {
        return true;
      }
    }
    return false;
  };
  for (const auto& f: topLevel)
  {
    if (walk(f))
    {
      break;
    }
  }
  return *result;
}

void renderFormField(const ArgFormField& f,
                     std::size_t depth,
                     std::size_t& leafCursor,
                     std::size_t focusedLeafIdx,
                     std::size_t nameWidth,
                     ftxui::Elements& out)
{
  const auto pad = [](std::string s, std::size_t width)
  {
    if (s.size() < width)
    {
      s.append(width - s.size(), ' ');
    }
    return s;
  };

  const std::string indent(depth * 2U, ' ');

  const auto valueBodyFor = [](const ArgFormField& leaf, bool focused)
  {
    ftxui::Element elem;
    switch (leaf.editor)
    {
      case EditorKind::boolean:
      {
        const bool isTrue = (leaf.text == "true");
        elem = ftxui::text(std::string(isTrue ? "[x] true " : "[ ] false"));
        break;
      }
      case EditorKind::enumeration:
      case EditorKind::variantType:
      case EditorKind::integerSpin:
      case EditorKind::unitSelector:
      {
        auto body = leaf.text.empty() ? std::string("(empty)") : leaf.text;
        elem = ftxui::hbox({ftxui::text("◀ "), ftxui::text(body), ftxui::text(" ▶")});
        break;
      }
      case EditorKind::text:
      default:
      {
        auto valueText = leaf.text.empty() ? std::string("(empty)") : leaf.text;
        elem = ftxui::text(valueText);
        break;
      }
    }
    if (leaf.text.empty() && !focused && leaf.editor == EditorKind::text)
    {
      elem = elem | styles::mutedText();
    }
    else if (focused)
    {
      elem = elem | ftxui::inverted;
    }
    else if (!leaf.userEdited)
    {
      elem = elem | styles::mutedText();
    }
    return elem;
  };

  if (f.kind == FieldKind::quantityGroup && f.children.size() == 2)
  {
    const auto& valueLeaf = f.children[0];
    const auto& unitLeaf = f.children[1];
    const bool valueFocused = (leafCursor == focusedLeafIdx);
    const bool unitFocused = (leafCursor + 1 == focusedLeafIdx);
    const bool rowFocused = valueFocused || unitFocused;
    leafCursor += 2;

    ftxui::Elements cols;
    cols.push_back(ftxui::text(rowFocused ? "▸ " : "  ") | (rowFocused ? styles::accent() : styles::mutedText()));
    cols.push_back(ftxui::text(indent));

    auto nameElem = ftxui::text(pad(f.name, nameWidth) + "  ");
    cols.push_back(rowFocused ? (nameElem | ftxui::bold) : (nameElem | styles::mutedText()));

    cols.push_back(valueBodyFor(valueLeaf, valueFocused));
    cols.push_back(ftxui::text("  "));
    cols.push_back(valueBodyFor(unitLeaf, unitFocused));

    if (!valueLeaf.validationError.empty())
    {
      cols.push_back(ftxui::text("  ⚠ " + valueLeaf.validationError) | styles::errorMessage());
    }
    if (!unitLeaf.validationError.empty())
    {
      cols.push_back(ftxui::text("  ⚠ " + unitLeaf.validationError) | styles::errorMessage());
    }

    auto row = ftxui::hbox(std::move(cols));
    if (rowFocused)
    {
      row = row | ftxui::focus;
    }
    out.push_back(std::move(row));
    return;
  }

  if (!f.isLeaf())
  {
    const bool isSequence = (f.kind == FieldKind::sequenceGroup);
    const bool isOptional = (f.kind == FieldKind::optionalGroup);

    ftxui::Elements header = {ftxui::text("  " + indent),
                              ftxui::text(f.name) | ftxui::bold | styles::mutedText(),
                              ftxui::text(": ") | styles::mutedText(),
                              ftxui::text(f.typeName) | styles::typeName()};
    if (isSequence)
    {
      std::string decoration = "  (" + std::to_string(f.children.size());
      if (f.type.has_value())
      {
        // Unwrap alias so a named `sequence<i32, 5> Small;` gets the same decoration as a
        // directly-expressed type.
        ConstTypeHandle<> t = f.type.value();
        while (const auto* a = t->asAliasType())
        {
          t = a->getAliasedType();
        }
        if (const auto* seqType = t->asSequenceType(); seqType != nullptr)
        {
          if (seqType->hasFixedSize())
          {
            decoration += ", fixed";
          }
          else if (auto max = seqType->getMaxSize(); max.has_value())
          {
            decoration += "/";
            decoration += std::to_string(*max);
          }
        }
      }
      decoration += f.children.size() == 1 ? " element)" : " elements)";
      header.push_back(ftxui::text(std::move(decoration)) | styles::mutedText());
    }
    if (isOptional)
    {
      header.push_back(ftxui::text(f.optionalIsEmpty ? "  (none)" : "  (filled)") | styles::mutedText());
    }
    out.push_back(ftxui::hbox(std::move(header)));

    if (isSequence && f.children.empty())
    {
      out.push_back(ftxui::hbox({ftxui::text("  " + indent + "  "),
                                 ftxui::text("(empty, press Ctrl+N to add an element)") | styles::mutedText()}));
      return;
    }
    if (isOptional && f.optionalIsEmpty)
    {
      out.push_back(ftxui::hbox(
        {ftxui::text("  " + indent + "  "), ftxui::text("(none, press Ctrl+O to set a value)") | styles::mutedText()}));
      return;
    }

    if (isOptional && f.children.size() == 1 && !f.children[0].isLeaf())
    {
      if (f.children[0].kind == FieldKind::quantityGroup)
      {
        renderFormField(f.children[0], depth + 1, leafCursor, focusedLeafIdx, nameWidth, out);
        return;
      }
      for (const auto& c: f.children[0].children)
      {
        renderFormField(c, depth + 1, leafCursor, focusedLeafIdx, nameWidth, out);
      }
      return;
    }

    for (const auto& c: f.children)
    {
      renderFormField(c, depth + 1, leafCursor, focusedLeafIdx, nameWidth, out);
    }
    return;
  }

  const bool focused = (leafCursor == focusedLeafIdx);
  ++leafCursor;

  ftxui::Elements cols;

  cols.push_back(ftxui::text(focused ? "▸ " : "  ") | (focused ? styles::accent() : styles::mutedText()));
  cols.push_back(ftxui::text(indent));

  auto nameElem = ftxui::text(pad(f.name, nameWidth) + "  ");
  cols.push_back(focused ? (nameElem | ftxui::bold) : (nameElem | styles::mutedText()));
  cols.push_back(valueBodyFor(f, focused));

  if (!f.validationError.empty())
  {
    cols.push_back(ftxui::text("  ⚠ " + f.validationError) | styles::errorMessage());
  }

  auto row = ftxui::hbox(std::move(cols));
  if (focused)
  {
    row = row | ftxui::focus;
  }
  out.push_back(std::move(row));
}

ftxui::Element renderArgForm(const ArgForm& form)
{
  auto headerRow = ftxui::hbox({ftxui::text("> ") | ftxui::bold | styles::accent(),
                                ftxui::text(form.objectName() + ".") | styles::mutedText(),
                                ftxui::text(form.methodName()) | ftxui::bold});

  std::size_t nameWidth = 0;
  for (const auto& f: form.fields())
  {
    collectFormColumnWidths(f, nameWidth);
  }

  ftxui::Elements rows;
  std::size_t leafCursor = 0;
  for (const auto& f: form.fields())
  {
    renderFormField(f, 0, leafCursor, form.focusedIndex(), nameWidth, rows);
  }
  constexpr int maxFieldsAreaLines = 15;
  auto fieldsArea = ftxui::vbox(std::move(rows)) | ftxui::vscroll_indicator | ftxui::yframe |
                    ftxui::size(ftxui::HEIGHT, ftxui::LESS_THAN, maxFieldsAreaLines);

  ftxui::Element hintRow = ftxui::emptyElement();
  if (form.leafCount() > 0)
  {
    const auto& leaf = form.focusedField();
    const auto& source = hintSourceFor(form.fields(), &leaf);
    auto hintCols = focusedHintElements(source);
    if (!hintCols.empty())
    {
      ftxui::Elements row = {ftxui::text("  ")};
      for (auto& h: hintCols)
      {
        row.push_back(std::move(h));
      }
      hintRow = ftxui::hbox(std::move(row));
    }
  }

  return ftxui::vbox({headerRow, fieldsArea, hintRow});
}

std::string formModeHint(const ArgForm& form)
{
  std::vector<std::string> parts;

  if (form.leafCount() > 1)
  {
    parts.emplace_back("Tab/↓ next");
    parts.emplace_back("Shift+Tab/↑ prev");
  }

  switch (form.focusedEditor())
  {
    case EditorKind::boolean:
      parts.emplace_back("Space/←/→ toggle");
      break;
    case EditorKind::enumeration:
      parts.emplace_back("←/→ cycle");
      break;
    case EditorKind::variantType:
      parts.emplace_back("←/→ cycle type");
      break;
    case EditorKind::integerSpin:
      parts.emplace_back("←/→ ±1");
      break;
    case EditorKind::unitSelector:
      parts.emplace_back("←/→ cycle unit");
      break;
    case EditorKind::text:
    default:
      break;
  }

  const bool canAdd = form.focusedCanAddElement();
  const bool canRemove = form.focusedCanRemoveElement();
  if (canAdd && canRemove)
  {
    parts.emplace_back("Ctrl+N/X add/rm");
  }
  else if (canAdd)
  {
    parts.emplace_back("Ctrl+N add");
  }
  else if (canRemove)
  {
    parts.emplace_back("Ctrl+X rm");
  }
  if (form.focusedIsInsideOptional())
  {
    parts.emplace_back("Ctrl+O toggle");
  }

  if (form.leafCount() > 0)
  {
    parts.emplace_back("Enter submit");
  }
  parts.emplace_back("Esc cancel");

  std::string out;
  for (std::size_t i = 0; i < parts.size(); ++i)
  {
    if (i > 0)
    {
      out += "   ";
    }
    out += parts[i];
  }
  return out;
}

//--------------------------------------------------------------------------------------------------------------
// Watch pane rendering helper
//--------------------------------------------------------------------------------------------------------------

ftxui::Elements renderWatchEntries(const std::map<std::string, WatchEntry>& watches)
{
  ftxui::Elements watchRows;

  if (watches.empty())
  {
    watchRows.push_back(ftxui::text("No active watches.") | styles::mutedText());
    watchRows.push_back(ftxui::text(""));
    watchRows.push_back(ftxui::text("Use watch <obj> or") | styles::mutedText());
    watchRows.push_back(ftxui::text("watch <obj>.<prop>") | styles::mutedText());
    return watchRows;
  }

  struct PropEntry
  {
    std::string propName;
    const WatchEntry* entry;
  };
  std::map<std::string, std::vector<PropEntry>> sections;
  for (const auto& [key, entry]: watches)
  {
    auto dot = key.rfind('.');
    std::string objectPath = (dot != std::string::npos) ? key.substr(0, dot) : "";
    std::string propName = (dot != std::string::npos) ? key.substr(dot + 1) : key;
    sections[objectPath].push_back({std::move(propName), &entry});
  }
  bool firstSection = true;
  for (const auto& [objectPath, props]: sections)
  {
    if (!firstSection)
    {
      watchRows.push_back(ftxui::text(""));
    }
    firstSection = false;

    if (!objectPath.empty())
    {
      watchRows.push_back(ftxui::text(objectPath) | ftxui::bold | styles::paneFg());
    }

    for (std::size_t i = 0; i < props.size(); ++i)
    {
      const auto& prop = props[i];
      const bool isLast = (i + 1 == props.size());

      const std::string connector = std::string(isLast ? unicode::cornerEnd : unicode::branchTee) + " ";
      const std::string continuation = isLast ? "  " : std::string(unicode::verticalBar) + " ";

      auto connectorDeco = ftxui::color(styles::treeConnector());
      auto label = ftxui::text(prop.propName);
      auto value = prop.entry->value;

      if (prop.entry->pending)
      {
        label = label | styles::mutedText();
        value = value | styles::mutedText();
      }
      else if (!prop.entry->connected)
      {
        label = label | styles::mutedText();
        value = ftxui::text("(disconnected)") | styles::mutedText();
      }
      else if (prop.entry->age > 0U)
      {
        label = label | ftxui::bold;
        value = value | ftxui::bold;
      }

      if (prop.entry->inlineValue)
      {
        watchRows.push_back(ftxui::hbox(
          {ftxui::text(connector) | connectorDeco, std::move(label), ftxui::text(": ") | connectorDeco, value}));
      }
      else
      {
        watchRows.push_back(ftxui::hbox({ftxui::text(connector) | connectorDeco, std::move(label)}));
        if (isLast)
        {
          watchRows.push_back(ftxui::hbox({ftxui::text("   "), value}));
        }
        else
        {
          watchRows.push_back(ftxui::hbox({ftxui::separatorLight() | connectorDeco, ftxui::text("  "), value}));
        }
      }
    }
  }

  return watchRows;
}

}  // namespace sen::components::term
