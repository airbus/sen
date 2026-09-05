// === html_generator_test.cpp =========================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "every_kind_model.h"
#include "sen/gen/html.h"

// sen
#include "sen/core/lang/stl_parser.h"
#include "sen/core/lang/stl_resolver.h"
#include "sen/core/lang/stl_scanner.h"
#include "sen/core/lang/stl_statement.h"

// 3rd party
#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

// std
#include <algorithm>
#include <filesystem>
#include <regex>
#include <set>
#include <string>
#include <vector>

namespace
{

// The model the application reads, recovered from the file the generator writes.
class AnHtmlGenerator: public ::testing::Test
{
protected:
  void generate(const std::string& stl, const std::string& title = "Model")
  {
    sen::lang::StlScanner scanner {stl};
    sen::lang::StlParser parser {scanner.scanTokens()};
    statements_ = parser.parse();

    sen::lang::ResolverContext resolverContext {};
    sen::lang::StlResolver resolver {statements_, resolverContext, context_};
    resolver.resolve({});

    files_ = sen::gen::HtmlGenerator {}.generate(context_, title);
  }

  [[nodiscard]] const std::string& file(const std::string& path) const
  {
    const auto found = files_.find(path);
    if (found != files_.end())
    {
      return found->second;
    }
    ADD_FAILURE() << "no file named " << path;
    static const std::string none;
    return none;
  }

  // model.js hands a global the result of parsing a string. The assertions want the value
  // inside it, which takes two steps: the string literal, then the model it spells out.
  [[nodiscard]] nlohmann::json model() const
  {
    const auto& text = file("model.js");
    const auto open = text.find('"');
    const auto close = text.rfind('"');
    if (open == std::string::npos || close == std::string::npos || close <= open)
    {
      ADD_FAILURE() << "model.js carries no quoted payload";
      return nlohmann::json::object();
    }
    const auto literal = nlohmann::json::parse(text.substr(open, close - open + 1));
    return nlohmann::json::parse(literal.get<std::string>());
  }

private:
  std::vector<sen::lang::StlStatement> statements_;
  sen::lang::TypeSetContext context_;
  sen::gen::HtmlGenerator::FileContents files_;
};

constexpr auto twoClasses = R"(package t;

// A base with something of its own.
class Base
{
  var one : i32;
}

// Inherits one and adds another.
class Derived: extends Base
{
  var two : f32;
}
)";

using sen::gen::test::everyKindStl;

TEST_F(AnHtmlGenerator, writesTheApplicationBesideTheModel)
{
  generate(twoClasses);

  for (const auto* path: {"index.html", "app.css", "app.js", "logo.svg", "model.js"})
  {
    EXPECT_FALSE(file(path).empty()) << path << " is empty";
  }
}

// A title is the caller's text and reaches the page as markup.
TEST_F(AnHtmlGenerator, escapesTheTitle)
{
  generate(twoClasses, R"(Ships & <b>Boats</b>)");

  const auto& page = file("index.html");
  EXPECT_EQ(page.find("<b>Boats</b>"), std::string::npos);
  EXPECT_EQ(page.find("{{ title }}"), std::string::npos) << "a substitution site was left unfilled";

  // Both sites, not just the first: the shell names the model in the tab and in the heading,
  // and a loop that stopped after one left the heading reading "{{ title }}".
  const std::string escaped = "Ships &amp; &lt;b&gt;Boats&lt;/b&gt;";
  const auto first = page.find(escaped);
  ASSERT_NE(first, std::string::npos);
  EXPECT_NE(page.find(escaped, first + escaped.size()), std::string::npos) << "the title reached only one site";
}

// Every entry carries every key, so nothing the application reads is absent.
TEST_F(AnHtmlGenerator, everyEntryCarriesTheWholeShape)
{
  generate(twoClasses);

  const auto types = model()["types"];
  ASSERT_FALSE(types.empty());
  for (const auto& [name, entry]: types.items())
  {
    for (const auto* key: {"name",
                           "package",
                           "kind",
                           "desc",
                           "ancestry",
                           "groups",
                           "members",
                           "methods",
                           "events",
                           "facts",
                           "table",
                           "usedBy",
                           "direct",
                           "inherited",
                           "total"})
    {
      ASSERT_TRUE(entry.contains(key)) << name << " has no " << key;
      EXPECT_FALSE(entry[key].is_null()) << name << "." << key << " is null";
    }
  }
}

// Primitives are referenced but not declared, so they are collected while walking. Without
// an entry each their references resolve to nothing.
TEST_F(AnHtmlGenerator, givesReferencedPrimitivesAnEntry)
{
  generate(twoClasses);

  const auto types = model()["types"];
  ASSERT_TRUE(types.contains("i32")) << "i32 is referenced but has no entry";
  EXPECT_EQ(types["i32"]["kind"], "builtins");
  EXPECT_EQ(types["i32"]["package"], "");
}

// Every name the model mentions has to be a name the model holds, or a link goes nowhere.
TEST_F(AnHtmlGenerator, resolvesEveryReferenceItEmits)
{
  generate(everyKindStl);

  const auto types = model()["types"];
  ASSERT_FALSE(types.empty());
  for (const auto& [name, entry]: types.items())
  {
    for (const auto& member: entry["members"])
    {
      EXPECT_TRUE(types.contains(member["type"].get<std::string>()))
        << name << "." << member["name"] << " has type " << member["type"] << ", which is not in the model";
    }
    for (const auto& ancestor: entry["ancestry"])
    {
      EXPECT_TRUE(types.contains(ancestor.get<std::string>())) << name << " descends from an absent " << ancestor;
    }
    for (const auto& used: entry["usedBy"])
    {
      EXPECT_TRUE(types.contains(used.get<std::string>())) << name << " is used by an absent " << used;
    }
    // The facts too. Leaving them out is why an enum's storage type and the kernel's own
    // Duration and TimeStamp sat unresolved in every generated page.
    for (const auto* key: {"element", "optional", "aliased", "representation"})
    {
      if (entry["facts"].contains(key))
      {
        EXPECT_TRUE(types.contains(entry["facts"][key].get<std::string>()))
          << name << " states " << key << " " << entry["facts"][key] << ", which is not in the model";
      }
    }
    for (const auto& callable: entry["methods"])
    {
      const auto returned = callable["returns"].get<std::string>();
      EXPECT_TRUE(returned.empty() || types.contains(returned))
        << name << "." << callable["name"] << " returns " << returned << ", which is not in the model";
    }
  }
}

// E4: the shell names the other files and the global; nothing else checks they agree, and
// renaming either side ships a page that loads nothing and says nothing.
TEST_F(AnHtmlGenerator, theShellNamesFilesThatAreEmitted)
{
  generate(twoClasses);

  const auto& shell = file("index.html");
  for (const auto* name: {"app.css", "app.js", "model.js"})
  {
    EXPECT_NE(shell.find(std::string {"\""} + name + "\""), std::string::npos) << "the shell does not load " << name;
    EXPECT_FALSE(file(name).empty()) << name << " is named by the shell but empty";
  }
}

TEST_F(AnHtmlGenerator, theModelAndTheApplicationAgreeOnTheGlobal)
{
  generate(twoClasses);

  const auto& data = file("model.js");
  const auto assigned = data.substr(0, data.find('='));
  EXPECT_FALSE(assigned.empty());
  EXPECT_NE(file("app.js").find(assigned), std::string::npos)
    << "model.js assigns " << assigned << ", which the application never reads";
}

TEST_F(AnHtmlGenerator, groupsInheritedMembersByTheClassDeclaringThem)
{
  generate(twoClasses);

  const auto derived = model()["types"]["t.Derived"];
  EXPECT_EQ(derived["direct"], 1);
  EXPECT_EQ(derived["inherited"], 1);
  EXPECT_EQ(derived["total"], 2);
  ASSERT_EQ(derived["groups"].size(), 1U);
  EXPECT_EQ(derived["groups"][0]["from"], "t.Base");
  EXPECT_EQ(derived["groups"][0]["members"][0]["name"], "one");
}

TEST_F(AnHtmlGenerator, recordsWhatUsesEachType)
{
  generate(twoClasses);

  const auto types = model()["types"];
  const auto& usedBy = types["i32"]["usedBy"];
  EXPECT_NE(std::find(usedBy.begin(), usedBy.end(), "t.Base"), usedBy.end());
}

// The kernel does not require a description to be UTF-8. A strict dump refused the whole
// model over one byte.
TEST_F(AnHtmlGenerator, survivesADescriptionThatIsNotUtf8)
{
  std::string stl =
    "package t;\n\n// Description with a lone continuation byte: \x80 here.\nstruct S\n{\n  a : i32\n}\n";

  EXPECT_NO_THROW(generate(stl));
  EXPECT_FALSE(file("model.js").empty());
}

// An empty model is a model. It should produce the application, not an error.
TEST_F(AnHtmlGenerator, generatesForAModelThatDeclaresNothing)
{
  generate("package t;\n");

  EXPECT_FALSE(file("index.html").empty());
  EXPECT_TRUE(model()["types"].empty());
}

// The kind decides which group a type sits under and which filter reaches it, so a wrong
// one hides a type in plain sight.
TEST_F(AnHtmlGenerator, namesTheKindOfEveryTypeItEmits)
{
  generate(everyKindStl);

  const auto types = model()["types"];
  EXPECT_EQ(types["k.deep.Metres"]["kind"], "quantities");
  EXPECT_EQ(types["k.deep.Colour"]["kind"], "enumerations");
  EXPECT_EQ(types["k.deep.Point"]["kind"], "structures");
  EXPECT_EQ(types["k.deep.Figure"]["kind"], "variants");
  EXPECT_EQ(types["k.deep.Track"]["kind"], "sequences");
  EXPECT_EQ(types["k.deep.Triple"]["kind"], "sequences");
  EXPECT_EQ(types["k.deep.MaybeColour"]["kind"], "optionals");
  EXPECT_EQ(types["k.deep.Distance"]["kind"], "aliases");
  EXPECT_EQ(types["k.deep.Derived"]["kind"], "classes");
  EXPECT_EQ(types["f32"]["kind"], "builtins");
}

// What defines a type that has no members. Stated as fields, because prose cannot be read
// back: a sequence once reached the page as "sequence<?>".
TEST_F(AnHtmlGenerator, statesWhatDefinesATypeWithNoMembers)
{
  generate(everyKindStl);

  const auto types = model()["types"];

  EXPECT_EQ(types["k.deep.Metres"]["facts"]["representation"], "f32");
  EXPECT_EQ(types["k.deep.Metres"]["facts"]["unit"], "m");
  EXPECT_EQ(types["k.deep.Colour"]["facts"]["representation"], "u8");
  EXPECT_EQ(types["k.deep.MaybeColour"]["facts"]["optional"], "k.deep.Colour");
  EXPECT_EQ(types["k.deep.Distance"]["facts"]["aliased"], "k.deep.Metres");

  const auto bounded = types["k.deep.Track"]["facts"];
  EXPECT_EQ(bounded["element"], "k.deep.Point");
  EXPECT_EQ(bounded["bounded"], true);
  EXPECT_EQ(bounded["fixedSize"], false);
  EXPECT_EQ(bounded["maxSize"], 8);

  const auto fixed = types["k.deep.Triple"]["facts"];
  EXPECT_EQ(fixed["element"], "k.deep.Metres");
  EXPECT_EQ(fixed["fixedSize"], true);
  EXPECT_EQ(fixed["maxSize"], 3);
}

TEST_F(AnHtmlGenerator, listsTheValuesOfAnEnumeration)
{
  generate(everyKindStl);

  const auto table = model()["types"]["k.deep.Colour"]["table"];
  ASSERT_EQ(table["rows"].size(), 3U);
  EXPECT_EQ(table["rows"][0][0], "red");
  EXPECT_EQ(table["rows"][0][1], "0");
  EXPECT_EQ(table["rows"][2][0], "blue");
  EXPECT_EQ(table["rows"][2][1], "2");
}

TEST_F(AnHtmlGenerator, listsTheAlternativesOfAVariant)
{
  generate(everyKindStl);

  const auto rows = model()["types"]["k.deep.Figure"]["table"]["rows"];
  ASSERT_EQ(rows.size(), 2U);
  EXPECT_EQ(rows[0][0], "k.deep.Point");
  EXPECT_EQ(rows[1][0], "k.deep.Circle");
}

// No FOM in the corpus declares a method or an event, so nothing but STL exercises this.
TEST_F(AnHtmlGenerator, emitsMethodsAndEventsWithTheirArguments)
{
  generate(everyKindStl);

  const auto derived = model()["types"]["k.deep.Derived"];

  ASSERT_EQ(derived["methods"].size(), 1U);
  const auto method = derived["methods"][0];
  EXPECT_EQ(method["name"], "measure");
  EXPECT_EQ(method["returns"], "k.deep.Metres");
  ASSERT_EQ(method["args"].size(), 1U);
  EXPECT_EQ(method["args"][0]["name"], "from");
  EXPECT_EQ(method["args"][0]["type"], "k.deep.Point");

  ASSERT_EQ(derived["events"].size(), 1U);
  const auto event = derived["events"][0];
  EXPECT_EQ(event["name"], "moved");
  ASSERT_EQ(event["args"].size(), 1U);
  EXPECT_EQ(event["args"][0]["type"], "k.deep.Point");
}

// A type can be named from a member, a sequence element, an optional, an alias, a variant
// alternative, a return type or an argument. Walking only members undercounted it badly.
TEST_F(AnHtmlGenerator, countsAReferenceFromEveryPlaceOneCanAppear)
{
  generate(everyKindStl);

  const auto types = model()["types"];
  const auto uses = [&](const std::string& type, const std::string& user)
  {
    const auto& list = types[type]["usedBy"];
    return std::find(list.begin(), list.end(), user) != list.end();
  };

  EXPECT_TRUE(uses("k.deep.Metres", "k.deep.Point")) << "a member's type";
  EXPECT_TRUE(uses("k.deep.Point", "k.deep.Track")) << "a sequence element";
  EXPECT_TRUE(uses("k.deep.Metres", "k.deep.Triple")) << "a fixed-size sequence element";
  EXPECT_TRUE(uses("k.deep.Colour", "k.deep.MaybeColour")) << "what an optional wraps";
  EXPECT_TRUE(uses("k.deep.Metres", "k.deep.Distance")) << "what an alias stands for";
  EXPECT_TRUE(uses("k.deep.Point", "k.deep.Figure")) << "a variant alternative";
  EXPECT_TRUE(uses("k.deep.Metres", "k.deep.Derived")) << "a method's return type";
  EXPECT_TRUE(uses("k.deep.Point", "k.deep.Derived")) << "a method or event argument";

  // and nothing counts itself
  for (const auto& [name, entry]: types.items())
  {
    const auto& list = entry["usedBy"];
    EXPECT_EQ(std::find(list.begin(), list.end(), name), list.end()) << name << " is listed as using itself";
  }
}

// The primitives are collected while walking, through state that lives for one call. If any
// of it outlived the call, a second run would differ from the first.
TEST_F(AnHtmlGenerator, producesTheSameModelEveryTime)
{
  // Everything but the stamp: the point is that no state survives a call, not that the clock
  // stands still between two of them.
  const auto withoutTheStamp = [this]
  {
    auto model = this->model();
    model["meta"].erase("generated");
    return model.dump();
  };

  generate(everyKindStl);
  const auto first = withoutTheStamp();

  generate(everyKindStl);
  EXPECT_EQ(first, withoutTheStamp());

  sen::gen::HtmlGenerator other;
  EXPECT_EQ(other.generate(sen::lang::TypeSetContext {}, "t").size(), 5U);
}

// Descriptions reach the page as one run of prose. The kernel joins wrapped lines before the
// generator sees them, but runs of spaces and tabs survive that far and are collapsed here.
TEST_F(AnHtmlGenerator, collapsesRunsOfWhitespaceInDescriptions)
{
  generate("package t;\n\n// A  description   with     runs\tof whitespace.\nstruct S\n{\n  a : i32\n}\n");

  const auto desc = model()["types"]["t.S"]["desc"].get<std::string>();
  EXPECT_EQ(desc, "A description with runs of whitespace.");
}

// A kind with no name reaches the page as its bare identifier. The names live in the
// generator so a new kind is named once; this is what makes sure it was.
TEST_F(AnHtmlGenerator, namesEveryKindItEmits)
{
  generate(everyKindStl);

  const auto model = this->model();
  const auto named = model["meta"]["kinds"];
  for (const auto& [name, entry]: model["types"].items())
  {
    const auto kind = entry["kind"].get<std::string>();
    ASSERT_TRUE(named.contains(kind)) << name << " is a " << kind << ", which has no name";
    EXPECT_FALSE(named[kind]["one"].get<std::string>().empty()) << kind << " has no singular";
    EXPECT_FALSE(named[kind]["many"].get<std::string>().empty()) << kind << " has no plural";
  }
}

// Every `<object>.<key>` the application reads, for one of the objects the model supplies.
[[nodiscard]] std::set<std::string> keysReadOff(const std::string& script, const std::string& object)
{
  std::set<std::string> keys;
  // Anchored on a word boundary, or the pattern also matches the tail of a longer name.
  const std::regex pattern {R"(\b)" + object + R"(\.([A-Za-z_]\w*))"};
  for (auto it = std::sregex_iterator {script.begin(), script.end(), pattern}; it != std::sregex_iterator {}; ++it)
  {
    keys.insert((*it)[1].str());
  }
  return keys;
}

// The generator writes the model and the application reads it, and nothing but agreement
// between two files keeps them in step. Three times a key was renamed on one side only, and
// each showed up as an empty area of the page rather than as an error.
//
// The application is emitted here, so its reads can be checked against what was written.
TEST_F(AnHtmlGenerator, emitsEveryKeyTheApplicationReads)
{
  generate(everyKindStl);

  const auto& script = file("app.js");
  const auto model = this->model();

  // Every entry, not the first: the first by key order is a primitive that addPrimitives
  // builds, so checking only that one passes while anything entryFor emits is missing.
  for (const auto& key: keysReadOff(script, "t"))
  {
    for (const auto& [name, entry]: model["types"].items())
    {
      ASSERT_TRUE(entry.contains(key)) << "the application reads t." << key << ", which " << name << " does not carry";
    }
  }

  for (const auto& key: keysReadOff(script, "M.meta"))
  {
    EXPECT_TRUE(model["meta"].contains(key)) << "the application reads M.meta." << key << ", which is not emitted";
  }

  // Facts are per kind, so the fixture has to declare one of everything for this to mean
  // anything. `min` and `max` are the exception: the meta layer carries quantity limits but
  // neither STL nor a FOM can express one, so nothing can produce them to be found here.
  std::set<std::string> facts;
  for (const auto& [name, type]: model["types"].items())
  {
    for (const auto& [key, value]: type["facts"].items())
    {
      facts.insert(key);
    }
  }
  for (const auto& key: keysReadOff(script, "facts"))
  {
    if (key == "min" || key == "max")
    {
      continue;
    }
    EXPECT_TRUE(facts.count(key) == 1U) << "the application reads facts." << key << ", which no type states";
  }
}

// A named optional is a type in its own right, not the type it wraps.
TEST_F(AnHtmlGenerator, namesAMemberByItsDeclaredTypeNotWhatItWraps)
{
  generate(everyKindStl);

  const auto types = model()["types"];
  const auto members = types["k.deep.Derived"]["members"];
  const auto tint =
    std::find_if(members.begin(), members.end(), [](const auto& member) { return member["name"] == "tint"; });
  ASSERT_NE(tint, members.end());
  EXPECT_EQ((*tint)["type"], "k.deep.MaybeColour");

  // and the optional is credited with the use, rather than the type inside it
  const auto& usedBy = types["k.deep.MaybeColour"]["usedBy"];
  EXPECT_NE(std::find(usedBy.begin(), usedBy.end(), "k.deep.Derived"), usedBy.end());
}

// A void return is the absence of a type. Naming it put "void" in the index as a type
// somebody had declared, with the class listed as using it.
TEST_F(AnHtmlGenerator, doesNotPublishVoidAsAType)
{
  generate(everyKindStl);

  const auto model = this->model();
  EXPECT_FALSE(model["types"].contains("void"));
  EXPECT_FALSE(model["meta"]["counts"]["kinds"].contains("void"));

  const auto methods = model["types"]["k.deep.Base"]["methods"];
  ASSERT_FALSE(methods.empty());
  EXPECT_EQ(methods[0]["name"], "settle");
  EXPECT_EQ(methods[0]["returns"], "");
}

// A struct carries what its parent declares. Reading only its own fields showed a page
// stating it had one member when it had two.
TEST_F(AnHtmlGenerator, givesAStructWhatItInherits)
{
  generate(everyKindStl);

  const auto circle = model()["types"]["k.deep.Circle"];
  EXPECT_EQ(circle["ancestry"], nlohmann::json::array({"k.deep.Point"}));
  ASSERT_EQ(circle["groups"].size(), 1U);
  EXPECT_EQ(circle["groups"][0]["from"], "k.deep.Point");
  EXPECT_EQ(circle["inherited"], 2);
  EXPECT_EQ(circle["total"], 4);
}

// Both sides of total = direct + inherited must count the same things.
TEST_F(AnHtmlGenerator, countsCallablesOnBothSidesOfTheTotal)
{
  generate(everyKindStl);

  const auto types = model()["types"];
  const auto& base = types["k.deep.Base"];
  // one property and one method
  EXPECT_EQ(base["direct"], 2);
  EXPECT_EQ(base["total"], 2);

  // the child is told it inherits exactly what the parent says it holds
  const auto& middle = types["k.deep.Middle"];
  EXPECT_EQ(middle["inherited"], base["total"]);
}

// What a type is built on is a use of it, and the one a reader most wants counted.
TEST_F(AnHtmlGenerator, countsAParentAsAUseOfIt)
{
  generate(everyKindStl);

  const auto types = model()["types"];
  const auto uses = [&](const std::string& type, const std::string& user)
  {
    const auto& list = types[type]["usedBy"];
    return std::find(list.begin(), list.end(), user) != list.end();
  };
  EXPECT_TRUE(uses("k.deep.Base", "k.deep.Middle")) << "a class does not count its parent";
  EXPECT_TRUE(uses("k.deep.Point", "k.deep.Circle")) << "a struct does not count its parent";
}

// An enumeration's first table column holds enumerator names, not type references. An
// enumerator may legally be called string or f32, which named the built-in as a use.
TEST_F(AnHtmlGenerator, doesNotReadEnumeratorNamesAsTypeReferences)
{
  generate(R"(package t;

enum Trap : u8 { string, f32, other }

struct Holder { k : Trap, s : string, x : f32 }
)");

  const auto types = model()["types"];
  for (const auto* builtin: {"string", "f32"})
  {
    ASSERT_TRUE(types.contains(builtin));
    const auto& usedBy = types[builtin]["usedBy"];
    EXPECT_EQ(std::find(usedBy.begin(), usedBy.end(), "t.Trap"), usedBy.end())
      << builtin << " is listed as used by the enumeration that merely names an enumerator so";
    EXPECT_NE(std::find(usedBy.begin(), usedBy.end(), "t.Holder"), usedBy.end())
      << builtin << " lost the use that is real";
  }
}

TEST_F(AnHtmlGenerator, writesOnlyRelativePaths)
{
  generate(twoClasses);

  const auto files = sen::gen::HtmlGenerator {}.generate(sen::lang::TypeSetContext {}, "t");
  ASSERT_FALSE(files.empty());
  for (const auto& [relPath, body]: files)
  {
    EXPECT_TRUE(relPath.is_relative()) << relPath << " is not relative to the output directory";
    EXPECT_EQ(relPath.parent_path(), std::filesystem::path {}) << relPath << " is not a bare name";
    EXPECT_EQ(relPath.string().find(".."), std::string::npos) << relPath << " climbs out of the output directory";
  }
}

}  // namespace
