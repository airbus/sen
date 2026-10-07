// === clipboard_test.cpp ==============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "clipboard.h"

// google test
#include <gtest/gtest.h>

// std
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <ios>
#include <iterator>
#include <string>
#include <string_view>

#ifndef _WIN32
#  include <fcntl.h>
#  include <unistd.h>
#endif

namespace sen::components::term
{
namespace
{

#ifndef _WIN32

/// Redirects stdout to a temp file while `fn` runs, then returns the bytes it
/// wrote. Uses POSIX dup/dup2 so it also captures `std::fputs(..., stdout)` in
/// addition to `std::cout`.
template <typename Fn>
std::string captureStdout(Fn&& fn)
{
  std::array<char, 64> tmpl {};
  std::strncpy(tmpl.data(), "/tmp/term_clip_test_XXXXXX", tmpl.size() - 1U);
  // NOLINTNEXTLINE(misc-include-cleaner) POSIX; <cstdlib> need not declare it
  int fd = ::mkstemp(tmpl.data());
  if (fd < 0)
  {
    return {};
  }
  std::fflush(stdout);
  int savedStdout = ::fcntl(STDOUT_FILENO, F_DUPFD_CLOEXEC, 0);  // NOLINT(hicpp-vararg)
  ::dup2(fd, STDOUT_FILENO);
  ::close(fd);

  fn();

  std::fflush(stdout);
  ::dup2(savedStdout, STDOUT_FILENO);
  ::close(savedStdout);

  std::ifstream in(tmpl.data(), std::ios::binary);
  std::string contents((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
  ::unlink(tmpl.data());
  return contents;
}

/// Unset display env vars so `clipboard::copy` doesn't spawn `xclip`/`wl-copy`
/// during the test. We only want to exercise the terminal-escape path.
class NoDisplayEnv
{
public:
  NoDisplayEnv()
  {
    saveAndUnset("WAYLAND_DISPLAY", savedWayland_);
    saveAndUnset("DISPLAY", savedDisplay_);
  }

  ~NoDisplayEnv()
  {
    restore("WAYLAND_DISPLAY", savedWayland_);
    restore("DISPLAY", savedDisplay_);
  }

  NoDisplayEnv(const NoDisplayEnv&) = delete;
  NoDisplayEnv& operator=(const NoDisplayEnv&) = delete;
  NoDisplayEnv(NoDisplayEnv&&) = delete;
  NoDisplayEnv& operator=(NoDisplayEnv&&) = delete;

private:
  static void saveAndUnset(const char* name, std::string& slot)
  {
    const char* v = std::getenv(name);
    if (v != nullptr)
    {
      slot = v;
      // NOLINTNEXTLINE(misc-include-cleaner) POSIX, as above
      ::unsetenv(name);
    }
  }
  static void restore(const char* name, const std::string& slot)
  {
    if (!slot.empty())
    {
      // NOLINTNEXTLINE(misc-include-cleaner) POSIX, as above
      ::setenv(name, slot.c_str(), 1);
    }
  }

  std::string savedWayland_;
  std::string savedDisplay_;
};

//--------------------------------------------------------------------------------------------------------------
// Terminal escape framing
//--------------------------------------------------------------------------------------------------------------

/// @test
/// A copy writes the payload inside the OSC 52 framing the terminal reads.
TEST(Clipboard, EmitsOsc52WrapperAroundPayload)
{
  NoDisplayEnv guard;
  auto out = captureStdout([] { clipboard::copy("hello"); });
  ASSERT_FALSE(out.empty());
  // OSC 52 framing: ESC ] 5 2 ; c p ; <base64> BEL
  EXPECT_EQ(out.substr(0, 8), std::string("\x1b]52;cp;", 8));
  EXPECT_EQ(out.back(), '\x07');
}

/// @test
/// Copying empty text still writes the framing, with nothing between the prefix and the
/// terminator.
TEST(Clipboard, EmitsEscapeEvenForEmptyText)
{
  NoDisplayEnv guard;
  auto out = captureStdout([] { clipboard::copy(""); });
  // Expect the prefix and terminator with nothing in between.
  EXPECT_EQ(out, std::string("\x1b]52;cp;\x07", 9));
}

#endif  // !_WIN32

//--------------------------------------------------------------------------------------------------------------
// Base64, on every platform
//--------------------------------------------------------------------------------------------------------------
//
// These call the encoder directly. Reading it back out of the terminal escape needs stdout redirected,
// which is written for POSIX only, and the encoder is the one piece here that behaves identically on
// every platform.

/// @test
/// A length that is a multiple of three encodes with no padding.
TEST(Clipboard, Base64NoPaddingFor3ByteMultiple)
{
  // "abc" -> YWJj (three bytes are four characters, no '=').
  EXPECT_EQ(clipboard::base64Encode("abc"), "YWJj");
}

/// @test
/// Two bytes encode with one padding character.
TEST(Clipboard, Base64SinglePaddingFor2Bytes) { EXPECT_EQ(clipboard::base64Encode("ab"), "YWI="); }

/// @test
/// One byte encodes with two padding characters.
TEST(Clipboard, Base64DoublePaddingFor1Byte) { EXPECT_EQ(clipboard::base64Encode("a"), "YQ=="); }

/// @test
/// Empty input encodes to an empty string.
TEST(Clipboard, Base64EmptyInput) { EXPECT_EQ(clipboard::base64Encode(""), ""); }

/// @test
/// Bytes above the ASCII range encode correctly, reaching the top of the six bit range.
TEST(Clipboard, Base64HandlesNonAsciiBytes)
{
  // 0xFF 0xFE 0xFD -> //79, which exercises the top of the 6-bit range and both alphabet tails.
  EXPECT_EQ(clipboard::base64Encode(std::string("\xFF\xFE\xFD", 3)), "//79");
}

/// @test
/// A known string encodes to its known base64 form.
TEST(Clipboard, Base64KnownValueHelloWorld)
{
  EXPECT_EQ(clipboard::base64Encode("Hello, World!"), "SGVsbG8sIFdvcmxkIQ==");
}

/// @test
/// The output holds only base64 alphabet characters and padding.
TEST(Clipboard, Base64OutputUsesOnlyAlphabetChars)
{
  const auto encoded = clipboard::base64Encode("The quick brown fox jumps over the lazy dog");
  EXPECT_FALSE(encoded.empty());
  for (char c: encoded)
  {
    const bool ok =
      (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '+' || c == '/' || c == '=';
    EXPECT_TRUE(ok) << "Unexpected character in base64 payload: 0x" << std::hex << static_cast<int>(c);
  }
}

//--------------------------------------------------------------------------------------------------------------
// The worker, and what it can say afterwards
//--------------------------------------------------------------------------------------------------------------

#ifndef _WIN32

/// Sets a display so `copy` takes the local-helper path, and puts back whatever was there.
/// The mirror of NoDisplayEnv above, which exists to keep that path out of the escape tests.
class WithDisplayEnv
{
public:
  WithDisplayEnv()
  {
    const char* existing = std::getenv("DISPLAY");
    if (existing != nullptr)
    {
      saved_ = existing;
    }
    // NOLINTNEXTLINE(misc-include-cleaner) POSIX, as above
    ::setenv("DISPLAY", ":0", 1);
  }

  ~WithDisplayEnv()
  {
    if (saved_.empty())
    {
      // NOLINTNEXTLINE(misc-include-cleaner) POSIX, as above
      ::unsetenv("DISPLAY");
    }
    else
    {
      // NOLINTNEXTLINE(misc-include-cleaner) POSIX, as above
      ::setenv("DISPLAY", saved_.c_str(), 1);
    }
  }

  WithDisplayEnv(const WithDisplayEnv&) = delete;
  WithDisplayEnv& operator=(const WithDisplayEnv&) = delete;
  WithDisplayEnv(WithDisplayEnv&&) = delete;
  WithDisplayEnv& operator=(WithDisplayEnv&&) = delete;

private:
  std::string saved_;
};

/// @test
/// Reading the last failure clears it, so a reason is shown once rather than on every frame.
///
/// The assertion is deliberately weaker than what the test exercises. With a display set, the copy
/// takes the local-helper path, which on a machine with no helper installed fails through four
/// commands and on a developer's machine with xclip or pbcopy succeeds. Asserting which of those
/// happened would be a statement about the machine rather than about Sen. What holds everywhere is
/// the contract the header states: reading it clears it.
TEST(Clipboard, ReadingTheLastFailureClearsIt)
{
  {
    const WithDisplayEnv display;
    auto out = captureStdout([] { clipboard::copy("to the helper"); });
    EXPECT_FALSE(out.empty()) << "the terminal escape goes out whatever the local helper does";
    clipboard::shutdown();
  }

  const auto first = clipboard::takeFailure();
  const auto second = clipboard::takeFailure();

  if (first.has_value())
  {
    EXPECT_FALSE(first->empty()) << "a failure was reported with nothing to read";
  }
  EXPECT_FALSE(second.has_value()) << "the reason was still there after being read";
}

/// @test
/// Ensure that if there is a fail that closes the pipe before we finish writing,
/// term component catches the SIGPIPE and survive instead of crashing.
TEST(Clipboard, WritingToClosedPipeDoesNotCrash)
{
  {
    // Force a broken environment that will trigger a SIGPIPE.
    const WithDisplayEnv display;
    std::string hugePayload(1024 * 1024, 'A');

    auto out = captureStdout([&] { clipboard::copy(hugePayload); });
    clipboard::shutdown();
  }

  const auto failure = clipboard::takeFailure();
  EXPECT_TRUE(failure.has_value());
  EXPECT_FALSE(failure->empty());
}

/// @test
/// Shutting down is safe with nothing in flight and safe twice. It is called on the way out of the
/// component, which is a path that also runs when the terminal never copied anything.
TEST(Clipboard, ShutdownIsSafeWithNothingInFlight)
{
  clipboard::shutdown();
  clipboard::shutdown();

  EXPECT_FALSE(clipboard::takeFailure().has_value());
}

#endif  // _WIN32

}  // namespace
}  // namespace sen::components::term
