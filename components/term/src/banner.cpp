// === banner.cpp ======================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "banner.h"

// local
#include "styles.h"
#include "unicode.h"

// sen
#include "sen/core/base/checked_conversions.h"

// ftxui
#include <ftxui/screen/terminal.hpp>

// std
#include <cstddef>
#include <random>
#include <string>
#include <vector>

namespace sen::components::term
{

using sen::std_util::checkedConversion;

//--------------------------------------------------------------------------------------------------------------
// Helpers
//--------------------------------------------------------------------------------------------------------------

namespace
{

const std::vector<BannerQuote> allQuotes = {
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
  {"Talk is cheap. Show me the code", "Linus Torvalds"},
  {"Essentials never lose their value", "Tamil saying"},
  {"Architecture is strategy", "Old engineering proverb"},
  {"The function of good software is to make the complex appear to be simple", "Grady Booch"},
  {"Code is read much more often than it is written", "Guido Van Rossum"},
  {"The longer you wait before fixing a bug, the costlier it is to fix", "Chris Pine"},
  {"Without design, programming is the art of adding bugs to an empty text file", "Rick Cook"},
  {"It is very hard to predict, especially the future", "Niels Bohr"},
  {"Good design adds value faster than it adds cost", "Dijkstra"},
  {"Temporary solutions often become permanent problems", "John Shore"},
  {"Don't document the problem, fix it", "Robert Martin"},
  {"No amount of testing can prove a software right, 1 test can prove it wrong", "Alex Lowe"},
  {"Absence of evidence is not evidence of absence", "Robert D. Schneider"},
  {"Before you say you can't do something, try it", "Seymour Papert"},
  {"I'm not a great programmer; just a good programmer with great habits", "Kent Beck"},
  {"Architecture is the tension between coupling and cohesion", "Joshua Bloch"},
  {"If you automate a mess, you get an automated mess", "Vint Cerf"},
  {"In a room full of top SW designers, if two agree, that's a majority", "Roy Carlson"},
  {"Incorrect documentation is often worse than no documentation", "Eric Evans"},
  {"Simplicity doesn't precede complexity, but follows it", "Michael J. Saylor"},
  {"In essence, engineering is doing what you want with what you have", "J. Carmack"},
  {"An error doesn't become a mistake until you refuse to correct it", "Alan Kay"},
  {"Don't Gather Requirements, Dig for Them", "Andrew Hunt"},
  {"If you're a technical lead, you need to be coding", "Martin Fowler"},
  {"Quality is never an accident; it is always the result of intelligent effort", "John Ruskin"},
  {"Watch the little things; a small leak will sink a great ship", "Benjamin Franklin"},
  {"The nice thing about standards is that there are so many to choose from", "Andrew S. Tannenbaum"},
  {"The optimum committee has no members", "Norman Ralph Augustine"},
  {"Complexity has nothing to do with intelligence, simplicity does", "Larry Bossidy"},
  {"Those who can imagine anything, can create the impossible", "Alan Turing"},
  {"You haven't mastered a tool until you understand when it should not be used", "Kelsey Hightower"},
  {"One man's crappy software is another man's full time job", "Jessica Gaston"},
  {"Every craftsman starts the journey with a set of good-quality tools", "Ada Lovelace"},
  {"If you think good architecture is expensive, try bad architecture", "Jack Reeves"},
  {"The value of a prototype is in the education it gives you, not in the code", "Zhuowei Zhang"},
  {"If it doesn't work, it doesn't matter how fast it doesn't work", "Leah Culver"},
  {"If you lie to the compiler, it will get its revenge", "Henry Spencer"},
  {"The software is not finished until the last user is dead", "Dennie Tassel"},
  {"Write simple parts connected by clean interfaces", "Eric S. Raymond"},
  {"Design programs to be connected to other programs", "Eric S. Raymond"},
  {"Robustness is the child of transparency and simplicity", "Eric S. Raymond"},
  {"Fold knowledge into data so program logic can be stupid and robust", "Eric S. Raymond"},
  {"Clarity is better than cleverness", "Eric S. Raymond"},
  {"When a program has nothing surprising to say, it should say nothing", "Eric S. Raymond"},
  {"When you must fail, fail noisily and as soon as possible", "Eric S. Raymond"},
  {"Design for simplicity; add complexity only where you must", "Eric S. Raymond"},
  {"Prototype before polishing. Get it working before you optimize it", "Eric S. Raymond"},
  {"The real hero of programming is the one who writes negative code", "Doug McIlroy"},
  {"When in doubt, use brute force", "Ken Thompson"},
  {"Minimize Coupling, Maximize Cohesion", "Old proverb"},
  {"Document 'Why'", "Old engineering proverb"},
  {"Share early, share often", "Old engineering proverb"},
  {"Never attribute to malice that which is adequately explained by stupidity", "Hanlon's razor"},
  {"The purpose of computation is insight, not numbers", "Richard Hamming"},
  {"Efficiency is doing things right; effectiveness is doing the right things", "Peter F. Drucker"},
  {"How can I improve the code so that this comment isn't needed?", "Steve McConnell"},
  {"All models are wrong, some are useful", "George Box"},
  {"The secret of getting ahead is getting started", "Mark Twain"},
  {"Hope is not a strategy", "SRE motto"},
  {"Chance favours the prepared mind", "Louis Pasteur"},
  {"Remember that all is opinion", "Marcus Aurelius"},
  {"Mistakes are the portals of discovery", "James Joyce"},
  {"Intellectuals solve problems, geniuses prevent them", "Einstein"},
  {"A system increases its complexity unless work is done to reduce it", "Meir M. Lehman"},
  {"Failure free operations require experience with failure", "Richard I. Cook"},
  {"Don't fight forces, use them", "R. Buckminster Fuller"},
  {"An essential aspect of creativity is not being afraid to fail", "Edwin Land"},
  {"It's not that we need new ideas, but we need to stop having old ideas", "Edwin Land"},
  {"All the easy problems have been solved", "Kinkler's Second Law"},
  {"Work expands so as to fill the time available for its completion", "Parkinson's Law"},
  {"The best way to predict the future is to invent it", "Alan Kay"},
  {"You miss 100% of the shots you don't take", "Wayne Gretzky"},
  {"I never lose. I either win or I learn", "Nelson Mandela"},
  {"Before optimizing, use a profiler", "Mike Morton"},
  {"The cheapest and most reliable components are those that aren't there", "Gordon Bell"},
  {"Omit needless words", "Strunk and White"},
  {"If the code and the comments disagree, then both are probably wrong", "Norm Schryer"},
  {"In interface design, always do the least surprising thing", "Eric S. Raymond"},
  {"Make it correct, clear, concise, and fast. In that order", "Rebecca Wirfs-Brock"},
  {"Don't believe everything you read on a computer screen", "Abraham Lincoln"},
  {"Don't take advice from those who don't have to live with the consequences", "Mark Cuban"},
  {"It's easy to optimize a correct system, but hard to correct an optimized one", "Anonymous"},
  {"Innovation distinguishes between a leader and a follower", "Steve Jobs"},
  {"Reality is that which, when you stop believing in it, doesn't go away", "Philip K. Dick"},
  {"SW complexity comes from trying to make one thing do two things", "Ryan Singer"},
  {"No battle was ever won according to plan, no battle was ever won without one", "Dwight D. Eisenhower"},
  {"Controlling complexity is the essence of computer programming", "Brian Kernighan"},
  {"The best code is no code at all", "Jeff Atwood"},
  {"There are only two hard things in CS: cache invalidation and naming things", "Phil Karlton"},
  {"The most dangerous phrase is: we've always done it this way", "Grace Hopper"},
  {"Deleted code is debugged code", "Jeff Sickel"},
  {"Plan to throw one away; you will, anyhow", "Fred Brooks"},
  {"Any sufficiently advanced technology is indistinguishable from magic", "Arthur C. Clarke"},
  {"Measure twice, cut once", "Old proverb"},
  {"If you can't explain it simply, you don't understand it well enough", "Einstein"},
  {"Everything should be made as simple as possible, but not simpler", "Einstein"},
  {"The best time to plant a tree was 20 years ago. The second best time is now", "Chinese proverb"},
  {"First, solve the problem. Then, write the code", "John Johnson"},
  {"Software and cathedrals are much the same: first we build them, then we pray", "Sam Redwine"},
  {"Weeks of coding can save you hours of planning", "Anonymous"},
  {"It works on my machine", "Every developer, at some point"},
  {"Debugging is twice as hard as writing the code in the first place", "Brian Kernighan"},
  {"The only way to go fast is to go well", "Robert Martin"},
  {"Code never lies, comments sometimes do", "Ron Jeffries"},
  {"Premature optimization is the root of all evil", "Donald Knuth"},
  {"All non-trivial abstractions, to some degree, are leaky", "Joel Spolsky"},
  {"The key to performance is elegance, not battalions of special cases", "Jon Bentley"},
  {"Ship it and iterate", "Old startup proverb"},
  {"The expert in anything was once a beginner", "Helen Hayes"},
  {"Computers are good at following instructions, but not at reading your mind", "Donald Knuth"},
  {"The only constant in technology is change", "Marc Benioff"},
  {"Simplicity is prerequisite for reliability", "Dijkstra"},
  {"How do you make a small fortune in software? Start with a large fortune", "Anonymous"},
};

[[nodiscard]] const BannerQuote& getRandomQuote()
{
  static std::mt19937 gen(std::random_device {}());
  std::uniform_int_distribution<std::size_t> dist(0, allQuotes.size() - 1U);
  return allQuotes[dist(gen)];
}

}  // namespace

Span<const BannerQuote> getBannerQuotes() { return allQuotes; }

//--------------------------------------------------------------------------------------------------------------
// Public API
//--------------------------------------------------------------------------------------------------------------

ftxui::Element renderBanner(std::string_view version, std::string_view compiler, std::string_view buildMode)
{
  const auto& quote = getRandomQuote();

  auto title = ftxui::hbox({
    ftxui::text("  Sen") | ftxui::bold | styles::bannerTitle(),
    ftxui::text(" v" + std::string(version)) | styles::bannerText(),
    ftxui::text("  ") | styles::mutedText(),
    ftxui::text(std::string(compiler) + " [" + std::string(buildMode) + "]") | styles::mutedText(),
  });

  constexpr std::size_t margin = 2;

  // If quote + author fit on one line, show them together; otherwise author goes on the next line.
  auto quoteStr = std::string("  ") + std::string(quote.text);
  auto authorStr = std::string("- ") + std::string(quote.author);
  ftxui::Element quoteLine;
  if (quoteStr.size() + 2 + authorStr.size() <= bannerWidth)
  {
    quoteLine = ftxui::hbox({
      ftxui::text(quoteStr) | ftxui::italic | ftxui::color(styles::bannerQuoteColor()),
      ftxui::text("  "),
      ftxui::text(authorStr) | ftxui::color(styles::bannerAuthorColor()),
    });
  }
  else
  {
    quoteLine = ftxui::vbox({
      ftxui::text(quoteStr) | ftxui::italic | ftxui::color(styles::bannerQuoteColor()),
      ftxui::hbox(
        {ftxui::filler(), ftxui::text(authorStr) | ftxui::color(styles::bannerAuthorColor()), ftxui::text("  ")}),
    });
  }
  quoteLine = quoteLine | ftxui::size(ftxui::WIDTH, ftxui::LESS_THAN, checkedConversion<int>(bannerWidth) + 1);

  // Color bar: a single row of block characters in theme colors.
  const auto& theme = activeTheme();
  const ftxui::Color barColors[] = {
    theme.error,
    theme.success,
    theme.valueString,
    theme.info,
    theme.accent,
    theme.completionObject,
  };

  constexpr std::size_t colorCount = 6;

  constexpr auto barWidth = bannerWidth - margin;
  auto segLen = barWidth / colorCount;
  auto remainder = barWidth % colorCount;

  ftxui::Elements barElements;
  barElements.push_back(ftxui::text("  "));
  for (std::size_t i = 0; i < colorCount; ++i)
  {
    auto n = segLen + (i < remainder ? 1 : 0);
    std::string seg;
    for (std::size_t j = 0; j < n; ++j)
    {
      seg += unicode::blockBar;
    }
    barElements.push_back(ftxui::text(seg) | ftxui::color(barColors[i]));
  }
  auto colorBar = ftxui::hbox(std::move(barElements));

  return ftxui::vbox({title, colorBar, quoteLine});
}

}  // namespace sen::components::term
