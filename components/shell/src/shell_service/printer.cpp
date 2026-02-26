// === printer.cpp =====================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "printer.h"

// component
#include "box_chars.h"
#include "styles.h"
#include "terminal.h"
#include "util.h"

// sen
#include "sen/core/base/assert.h"
#include "sen/core/base/compiler_macros.h"
#include "sen/core/base/duration.h"
#include "sen/core/base/integer_compare.h"
#include "sen/core/base/timestamp.h"
#include "sen/core/base/version.h"
#include "sen/core/io/util.h"
#include "sen/core/meta/alias_type.h"
#include "sen/core/meta/class_type.h"
#include "sen/core/meta/enum_type.h"
#include "sen/core/meta/method.h"
#include "sen/core/meta/native_types.h"
#include "sen/core/meta/optional_type.h"
#include "sen/core/meta/property.h"
#include "sen/core/meta/quantity_type.h"
#include "sen/core/meta/sequence_type.h"
#include "sen/core/meta/struct_type.h"
#include "sen/core/meta/time_types.h"
#include "sen/core/meta/type.h"
#include "sen/core/meta/type_visitor.h"
#include "sen/core/meta/var.h"
#include "sen/core/meta/variant_type.h"
#include "sen/core/obj/callback.h"
#include "sen/core/obj/object.h"
#include "sen/kernel/kernel.h"

// generated code
#include "stl/sen/kernel/basic_types.stl.h"
#include "stl/shell.stl.h"

// std
#include <algorithm>
#include <array>
#include <cassert>
#include <chrono>
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <iomanip>
#include <ios>
#include <list>
#include <random>
#include <sstream>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

namespace sen::components::shell
{

namespace
{

//--------------------------------------------------------------------------------------------------------------
// Helpers
//--------------------------------------------------------------------------------------------------------------

struct Quote
{
  std::string text;
  std::string author;
};

[[nodiscard]] const Quote& getRandomQuote()
{
  static std::vector<Quote> quotes = {
    {"For every proverb, there is an equal and opposite proverb", "Philip Wadler"},
    {"Fast, cheap, and reliable : choose two", "Old proverb"},
    {"If you have too many special cases, you're doing it wrong", "Craig Zerouni"},
    {"One person's constant is another person's variable", "Susan Gerhart"},
    {"One person's data is another person's program", "Guy L. Steele, Jr."},
    {"Sometimes the problem is to discover what the problem is", "Gordon Glegg"},
    {"New levels, new devils", "Anonymous"},
    {"Defining interfaces is the most important part of system design", "Butler W. Lampson"},
    {"If in doubt, leave it out", "Old proverb"},
    {"Handle normal and worst cases separately", "Butler W. Lampson"},
    {"I don't trust security people to do sane things", "Linus Torvalds"},
    {"Talk is cheap. Show me the code", "Linus Torvalds"},
    {"Essentials never lose their value", "Tamil saying"},
    {"Beware of what is beyond your control", "Tamil saying"},
    {"It is easy to criticize", "Tamil saying"},
    {"Architecture is strategy", "Old engineering proverb"},
    {"The function of good software is to make the complex appear to be simple", "Grady Booch"},
    {"Code is read much more often than it is written", "Guido Van Rossum"},
    {"The longer you wait before fixing a bug, the costlier it is to fix", "Chris Pine"},
    {"Without design, programming is the art of adding bugs to an empty text file", "Rick Cook"},
    {"It is very hard to predict, especially the future", "Niels Bohr"},
    {"Good design adds value faster than it adds cost", "Dijkstra"},
    {"Temporary solutions often become permanent problems", "John Shore"},
    {"Don’t document the problem, fix it", "Robert Martin"},
    {"No amount of testing can prove a software right, 1 test can prove it wrong", "Alex Lowe"},
    {"Absence of evidence is not evidence of absence", "Robert D. Schneider"},
    {"Before you say you can’t do something, try it", "Seymour Papert"},
    {"I’m not a great programmer; just a good programmer with great habits", "Norman Ralph Augustine"},
    {"Architecture is the tension between coupling and cohesion", "Joshua Bloch"},
    {"If you automate a mess, you get an automated mess", "Vint Cerf"},
    {"In a room full of top SW designers, if two agree, that's a majority", "Roy Carlson"},
    {"Incorrect documentation is often worse than no documentation", "Eric Evans"},
    {"Innovation distinguishes between a leader and a follower", "Jose M. Aguilar"},
    {"Simplicity doesn't precede complexity, but follows it", "Michael J. Saylor"},
    {"In essence, engineering is doing what you want with what you have", "J. Carmack"},
    {"An error doesn't become a mistake until you refuse to correct it", "Alan Kay"},
    {"Don't Gather Requirements, Dig for Them", "Andrew Hunt"},
    {"If you're a technical lead, you need to be coding", "Martin Fowler"},
    {"Quality is never an accident; it is always the result of intelligent effort", "John Ruskin"},
    {"Redundant comments are just places to collect lies and misinformation", "Robert Martin"},
    {"Watch the little things; a small leak will sink a great ship", "Benjamin Franklin"},
    {"The nice thing about standards is that there are so many to choose from", "Andrew S. Tannenbaum"},
    {"If you get tired of writing for loops, take a break and continue later", "David Walker"},
    {"The optimum committee has no members", "Norman Ralph Augustine"},
    {"SW testing isn't only ensuring absence of bugs but the presence of value", "Amit Kalantri"},
    {"Complexity has nothing to do with intelligence, simplicity does", "Larry Bossidy"},
    {"Those who can imagine anything, can create the impossible", "Alan Turing"},
    {"You haven't mastered a tool until you understand when it should not be used", "Kelsey Hightower"},
    {"One man's crappy software is another man's full time job", "Jessica Gaston"},
    {"Every craftsman starts the journey with a set of good-quality tools", "Ada Lovelace"},
    {"If you think good architecture is expensive, try bad architecture", "Jack Reeves"},
    {"The value of a prototype is in the education it gives you, not in the code", "Zhuowei Zhang"},
    {"If it doesn’t work, it doesn’t matter how fast it doesn’t work", "Leah Culver"},
    {"If you lie to the compiler, it will get its revenge", "Joel Spolsky"},
    {"The software is not finished until the last user is dead", "Dennie Tassel"},
    {"Write simple parts connected by clean interfaces", "Eric S. Raymond"},
    {"Design programs to be connected to other programs", "Eric S. Raymond"},
    {"Separate policy from mechanism; separate interfaces from engines", "Eric S. Raymond"},
    {"Robustness is the child of transparency and simplicity", "Eric S. Raymond"},
    {"Fold knowledge into data so program logic can be stupid and robust", "Eric S. Raymond"},
    {"Design for the future, because it will be here sooner than you think", "Eric S. Raymond"},
    {"Clarity is better than cleverness", "Eric S. Raymond"},
    {"Design for visibility to make inspection and debugging easier", "Eric S. Raymond"},
    {"Design for simplicity; add complexity only where you must", "Eric S. Raymond"},
    {"When a program has nothing surprising to say, it should say nothing", "Eric S. Raymond"},
    {"When you must fail, fail noisily and as soon as possible", "Eric S. Raymond"},
    {"Programmer time is expensive; conserve it in preference to machine time", "Eric S. Raymond"},
    {"Avoid hand-hacking; write programs to write programs when you can", "Eric S. Raymond"},
    {"Prototype before polishing. Get it working before you optimize it", "Eric S. Raymond"},
    {"The real hero of programming is the one who writes negative code", "Doug McIlroy"},
    {"When in doubt, use brute force", "Ken Thompson"},
    {"The simple things you see are all complicated", "song by The Who"},
    {"Conscious ignorance is the prelude to every real advance in science", "James Clerk Maxwell"},
    {"Science produces ignorance at a faster rate than it produces knowledge", "Stuart Firestein"},
    {"Minimize Coupling, Maximize Cohesion", "Old proverb"},
    {"Fail Quickly: makes evolution less painful and improves testability", "Old proverb"},
    {"Document 'Why'", "Old engineering proverb"},
    {"Share early, share often", "Old engineering proverb"},
    {"Reality is that which, when you stop believing in it, doesn't go away", "Philip K. Dick"},
    {"Never attribute to malice that which is adequately explained by stupidity", "Hanlon's razor"},
    {"On the 7th day, God did a sprint retrospective", "@iamdevloper"},
    {"What do we want? ASYNCHRONICITY! When do we want it? THAT'S IRRELEVANT!", "@iamdevloper"},
    {"Values aren't achieved through specialization. Imagine a VP of teamwork", "Startup L. Jackson"},
    {"The purpose of computation is insight, not numbers", "Richard Hamming"},
    {"The number of UNIX installations is now above 20, and more are expected", "Unix Manual (1973)"},
    {"Efficiency is doing things right; effectiveness is doing the right things", "Peter F. Drucker"},
    {"Doubt kill more dreams than failure", "Suzy Kassem"},
    {"How can I improve the code so that this comment isn't needed?", "Steve McConnell"},
    {"All models are wrong, some are useful", "Anonymous"},
    {"First, catch your rabbit", "Old recipe for rabbit stew"},
    {"The secret of getting ahead is getting started", "Mark Twain"},
    {"Hope is not a strategy", "SRE motto"},
    {"Chance favours the prepared mind", "Louis Pasteur"},
    {"Remember that all is opinion", "Marcus Aurelius"},
    {"Complexity is not a sign of intelligence; simplify", "Peter Bevelacqua"},
    {"Mistakes are the portals of discovery", "James Joyce"},
    {"My superpower is reading the manual", "@DrBazza"},
    {"Life is like a box of terrible analogies", "Oscar Wilde"},
    {"Beware the barrenness of a busy life", "Socrates"},
    {"Intellectuals solve problems, geniuses prevent them", "Einstein"},
    {"A system increases its complexity unless work is done to reduce it", "Meir M. Lehman"},
    {"Failure free operations require experience with failure", "Richard I. Cook"},
    {"Don't fight forces, use them", "R. Buckminster Fuller"},
    {"An essential aspect of creativity is not being afraid to fail", "Edwin Land"},
    {"It's not that we need new ideas, but we need to stop having old ideas", "Edwin Land"},
    {"Even a broken clock is right twice a day", "Anonymous"},
    {"All the easy problems have been solved", "Kinkler's Second Law"},
    {"Work expands so as to fill the time available for its completion", "Parkinson's Law"},
    {"The one who says it cannot be done should never interrupt the one doing it", "Roman Rule"},
    {"Whatever it is that hits the fan will not be evenly distributed", "Law of Probability Dispersal"},
    {"Don't keep doing what doesn't work", "Anonymous"},
    {"The best way to predict the future is to invent it", "Anonymous"},
    {"The future ain't what it used to be", "Yogi Berra"},
    {"When you change the way you look at things, the things you look at change", "Max Planck"},
    {"You miss 100% of the shots you don't take", "Wayne Gretzky"},
    {"Ignorance may be bliss, but only if you can remain ignorant until the end", "@nugget"},
    {"The invention of the wheel was a huge job killer", "@dfilppi"},
    {"I never lose. I either win or I learn", "Nelson Mandela"},
    {"Change is inevitable. Except from a vending machine", "Anonymous"},
    {"Before optimizing, using a profiler", "Mike Morton"},
    {"The cheapest and most reliable components are those that aren't there", "Gordon Bell"},
    {"Omit needless words", "Strunk and White"},
    {"If the code and the comments disagree, then both are probably wrong", "Norm Schryer"},
    {"In interface design, always do the least surprising thing", "Eric S.Raymond"},
    {"If your commit message is > 15 words you should bring it to your therapist", "@iamdevloper"},
    {"The practical genius does not know everything, but knows where to find it", "Old proverb"},
    {"With quick & dirty, the 'dirty' remains long after 'quick' gets forgotten", "McConnell"},
    {"Talent hits a target nobody can hit, genius hits a target nobody can see", "Schopenhauer"},
    {"SW complexity comes from trying to make one thing do two things", "Ryan Singer"},
    {"No battle was ever won according to plan, no battle was ever won without one", "Dwight D. Eisenhower"},
    {"It's easy to optimize a correct system, but hard to correct an optimized one", "Anonymous"},
    {"Looking back, it was harder to see what the problems were than to solve them", "Charles Darwin"},
    {"A general-purpose product is harder to design than a special-purpose one", "Frederick P. B.rooks"},
    {"Make it correct, clear, concise, and fast. In that order", "Rebecca Wirfs-Brock"},
    {"Discovery: seeing what everyone has seen + thinking what nobody has thought", "Milt Bryce"},
    {"Knowledge isn't threatened by ignorance, but by the illusion of knowledge", "Daniel J. Boorstin"},
    {"The only ones who see the whole picture, are those who step out of the frame", "Salman Rushdie"},
    {"Don't believe everything you read on a computer screen", "Abraham Lincoln"},  // joke
    {"Don't take advice from those who don't have to live with the consequences", "Mark Cuban"},
    {"The issue on SW is ensuring everyone understands what everyone else is doing", "Michael Feathers"}};

  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_int_distribution<std::size_t> distribution(0, quotes.size() - 1U);
  return quotes.at(distribution(gen));
}

[[nodiscard]] inline bool printedValueRequiresNewLine(const Var& val)
{
  return val.holds<VarMap>() || (val.holds<VarList>() && !val.get<VarList>().empty());
}

[[nodiscard]] inline const char* yesNo(bool value) noexcept { return value ? "yes" : "no"; }

template <typename T>
[[nodiscard]] inline std::string intToHex(T i)
{
  std::stringstream stream;
  stream << "0x" << std::setfill('0') << std::setw(sizeof(T) + sizeof(T)) << std::hex << i;
  return stream.str();
}

std::string getQualName(const Type& type)
{
  if (const auto* custom = type.asCustomType())
  {
    return std::string(custom->getQualifiedName());
  }
  return std::string(type.getName());
}

[[nodiscard]] inline const char* getTransportStr(TransportMode mode)
{
  switch (mode)
  {
    case TransportMode::confirmed:
      return "reliable";

    case TransportMode::unicast:
      return "unicast";

    case TransportMode::multicast:
      return "multicast";

    default:
      SEN_UNREACHABLE();
  }
}

inline void printTime(TimeStyle timeStyle, Terminal* term, TimeStamp val)
{
  switch (timeStyle)
  {
    case TimeStyle::timestampUTC:
    {
      term->cprint(numberValueStyle, val.toUtcString());
    }
    break;

    case TimeStyle::timestampLocal:
    {
      term->cprint(numberValueStyle, val.toLocalString());
    }
    break;

    default:
      break;
  }
}

//--------------------------------------------------------------------------------------------------------------
// Logo-related
//--------------------------------------------------------------------------------------------------------------

struct LogoTextColor
{
  Color textColor1;
  Color textColor2;
  Color textColor3;
  Color textColor4;
  Color barA;
  Color barB;
};

[[nodiscard]] uint8_t interpolateComponent(uint8_t startValue,
                                           uint8_t endValue,
                                           std::size_t stepNumber,
                                           std::size_t lastStepNumber)
{
  const auto step = static_cast<uint8_t>(stepNumber);
  const auto last = static_cast<uint8_t>(lastStepNumber);

  return (endValue - startValue) * step / last + startValue;
}

[[nodiscard]] Color interpolate(const Color& from, const Color& to, std::size_t stepNumber, std::size_t lastStepNumber)
{
  return {interpolateComponent(from.r, to.r, stepNumber, lastStepNumber),
          interpolateComponent(from.g, to.g, stepNumber, lastStepNumber),
          interpolateComponent(from.b, to.b, stepNumber, lastStepNumber)};
}

void writeLogo(Terminal* term, const LogoTextColor& pallete)
{
  term->setFgColor(pallete.textColor1);
  term->print("    _________  __ \n");
  term->setFgColor(pallete.textColor2);
  term->print("   / __/ __/ |/ / \n");
  term->setFgColor(pallete.textColor3);
  term->print("  _\\ \\/ __/    /  \n");
  term->setFgColor(pallete.textColor4);
  term->print(" /___/___/_/|_/   \n");
}

void printLine(std::size_t length, Terminal* term, const Color& src, const Color& dst)
{
  for (std::size_t i = 0; i < length; ++i)
  {
    term->setFgColor(interpolate(src, dst, i, length));
    term->print(boxBox);
  }
}

void printLogo(Terminal* term, std::size_t lineLength, const LogoTextColor& pallete)
{
  term->hideCursor();
  writeLogo(term, pallete);
  printLine(lineLength, term, pallete.barA, pallete.barB);

  const auto& quote = getRandomQuote();

  // Print quote text
  constexpr int minQuoteOffset {4};
  const int requestedQuoteOffset = static_cast<int>(lineLength - quote.text.size());
  const std::string::size_type baseIndentLength = std::max(minQuoteOffset, requestedQuoteOffset);
  const auto baseIndent = std::string(baseIndentLength, ' ');

  term->newLine();
  term->newLine();
  term->print(baseIndent);
  std::string fittedText = quote.text;
  {
    const int availableSpace = static_cast<int>(lineLength - baseIndentLength);
    constexpr std::string_view shortSuffix = "...";

    assert(sen::std_util::cmp_greater_equal(availableSpace, shortSuffix.size()) &&
           "There should always be some space available to print to.");
    if (sen::std_util::cmp_less(availableSpace, fittedText.size()))
    {  // Shorten the quote in case we cannot print it on one line.
      const int cutOff = static_cast<int>(availableSpace - shortSuffix.size());
      fittedText.replace(cutOff, shortSuffix.size(), shortSuffix);
      fittedText.erase(availableSpace);
    }
    term->cprint(quoteStyle, fittedText);
  }

  // Print quote author
  const int requestedAuthorOffset = static_cast<int>(fittedText.size() - quote.author.size() - 2);
  const std::string::size_type authorIndentLength = baseIndentLength + std::max(0, requestedAuthorOffset);
  const auto authorIndent = std::string(authorIndentLength, ' ');

  if (const int availableSpace = static_cast<int>(lineLength - authorIndentLength);
      sen::std_util::cmp_greater_equal(availableSpace, quote.author.size()))
  {  // only print the author if there is enough space
    term->newLine();
    term->print(authorIndent);
    term->print("- ");
    term->cprint(authorStyle, quote.author);
  }

  // Print follow up spacing
  term->newLine();
  term->newLine();
}

const LogoTextColor& getPallete()
{
  static constexpr LogoTextColor c1 = {
    toColor(0x005b96), toColor(0x6497b1), toColor(0xb3cde0), toColor(0xb3cde0), toColor(0x1cb5e0), toColor(0x00357a)};

  static constexpr LogoTextColor c2 = {
    toColor(0x89b5af), toColor(0x96c7c1), toColor(0xded9c4), toColor(0xd0cab2), toColor(0x827397), toColor(0x363062)};

  static constexpr std::array<LogoTextColor, 2> palletes = {c1, c2};

  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_int_distribution<std::size_t> distr(0, palletes.size() - 1U);
  return palletes.at(distr(gen));
}

void printBuffer(Terminal* term, BufferStyle style, const VarList& list, std::size_t indent)
{
  if (list.empty())
  {
    term->cprint(emptyValueStyle, "<empty>");
    return;
  }

  if (style == BufferStyle::hexdump)
  {
    constexpr auto maxLineLength = 8U;
    if (list.size() <= maxLineLength)
    {
      for (const auto& byte: list)
      {
        term->cprintf(numberValueStyle, "%02X ", byte.get<uint8_t>());  // NOLINT
      }
    }
    else
    {
      for (std::size_t i = 0U; i < list.size(); ++i)
      {
        if (i % maxLineLength == 0)
        {
          if (i != 0)
          {
            term->newLine();
          }
          term->printf("%*s", static_cast<int>(indent + 2U), "");  // NOLINT
        }

        term->cprintf(numberValueStyle, "%02X ", list[i].get<uint8_t>());  // NOLINT
      }
    }
  }
  else
  {
    term->cprintf(enumValueStyle, "%zu bytes", list.size());  // NOLINT
  }
}

//--------------------------------------------------------------------------------------------------------------
// TypeWriter
//--------------------------------------------------------------------------------------------------------------

class TypeWriter final: public TypeVisitor
{
public:
  SEN_NOCOPY_NOMOVE(TypeWriter)

public:
  TypeWriter(const Var& value, size_t level, Terminal* term, BufferStyle bufferStyle, TimeStyle timeStyle)
    : value_(value), level_(level), term_(term), bufferStyle_(bufferStyle), timeStyle_(timeStyle)
  {
  }

  ~TypeWriter() override = default;

public:
  // root type
  void apply(const Type& type) override
  {
    std::ignore = type;

    if (value_.isEmpty())
    {
      term_->cprint(emptyValueStyle, "<empty>");
    }
    else
    {
      term_->cprintf(numberValueStyle, "%s", value_.getCopyAs<std::string>().c_str());  // NOLINT
    }
  }

  // root type
  void apply(const BoolType& type) override
  {
    std::ignore = type;
    term_->cprintf(enumValueStyle, "%s", value_.getCopyAs<std::string>().c_str());  // NOLINT
  }

  // native types
  void apply(const DurationType& type) override
  {
    std::ignore = type;

    auto val = getCopyAs<Duration>(value_);
    auto fs = std::chrono::duration<double, std::chrono::seconds::period>(val.toChrono());
    term_->cprintf(numberValueStyle, "%f", fs.count());  // NOLINT
    term_->cprint(descriptionStyle, " s");
  }

  void apply(const TimestampType& type) override
  {
    std::ignore = type;
    printTime(timeStyle_, term_, getCopyAs<TimeStamp>(value_));
  }

  void apply(const StringType& type) override
  {
    std::ignore = type;
    if (value_.isEmpty())
    {
      term_->cprint(emptyValueStyle, "<empty>");
    }
    else
    {
      term_->cprintf(stringValueStyle, "\"%s\"", getCopyAs<std::string>(value_).c_str());  // NOLINT
    }
  }

  // custom types
  void apply(const EnumType& type) override
  {
    auto val = getCopyAs<uint32_t>(value_);

    const auto* enumeration = type.getEnumFromKey(val);
    if (enumeration != nullptr)
    {
      term_->cprintf(enumValueStyle, "%s", enumeration->name.c_str());  // NOLINT
    }
    else
    {
      term_->cprint(enumValueStyle, "?");  // NOLINT
      return;
    }

    // get the description
    std::string desc = enumeration->description;
    trim(desc);

    if (desc.length() > 60)  // NOLINT
    {
      desc = desc.substr(0, 58);  // NOLINT
      desc.append("..");
    }

    // print it if present
    if (!desc.empty())
    {
      term_->cprintf(descriptionStyle, " (%s)", desc.c_str());  // NOLINT
    }
  }

  void apply(const StructType& type) override
  {
    if (type.getFields().empty())
    {
      term_->cprint(emptyValueStyle, "<empty>");
    }
    else
    {
      printVarMap(value_.get<VarMap>(), type);
    }
  }

  void apply(const VariantType& type) override
  {
    if (!value_.holds<KeyedVar>())
    {
      throwRuntimeError("invalid variant content");
    }

    const auto& [typeIndex, elementValue] = value_.get<KeyedVar>();

    const auto& fields = type.getFields();
    if (typeIndex >= fields.size())
    {
      throwRuntimeError("invalid type index for variant type");
    }

    const auto& field = fields[static_cast<size_t>(typeIndex)];

    term_->newLine();
    level_++;
    term_->printf("%*s- type:  %s", static_cast<int>(getIndent()), "", field.type->getName().data());  // NOLINT
    term_->newLine();
    term_->printf("%*s- value: ", static_cast<int>(getIndent()), "");  // NOLINT

    bool fieldRequiresNewLine = printedValueRequiresNewLine(*elementValue);

    // new line if the printed value requires nesting
    if (fieldRequiresNewLine)
    {
      level_++;
      term_->newLine();
    }

    // print the value
    TypeWriter tv(*elementValue, level_, term_, bufferStyle_, timeStyle_);
    field.type->accept(tv);

    // undo the new line, if any
    if (fieldRequiresNewLine)
    {
      level_--;
    }

    level_--;
  }

  void apply(const SequenceType& type) override
  {
    if (type.getElementType()->isUInt8Type())
    {
      printBuffer(term_, bufferStyle_, value_.get<VarList>(), getIndent());
    }
    else
    {
      printVarList(value_.get<VarList>(), type);
    }
  }

  void apply(const QuantityType& type) override
  {
    term_->cprintf(numberValueStyle,                         // NOLINT
                   "%s ",                                    // NOLINT
                   getCopyAs<std::string>(value_).c_str());  // NOLINT

    if (type.getUnit())
    {
      term_->cprint(descriptionStyle, type.getUnit(Unit::ensurePresent).getNamePlural());
    }
  }

  void apply(const AliasType& type) override
  {
    TypeWriter writer(value_, level_, term_, bufferStyle_, timeStyle_);
    type.getAliasedType()->accept(writer);
  }

  void apply(const OptionalType& type) override
  {
    if (value_.isEmpty())
    {
      term_->cprint(emptyValueStyle, "<empty>");
    }
    else
    {
      TypeWriter writer(value_, level_, term_, bufferStyle_, timeStyle_);
      type.getType()->accept(writer);
    }
  }

private:
  void printVarMap(const VarMap& theMap, const StructType& t)
  {
    if (theMap.empty())
    {
      return;
    }

    const auto& fields = t.getFields();

    auto finalIter = fields.end();
    --finalIter;  // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)

    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
    for (auto itr = fields.begin(); itr != fields.end(); ++itr)
    {
      const std::string fieldName = itr->name;
      const Type& fieldType = *itr->type;

      auto fieldItr = theMap.find(fieldName);
      if (fieldItr != theMap.end())
      {
        level_++;
        term_->printf("%*s- %s: ", static_cast<int>(getIndent()), "", fieldName.c_str());  // NOLINT

        const Var& fieldValue(fieldItr->second);

        bool fieldRequiresNewLine = printedValueRequiresNewLine(fieldValue);

        // new line if the printed value is a nested struct
        if (fieldRequiresNewLine)
        {
          level_++;
          term_->newLine();
        }

        TypeWriter tv(fieldValue, level_, term_, bufferStyle_, timeStyle_);
        fieldType.accept(tv);

        if (fieldRequiresNewLine)
        {
          level_--;
        }

        if (itr != finalIter)
        {
          term_->newLine();
        }

        level_--;
      }
    }
  }

  void printVarList(const VarList& theList, const SequenceType& t)
  {
    if (theList.empty())
    {
      term_->cprint(emptyValueStyle, "<empty>");
    }
    else
    {
      size_t numberWidth = 0;
      if (theList.size() >= 10 && theList.size() < 100)  // NOLINT(readability-magic-numbers)
      {
        numberWidth = 2;
      }
      else if (theList.size() >= 100 && theList.size() < 1000)  // NOLINT(readability-magic-numbers)
      {
        numberWidth = 3;
      }
      else
      {
        // nothing to do here
      }

      size_t i = 0;
      for (auto& elem: theList)
      {
        const auto indent = static_cast<int>(getIndent());
        const auto numWidth = static_cast<int>(numberWidth);
        term_->cprintf(listIndexStyle, "%*s[%*zd] ", indent, "", numWidth, i);  // NOLINT

        bool fieldRequiresNewLine = printedValueRequiresNewLine(elem);

        // new line if the printed value is a nested struct
        if (fieldRequiresNewLine)
        {
          level_++;
          term_->newLine();
        }

        TypeWriter tv(elem, level_, term_, bufferStyle_, timeStyle_);
        t.getElementType()->accept(tv);

        if (fieldRequiresNewLine)
        {
          level_--;
        }

        if (i != theList.size() - 1)
        {
          term_->newLine();
        }

        i++;
      }
    }
  }

  [[nodiscard]] size_t getIndent() const noexcept { return level_ * 2U; }

private:
  const Var& value_;
  size_t level_;
  Terminal* term_;
  BufferStyle bufferStyle_;
  TimeStyle timeStyle_;
};

//--------------------------------------------------------------------------------------------------------------
// TypeDescriptionPrinter
//--------------------------------------------------------------------------------------------------------------

class TypeDescriptionPrinter final: public TypeVisitor
{
public:
  SEN_NOCOPY_NOMOVE(TypeDescriptionPrinter)

public:
  explicit TypeDescriptionPrinter(Terminal* term) noexcept: term_(term) {}

  ~TypeDescriptionPrinter() override = default;

public:
  // root type
  void apply(const Type& type) override { printHeader(type, "TYPE"); }

  // native types
  void apply(const NativeType& type) override { printHeader(type, "NATIVE TYPE"); }
  void apply(const IntegralType& type) override
  {
    printHeader(type, "INTEGRAL TYPE");
    term_->cprintf(descriptionStyle, "      size in bytes: %ld\n", type.getByteSize());        // NOLINT
    term_->cprintf(descriptionStyle, "      is signed:     %s\n", yesNo(type.isSigned()));     // NOLINT
    term_->cprintf(descriptionStyle, "      has infinity:  %s\n", yesNo(type.hasInfinity()));  // NOLINT
  }
  void apply(const RealType& type) override
  {
    printHeader(type, "REAL TYPE");
    term_->cprintf(descriptionStyle, "      size in bytes: %ld\n", type.getByteSize());        // NOLINT
    term_->cprintf(descriptionStyle, "      epsilon:       %e\n", type.getEpsilon());          // NOLINT
    term_->cprintf(descriptionStyle, "      min value:     %e\n", type.getMinValue());         // NOLINT
    term_->cprintf(descriptionStyle, "      max value:     %e\n", type.getMaxValue());         // NOLINT
    term_->cprintf(descriptionStyle, "      is signed:     %s\n", yesNo(type.isSigned()));     // NOLINT
    term_->cprintf(descriptionStyle, "      has infinity:  %s\n", yesNo(type.hasInfinity()));  // NOLINT
  }

  // custom types
  void apply(const EnumType& type) override
  {
    printHeader(type, "ENUMERATION");

    std::vector<std::string> header;
    std::vector<std::vector<std::string>> table;

    header.emplace_back("name");
    header.emplace_back("key");
    header.emplace_back("desc");

    const auto& enums = type.getEnums();
    for (const auto& enumerator: enums)
    {
      std::vector<std::string> row;
      row.emplace_back(enumerator.name);
      row.emplace_back(fromFormat("[%d]", enumerator.key));  // NOLINT(hicpp-vararg)
      row.emplace_back(enumerator.description);
      table.push_back(std::move(row));
    }

    printTitle("ENUMERATORS\n");
    Printer::printTable(header, table, 4, false, term_, descriptionStyle);
  }
  void apply(const StructType& type) override
  {
    printHeader(type, "STRUCTURE");

    std::vector<std::string> header;
    std::vector<std::vector<std::string>> table;

    header.emplace_back("");
    header.emplace_back("name");
    header.emplace_back("type");
    header.emplace_back("desc");

    for (const auto& field: type.getFields())
    {
      std::vector<std::string> row;
      row.emplace_back("-");
      row.emplace_back(field.name.c_str());
      row.emplace_back(fromFormat("[%s]", getQualName(*field.type).data()));  // NOLINT
      row.emplace_back(field.description);
      table.push_back(std::move(row));
    }

    printTitle("FIELDS\n");
    Printer::printTable(header, table, 4, false, term_, descriptionStyle);
  }
  void apply(const VariantType& type) override
  {
    printHeader(type, "VARIANT");
    std::vector<std::string> header;
    std::vector<std::vector<std::string>> table;

    header.emplace_back("");
    header.emplace_back("type");
    header.emplace_back("desc");

    for (const auto& element: type.getFields())
    {
      std::vector<std::string> row;
      row.emplace_back("-");
      row.emplace_back(getQualName(*element.type).data());
      row.emplace_back(element.description);
      table.push_back(std::move(row));
    }

    printTitle("TYPES\n");
    Printer::printTable(header, table, 2, false, term_, descriptionStyle);
  }
  void apply(const SequenceType& type) override
  {
    printHeader(type, "SEQUENCE TYPE");
    term_->cprintf(descriptionStyle,                          // NOLINT
                   "      element type:  %s\n",               // NOLINT
                   type.getElementType()->getName().data());  // NOLINT

    term_->cprintf(descriptionStyle, "      is bounded:    %s\n", yesNo(type.isBounded()));  // NOLINT
    if (type.isBounded())
    {
      term_->cprintf(descriptionStyle, "      max size:      %lu\n", type.getMaxSize().value());  // NOLINT
    }
    term_->cprintf(descriptionStyle, "      fixed size:    %s\n", yesNo(type.hasFixedSize()));  // NOLINT
  }
  void apply(const ClassType& type) override
  {
    printHeader(type, type.isInterface() ? "INTERFACE" : "CLASS");

    if (const PropertyList& props = type.getProperties(ClassType::SearchMode::includeParents); !props.empty())
    {
      printTitle("PROPERTIES\n");

      std::vector<std::string> header;
      std::vector<std::vector<std::string>> table;

      header.emplace_back("type");
      header.emplace_back("name");
      header.emplace_back("flags");
      header.emplace_back("desc");

      for (const auto& prop: props)
      {
        const auto category = prop->getCategory();
        bool isStatic = category == PropertyCategory::staticRO || category == PropertyCategory::staticRW;
        bool isReadOnly = category == PropertyCategory::staticRO || category == PropertyCategory::dynamicRO;

        std::vector<std::string> row;
        row.emplace_back(fromFormat("[%s]", prop->getType()->getName().data()));  // NOLINT
        row.emplace_back(prop->getName());
        row.emplace_back(fromFormat(  // NOLINT
                           "%s-%s-%s",
                           isStatic ? "st" : "dy",
                           isReadOnly ? "ro" : "rw",
                           getTransportStr(prop->getTransportMode()))
                           .c_str());

        row.emplace_back(prop->getDescription().data());
        table.push_back(row);
      }

      Printer::printTable(header, table, 4, false, term_, descriptionStyle);
      term_->newLine();
    }

    if (const MethodList& methods = type.getMethods(ClassType::SearchMode::includeParents); !methods.empty())
    {
      printTitle("METHODS\n");

      std::vector<std::string> header;
      std::vector<std::vector<std::string>> table;

      header.emplace_back("-");
      header.emplace_back("name");
      header.emplace_back("desc");

      for (const auto& method: methods)
      {
        std::vector<std::string> row;
        header.emplace_back("-");
        row.emplace_back(method->getName());
        row.emplace_back(method->getDescription().data());
        table.push_back(row);
      }

      Printer::printTable(header, table, 4, false, term_, descriptionStyle);
      term_->newLine();
    }

    const auto& events = type.getEvents(ClassType::SearchMode::includeParents);
    if (!events.empty())
    {
      printTitle("EVENTS\n");

      std::vector<std::string> header;
      std::vector<std::vector<std::string>> table;

      header.emplace_back("-");
      header.emplace_back("name");
      header.emplace_back("tr");
      header.emplace_back("desc");

      for (const auto& ev: events)
      {
        std::vector<std::string> row;
        header.emplace_back("-");
        row.emplace_back(ev->getName());
        row.emplace_back(getTransportStr(ev->getTransportMode()));
        row.emplace_back(ev->getDescription().data());
        table.push_back(row);
      }

      Printer::printTable(header, table, 4, false, term_, descriptionStyle);
      term_->newLine();
    }
  }
  void apply(const QuantityType& type) override
  {
    printHeader(type, "QUANTITY TYPE ");
    term_->cprintf(                                                                   // NOLINT
      descriptionStyle,                                                               // NOLINT
      "      unit:  %s\n",                                                            // NOLINT
      type.getUnit() ? type.getUnit(Unit::ensurePresent).getName().data() : "none");  // NOLINT
  }
  void apply(const AliasType& type) override
  {
    printHeader(type, "ALIAS TYPE ");
    term_->cprintf(descriptionStyle,                          // NOLINT
                   "      aliased type:  %s\n",               // NOLINT
                   type.getAliasedType()->getName().data());  // NOLINT
  }

private:
  void printTitle(std::string_view title) const
  {
    term_->print("  ");
    term_->cprint(titleStyle, title.data());
  }

  void printHeader(const Type& type, std::string_view header)
  {
    term_->newLine();
    printTitle(header);
    term_->cprintf(typenameStyle, " %s", getQualName(type).data());  // NOLINT

    // parents
    if (const auto* metaClass = type.asClassType())
    {
      const auto& parents = metaClass->getParents();
      if (!parents.empty())
      {
        if (parents.size() > 1)
        {
          term_->cprint(descriptionStyle, " (parents: ");
        }
        else
        {
          term_->cprint(descriptionStyle, " (parent: ");
        }

        for (auto itr = parents.begin(); itr != parents.end(); ++itr)
        {
          term_->cprint(typenameStyle, (*itr)->getQualifiedName().data());
          if (itr + 1 != parents.end())
          {
            term_->cprint(descriptionStyle, ", ");
          }
        }
        term_->cprint(descriptionStyle, ")");
      }
    }
    else if (const auto* metaStruct = type.asStructType())
    {
      if (auto parent = metaStruct->getParent())
      {
        term_->cprint(descriptionStyle, " (parent: ");
        term_->cprint(typenameStyle, parent.value()->getQualifiedName().data());
        term_->cprint(descriptionStyle, ")");
      }
    }
    else
    {
      // no code needed
    }

    term_->newLine();

    if (!type.getDescription().empty())
    {
      term_->newLine();
      term_->print("  ");
      term_->cprint(titleStyle, "DESCRIPTION\n");
      term_->print("    ");
      term_->cprintf(descriptionStyle,                                                           // NOLINT
                     "%s\n",                                                                     // NOLINT
                     Printer::formatTextForWidth(type.getDescription().data(), 80, 4).c_str());  // NOLINT
    }

    term_->newLine();
  }

private:
  Terminal* term_;
};

}  // namespace

//--------------------------------------------------------------------------------------------------------------
// Printer
//--------------------------------------------------------------------------------------------------------------

Printer::Printer(Terminal* terminal, BufferStyle bufferStyle, TimeStyle timeStyle)
  : terminal_(terminal), bufferStyle_(bufferStyle), timeStyle_(timeStyle)
{
}

void Printer::printWelcome(Terminal* terminal)
{
  const auto& build = kernel::Kernel::getBuildInfo();
  auto* gitStatus = StringConversionTraits<kernel::GitStatus>::toString(build.gitStatus).data();

  auto line1 =
    fromFormat("compiler %s [mode: %s]", build.compiler.c_str(), build.debugMode ? "debug" : "release");  // NOLINT

  auto line2 = std::string("revision ").append(SEN_VERSION_STRING);              // NOLINT
  auto line3 = fromFormat("branch   %s [%s]", build.gitRef.c_str(), gitStatus);  // NOLINT

  auto infoWidth = std::max(line2.size(), line1.size());
  infoWidth = std::max(infoWidth, line3.size());
  const auto infoMargin = 27;
  const auto bannerLength = infoMargin + infoWidth;
  const auto totalHeight = 6U;

  terminal->hideCursor();
  for (unsigned int i = 0; i < totalHeight; ++i)
  {
    terminal->newLine();
  }
  terminal->saveCursorPosition();

  terminal->moveCursorUp(3);
  terminal->moveCursorAllLeft();
  terminal->moveCursorRight(17);                                                                          // NOLINT
  terminal->cprintf(versionStyle, "v%d.%d.%d", SEN_VERSION_MAJOR, SEN_VERSION_MINOR, SEN_VERSION_PATCH);  // NOLINT

  terminal->moveCursorUp(2);
  terminal->moveCursorAllLeft();
  terminal->moveCursorRight(infoMargin);
  terminal->cprint(buildInfoStyle, line1);

  terminal->moveCursorAllLeft();
  terminal->moveCursorRight(infoMargin);
  terminal->moveCursorDown();
  terminal->cprint(buildInfoStyle, line2);

  terminal->moveCursorAllLeft();
  terminal->moveCursorRight(infoMargin);
  terminal->moveCursorDown();
  terminal->cprint(buildInfoStyle, line3);

  terminal->moveCursorAllLeft();
  terminal->moveCursorUp(3);
  printLogo(terminal, bannerLength, getPallete());

  terminal->restoreCursorPosition();
  terminal->showCursor();
}

void Printer::printError(const char* fmt, ...) const  // NOLINT
{
  terminal_->newLine();
  terminal_->print("  ");
  terminal_->cprint(errorStyle, "Error:");
  terminal_->print(" ");

  va_list args;  // NOLINT
  va_start(args, fmt);
  terminal_->vcprintf(errorMessageStyle, fmt, args);
  va_end(args);

  terminal_->newLine();
}

void Printer::printMethodCallResult(const MethodResult<Var>& var, const Method* method, std::string_view command) const
{
  std::ignore = command;

  auto type = method->getReturnType();

  if (var.isOk())
  {
    terminal_->newLine();
    printValue(var.getValue(), type->isSequenceType() ? 1U : 0U, type.type());
    terminal_->newLine();
  }
  else
  {
    try
    {
      std::rethrow_exception(var.getError());
    }
    catch (const std::exception& err)
    {
      printError("call error: %s", err.what());  // NOLINT
    }
  }
}

void Printer::printValue(const Var& value, size_t level, const Type* type) const
{
  TypeWriter writer(value, level, terminal_, bufferStyle_, timeStyle_);
  type->accept(writer);
}

void Printer::printProperties(Object* object) const
{
  auto props = object->getClass()->getProperties(ClassType::SearchMode::includeParents);
  if (props.empty())
  {
    return;
  }

  terminal_->newLine();

  terminal_->print("state at ");
  printTime(timeStyle_, terminal_, object->getLastCommitTime());
  terminal_->newLine();

  for (const auto& prop: props)
  {
    printPropertyValue(prop.get(), object->getPropertyUntyped(prop.get()));
  }
}

void Printer::printPropertyValue(const Property* prop, const Var& value) const
{
  const auto& propType = prop->getType();
  terminal_->printf("%*s- %s: ", 0, "", prop->getName().data());  // NOLINT

  size_t propLevel = 0;
  if (propType->isStructType())
  {
    propLevel = 0;
    terminal_->newLine();
  }
  else if (propType->isSequenceType() && !value.get<VarList>().empty())
  {
    propLevel = 1;
    terminal_->newLine();
  }
  else
  {
    // no code needed
  }

  printValue(value, propLevel, propType.type());
  terminal_->newLine();
}

void Printer::printDescription(const ClassType& owner, const Method* method) const
{
  terminal_->newLine();

  std::string constness;
  switch (method->getConstness())
  {
    case Constness::constant:
      constness = "const";
      break;
    case Constness::nonConstant:
      constness = "not-const";
      break;
  }

  printTitle("METHOD");
  terminal_->cprintf(typenameStyle, " %s", method->getName().data());  // NOLINT
  terminal_->cprint(descriptionStyle, " (class ");
  terminal_->cprint(typenameStyle, owner.getName());
  terminal_->cprintf(descriptionStyle, ") [%s]\n", constness.c_str());  // NOLINT

  if (!method->getDescription().empty())
  {
    terminal_->newLine();
    printTitle("DESCRIPTION\n");

    std::string description(method->getDescription());
    trimRight(description, ".");
    terminal_->cprintf(descriptionStyle, "  %s.\n", formatTextForWidth(description, 80, 4).c_str());  // NOLINT
  }

  if (!method->getArgs().empty())
  {
    terminal_->newLine();
    printTitle("ARGUMENTS\n");

    std::vector<std::string> header;
    std::vector<std::vector<std::string>> table;

    header.emplace_back("type");
    header.emplace_back("name");
    header.emplace_back("desc");

    for (const auto& arg: method->getArgs())
    {
      std::vector<std::string> row;
      row.push_back(fromFormat("[%s]", getQualName(*arg.type).data()));  // NOLINT
      row.push_back(fromFormat("%s", arg.name.c_str()));                 // NOLINT
      row.push_back(arg.description.empty() ? "<not documented>" : arg.description);

      table.push_back(row);
    }

    printTable(header, table, 4, false, terminal_, descriptionStyle);
    terminal_->newLine();
  }

  terminal_->newLine();
  printTitle("RETURN VALUE");
  terminal_->cprintf(typenameStyle, " (%s)\n", method->getReturnType()->getName().data());  // NOLINT
}

void Printer::printDescription(const Object& instance) const
{
  terminal_->newLine();
  printTitle("OBJECT");
  terminal_->cprintf(descriptionStyle, " %s [id ", instance.getName().data());        // NOLINT
  terminal_->cprintf(typenameStyle, "%s", intToHex(instance.getId().get()).c_str());  // NOLINT
  terminal_->cprintf(descriptionStyle, "]\n");                                        // NOLINT

  TypeDescriptionPrinter printer(terminal_);
  instance.getClass()->accept(printer);
}

void Printer::printDescription(const Type& type) const
{
  TypeDescriptionPrinter printer(terminal_);
  type.accept(printer);
}

void Printer::printInstances(const std::list<Object*>& instances) const
{
  if (instances.empty())
  {
    return;
  }

  std::vector<std::string> header;
  std::vector<std::vector<std::string>> table;

  header.emplace_back("");
  header.emplace_back("name");
  header.emplace_back("class");
  header.emplace_back("locality");

  for (const auto* elem: instances)
  {
    std::vector<std::string> row;
    row.emplace_back("-");
    row.emplace_back(fromFormat("%s", elem->getName().c_str()));                                   // NOLINT
    row.emplace_back(fromFormat("[%s]", elem->getClass()->getName().data()));                      // NOLINT
    row.emplace_back(fromFormat("[%s]", elem->asNativeObject() != nullptr ? "Local" : "Remote"));  // NOLINT
    table.push_back(row);
  }

  printTable(header, table, 1, false, terminal_);
}

void Printer::printEnumError(const std::string& argName, const EnumType* type) const
{
  terminal_->cprint(errorStyle, "  Error: ");

  if (argName.empty())
  {
    terminal_->cprint(informationStyle, "Invalid enumeration value. Allowed values are:\n");
  }
  else
  {
    terminal_->cprintf(  // NOLINT
      informationStyle,
      "Invalid enumeration for argument '%s'. Allowed values are:\n",
      argName.c_str());  // NOLINT
  }

  std::vector<std::string> header;
  std::vector<std::vector<std::string>> table;

  header.emplace_back("");
  header.emplace_back("name");
  header.emplace_back("key");
  header.emplace_back("desc");

  for (const auto& e: type->getEnums())
  {
    std::vector<std::string> row;
    row.emplace_back("-");
    row.push_back(e.name);
    row.push_back(fromFormat("[%d]", e.key));  // NOLINT
    row.push_back(e.description);
    table.push_back(row);
  }

  printTable(header, table, 2, false, terminal_);
}

void Printer::printTable(const std::vector<std::string>& header,
                         const std::vector<std::vector<std::string>>& table,
                         unsigned indentation,
                         bool printHeader,
                         Terminal* terminal,
                         const Style& style)
{
  // calc the max widths
  std::vector<size_t> maxWidths(header.size(), 0);

  for (size_t j = 0; j < header.size(); ++j)
  {
    maxWidths[j] = (maxWidths[j] < header[j].length() ? header[j].length() : maxWidths[j]);
  }

  for (const auto& row: table)
  {
    for (size_t j = 0; j < row.size(); ++j)
    {
      maxWidths[j] = (maxWidths[j] < row[j].length() ? row[j].length() : maxWidths[j]);
    }
  }

  // write header
  if (printHeader)
  {
    terminal->print(std::string(indentation, ' '));
    for (size_t i = 0; i < header.size(); ++i)
    {
      terminal->cprint(style, header[i]);
      size_t width = maxWidths[i] - header[i].length() + 1;

      terminal->print(std::string(width, ' '));
    }
    terminal->newLine();
  }

  uint32_t termW = 0;
  uint32_t termH = 0;
  terminal->getSize(termH, termW);

  // write rows
  for (const auto& row: table)
  {
    size_t printedChars = 0;

    terminal->print(std::string(indentation, ' '));
    printedChars += indentation;

    for (size_t j = 0; j < row.size(); ++j)
    {
      if (j == row.size() - 1)  // last column
      {
        const auto& lastColumn = row[j];
        size_t availableSpace = termW - printedChars;
        if (lastColumn.size() > availableSpace)
        {
          terminal->cprint(style, lastColumn.substr(0, availableSpace - 2) + "..");
        }
        else
        {
          terminal->cprint(style, lastColumn);
        }
      }
      else
      {
        terminal->cprint(style, row[j] + " ");
        printedChars += row[j].size() + 1;

        size_t width = maxWidths[j] - row[j].length();
        terminal->print(std::string(width, ' '));
        printedChars += width;
      }
    }

    terminal->newLine();
  }
}

std::string Printer::formatTextForWidth(const std::string& text, size_t width, size_t indent)
{
  std::string result;
  std::string currentLine;

  auto words = ::sen::impl::split(text, ' ');
  for (const auto& word: words)
  {
    if (currentLine.length() + word.length() > width)
    {
      trimRight(currentLine);

      result += currentLine;
      result += "\n";
      result += std::string(indent, ' ');

      currentLine = word;
      currentLine.append(" ");
    }
    else
    {
      currentLine.append(word);
      currentLine.append(" ");
    }
  }

  if (!currentLine.empty())
  {
    result.append(currentLine);
  }

  // Trim last space
  trimRight(result);
  return result;
}

void Printer::printTitle(std::string_view title) const
{
  terminal_->print("  ");
  terminal_->cprint(titleStyle, title.data());
}

}  // namespace sen::components::shell
