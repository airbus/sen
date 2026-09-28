// === clipboard.cpp ===================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "clipboard.h"

// sen
#include "sen/kernel/component_api.h"

// std
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <utility>

#if !defined(_WIN32)
// sigset_t, sigemptyset, sigaddset, SIGPIPE and pthread_sigmask, for the SIGPIPE mask below. POSIX puts
// all of them here; <csignal> does not declare pthread_sigmask, and there is no Windows equivalent of
// any of it.
#  include <signal.h>  // NOLINT(hicpp-deprecated-headers,modernize-deprecated-headers)
#endif

namespace sen::components::term::clipboard
{

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

namespace
{

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
  // NOLINTNEXTLINE(cert-env33-c) the command is this file's own literal and the text arrives on stdin
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
  // `cmd` is one of this file's own literals and the text arrives on stdin, so nothing a user typed
  // reaches the shell. Replacing this with posix_spawn would remove the shell altogether: B-084.
  // popen and pclose are POSIX, declared in <stdio.h>; <cstdio> does not have to declare them, so
  // include-cleaner asks for a header the C++ spelling cannot provide.
  FILE* pipe = popen(cmd, "w");  // NOLINT(cert-env33-c,misc-include-cleaner)
  if (pipe == nullptr)
  {
    return false;
  }
  std::fwrite(text.data(), 1U, text.size(), pipe);
  return pclose(pipe) == 0;  // NOLINT(misc-include-cleaner)
}

/// Blocks SIGPIPE for the calling thread only, for as long as it is alive. A process-wide
/// std::signal() would change every other component's disposition, and two callers would race on
/// the restore.
class ThreadSigpipeBlock
{
public:
  ThreadSigpipeBlock()
  {
    // include-cleaner names glibc's private <bits/...> as the provider here, which neither macOS nor
    // Windows has. <signal.h> above is the portable spelling.
    // NOLINTNEXTLINE(misc-include-cleaner)
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

namespace
{

/// One worker for the whole process, with a single pending slot.
///
/// One worker rather than a detached thread per copy. A drag-select loop creates a copy per mouse release,
/// so several threads would be in flight at once, and a missing or wedged helper would leave every one of
/// them hanging for the life of the process with nothing owning them. Latest text wins, because an older
/// pending copy the user has already replaced is not worth a fork.
class Writer
{
public:
  ~Writer() { stop(); }

  Writer(const Writer&) = delete;
  Writer& operator=(const Writer&) = delete;
  Writer(Writer&&) = delete;
  Writer& operator=(Writer&&) = delete;

  Writer() = default;

  void submit(std::string text)
  {
    {
      const std::lock_guard lock(mutex_);
      if (stopping_)
      {
        return;
      }
      pending_ = std::move(text);
      hasPending_ = true;
    }
    ensureWorker();
    work_.notify_one();
  }

  [[nodiscard]] std::optional<std::string> takeFailure()
  {
    const std::lock_guard lock(mutex_);
    std::optional<std::string> out;
    out.swap(failure_);
    return out;
  }

  void stop()
  {
    std::unique_lock lock(mutex_);
    if (!worker_.joinable())
    {
      stopping_ = true;
      return;
    }
    stopping_ = true;
    work_.notify_all();

    // Bounded. A helper that has not returned in two seconds is wedged, and term's shutdown is not worth
    // holding for it, so the thread is abandoned instead. At most one, ever.
    if (idle_.wait_for(lock, std::chrono::seconds(2), [this] { return !busy_ && !hasPending_; }))
    {
      auto worker = std::move(worker_);
      lock.unlock();
      worker.join();
    }
    else
    {
      worker_.detach();
    }
  }

private:
  void ensureWorker()
  {
    const std::lock_guard lock(mutex_);
    if (worker_.joinable() || stopping_)
    {
      return;
    }
    worker_ = std::thread([this] { run(); });
  }

  void run()
  {
    // Created with std::thread rather than through the kernel, so it needs the crash handler's
    // alternate signal stack asked for explicitly.
    kernel::KernelApi::prepareCurrentThreadForCrashReports();

    std::unique_lock lock(mutex_);
    while (true)
    {
      work_.wait(lock, [this] { return hasPending_ || stopping_; });
      if (!hasPending_)
      {
        return;  // stopping, with nothing left to write
      }

      auto text = std::move(pending_);
      hasPending_ = false;
      busy_ = true;
      lock.unlock();

      const bool ok = writeLocal(text);

      lock.lock();
      busy_ = false;
      if (!ok)
      {
        // Reported rather than swallowed: without a helper on the machine only the terminal escape
        // went out, and if the terminal ignores that too the user was told a copy had happened.
        failure_ =
          "The clipboard helper failed, so the copy may only have reached the terminal. "
          "Install pbcopy, wl-copy, xclip or xsel for a local copy.";
      }
      idle_.notify_all();
    }
  }

  std::mutex mutex_;
  std::condition_variable work_;
  std::condition_variable idle_;
  std::thread worker_;
  std::string pending_;
  std::optional<std::string> failure_;
  bool hasPending_ = false;
  bool busy_ = false;
  bool stopping_ = false;
};

Writer& writer()
{
  static Writer instance;
  return instance;
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

  writer().submit(std::string(text));
}

std::optional<std::string> takeFailure() { return writer().takeFailure(); }

void shutdown() { writer().stop(); }

}  // namespace sen::components::term::clipboard
