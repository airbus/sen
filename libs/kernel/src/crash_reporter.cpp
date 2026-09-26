// === crash_reporter.cpp ==============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "crash_reporter.h"

// sen
#include "sen/core/base/compiler_macros.h"
#include "sen/core/base/detail/assert_impl.h"
#include "sen/core/base/span.h"
#include "sen/core/base/uuid.h"
#include "sen/core/base/version.h"
#include "sen/core/io/util.h"
#include "sen/core/meta/var.h"
#include "sen/kernel/kernel.h"
#include "sen/kernel/kernel_config.h"
#include "sen/kernel/transport.h"
#include "sen/kernel/util.h"

// spdlog
#include <spdlog/common.h>
#include <spdlog/details/log_msg.h>
#include <spdlog/details/registry.h>
#include <spdlog/logger.h>
#include <spdlog/sinks/base_sink.h>

// generated code
#include "stl/sen/kernel/basic_types.stl.h"
#include "stl/sen/kernel/kernel_objects.stl.h"

// cpptrace
#include "cpptrace/basic.hpp"
#include "cpptrace/exceptions.hpp"

// crashpad
// Before the crashpad headers rather than with the windows block below: client/crashpad_client.h
// includes <windows.h> itself, and once it has, min and max are macros that break std::max.
#ifdef _WIN32
#  define NOMINMAX 1
#endif
#include "client/annotation.h"
#include "client/crashpad_client.h"
#include "handler/handler_main.h"
#include "util/file/file_io.h"
#ifdef __linux__
#  include "util/linux/socket.h"
#endif

// linux
#ifdef __linux__
#  include <climits>
#endif

// posix
#ifndef _WIN32
#  include <fcntl.h>
// <csignal> declares only the C subset. sigset_t, sigemptyset, sigprocmask and kill are POSIX.
// NOLINTNEXTLINE(hicpp-deprecated-headers,modernize-deprecated-headers)
#  include <signal.h>
#  include <sys/prctl.h>
#  include <sys/syscall.h>
#  include <sys/types.h>
#  include <sys/wait.h>
#  include <unistd.h>
#endif

// windows
#ifdef _WIN32
#  include <stdio.h>
#  include <tchar.h>
#  include <windows.h>
#endif

// std
#include <algorithm>
#include <array>
#include <atomic>
#include <cctype>
#include <cerrno>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <ios>
#include <iterator>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <ostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

// environ, which the report reads to collect the environment
#if defined(__APPLE__) || defined(__linux__)
#  include <unistd.h>
#endif

namespace sen::kernel::impl
{

//--------------------------------------------------------------------------------------------------------------
// Constants
//--------------------------------------------------------------------------------------------------------------

namespace
{

// What the kernel knows and a dump does not carry. The handler process reads them out of the
// crashing process, so nothing here has to be safe to touch from a signal handler.
// NOLINTBEGIN(cert-err58-cpp): crashpad requires these at namespace scope.
crashpad::StringAnnotation<32U> phaseAnnotation {"sen.phase"};
crashpad::StringAnnotation<32U> versionAnnotation {"sen.version"};
crashpad::StringAnnotation<256U> buildAnnotation {"sen.build"};
crashpad::StringAnnotation<256U> sourceAnnotation {"sen.source"};
crashpad::StringAnnotation<128U> applicationAnnotation {"sen.application"};
crashpad::StringAnnotation<(5U * 4096U) - 1U> componentsAnnotation {"sen.components"};
// NOLINTEND(cert-err58-cpp)

/// The compiler, mode, word size and build time, as one line.
std::string describeBuild(const BuildInfo& buildInfo)
{
  std::string description {buildInfo.compiler};
  description.append(buildInfo.debugMode ? ", debug" : ", release");
  description.append(buildInfo.wordSize == WordSize::bits64 ? ", 64 bit" : ", 32 bit");

  // A build with no git to ask has no timestamp either. Leave the label off rather than write one
  // with nothing after it, which reads as a value that failed to arrive.
  if (!buildInfo.buildTime.empty())
  {
    description.append(", built ").append(buildInfo.buildTime);
  }
  return description;
}

std::string describeSource(const BuildInfo& buildInfo)
{
  const char* status = "unknown";
  switch (buildInfo.gitStatus)
  {
    case GitStatus::clean:
      status = "clean";
      break;
    case GitStatus::modified:
      status = "modified";
      break;
    case GitStatus::unknown:
      break;
  }

  if (buildInfo.gitRef.empty() && buildInfo.gitHash.empty())
  {
    return "not built from a git checkout";
  }

  std::string description {buildInfo.gitRef};
  if (!description.empty() && !buildInfo.gitHash.empty())
  {
    description.append(" ");
  }
  description.append(buildInfo.gitHash).append(", ").append(status);
  return description;
}

// StartHandler arms the out-of-process handler and returns, but Crashpad keeps using this client
// afterwards, so it must live as long as the process.
crashpad::CrashpadClient& crashpadClient()
{
  static crashpad::CrashpadClient client;
  return client;
}

#ifdef _WIN32
/// Tells a Sen executable it was started to be the crash handler. Only Windows starts one this
/// way; see sen::kernel::crash::runHandlerIfRequested.
constexpr const char* handlerArgument = "--sen-crashpad-handler";

/// Where this process's own executable is. Windows starts the handler by running it again.
std::filesystem::path runningExecutable()
{
  std::wstring buffer(MAX_PATH, L'\0');
  for (;;)
  {
    const auto length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
    if (length == 0U)
    {
      return {};
    }
    if (length < buffer.size())
    {
      buffer.resize(length);
      return std::filesystem::path(buffer);
    }
    // The name did not fit, so the call truncated it and there is no way to ask how much it needed.
    if (buffer.size() >= 65536U)
    {
      return {};
    }
    buffer.resize(buffer.size() * 2U);
  }
}
#endif

#ifdef __linux__
/// Closes what the handler inherited. It is a fork rather than a fresh program, so it holds every
/// file and socket the host had open, and those would stay open past the host's death, long
/// enough for a restart to fail to take back a port that should be free.
void closeInheritedDescriptors(int keep)
{
#  if defined(SYS_close_range)
  // An empty range is rejected rather than ignored, so when the descriptor to keep is the first one
  // there is nothing below it to close.
  // NOLINTNEXTLINE(hicpp-vararg): syscall is variadic and has no other spelling
  const bool closedBelow = keep <= 3 || ::syscall(SYS_close_range, 3U, static_cast<unsigned int>(keep) - 1U, 0U) == 0;
  // Never below 3, or a socket that landed on a low descriptor takes stdout and stderr with it.
  // The sweep further down starts at 3 for the same reason.
  const auto firstAbove = static_cast<unsigned int>(std::max(keep + 1, 3));
  // NOLINTNEXTLINE(hicpp-vararg): as above
  const bool closedAbove = ::syscall(SYS_close_range, firstAbove, ~0U, 0U) == 0;
  if (closedBelow && closedAbove)
  {
    return;
  }
#  endif
  // sysconf can decline to answer; sweep a plausible range when it does.
  const auto reported = ::sysconf(_SC_OPEN_MAX);
  const std::int64_t limit = reported > 0 ? reported : 1024;
  for (std::int64_t descriptor = 3; descriptor < limit; ++descriptor)
  {
    if (descriptor != keep)
    {
      ::close(static_cast<int>(descriptor));
    }
  }
}

/// How many threads this process is running, which decides whether forking a handler is safe.
int runningThreadCount()
{
  std::ifstream status {"/proc/self/status"};
  std::string line;
  constexpr std::string_view key {"Threads:"};
  while (std::getline(status, line))
  {
    if (std::string_view {line}.substr(0U, key.size()) == key)
    {
      int count = -1;
      std::istringstream rest {line.substr(key.size())};
      rest >> count;
      return rest.fail() ? -1 : count;
    }
  }
  return -1;
}

/// Runs the handler as a grandchild, so the host is never handed a child it did not create and
/// never has to reap one. Everything the child needs is built before the fork, so it allocates
/// nothing of its own before HandlerMain does.
// The suppression sits here because include-cleaner reports a symbol once, at its first use in the
// file, and this signature is now that. glibc declares pid_t in a private header it will not name.
// NOLINTNEXTLINE(misc-include-cleaner)
bool startHandlerByForking(const std::filesystem::path& database, std::string& reason, pid_t& startedPid)
{
  // Nothing downstream can tell one failure here from another: the process dies of its own signal
  // either way, and a missing dump is the only sign.
  const auto fail = [&reason](const char* step)
  {
    reason = std::string {step} + " failed: " + std::strerror(errno);
    return false;
  };

  // Both ends need SO_PASSCRED or the handler's first exchange fails with "missing credentials",
  // so the pair is made by Crashpad rather than by hand: the options then match its own protocol.
  crashpad::ScopedFileHandle clientEnd;
  crashpad::ScopedFileHandle handlerEnd;
  if (!crashpad::UnixCredentialSocket::CreateCredentialSocketpair(&clientEnd, &handlerEnd))
  {
    return fail("creating the credential socket pair");
  }

  // The handler's own id has to reach the parent: Crashpad makes it this process's ptracer with
  // prctl, and on a kernel with Yama that is the only reason it can read this process. Asking
  // Crashpad to learn the id over the socket instead is no good, because the process that forked it
  // has already gone.
  std::array<int, 2U> pidPipe {-1, -1};
  if (::pipe(pidPipe.data()) != 0)
  {
    return fail("creating the pipe for the handler's id");
  }

  // The handler treats EOF on this socket as "the host is gone". Without close-on-exec every
  // program the host later execs inherits a copy, and a handler whose host has died waits forever.
  const int clientDescriptor = clientEnd.get();
  const int handlerDescriptor = handlerEnd.get();
  // NOLINTNEXTLINE(hicpp-vararg)
  if (::fcntl(clientDescriptor, F_SETFD, FD_CLOEXEC) != 0)
  {
    const bool failed = fail("setting close-on-exec on the client socket");
    ::close(pidPipe[0]);
    ::close(pidPipe[1]);
    return failed;
  }

  std::string name {"crashpad_handler"};
  std::string databaseArgument {"--database=" + database.string()};
  std::string clientArgument {"--initial-client-fd=" + std::to_string(handlerDescriptor)};
  std::string rateArgument {"--no-rate-limit"};
  // Crashpad's client always reports itself as sharing the socket and then waits for a signal, so
  // without this the handler answers with a socket dialogue nothing reads and no dump is written.
  // It only shows where the handler needs that dialogue, which is a kernel with Yama enabled.
  std::string sharedArgument {"--shared-client-connection"};
  std::array<char*, 6U> arguments {
    name.data(), databaseArgument.data(), clientArgument.data(), rateArgument.data(), sharedArgument.data(), nullptr};

  // NOLINTNEXTLINE(misc-include-cleaner)
  const pid_t intermediate = ::fork();
  if (intermediate < 0)
  {
    const bool failed = fail("forking");
    ::close(pidPipe[0]);
    ::close(pidPipe[1]);
    return failed;
  }

  if (intermediate == 0)
  {
    // Leaving the client's end open here would stop the handler ever seeing the host die.
    ::close(clientDescriptor);
    ::close(pidPipe[0]);
    const pid_t handler = ::fork();
    if (handler == 0)
    {
      // It is a fork of Sen, so without this it appears in ps under Sen's own command line and
      // nothing distinguishes the two.
      // NOLINTNEXTLINE(hicpp-vararg,misc-include-cleaner)
      ::prctl(PR_SET_NAME, "sen-crashpad", 0L, 0L, 0L);

      // A mask is inherited across fork, and sen run blocks SIGTERM and SIGINT before arming. kill
      // and pkill would then do nothing against a stranded handler, and Crashpad's own SIGTERM
      // handling would never run.
      // NOLINTNEXTLINE(misc-include-cleaner)
      sigset_t empty {};
      ::sigemptyset(&empty);
      ::sigprocmask(SIG_SETMASK, &empty, nullptr);

      // Out of the host's process group, so closing the terminal does not take the handler down.
      ::setsid();
      closeInheritedDescriptors(handlerDescriptor);
      // HandlerMain parses with getopt_long and does not reset this itself, and a fork still holds
      // whatever the host's own argument parsing left behind. Zero, not one: glibc and musl
      // reinitialise only on zero, and one leaves the mid-option state pointing into the host argv.
      // NOLINTNEXTLINE(misc-include-cleaner)
      optind = 0;
      const int status = crashpad::HandlerMain(static_cast<int>(arguments.size()) - 1, arguments.data(), nullptr);
      // HandlerMain returns only when the handler is giving up, and this child has no logger of its
      // own. Straight to stderr, because a handler killed by a signal writes nothing at all and the
      // absence of this line is then the answer.
      const std::string note {"sen: the crash handler exited with status " + std::to_string(status) + "\n"};
      std::ignore = ::write(STDERR_FILENO, note.data(), note.size());
      ::_exit(status);
    }
    const auto written = handler < 0 ? ssize_t {-1} : ::write(pidPipe[1], &handler, sizeof(handler));
    ::_exit(written == static_cast<ssize_t>(sizeof(handler)) ? EXIT_SUCCESS : EXIT_FAILURE);
  }

  handlerEnd.reset();
  ::close(pidPipe[1]);

  // The fork above raises SIGCHLD by itself, and a signal arriving during these two calls
  // interrupts them. Without the retries, arming fails for a reason unrelated to Crashpad.
  pid_t handlerPid = -1;
  ssize_t readBytes = -1;
  do
  {
    readBytes = ::read(pidPipe[0], &handlerPid, sizeof(handlerPid));
  } while (readBytes < 0 && errno == EINTR);
  ::close(pidPipe[0]);

  int status = 0;
  pid_t reaped = -1;
  do
  {
    reaped = ::waitpid(intermediate, &status, 0);
  } while (reaped < 0 && errno == EINTR);
  if (reaped < 0)
  {
    return fail("waiting for the intermediate process");
  }
  if (readBytes != static_cast<ssize_t>(sizeof(handlerPid)))
  {
    reason = "the handler's id never arrived";
    return false;
  }
  // NOLINTNEXTLINE(misc-include-cleaner)
  if (!WIFEXITED(status) || WEXITSTATUS(status) != EXIT_SUCCESS)
  {
    reason = "the intermediate process could not fork the handler";
    return false;
  }

  if (!crashpadClient().SetHandlerSocket(std::move(clientEnd), handlerPid))
  {
    // Crashpad has taken the socket into a singleton it never releases, so the handler will not
    // see the host go away and would sit there for the life of this process doing nothing.
    ::kill(handlerPid, SIGKILL);
    reason = "crashpad would not take the handler socket";
    return false;
  }

  // The handler is a grandchild, reparented away, so nothing here ever reaps it or hears it fail.
  // A HandlerMain that rejects its arguments or cannot open the database is gone by now, and
  // without this arm() reports success for a handler that is not running. It catches only a death
  // this quick; one that comes later still leaves no dump and no warning.
  // NOLINTNEXTLINE(misc-include-cleaner)
  if (::kill(handlerPid, 0) != 0 && errno == ESRCH)
  {
    reason = "the handler exited immediately after starting";
    return false;
  }

  startedPid = handlerPid;
  return true;
}
#endif

}  // namespace

//--------------------------------------------------------------------------------------------------------------
// LogRing
//--------------------------------------------------------------------------------------------------------------

namespace
{

/// Cleared once the statics the report reads start going away. Never destroyed itself: it is
/// constant-initialised at namespace scope rather than on first use.
std::atomic<bool> reportableStatics {true};

/// Constructed at the end of install(), so it is destroyed before spdlog's registry and everything
/// else the report reads. A std::terminate during static destruction then aborts without them.
struct StaticLifetimeGuard
{
  StaticLifetimeGuard() = default;
  ~StaticLifetimeGuard() { reportableStatics.store(false, std::memory_order_release); }
  SEN_NOCOPY_NOMOVE(StaticLifetimeGuard)
};

/// Outside LogRing because a base class is constructed before its members: passing a member's
/// address to Annotation would take that address before the member exists.
std::array<char, 16U * 1024U> logBuffer {};

/// The recent log lines, where the handler can read them from outside the dying process. Not
/// spdlog's own backlog: reaching that means running code and taking locks, and after a fault
/// nothing of Sen's runs.
class LogRing: public crashpad::Annotation
{
public:
  SEN_NOCOPY_NOMOVE(LogRing)

  LogRing(): Annotation(Type::kString, "sen.logs", logBuffer.data(), ConcurrentAccessGuardMode::kUnguarded) {}
  ~LogRing() = default;

  /// Appends one formatted line, dropping whole lines off the front to make room. A line longer
  /// than the whole buffer keeps its tail, which is where the message is. Dropping is line-based,
  /// so a formatter built without an end-of-line would leave the buffer holding a single line.
  void append(std::string_view line)
  {
    if (line.size() >= logBuffer.size())
    {
      line = line.substr(line.size() - logBuffer.size() + 1U);
    }

    if (used_ + line.size() > logBuffer.size())
    {
      dropOldest(line.size());
    }

    std::memcpy(logBuffer.data() + used_, line.data(), line.size());
    used_ += line.size();
    SetSize(static_cast<ValueSizeType>(used_));
  }

private:
  /// Removes whole lines from the front until `needed` bytes are free, so the value never starts
  /// mid-message.
  void dropOldest(std::size_t needed)
  {
    std::size_t cut = 0U;
    while (logBuffer.size() - (used_ - cut) < needed && cut < used_)
    {
      const auto* newline = static_cast<const char*>(std::memchr(logBuffer.data() + cut, '\n', used_ - cut));
      cut = newline == nullptr ? used_ : static_cast<std::size_t>(newline - logBuffer.data()) + 1U;
    }

    // The size is published after the move, which leaves a window where it still describes the
    // pre-shift buffer and the newest lines appear twice. Publishing it first closes that window
    // and opens a worse one, where the reader gets the oldest bytes and loses the newest.
    used_ -= cut;
    std::memmove(logBuffer.data(), logBuffer.data() + cut, used_);
    SetSize(static_cast<ValueSizeType>(used_));
  }

  std::size_t used_ = 0U;
};

LogRing& logRing()
{
  static LogRing ring;
  return ring;
}

}  // namespace

/// Mirrors every line a logger emits into the ring, and into the report instead while
/// collectLogs() is replaying the backlog. The loggers hold it for the life of the process, so
/// nothing removes it while the process is ending.
///
/// Declared outside the anonymous namespace: CrashReporter's friend declaration names this class,
/// and a class in the anonymous namespace would be a different one.
class LogRingSink: public spdlog::sinks::base_sink<std::mutex>
{
protected:
  void sink_it_(const spdlog::details::log_msg& msg) override
  {
    spdlog::memory_buf_t formatted;
    base_sink<std::mutex>::formatter_->format(msg, formatted);
    const std::string_view text(formatted.data(), formatted.size());

    // spdlog's backlog holds lines the logger's level kept out of the sinks, so the report wants
    // the replay. The ring has no use for it: 2048 replayed lines would evict the ones that matter.
    if (CrashReporter::get().replayingBacklog())
    {
      // The formatter is shared with every logger and any of them can set a pattern, so the line
      // ending is neither guaranteed to be "\n" nor to be one character.
      std::string_view line {text};
      while (!line.empty() && (line.back() == '\n' || line.back() == '\r'))
      {
        line.remove_suffix(1U);
      }
      if (!line.empty())
      {
        CrashReporter::get().addLog(msg.time, line);
      }
      return;
    }

    logRing().append(text);
  }

  void flush_() override {}
};

namespace
{

/// One sink for the whole process, so a logger made later can be given the same one. Built inside
/// the function because a component thread can ask for it through captureLogsFrom() while another
/// is still arming, and static initialisation is the only thing serialising those two.
const std::shared_ptr<spdlog::sinks::sink>& logRingSink()
{
  static const std::shared_ptr<spdlog::sinks::sink> sink = []
  {
    auto created = std::make_shared<LogRingSink>();
    created->set_level(spdlog::level::trace);
    return created;
  }();

  return sink;
}

}  // namespace

//--------------------------------------------------------------------------------------------------------------
// CrashReporter
//--------------------------------------------------------------------------------------------------------------

CrashReporter& CrashReporter::get()
{
  static CrashReporter instance;
  return instance;
}

bool CrashReporter::install(spdlog::logger* kernelLogger, const std::filesystem::path& reportDirectory)
{
  const auto& buildInfo = Kernel::getBuildInfo();

  report_.senData.version = SEN_VERSION_STRING;

  report_.senData.compiler = buildInfo.compiler;
  report_.senData.debugMode = buildInfo.debugMode;
  report_.senData.buildTime = buildInfo.buildTime;
  report_.senData.wordSize = buildInfo.wordSize;
  report_.senData.gitRef = buildInfo.gitRef;
  report_.senData.gitHash = buildInfo.gitHash;
  report_.senData.gitStatus = buildInfo.gitStatus;

  // The same identity the report carries, put where a dump can be read without one. A module list
  // names the files that were loaded; it does not say which Sen they are.
  versionAnnotation.Set(SEN_VERSION_STRING);
  buildAnnotation.Set(describeBuild(buildInfo).c_str());
  sourceAnnotation.Set(describeSource(buildInfo).c_str());

  report_.processData.processInfo = getOwnProcessInfo("");

  kernelLogger_ = kernelLogger;
  configureKernelLogger(kernelLogger);

  // A minidump of an uncaught exception is taken after unwinding, so the frames between the throw
  // and here are gone by the time it is written. The terminate handler records them.
  //
  // Installed once only. Everything else in install() can be repeated, which matters because
  // arming can fail and a caller may fix the cause and try again.
  if (!terminateHandlerInstalled_)
  {
    std::set_terminate(terminationHandler);
    terminateHandlerInstalled_ = true;
  }

  startCrashpad(reportDirectory);
  setPhase("arming");

  // Constructed before the guard below, so they outlive it: statics are destroyed in reverse order
  // of construction, and the guard's flag is what tells the terminate handler whether they are
  // still there. The ring is left out because it is trivially destructible and never destroyed.
  std::ignore = &logRingSink();
  std::ignore = MetaTypeTrait<ErrorReport>::meta();  // NOLINT(misc-include-cleaner)

  // The braces are required: a const object of a class with no user-provided default constructor
  // needs an initialiser in C++17. gcc and clang accept it without one, MSVC is not obliged to.
  static const StaticLifetimeGuard lifetimeGuard {};
  std::ignore = &lifetimeGuard;

  return crashpadStarted_;
}

/// Hands fault reporting to Crashpad. The signal handler that catches a fault is Crashpad's, and
/// all it does is ask another process for a dump; the unwinding and the writing happen outside the
/// process that is dying.
void CrashReporter::startCrashpad(const std::filesystem::path& reportDirectory)
{
  if (crashpadStarted_)
  {
    // The handler was given its directory when it started and cannot be told another one, so a
    // second directory would move the report and leave the dumps behind.
    if (!reportDirectory.empty() && reportDirectory != reportDirectory_ && kernelLogger_ != nullptr)
    {
      kernelLogger_->warn("crash reporting is already armed: dumps and reports stay under {}",
                          reportDirectory_.string());
    }
    return;
  }

  std::error_code ec;
  const auto database = reportDirectory.empty() ? std::filesystem::temp_directory_path(ec) : reportDirectory;
  reportDirectory_ = database;
  if (ec)
  {
    if (kernelLogger_ != nullptr)
    {
      kernelLogger_->warn("crash dumps are off: no directory to write them to ({}); set crashReportDir", ec.message());
    }
    return;
  }
  std::filesystem::create_directories(database, ec);
  if (ec)
  {
    if (kernelLogger_ != nullptr)
    {
      kernelLogger_->warn("crash dumps are off: {} could not be created ({})", database.string(), ec.message());
    }
    return;
  }

  std::string reason;

#ifdef _WIN32
  if (!handlerDispatchWired)
  {
    // Starting it here would run this executable again as an ordinary program instead of a
    // handler, which for a host that is not cli_run means a second copy of itself.
    if (kernelLogger_ != nullptr)
    {
      kernelLogger_->warn(
        "crash dumps are off: sen::kernel::crash::runHandlerIfRequested was not called from main, "
        "so the handler cannot be started. An uncaught exception is still reported.");
    }
    return;
  }

  const auto self = runningExecutable();
  // No upload: the dump is for whoever runs the process, and Sen sends nothing anywhere.
  const bool started = !self.empty() && crashpadClient().StartHandler(base::FilePath {self.native()},
                                                                      base::FilePath {database.native()},
                                                                      base::FilePath {database.native()},
                                                                      "",
                                                                      "",
                                                                      {},
                                                                      {handlerArgument},
                                                                      false,
                                                                      false);
  reason = self.empty() ? "this executable's own path is not known" : "crashpad would not start the handler";
#elif defined(__linux__)
  // Warned before the fork: the failure it describes is a handler that never answers, and nothing
  // later reports that.
  const auto threads = runningThreadCount();
  if (threads > 1 && kernelLogger_ != nullptr)
  {
    kernelLogger_->warn(
      "crash reporting is arming in a process that already runs {} threads. The "
      "handler is a fork of it and a fork keeps the locks the other threads hold, "
      "so it may never start. Arm before the threads exist.",
      threads);
  }
  pid_t handlerPid = -1;
  const bool started = startHandlerByForking(database, reason, handlerPid);
  // Crashpad tags its own messages with the pid that wrote them, and the two processes report
  // different faults. Without this there is no way to tell which of them a message came from.
  if (started && kernelLogger_ != nullptr)
  {
    kernelLogger_->debug("the crash handler runs as process {}", handlerPid);
  }
#else
  // Every platform Sen builds for is covered above. Anywhere else reports an uncaught exception
  // and nothing else.
  const bool started = false;
  reason = "this platform has no handler";
#endif

  if (!started)
  {
    if (kernelLogger_ != nullptr)
    {
      kernelLogger_->warn("the crash handler could not be started, so a fault will leave no dump: {}", reason);
    }
    return;
  }

  crashpadStarted_ = true;

  // The process that writes the dump cannot say where it put it, because by then this one is gone.
  if (kernelLogger_ != nullptr)
  {
    kernelLogger_->debug("a fault will be dumped under {}", (database / "pending").string());
  }
}

bool CrashReporter::handlerDispatchWired = false;

std::optional<int> CrashReporter::runHandler(int argc, char* argv[])
{
  // Before the argument check: Windows needs to know only that main called this, whatever the
  // arguments were.
  handlerDispatchWired = true;

#ifndef _WIN32
  // Only Windows starts the handler this way, so answering the argument anywhere else would let a
  // caller's own arguments turn their program into one.
  std::ignore = argc;
  std::ignore = argv;
  return std::nullopt;
#else
  if (argc < 2 || argv == nullptr)
  {
    return std::nullopt;
  }

  // Crashpad decides where its own arguments go, and that ordering cannot be tested from here,
  // so ours is looked for at any position. It is then removed: HandlerMain does not know it.
  std::vector<char*> arguments;
  arguments.reserve(static_cast<std::size_t>(argc) + 1U);
  bool startedToBeTheHandler = false;
  for (int index = 0; index < argc; ++index)
  {
    if (index > 0 && argv[index] != nullptr && std::string_view {argv[index]} == handlerArgument)
    {
      startedToBeTheHandler = true;
      continue;
    }
    arguments.push_back(argv[index]);
  }

  if (!startedToBeTheHandler)
  {
    return std::nullopt;
  }

  arguments.push_back(nullptr);
  return crashpad::HandlerMain(static_cast<int>(arguments.size()) - 1, arguments.data(), nullptr);
#endif
}

void CrashReporter::captureLogs()
{
  // Set before the walk. A logger registered while the walk runs would otherwise be missed either
  // way: captureLogsFrom() would read false and skip it, and the walk would already be past it.
  // Setting it first costs at most a repeat attempt, which the find below absorbs.
  logsCaptured_.store(true, std::memory_order_release);

  // Appended rather than replacing what is there: a crash report is not a reason to stop a process
  // logging where it was told to. The find keeps a second kernel from giving a surviving logger
  // the ring twice, which would duplicate every line and halve what the ring holds.
  //
  // The push is not safe against a logger another thread is emitting through: spdlog walks that
  // vector without a lock and offers nothing to hold, so arming before the threads start is the
  // only protection. A second kernel configuring its logging alongside the first is not covered.
  spdlog::details::registry::instance().apply_all(
    [](const auto& logger)
    {
      auto& sinks = logger->sinks();
      if (std::find(sinks.begin(), sinks.end(), logRingSink()) == sinks.end())
      {
        sinks.push_back(logRingSink());
      }
    });
}

void CrashReporter::captureLogsFrom(const std::shared_ptr<spdlog::logger>& logger)
{
  // A logger made after captureLogs() ran would otherwise never reach the ring, and components
  // make theirs while they load.
  if (logsCaptured_.load(std::memory_order_acquire) && logger)
  {
    auto& sinks = logger->sinks();
    if (std::find(sinks.begin(), sinks.end(), logRingSink()) == sinks.end())
    {
      sinks.push_back(logRingSink());
    }
  }
}

void CrashReporter::prepareCurrentThread()
{
  // Crashpad declares this on Linux and Android only; elsewhere there is nothing to do.
#ifdef __linux__
  if (get().crashpadStarted_)
  {
    std::ignore = crashpad::CrashpadClient::InitializeSignalStackForThread();
  }
#endif
}

void CrashReporter::setPhase(const char* phase)
{
  // Set() copies the string with strncpy, which crashes on a null pointer.
  if (phase != nullptr)
  {
    phaseAnnotation.Set(phase);
  }
}

void CrashReporter::setComponents(Span<const ComponentInfo> loaded, Span<const ComponentInfo> imported)
{
  std::string inventory {configuredComponents_};
  const auto append = [&inventory](const ComponentInfo& info, const char* kind)
  {
    inventory.append(info.name)
      .append(" ")
      .append(info.buildInfo.version.empty() ? "?" : info.buildInfo.version)
      .append(" ")
      .append(info.buildInfo.compiler.empty() ? "?" : info.buildInfo.compiler)
      .append(info.buildInfo.debugMode ? " debug" : " release");

    if (!info.buildInfo.gitHash.empty())
    {
      inventory.append(" ").append(info.buildInfo.gitHash);
      if (info.buildInfo.gitStatus == GitStatus::modified)
      {
        inventory.append("+modified");
      }
    }

    inventory.append(" ").append(kind).append("\n");
  };

  for (const auto& info: loaded)
  {
    append(info, "loaded");
  }
  for (const auto& info: imported)
  {
    append(info, "imported");
  }

  // Without this the last entry is followed by a blank line in anything that prints the value.
  if (!inventory.empty())
  {
    inventory.pop_back();
  }
  componentsAnnotation.Set(inventory.c_str());
}

void CrashReporter::registerKernel(const KernelConfig& config)
{
  // One process has one report. A second kernel replaces the first one's name, parameters and
  // component inventory, so a crash in either is described as the last one to register.
  if (kernelRegistered_ && kernelLogger_ != nullptr)
  {
    kernelLogger_->warn("a second kernel is registering: crash reports will describe '{}' from now on",
                        config.getParams().appName);
  }
  kernelRegistered_ = true;

  report_.senData.kernelParams = config.getParams();
  report_.senData.kernelProtocol = getKernelProtocolVersion();

  // The same inventory the report carries, in the form the handler can read out of a dying
  // process. Crashpad truncates it if it does not fit, which still says what was loaded first.
  configuredComponents_.clear();
  for (const auto& elem: config.getPluginsToLoad())
  {
    configuredComponents_.append(elem.path).append(" configured\n");
  }
  for (const auto& elem: config.getComponentsToLoad())
  {
    configuredComponents_.append(elem.component.info.name).append(" configured\n");
  }
  for (const auto& elem: config.getPipelinesToLoad())
  {
    configuredComponents_.append(elem.name).append(" configured\n");
  }
  setComponents({}, {});
  applicationAnnotation.Set(config.getParams().appName.c_str());

  for (const auto& elem: config.getPluginsToLoad())
  {
    report_.senData.loadedComponents.push_back({elem.path, elem.config});
  }

  for (const auto& elem: config.getPipelinesToLoad())
  {
    BuiltComponentParams params;
    params.config = elem.config;
    params.name = elem.name;
    params.imports = elem.imports;
    params.period = elem.period;

    report_.senData.builtComponents.push_back(std::move(params));
  }
}

void CrashReporter::setTransportVersion(uint32_t transportVersion)
{
  report_.senData.transportProtocol.emplace(transportVersion);
}

void CrashReporter::doTermination()
{
  collectProcessData();
  collectErrorData();
  writeReport();
}

[[noreturn]] void CrashReporter::terminationHandler()
{
  // Anything under doTermination() that throws reaches std::terminate again, which is here, so the
  // thread-local flag stops this thread recursing. A second thread waits for the one reporting,
  // under a timeout: a wedged reporter must not turn a crash into a process that never exits.
  static std::timed_mutex reporting;
  static bool reported = false;
  thread_local bool inHandler = false;

  if (!inHandler && reportableStatics.load(std::memory_order_acquire))
  {
    inHandler = true;
    try
    {
      const std::unique_lock<std::timed_mutex> lock(reporting, std::chrono::seconds(20));
      if (lock.owns_lock() && !reported)
      {
        reported = true;
        get().doTermination();
      }
    }
    catch (...)  // NOLINT(bugprone-empty-catch)
    {
      // Even taking the lock can throw: try_lock_for is not noexcept everywhere. Anything escaping
      // here would land straight back in std::terminate.
    }
  }

  // abort(), not _Exit(): this is how the process would have ended without Sen, so the handler
  // dumps this route too and the operating system writes whatever core it was going to write.
  std::abort();
}

void CrashReporter::addLog(std::chrono::system_clock::time_point time, std::string_view msg)
{
  const std::lock_guard<std::mutex> lock(logsMutex_);
  logs_.insert({time, std::string(msg)});
}

bool CrashReporter::replayingBacklog() const noexcept { return replayingBacklog_.load(std::memory_order_acquire); }

void CrashReporter::collectLogs()
{
  // The replay must not reach the console, and replacing each logger's sinks would write to a
  // vector spdlog walks without a lock. Levels are atomic, so silencing sink by sink is safe. The
  // map holds shared pointers so a sink dropped while it is silenced is still alive to restore.
  std::map<std::shared_ptr<spdlog::sinks::sink>, spdlog::level::level_enum> silenced;

  // The replay can throw: spdlog rethrows what it cannot identify, and a caller's error handler
  // can throw too. Unwinding with the sinks off and the flag set would cost the report and every
  // line logged after it. The scope starts before the loop below, which allocates and can throw
  // as well.
  class ReplayScope
  {
  public:
    ReplayScope(CrashReporter& owner,
                const std::map<std::shared_ptr<spdlog::sinks::sink>, spdlog::level::level_enum>& saved) noexcept
      : reporter_(owner), levels_(saved)
    {
    }

    SEN_NOCOPY_NOMOVE(ReplayScope)

    ~ReplayScope()
    {
      reporter_.replayingBacklog_.store(false, std::memory_order_release);
      for (const auto& [sink, level]: levels_)
      {
        sink->set_level(level);
      }
    }

  private:
    CrashReporter& reporter_;
    const std::map<std::shared_ptr<spdlog::sinks::sink>, spdlog::level::level_enum>& levels_;
  };

  {
    const ReplayScope scope {*this, silenced};

    spdlog::details::registry::instance().apply_all(
      [&silenced](const auto& logger)
      {
        for (const auto& sink: logger->sinks())
        {
          // emplace only inserts the first time, so a sink two loggers share keeps the level it
          // had before any of this rather than the off it was just given.
          if (sink != logRingSink() && silenced.emplace(sink, sink->level()).second)
          {
            sink->set_level(spdlog::level::off);
          }
        }
      });

    replayingBacklog_.store(true, std::memory_order_release);
    spdlog::details::registry::instance().apply_all([](const auto& logger) { logger->dump_backtrace(); });
  }

  // Copied under the lock: the threads that were logging into it are still running.
  std::multimap<std::chrono::system_clock::time_point, std::string> collected;
  {
    const std::lock_guard<std::mutex> lock(logsMutex_);
    collected = logs_;
  }

  for (const auto& [time, message]: collected)
  {
    // ignore indicators added by spdlog
    if (message.find("****************** Backtrace ") != std::string::npos)
    {
      continue;
    }

    report_.processData.recentLogs.push_back(message);
  }
}

void CrashReporter::collectStackTrace(const cpptrace::stacktrace& trace)
{
  const auto lines = sen::impl::split(trace.to_string(), '\n');
  if (lines.size() > 1)
  {
    StringList bt;
    bt.reserve(lines.size());
    bt.insert(bt.begin(), std::next(lines.begin()), lines.end());
    report_.processData.stacktrace = bt;
  }
}

void CrashReporter::collectEnvironmentVars()
{
  // Split on the first '=' only: a value may contain more of them and may be empty. Splitting on
  // every one loses LS_COLORS and anything holding a query string.

#ifdef _WIN32
  LPTCH lpvEnv = GetEnvironmentStrings();
  if (lpvEnv == nullptr)
  {
    return;
  }

  LPTSTR lpszVariable = (LPTSTR)lpvEnv;
  while (*lpszVariable)
  {
    std::string environmentString(lpszVariable);

    if (const auto separator = environmentString.find('='); separator != std::string::npos)
    {
      EnvVar var;
      var.name = environmentString.substr(0U, separator);
      var.value = environmentString.substr(separator + 1U);
      report_.processData.environment.push_back(std::move(var));
    }

    lpszVariable += lstrlen(lpszVariable) + 1;
  }

  FreeEnvironmentStrings(lpvEnv);
#else
  for (auto env = environ; *env;  // NOLINT(misc-include-cleaner)
       ++env)                     // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
  {
    const std::string entry(*env);
    if (const auto separator = entry.find('='); separator != std::string::npos)
    {
      EnvVar var;
      var.name = entry.substr(0U, separator);
      var.value = entry.substr(separator + 1U);
      report_.processData.environment.push_back(std::move(var));
    }
  }
#endif
}

void CrashReporter::collectProcessData()
{
  collectEnvironmentVars();
  collectLogs();
}

UncaughtException CrashReporter::makeExceptionData(std::string_view message, ExceptionKind kind)
{
  UncaughtException exceptionData;
  exceptionData.exceptionKind = kind;
  exceptionData.message = message;

  report_.errorData.errorMessage.push_back(  // NOLINTNEXTLINE(misc-include-cleaner)
    std::string("Unhandled ").append(sen::toString(kind)).append(" exception: '").append(message).append("'"));

  return exceptionData;
}

void CrashReporter::collectExceptionData()
{
  bool hasOwnTrace = false;
  if (auto currentException = std::current_exception(); currentException != nullptr)
  {
    try
    {
      std::rethrow_exception(currentException);
    }
    catch (const cpptrace::exception& e)
    {
      report_.errorData.exceptionData = makeExceptionData(e.message(), ExceptionKind::runtime);
      collectStackTrace(e.trace());
      hasOwnTrace = true;
    }
    catch (const std::logic_error& e)
    {
      report_.errorData.exceptionData = makeExceptionData(e.what(), ExceptionKind::logic);
    }
    catch (const std::runtime_error& e)
    {
      report_.errorData.exceptionData = makeExceptionData(e.what(), ExceptionKind::runtime);
    }
    catch (const std::exception& e)
    {
      report_.errorData.exceptionData = makeExceptionData(e.what(), ExceptionKind::standard);
    }
    catch (...)
    {
      report_.errorData.exceptionData = makeExceptionData("?", ExceptionKind::unknown);
    }
  }
  else
  {
    report_.errorData.errorMessage.push_back("Terminate called without an active exception");
  }

  if (!hasOwnTrace)
  {
    collectStackTrace(cpptrace::generate_trace());
  }
}

void CrashReporter::collectErrorData()
{
  report_.errorData.time = TimeStamp(std::chrono::system_clock::now().time_since_epoch());
  if (sen::impl::lastReportedAssertionError)
  {
    report_.errorData.errorMessage.push_back("Violation: " + sen::impl::lastReportedAssertionError->str());
  }
  collectExceptionData();
}

std::filesystem::path CrashReporter::computeCrashReportFile() const
{
  std::string fileName;

  // The application names itself in the configuration and this goes into a path, so a name
  // carrying a separator or a ".." would choose the directory rather than the file.
  for (const char character: report_.senData.kernelParams.appName)
  {
    const bool alphanumeric = std::isalnum(static_cast<unsigned char>(character)) != 0;
    fileName.push_back(alphanumeric || character == '-' || character == '_' ? character : '_');
  }

  {
    std::stringstream ss;
    {
      std::time_t tt = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
      tm timeBuffer {};

#ifdef _WIN32
      gmtime_s(&timeBuffer, &tt);
#else
      std::ignore = gmtime_r(&tt, &timeBuffer);
#endif
      ss << std::put_time(&timeBuffer, "%Y_%m_%d_%H_%M_%S");
    }

    fileName.append("_").append(ss.str());
  }

  {
    const auto hash = UuidRandomGenerator()().getHash();
    std::stringstream ss;
    ss << "0x" << std::setfill('0') << std::setw(sizeof(hash) * 2U) << std::hex << hash;
    fileName.append("_").append(ss.str());
  }

  // Appended, not set with replace_extension(): that cuts at the last dot, so an application named
  // "flight.v2" would lose the timestamp and the random number with it and every crash would
  // overwrite the last one.
  fileName.append(".json");

  return fileName;
}

void CrashReporter::writeReport()
{
  if (!report_.senData.kernelParams.crashReportDisabled)
  {
    // The configuration key is the fallback for a process that registered a kernel without arming.
    std::error_code ec;
    auto reportPath = reportDirectory_;
    if (reportPath.empty())
    {
      reportPath = report_.senData.kernelParams.crashReportDir.empty()
                     ? std::filesystem::temp_directory_path(ec)
                     : std::filesystem::path(report_.senData.kernelParams.crashReportDir);
    }

    if (ec || !std::filesystem::exists(reportPath, ec))
    {
      // absolute() of a path that does not exist is that same path, so naming it here would just
      // repeat the directory that failed. Fall back to the temporary one and say which it is.
      std::error_code fallbackEc;
      const auto fallback = std::filesystem::temp_directory_path(fallbackEc);
      auto message = std::string("Cannot write the crash report to ").append(reportPath.string());

      if (fallbackEc)
      {
        message.append(", and there is no temporary directory either");
      }
      else
      {
        message.append(". Using ").append(fallback.string());
        reportPath = fallback;
      }

      report_.errorData.errorMessage.push_back(std::move(message));
    }

    // Without a directory the name below would be relative and the report would land in whatever
    // directory the process happened to be run from.
    if (reportPath.empty())
    {
      report_.errorData.errorMessage.emplace_back("No directory to write the crash report to");
      printErrorMessages();
      return;
    }

    auto fileName = reportPath / computeCrashReportFile();

    auto reportVariant = toVariant(report_);
    auto meta = MetaTypeTrait<ErrorReport>::meta();  // NOLINT(misc-include-cleaner)
    std::ignore = sen::impl::adaptVariant(*meta, reportVariant, meta, true);

    // After the write, so that a failed open does not leave the reader looking for a file that
    // was never created.
    {
      std::ofstream out;
      out.open(fileName);
      out << toJson(reportVariant) << std::endl;
      out.close();

      report_.errorData.errorMessage.push_back(
        out.fail() ? std::string("The crash report could not be written to ").append(fileName.string())
                   : std::string("Crash report written to ").append(fileName.string()));
    }
  }

  printErrorMessages();
}

void CrashReporter::printErrorMessages() const
{
  std::size_t maxLineSize = 0;
  for (const auto& line: report_.errorData.errorMessage)
  {
    maxLineSize = std::max(maxLineSize, line.size());
  }

  std::string separator(maxLineSize + 5, '-');
  separator.append("\n");

  fputs("\n", stderr);
  fputs(separator.c_str(), stderr);

  for (const auto& line: report_.errorData.errorMessage)
  {
    fputs("sen: ", stderr);
    fputs(line.c_str(), stderr);
    fputs("\n", stderr);
  }
  fputs(separator.c_str(), stderr);
  fflush(stderr);
}

void CrashReporter::configureKernelLogger(spdlog::logger* kernelLogger)
{
  if (kernelLogger == nullptr)
  {
    return;
  }

  // set a high logging level but filter at the sinks
  kernelLogger->set_level(spdlog::level::trace);
}

}  // namespace sen::kernel::impl
