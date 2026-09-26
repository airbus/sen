// === clipboard.cpp ===================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "clipboard.h"

// std
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <string_view>
#include <thread>
#include <tuple>

#if !defined(_WIN32)
#  include <pthread.h>

#  include <csignal>
#endif

namespace sen::components::term::clipboard
{

namespace
{

/// RFC 4648 base64 encoder. Needed to wrap the payload for the terminal-side
/// escape sequence.
std::string base64Encode(std::string_view input)
{
  constexpr std::string_view alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  std::string out;
  out.reserve(((input.size() + 2U) / 3U) * 4U);

  auto byteAt = [&](std::size_t k)
  { return k < input.size() ? static_cast<std::uint32_t>(static_cast<unsigned char>(input[k])) : 0U; };

  for (std::size_t i = 0U; i < input.size(); i += 3U)
  {
    const std::uint32_t triple = (byteAt(i) << 16U) | (byteAt(i + 1U) << 8U) | byteAt(i + 2U);
    out.push_back(alphabet[(triple >> 18U) & 0x3FU]);
    out.push_back(alphabet[(triple >> 12U) & 0x3FU]);
    out.push_back(i + 1U < input.size() ? alphabet[(triple >> 6U) & 0x3FU] : '=');
    out.push_back(i + 2U < input.size() ? alphabet[triple & 0x3FU] : '=');
  }
  return out;
}

/// Emit the terminal-side "set selection" escape targeting both clipboard and
/// primary. This is the only path that works transparently over SSH (the bytes
/// reach the client terminal, not the remote host). Support varies across
/// terminals but is harmless when ignored.
void emitTerminalEscape(std::string_view text)
{
  const auto encoded = base64Encode(text);
  std::fputs("\x1b]52;cp;", stdout);
  std::fwrite(encoded.data(), 1U, encoded.size(), stdout);
  std::fputs("\x07", stdout);
  std::fflush(stdout);
}

#if defined(_WIN32)

bool writeLocal(std::string_view text)
{
  // clip.exe reads from stdin and stores the payload in the single Windows
  // clipboard (no X11-style PRIMARY selection exists on Windows).
  FILE* pipe = _popen("clip", "wb");
  if (pipe == nullptr)
  {
    return false;
  }
  std::fwrite(text.data(), 1U, text.size(), pipe);
  return _pclose(pipe) == 0;
}

#else

/// Pipe `text` through a shell command. Returns true if the command exited
/// successfully. Assumes SIGPIPE is masked by the caller.
bool pipeToCommand(std::string_view text, const char* cmd)
{
  FILE* pipe = popen(cmd, "w");
  if (pipe == nullptr)
  {
    return false;
  }
  std::fwrite(text.data(), 1U, text.size(), pipe);
  return pclose(pipe) == 0;
}

#  if !defined(_WIN32)
/// Blocks SIGPIPE for the calling thread only, for as long as it is alive. std::signal() was used
/// here, which changes the disposition process-wide -- every other component's SIGPIPE behaviour
/// changed for the duration, and two concurrent callers raced on the restore.
class ThreadSigpipeBlock
{
public:
  ThreadSigpipeBlock()
  {
    sigset_t block;
    sigemptyset(&block);
    sigaddset(&block, SIGPIPE);
    blocked_ = ::pthread_sigmask(SIG_BLOCK, &block, &previous_) == 0;
  }

  ~ThreadSigpipeBlock()
  {
    if (blocked_)
    {
      ::pthread_sigmask(SIG_SETMASK, &previous_, nullptr);
    }
  }

  ThreadSigpipeBlock(const ThreadSigpipeBlock&) = delete;
  ThreadSigpipeBlock& operator=(const ThreadSigpipeBlock&) = delete;
  ThreadSigpipeBlock(ThreadSigpipeBlock&&) = delete;
  ThreadSigpipeBlock& operator=(ThreadSigpipeBlock&&) = delete;

private:
  sigset_t previous_ {};
  bool blocked_ = false;
};
#  endif

#  if defined(__APPLE__)

bool writeLocal(std::string_view text)
{
  ThreadSigpipeBlock sigpipeBlocked;
  return pipeToCommand(text, "pbcopy >/dev/null 2>&1");
}

#  else

bool writeLocal(std::string_view text)
{
  // Write to both CLIPBOARD (for Ctrl+V) and PRIMARY (for middle-click) so the
  // usual select-to-copy workflow matches what users expect from a desktop
  // terminal. A missing helper on one selection doesn't invalidate the other.
  ThreadSigpipeBlock sigpipeBlocked;
  bool ok = false;

  if (std::getenv("WAYLAND_DISPLAY") != nullptr)
  {
    const bool clip = pipeToCommand(text, "wl-copy >/dev/null 2>&1");
    const bool prim = pipeToCommand(text, "wl-copy --primary >/dev/null 2>&1");
    ok = clip || prim;
  }
  if (!ok && std::getenv("DISPLAY") != nullptr)
  {
    const bool clip = pipeToCommand(text, "xclip -selection clipboard -in >/dev/null 2>&1") ||
                      pipeToCommand(text, "xsel --clipboard --input >/dev/null 2>&1");
    const bool prim = pipeToCommand(text, "xclip -selection primary -in >/dev/null 2>&1") ||
                      pipeToCommand(text, "xsel --primary --input >/dev/null 2>&1");
    ok = clip || prim;
  }

  return ok;
}

#  endif  // __APPLE__
#endif    // _WIN32

bool isLocalSession()
{
#if defined(_WIN32) || defined(__APPLE__)
  return true;
#else
  return std::getenv("WAYLAND_DISPLAY") != nullptr || std::getenv("DISPLAY") != nullptr;
#endif
}

}  // namespace

void copy(std::string_view text)
{
  // OSC 52 goes out on this thread: it is one write to the tty and it is the only path that
  // survives an ssh session.
  emitTerminalEscape(text);

  if (!isLocalSession())
  {
    return;
  }

  // The helpers fork. Detach so the cycle is not held for the length of four process spawns; a
  // clipboard write nobody waits on has no ordering requirement and no result to report.
  std::thread([owned = std::string(text)]() { std::ignore = writeLocal(owned); }).detach();
}

}  // namespace sen::components::term::clipboard
