// === typst_generator_test.cpp ========================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "every_kind_model.h"
#include "sen/gen/typst.h"

// sen
#include "sen/core/lang/stl_parser.h"
#include "sen/core/lang/stl_resolver.h"
#include "sen/core/lang/stl_scanner.h"
#include "sen/core/lang/stl_statement.h"

// 3rd party
#include <gtest/gtest.h>

// std
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace
{

class ATypstGenerator: public ::testing::Test
{
protected:
  void generate(const std::string& stl, sen::gen::TypstOptions options = {})
  {
    resolve(stl);
    files_ = sen::gen::TypstGenerator {}.generate(context_, options);
  }

  // A source declares one package, and an import names a file, so a model spanning two
  // packages needs the imported one on disk for the resolver to find.
  void generate(const std::string& imported,
                const std::string& importedAs,
                const std::string& main,
                sen::gen::TypstOptions options = {})
  {
    // Tests run as concurrent processes, so the file has to be this test's own: sharing one
    // path means one process truncating what another is reading.
    const auto directory = std::filesystem::temp_directory_path() / "sen-typst-test" /
                           ::testing::UnitTest::GetInstance()->current_test_info()->name();
    std::filesystem::create_directories(directory);
    {
      std::ofstream written {directory / importedAs};
      written << imported;
      ASSERT_TRUE(written.good()) << "could not write " << (directory / importedAs).string();
    }

    sen::lang::ResolverContext resolverContext;
    resolverContext.includePaths = {directory};
    resolve(main, resolverContext);
    files_ = sen::gen::TypstGenerator {}.generate(context_, options);
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

  [[nodiscard]] const sen::gen::TypstGenerator::FileContents& files() const { return files_; }

  [[nodiscard]] bool contains(const std::string& path, const std::string& needle) const
  {
    return file(path).find(needle) != std::string::npos;
  }

  // For the checks that need two models rather than one document.
  void resolveInto(sen::lang::TypeSetContext& context, const std::string& stl)
  {
    sen::lang::StlScanner scanner {stl};
    sen::lang::StlParser parser {scanner.scanTokens()};
    statements_.push_back(parser.parse());

    const sen::lang::ResolverContext resolverContext {};
    sen::lang::StlResolver resolver {statements_.back(), resolverContext, context};
    resolver.resolve({});
  }

private:
  void resolve(const std::string& stl, sen::lang::ResolverContext resolverContext = {})
  {
    sen::lang::StlScanner scanner {stl};
    sen::lang::StlParser parser {scanner.scanTokens()};
    statements_.push_back(parser.parse());

    sen::lang::StlResolver resolver {statements_.back(), resolverContext, context_};
    resolver.resolve({});
  }

  // The resolved model points into these, so they outlive resolution.
  std::vector<std::vector<sen::lang::StlStatement>> statements_;
  sen::lang::TypeSetContext context_;
  sen::gen::TypstGenerator::FileContents files_;
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
  fn compute(a: i32) -> f32;
}
)";

using sen::gen::test::everyKindStl;

[[nodiscard]] sen::gen::TypstOptions excluding(const std::string& package)
{
  sen::gen::TypstOptions options;
  options.excludePackages = {package};
  return options;
}

// A qualified name carries a break opportunity after each dot, so a long one can wrap in a
// narrow column instead of overflowing it. Expectations on rendered names have to allow it.
[[nodiscard]] std::string zeroWidthSpace() { return "\u200b"; }

// For failure messages: a missing break opportunity is invisible in a log otherwise.
[[nodiscard]] std::string visible(std::string text)
{
  const std::string marker {"<ZWSP>"};
  const auto space = zeroWidthSpace();
  for (auto at = text.find(space); at != std::string::npos; at = text.find(space, at + marker.size()))
  {
    text.replace(at, space.size(), marker);
  }
  return text;
}

TEST_F(ATypstGenerator, writesAReferenceAStyleAndASkeleton)
{
  generate(twoClasses);
  for (const auto* path: {"reference.typ", "style.typ", "document.typ"})
  {
    EXPECT_FALSE(file(path).empty()) << path << " is empty";
  }
}

// The skeleton imports the style and includes the reference. If either name drifts the
// document compiles to nothing useful and nothing else would notice.
TEST_F(ATypstGenerator, theSkeletonNamesTheFilesThatAreEmitted)
{
  generate(twoClasses);
  EXPECT_TRUE(contains("document.typ", "\"style.typ\""));
  EXPECT_TRUE(contains("document.typ", "\"reference.typ\""));
  EXPECT_TRUE(contains("reference.typ", "#import \"style.typ\""));
}

// The style is the caller's to replace; emitting ours over theirs would overwrite it.
TEST_F(ATypstGenerator, doesNotEmitAStyleWhenTheCallerBringsOne)
{
  sen::gen::TypstOptions options;
  options.style = "house-style.typ";
  generate(twoClasses, options);
  EXPECT_EQ(files().count("style.typ"), 0U);
}

// An unset include point still tells a reader where their own content belongs.
TEST_F(ATypstGenerator, leavesAHintWhereACallerSuppliesNoPage)
{
  generate(twoClasses);
  EXPECT_TRUE(contains("document.typ", "your title page"));

  sen::gen::TypstOptions options;
  options.frontMatter = "front.typ";
  generate(twoClasses, options);
  EXPECT_TRUE(contains("document.typ", "#include \"front.typ\""));
  EXPECT_FALSE(contains("document.typ", "your title page"));
}

TEST_F(ATypstGenerator, opensEveryTypeAsAHeadingCarryingItsKind)
{
  generate(twoClasses);
  EXPECT_TRUE(contains("reference.typ", "==== #\"Base\" #chip(\"classes\", \"class\")"));
  EXPECT_TRUE(contains("reference.typ", "==== #\"Derived\" #chip(\"classes\", \"class\")"));
}

// The section heading already names the package, so repeating it on every heading, every
// summary row and every type cell is noise that grows with the depth of the package tree.
TEST_F(ATypstGenerator, writesNamesUnqualifiedInsideTheirOwnPackage)
{
  generate(twoClasses);
  EXPECT_TRUE(contains("reference.typ", "==== #\"Base\""));
  EXPECT_FALSE(contains("reference.typ", "==== #\"t."));
  EXPECT_TRUE(contains("reference.typ", "link(<t-t-Base>)[#\"Base\"]")) << "nor in the summary rows";

  // The index spans the document rather than one section, so there the package belongs.
  // Its needle carries the break opportunity a qualified name is written with.
  EXPECT_TRUE(contains("reference.typ", "[#\"t." + zeroWidthSpace() + "Base\"]"));
}

// The other half: shortening a name from elsewhere would hide that the link leaves the
// section. Only the package doing the printing may be dropped.
TEST_F(ATypstGenerator, keepsThePackageOnANameFromAnotherOne)
{
  generate(R"(package a;

// Something to point at.
struct Shared { value : i32 }
)",
           "a.stl",
           R"(package b;

import "a.stl"

// Holds one of a's.
class Holder
{
  var borrowed : a.Shared;
}
)");
  EXPECT_TRUE(contains("reference.typ", "\"a." + zeroWidthSpace() + "Shared\""))
    << "b refers out to a, so the package must show\n"
    << visible(file("reference.typ"));
}

// Typst treats a link to a label the document does not contain as an error rather than
// a dangling link, so a reference may only be made to a type that is present.
TEST_F(ATypstGenerator, doesNotLinkToATypeTheDocumentDoesNotContain)
{
  // Two packages, one excluded: the surviving one still names a type in the excluded one,
  // which is the only shape where the guard can be wrong. Excluding the only package would
  // leave nothing to link from, and the check would hold however the guard behaved.
  generate(R"(package a;

// Something to point at.
struct Shared { value : i32 }
)",
           "a.stl",
           R"(package b;

import "a.stl"

// Holds one of a's.
class Holder
{
  var borrowed : a.Shared;
}
)",
           excluding("a"));

  const auto& reference = file("reference.typ");
  EXPECT_NE(reference.find("Holder"), std::string::npos) << "b is still documented";
  EXPECT_EQ(reference.find("link(<t-a-Shared>)"), std::string::npos) << "but a is not, so nothing may link into it";
  EXPECT_NE(reference.find("a." + zeroWidthSpace() + "Shared"), std::string::npos)
    << "the name still shows, as plain text\n"
    << visible(reference);
}

TEST_F(ATypstGenerator, tabulatesWhatAClassCarriesAndWhatItCanBeAsked)
{
  generate(twoClasses);
  EXPECT_TRUE(contains("reference.typ", "table.header([*Name*], [*Type*], [*Flags*], [*Description*])"));
  EXPECT_TRUE(contains("reference.typ", "table.header([*Method*], [*Signature*], [*Description*])"));
  EXPECT_TRUE(contains("reference.typ", "compute")) << "the method should appear";
}

// Model prose is data, not markup. Typst reads a leading `=` as a heading, `//` as a
// comment, `~` as a non-breaking space and `--` as an en dash, so prose emitted as markup
// either fails to compile or silently changes what the model said. A string literal is
// read verbatim, and only the two characters that could end it need an escape.
TEST_F(ATypstGenerator, writesModelProseAsDataRatherThanMarkup)
{
  generate(R"(package t;

// = Deprecated: 3//4 of the frame, ~5 typical, 0 -- 100, and a quote " here.
class Awkward
{
  var one : i32;
}
)");

  const auto& reference = file("reference.typ");
  EXPECT_NE(reference.find(R"(= Deprecated: 3//4 of the frame, ~5 typical, 0 -- 100)"), std::string::npos)
    << "the model's own words have to survive intact";
  EXPECT_NE(reference.find(R"(and a quote \" here.)"), std::string::npos)
    << "and a quote, which would otherwise end the literal, has to be escaped";
}

TEST_F(ATypstGenerator, rendersEveryKindTheModelCanHold)
{
  generate(everyKindStl);

  const auto& reference = file("reference.typ");
  for (const auto* section: {"Object classes", "Fixed records", "Enumerations", "Variant records"})
  {
    EXPECT_NE(reference.find(section), std::string::npos) << section << " has no section";
  }
}

// A wrapper type is defined by what it wraps. Emitting the heading alone puts a page
// number against a name and tells the reader nothing they did not have from the index.
TEST_F(ATypstGenerator, saysWhatAWrapperTypeWraps)
{
  generate(everyKindStl);

  const auto& reference = file("reference.typ");
  // Each is written as the line of the language that declares it, which says what it is
  // and what it wraps at once.
  for (const auto* declared: {"#kw[optional]", "#kw[sequence]", "#kw[array]", "#kw[alias]", "#kw[quantity]"})
  {
    EXPECT_NE(reference.find(declared), std::string::npos) << "nothing is declared as: " << declared;
  }
}

// A section switch that is off must remove the section, or the option is decorative.
TEST_F(ATypstGenerator, honoursTheSectionSwitches)
{
  sen::gen::TypstOptions off;
  off.summaries = false;
  off.index = false;
  generate(twoClasses, off);

  EXPECT_EQ(file("reference.typ").find("#summary("), std::string::npos);
  EXPECT_EQ(file("reference.typ").find("= Index"), std::string::npos);

  generate(twoClasses);
  EXPECT_NE(file("reference.typ").find("#summary("), std::string::npos)
    << "and they must be present by default, or the check above proves nothing";
}

// Typst has two modes: inside a content block a link needs its leading hash, and a bare
// `link(<x>)` there is read as text plus a *label definition*. That yields a duplicate
// label and typst refuses the document — but only when someone runs it, so the rule is
// checked here where it costs nothing.
TEST_F(ATypstGenerator, writesContentModeLinksInsideContentBlocks)
{
  generate(everyKindStl);

  const auto& reference = file("reference.typ");
  std::size_t blocks = 0U;
  for (std::size_t at = reference.find("#facts["); at != std::string::npos; at = reference.find("#facts[", at + 1U))
  {
    ++blocks;
    const auto end = reference.find("]\n", at);
    const auto block = reference.substr(at, end - at);
    for (std::size_t use = block.find("link("); use != std::string::npos; use = block.find("link(", use + 1U))
    {
      EXPECT_TRUE(use > 0U && block[use - 1U] == '#')
        << "a bare link( inside a content block defines a label: " << block;
    }
  }
  EXPECT_GT(blocks, 0U) << "no content blocks were emitted, so this check proves nothing";
}

// A class can carry nothing but events, and they were reaching the page as a heading
// with no content under it at all.
TEST_F(ATypstGenerator, tabulatesWhatAClassAnnounces)
{
  generate(everyKindStl);
  EXPECT_TRUE(contains("reference.typ", "table.header([*Event*], [*Payload*], [*Description*])"));
}

// A struct's own getFields() is own-fields-only and, unlike a class, nothing else in the
// document said the type had a parent, so the inherited fields were unreachable.
TEST_F(ATypstGenerator, saysWhenARecordExtendsAnother)
{
  generate(everyKindStl);
  EXPECT_TRUE(contains("reference.typ", "Extends ")) << "Circle extends Point in the model";
}

// "at most 3" contradicted the model's own "always three" one line above it.
TEST_F(ATypstGenerator, distinguishesAFixedArrayFromABoundedOne)
{
  // The language has two keywords for this and they mean different things, so the
  // document has to use the one the model used.
  generate(everyKindStl);
  EXPECT_TRUE(contains("reference.typ", "#kw[array]")) << "array<Metres, 3> holds exactly three";
  EXPECT_TRUE(contains("reference.typ", "#kw[sequence]")) << "sequence<Point, 8> holds up to eight";
  EXPECT_TRUE(contains("reference.typ", "#lit[8]")) << "and the bound is shown";
}

// How wide an enumeration is on the wire is the fact an interface document is read for.
TEST_F(ATypstGenerator, saysHowAnEnumerationIsHeld)
{
  generate(everyKindStl);
  EXPECT_TRUE(contains("reference.typ", "Held as #mono("));
}

// What names a type is how a reader works out what a change to it would break. The
// option and the field both existed and neither was ever read.
TEST_F(ATypstGenerator, saysWhatNamesEachType)
{
  generate(everyKindStl);
  EXPECT_TRUE(contains("reference.typ", "Named by")) << "Point is named by Circle, Track and Figure";

  sen::gen::TypstOptions off;
  off.usedBy = false;
  generate(everyKindStl, off);
  EXPECT_FALSE(contains("reference.typ", "Named by")) << "and the switch has to remove it";
}

// The switch has to remove the section without taking the overview with it.
TEST_F(ATypstGenerator, honoursTheHierarchySwitch)
{
  generate(twoClasses);
  EXPECT_TRUE(contains("reference.typ", "#hierarchy("));

  sen::gen::TypstOptions off;
  off.hierarchy = false;
  generate(twoClasses, off);
  EXPECT_FALSE(contains("reference.typ", "#hierarchy("));
  EXPECT_TRUE(contains("reference.typ", "= Model overview")) << "without taking the overview with it";
}

// The caller's style was suppressed and never named, so the document imported a file
// that nothing wrote and could not compile at all.
TEST_F(ATypstGenerator, importsTheStyleTheCallerNamed)
{
  sen::gen::TypstOptions options;
  options.style = "house.typ";
  generate(twoClasses, options);

  EXPECT_TRUE(contains("document.typ", "#import \"house.typ\""));
  EXPECT_TRUE(contains("reference.typ", "#import \"house.typ\""));
  EXPECT_FALSE(contains("document.typ", "\"style.typ\""));
}

// The title is the one string a caller supplies that reaches the page, and it went in
// unescaped: a quote in it ended the literal and the document stopped compiling.
TEST_F(ATypstGenerator, escapesTheTitle)
{
  sen::gen::TypstOptions options;
  options.title = R"(Acme "Radar" ICD)";
  generate(twoClasses, options);
  EXPECT_TRUE(contains("document.typ", R"(Acme \"Radar\" ICD)"));
}

// Nothing said the generator was single-use, and a second run emitted links to types
// only the first model held.
TEST_F(ATypstGenerator, canBeRunTwice)
{
  sen::gen::TypstGenerator generator;

  sen::lang::TypeSetContext first;
  resolveInto(first, twoClasses);
  const auto once = generator.generate(first, {});

  sen::lang::TypeSetContext second;
  resolveInto(second, R"(package other;

// Nothing to do with the first model.
struct Lonely { value : i32 }
)");
  const auto twice = generator.generate(second, {});

  EXPECT_EQ(twice.at("reference.typ").find("t-t-Base"), std::string::npos)
    << "the second document must not link into the first model";
  EXPECT_NE(once.at("reference.typ").find("t-t-Base"), std::string::npos) << "which the first one did contain";
}

// A unit abbreviation is written for a machine -- "m_per_s_sq" -- and the page is read by
// a person. Typeset upright as SI asks, and inline rather than as a stacked fraction,
// which would be too tall for a line of declaration.
TEST_F(ATypstGenerator, typesetsAUnitAsMathematics)
{
  generate(R"(package t;

// Metres travelled each second, each second.
quantity<f32, m_per_s_sq> Acceleration;

// How warm it is.
quantity<f32, degC> Temperature;
)");

  const auto& reference = file("reference.typ");
  EXPECT_NE(reference.find(R"($"m"\/"s"^2$)"), std::string::npos) << "m_per_s_sq is metres per second squared";
  EXPECT_NE(reference.find("$\"\u00b0C\"$"), std::string::npos) << "degC carries a degree sign";
  EXPECT_EQ(reference.find("m_per_s_sq"), std::string::npos) << "and the machine spelling does not reach the page";
}

}  // namespace
