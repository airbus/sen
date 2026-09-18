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
#include <spdlog/details/null_mutex.h>
#include <spdlog/details/registry.h>
#include <spdlog/logger.h>
#include <spdlog/pattern_formatter.h>
#include <spdlog/sinks/base_sink.h>

// generated code
#include "stl/sen/kernel/basic_types.stl.h"
#include "stl/sen/kernel/kernel_objects.stl.h"

// cpptrace
#include "cpptrace/basic.hpp"
#include "cpptrace/exceptions.hpp"

// linux
#ifdef __linux__
#  include <execinfo.h>
#  include <fcntl.h>
// sigaction, siginfo_t and stack_t are POSIX, and <csignal> declares none of them.
// NOLINTNEXTLINE(hicpp-deprecated-headers,modernize-deprecated-headers)
#  include <signal.h>
#  include <unistd.h>

#  include <climits>
#  include <cstring>
#endif

// windows
#ifdef _WIN32
#  define NOMINMAX 1
#  include <stdio.h>
#  include <tchar.h>
#  include <windows.h>
#endif

// std
#include <algorithm>
#include <array>
#include <chrono>
#include <csignal>
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
#include <memory>
#include <ostream>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <system_error>
#include <utility>

// with Apple we need to explicitly declare this as an external symbol
#if defined(__APPLE__) || defined(__linux__)
#  include <unistd.h>
#endif

namespace sen::kernel::impl
{

namespace
{
// Read by the signal handler where nothing richer is allowed, and written from anywhere. Kept out of
// the platform blocks below: the phase is worth recording on every platform, and only the handler
// that reads it is Linux-only.
volatile std::sig_atomic_t currentPhase = 0;
}  // namespace

void CrashReporter::setPhase(Phase phase) noexcept { currentPhase = static_cast<std::sig_atomic_t>(phase); }

//--------------------------------------------------------------------------------------------------------------
// Constants
//--------------------------------------------------------------------------------------------------------------

namespace
{

#ifdef __linux__
// NOLINTNEXTLINE(misc-include-cleaner)
constexpr std::array<int, 5U> signalsToHandle = {SIGSEGV, SIGABRT, SIGFPE, SIGILL, SIGBUS};

// Everything the handler touches is prepared in prepareSignalHandling(), because none of it may be
// created once a signal has arrived.
constexpr std::size_t maxRecordedFrames = 64U;
constexpr std::size_t recordPathSize = 512U;
constexpr std::size_t recordSize = 64U + (maxRecordedFrames * 24U);

// The record's path. Empty means the handler writes to stderr instead.
std::array<char, recordPathSize> crashRecordPath {};

// A stack overflow leaves no stack to run a handler on, so the handler gets its own. Sized by hand
// because SIGSTKSZ is a sysconf() call on current glibc rather than a constant.
constexpr std::size_t alternateStackSize = 64U * 1024U;
std::array<char, alternateStackSize> alternateStack {};

// A signal handler may not allocate and may not throw, which rules out the usual answers to the
// three checks below: at() throws, a std::span still indexes, and an address cannot be recorded
// without converting it. Bounds are checked by hand instead, which is what the writer below is for.
// include-cleaner is also off here: glibc splits the POSIX signal declarations across internal
// headers, so it names those rather than <signal.h>, which is the header a reader should include.
// NOLINTBEGIN(cppcoreguidelines-pro-bounds-constant-array-index,cppcoreguidelines-pro-bounds-pointer-arithmetic,cppcoreguidelines-pro-type-reinterpret-cast,misc-include-cleaner)

/// Builds the crash record in a fixed buffer, by index rather than by pointer, and never throws or
/// allocates. Truncates in silence: a record missing its tail beats a handler that gives up.
class RecordWriter
{
public:
  explicit RecordWriter(std::array<char, recordSize>& buffer) noexcept: buffer_ {buffer} {}

  void append(std::string_view text) noexcept
  {
    for (const auto character: text)
    {
      if (used_ == buffer_.size())
      {
        return;
      }
      buffer_[used_++] = character;
    }
  }

  /// No locale, no stdio: everything printf does is out of bounds here, so the few numbers in a
  /// crash record are written by hand.
  void appendHex(std::uintptr_t value) noexcept
  {
    constexpr std::string_view digits = "0123456789abcdef";
    std::array<char, 2U * sizeof(std::uintptr_t)> reversed {};
    std::size_t digitCount = 0U;

    do
    {
      reversed[digitCount++] = digits[value & 0xFU];
      value >>= 4U;
    } while (value != 0U && digitCount < reversed.size());

    append("0x");
    while (digitCount != 0U)
    {
      append(std::string_view(&reversed[--digitCount], 1U));
    }
  }

  [[nodiscard]] std::size_t used() const noexcept { return used_; }

private:
  std::array<char, recordSize>& buffer_;
  std::size_t used_ {0U};
};

/// Writes the whole buffer, retrying a short write. write() is async-signal-safe; fwrite is not.
void writeAll(int fd, const char* data, std::size_t size)
{
  while (size != 0U)
  {
    const auto written = ::write(fd, data, size);
    if (written <= 0)
    {
      return;
    }
    data += written;
    size -= static_cast<std::size_t>(written);
  }
}

/// Copies one file to another descriptor with only open, read, write and close, which a handler
/// may call. Silent on failure: a crash record missing a section beats a handler that gives up.
void copyFile(const char* path, int fd)
{
  // NOLINTNEXTLINE(hicpp-vararg,cppcoreguidelines-pro-type-vararg): open is variadic by definition
  const auto source = ::open(path, O_RDONLY | O_CLOEXEC);
  if (source < 0)
  {
    return;
  }

  std::array<char, 4096U> buffer {};
  for (auto read = ::read(source, buffer.data(), buffer.size()); read > 0;
       read = ::read(source, buffer.data(), buffer.size()))
  {
    writeAll(fd, buffer.data(), static_cast<std::size_t>(read));
  }

  std::ignore = ::close(source);
}
#endif

}  // namespace

//--------------------------------------------------------------------------------------------------------------
// RedirectionSink
//--------------------------------------------------------------------------------------------------------------

class RedirectionSink: public spdlog::sinks::base_sink<spdlog::details::null_mutex>
{
  SEN_NOCOPY_NOMOVE(RedirectionSink)

public:
  RedirectionSink() = default;
  ~RedirectionSink() override = default;

protected:
  void sink_it_(const spdlog::details::log_msg& msg) override
  {
    spdlog::memory_buf_t formatted;
    formatter_->format(msg, formatted);

    if (formatted.size() > 0)
    {
      // we skip the last character to avoid storing a newline
      CrashReporter::get().addLog(msg.time, std::string_view(formatted.data(), formatted.size() - 1));
    }
  }
  void flush_() override {}

private:
  std::unique_ptr<spdlog::formatter> formatter_ = std::make_unique<spdlog::pattern_formatter>();
};

//--------------------------------------------------------------------------------------------------------------
// CrashReporter
//--------------------------------------------------------------------------------------------------------------

CrashReporter& CrashReporter::get()
{
  static CrashReporter instance;
  return instance;
}

void CrashReporter::install(spdlog::logger* kernelLogger)
{
  const auto& buildInfo = Kernel::getBuildInfo();

  // general data
  report_.senData.version = SEN_VERSION_STRING;

  // populate the sen data
  report_.senData.compiler = buildInfo.compiler;
  report_.senData.debugMode = buildInfo.debugMode;
  report_.senData.buildTime = buildInfo.buildTime;
  report_.senData.wordSize = buildInfo.wordSize;
  report_.senData.gitRef = buildInfo.gitRef;
  report_.senData.gitHash = buildInfo.gitHash;
  report_.senData.gitStatus = buildInfo.gitStatus;

  // process information
  report_.processData.processInfo = getOwnProcessInfo("");

  // configure the logger
  configureKernelLogger(kernelLogger);

  // register termination handlers
  previousHandler_ = std::set_terminate(terminationHandler);

#ifdef __linux__
  prepareSignalHandling();

  struct sigaction action {};
  action.sa_sigaction = linuxSignalHandler;
  action.sa_flags = SA_SIGINFO | SA_ONSTACK;
  std::ignore = sigemptyset(&action.sa_mask);

  for (auto sig: signalsToHandle)
  {
    std::ignore = sigaction(sig, &action, nullptr);
  }
#endif
}

void CrashReporter::registerKernel(const KernelConfig& config)
{
  report_.senData.kernelParams = config.getParams();
  report_.senData.kernelProtocol = getKernelProtocolVersion();

#ifdef __linux__
  {
    // Built now because the handler may only open() it. Empty falls back to stderr.
    const auto directory = report_.senData.kernelParams.crashReportDir.empty()
                             ? std::filesystem::temp_directory_path()
                             : std::filesystem::path(report_.senData.kernelParams.crashReportDir);
    const auto path = (directory / (report_.senData.kernelParams.appName + ".sen-crash")).string();
    if (path.size() + 1U <= crashRecordPath.size())
    {
      std::copy(path.begin(), path.end(), crashRecordPath.begin());
      crashRecordPath.at(path.size()) = '\0';
    }

    crashContextPath_ = (directory / (report_.senData.kernelParams.appName + ".sen-context.json")).string();
  }
#endif

  writeCrashContext();

  // loaded components
  for (const auto& elem: config.getPluginsToLoad())
  {
    report_.senData.loadedComponents.push_back({elem.path, elem.config});
  }

  // built components
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

void CrashReporter::uninstall()
{
  // A clean shutdown means the context describes nothing, so it goes rather than accumulating.
  if (!crashContextPath_.empty())
  {
    std::error_code ec;
    std::ignore = std::filesystem::remove(crashContextPath_, ec);
    crashContextPath_.clear();
  }

#ifdef __linux__
  for (auto sig: signalsToHandle)
  {
    signal(sig, SIG_DFL);
  }
#endif

  std::set_terminate(previousHandler_.value_or(std::abort));
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
  get().doTermination();
  std::_Exit(EXIT_FAILURE);
}

void CrashReporter::addLog(std::chrono::system_clock::time_point time, std::string_view msg)
{
  logs_.insert({time, std::string(msg)});
}

void CrashReporter::collectLogs()
{
  // install a sink to redirect the logs
  auto customSink = std::make_shared<RedirectionSink>();
  customSink->set_level(spdlog::level::trace);

  spdlog::details::registry::instance().apply_all(
    [&](const auto& logger)
    {
      // clear all existing sinks
      logger->sinks().clear();
      logger->sinks().push_back(customSink);
    });

  // collect the logs held in the backtrace
  spdlog::details::registry::instance().apply_all([](const auto& logger) { logger->dump_backtrace(); });

  // now that we have all the logs, we just need to translate them into the report
  for (const auto& [time, message]: logs_)
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
  // capture the stack trace
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

    if (auto elems = sen::impl::split(environmentString, '='); elems.size() == 2)
    {
      EnvVar var;
      var.name = elems.at(0);
      var.value = elems.at(1);
      report_.processData.environment.push_back(std::move(var));
    }

    lpszVariable += lstrlen(lpszVariable) + 1;
  }

  FreeEnvironmentStrings(lpvEnv);
#else
  for (auto env = environ; *env;  // NOLINT(misc-include-cleaner)
       ++env)                     // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
  {
    if (auto elems = sen::impl::split(*env, '='); elems.size() == 2)
    {
      EnvVar var;
      var.name = elems.at(0);
      var.value = elems.at(1);
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

  // first, append the application name
  fileName.append(report_.senData.kernelParams.appName);

  // append the current time
  {
    // compute the current time
    std::stringstream ss;
    {
      std::time_t tt = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
      tm timeBuffer {};

#ifdef WIN32
      gmtime_s(&timeBuffer, &tt);
#else
      std::ignore = gmtime_r(&tt, &timeBuffer);
#endif
      ss << std::put_time(&timeBuffer, "%Y_%m_%d_%H_%M_%S");
    }

    fileName.append("_").append(ss.str());
  }

  // append a random number
  {
    const auto hash = UuidRandomGenerator()().getHash();
    std::stringstream ss;
    ss << "0x" << std::setfill('0') << std::setw(sizeof(hash)) << std::hex << hash;
    fileName.append("_").append(ss.str());
  }

  // set the extension
  std::filesystem::path result = fileName;
  result.replace_extension(".json");

  return result;
}

void CrashReporter::writeCrashContext()
{
  if (report_.senData.kernelParams.crashReportDisabled || crashContextPath_.empty())
  {
    return;
  }

  auto reportVariant = toVariant(report_);
  auto meta = MetaTypeTrait<ErrorReport>::meta();  // NOLINT(misc-include-cleaner)
  std::ignore = sen::impl::adaptVariant(*meta, reportVariant, meta, true);

  std::ofstream out;
  out.open(crashContextPath_);
  out << toJson(reportVariant) << std::endl;
}

void CrashReporter::writeReport()
{
  if (!report_.senData.kernelParams.crashReportDisabled)
  {
    auto reportPath = report_.senData.kernelParams.crashReportDir.empty()
                        ? std::filesystem::temp_directory_path()
                        : std::filesystem::path(report_.senData.kernelParams.crashReportDir);

    if (!std::filesystem::exists(reportPath))
    {
      report_.errorData.errorMessage.push_back(
        std::string("Invalid path for crash report. Using ").append(absolute(reportPath).string()));
    }

    // compute a file name
    auto fileName = reportPath / computeCrashReportFile();

    // finalize the report
    report_.errorData.errorMessage.push_back(
      std::string("Crash report written to ").append(absolute(fileName).string()));

    // build the report
    auto reportVariant = toVariant(report_);
    auto meta = MetaTypeTrait<ErrorReport>::meta();  // NOLINT(misc-include-cleaner)
    std::ignore = sen::impl::adaptVariant(*meta, reportVariant, meta, true);

    // write to file
    {
      std::ofstream out;
      out.open(fileName);
      out << toJson(reportVariant) << std::endl;
      out.close();
    }
  }

  std::size_t maxLineSize = 0;
  for (const auto& line: report_.errorData.errorMessage)
  {
    maxLineSize = std::max(maxLineSize, line.size());
  }

  std::string separator(maxLineSize + 5, '-');
  separator.append("\n");

  // print what gets shown to the user
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

#ifdef __linux__
void CrashReporter::linuxSignalHandler(int signum, siginfo_t* info, void* /*context*/)
{
  std::array<void*, maxRecordedFrames> frames {};
  const auto count = ::backtrace(frames.data(), static_cast<int>(frames.size()));

  std::array<char, recordSize> buffer {};
  RecordWriter record {buffer};

  record.append("sen-crash 1\nsignal ");
  record.appendHex(static_cast<std::uintptr_t>(signum));
  record.append("\nphase ");
  record.appendHex(static_cast<std::uintptr_t>(currentPhase));
  record.append("\nfault ");
  record.appendHex(reinterpret_cast<std::uintptr_t>(info != nullptr ? info->si_addr : nullptr));
  record.append("\n");

  for (std::size_t i = 0U; i < static_cast<std::size_t>(count); ++i)
  {
    record.append("frame ");
    record.appendHex(reinterpret_cast<std::uintptr_t>(frames[i]));
    record.append("\n");
  }

  auto fd = STDERR_FILENO;
  auto opened = -1;
  if (crashRecordPath[0] != '\0')
  {
    // NOLINTNEXTLINE(hicpp-vararg,cppcoreguidelines-pro-type-vararg): open is variadic by definition
    opened = ::open(crashRecordPath.data(), O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0640);
  }
  if (opened >= 0)
  {
    fd = opened;
  }

  writeAll(fd, buffer.data(), record.used());

  // Sen builds position independent, so the loader places every module somewhere different on each
  // run and the addresses above mean nothing without knowing where each one started.
  writeAll(fd, "maps\n", 5U);
  copyFile("/proc/self/maps", fd);

  if (opened >= 0)
  {
    std::ignore = ::close(opened);
  }

  // Die of the signal rather than of _Exit, so the status is truthful and the operating system can
  // still write a core file.
  std::ignore = std::signal(signum, SIG_DFL);
  std::ignore = std::raise(signum);
}

void CrashReporter::prepareSignalHandling()
{
  stack_t signalStack {};
  signalStack.ss_sp = alternateStack.data();
  signalStack.ss_size = alternateStack.size();
  signalStack.ss_flags = 0;
  std::ignore = sigaltstack(&signalStack, nullptr);

  // backtrace() may load libgcc on first use, which allocates; do it while that is allowed.
  std::array<void*, 4U> warm {};
  std::ignore = ::backtrace(warm.data(), static_cast<int>(warm.size()));
}

// NOLINTEND(cppcoreguidelines-pro-bounds-constant-array-index,cppcoreguidelines-pro-bounds-pointer-arithmetic,cppcoreguidelines-pro-type-reinterpret-cast,misc-include-cleaner)

#endif

void CrashReporter::configureKernelLogger(spdlog::logger* kernelLogger)
{
  // set a high logging level but filter at the sinks
  kernelLogger->set_level(spdlog::level::trace);
}

}  // namespace sen::kernel::impl
