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
  int fd = ::mkstemp(tmpl.data());
  if (fd < 0)
  {
    return {};
  }
  std::fflush(stdout);
  int savedStdout = ::dup(STDOUT_FILENO);
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
      ::unsetenv(name);
    }
  }
  static void restore(const char* name, const std::string& slot)
  {
    if (!slot.empty())
    {
      ::setenv(name, slot.c_str(), 1);
    }
  }

  std::string savedWayland_;
  std::string savedDisplay_;
};

//--------------------------------------------------------------------------------------------------------------
// Terminal escape framing
//--------------------------------------------------------------------------------------------------------------

TEST(Clipboard, EmitsOsc52WrapperAroundPayload)
{
  NoDisplayEnv guard;
  auto out = captureStdout([] { clipboard::copy("hello"); });
  ASSERT_FALSE(out.empty());
  // OSC 52 framing: ESC ] 5 2 ; c p ; <base64> BEL
  EXPECT_EQ(out.substr(0, 8), std::string("\x1b]52;cp;", 8));
  EXPECT_EQ(out.back(), '\x07');
}

TEST(Clipboard, EmitsEscapeEvenForEmptyText)
{
  NoDisplayEnv guard;
  auto out = captureStdout([] { clipboard::copy(""); });
  // Expect the prefix and terminator with nothing in between.
  EXPECT_EQ(out, std::string("\x1b]52;cp;\x07", 9));
}

//--------------------------------------------------------------------------------------------------------------
// Base64 encoding
//--------------------------------------------------------------------------------------------------------------

std::string payloadFor(std::string_view text)
{
  NoDisplayEnv guard;
  auto out = captureStdout([text] { clipboard::copy(text); });
  // Strip the 8-byte prefix and trailing BEL.
  if (out.size() < 9U)
  {
    return {};
  }
  return out.substr(8U, out.size() - 9U);
}

TEST(Clipboard, Base64NoPaddingFor3ByteMultiple)
{
  // "abc" -> YWJj (9 * 4 / 3 = 4 chars, no '=').
  EXPECT_EQ(payloadFor("abc"), "YWJj");
}

TEST(Clipboard, Base64SinglePaddingFor2Bytes)
{
  // "ab" -> YWI= (one '=').
  EXPECT_EQ(payloadFor("ab"), "YWI=");
}

TEST(Clipboard, Base64DoublePaddingFor1Byte)
{
  // "a" -> YQ== (two '=').
  EXPECT_EQ(payloadFor("a"), "YQ==");
}

TEST(Clipboard, Base64HandlesNonAsciiBytes)
{
  // 0xFF 0xFE 0xFD -> //79 (fits the full 6-bit range).
  EXPECT_EQ(payloadFor(std::string("\xFF\xFE\xFD", 3)), "//79");
}

TEST(Clipboard, Base64KnownValueHelloWorld) { EXPECT_EQ(payloadFor("Hello, World!"), "SGVsbG8sIFdvcmxkIQ=="); }

TEST(Clipboard, Base64OutputUsesOnlyAlphabetChars)
{
  // Pick an input with every kind of triple to exercise all groups.
  const auto encoded = payloadFor("The quick brown fox jumps over the lazy dog");
  EXPECT_FALSE(encoded.empty());
  for (char c: encoded)
  {
    const bool ok =
      (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '+' || c == '/' || c == '=';
    EXPECT_TRUE(ok) << "Unexpected character in base64 payload: 0x" << std::hex << int(c);
  }
}

//--------------------------------------------------------------------------------------------------------------
// Return value semantics
//--------------------------------------------------------------------------------------------------------------

TEST(Clipboard, CopyReturnsTrueWhenNoLocalSessionAvailable)
{
  NoDisplayEnv guard;
  // No DISPLAY/WAYLAND_DISPLAY => isLocalSession() is false, so copy() reports
  // success on the strength of the terminal escape alone.
  auto ok = false;
  captureStdout([&ok] { ok = clipboard::copy("x"); });
  EXPECT_TRUE(ok);
}

#endif  // !_WIN32

}  // namespace
}  // namespace sen::components::term
