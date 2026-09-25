// === crash_reporter.h ================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_LIBS_KERNEL_SRC_CRASH_REPORTER_H
#define SEN_LIBS_KERNEL_SRC_CRASH_REPORTER_H

// sen
#include "sen/core/base/compiler_macros.h"
#include "sen/core/base/span.h"
#include "sen/kernel/kernel_config.h"

// generated code
#include "stl/sen/kernel/kernel_objects.stl.h"

// spdlog
#include <spdlog/logger.h>

// cpptrace
#include "cpptrace/cpptrace.hpp"

// std
#include <atomic>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>

namespace sen::kernel::impl
{

/// Singleton for dealing with process crashes.
class CrashReporter
{
  SEN_NOCOPY_NOMOVE(CrashReporter)

public:
  /// Get access to the single instance of this class.
  static CrashReporter& get();

public:
  /// Install crash reporting into the current process: a terminate handler for what a dump cannot
  /// show, and Crashpad for the dump itself. `reportDirectory` is where dumps are written; empty
  /// means the system temporary directory.
  /// Returns whether the out-of-process handler is running.
  bool install(spdlog::logger* kernelLogger, const std::filesystem::path& reportDirectory = {});

  /// Register the kernel information into the crash reporter.
  void registerKernel(const KernelConfig& config);

  /// The packages and components this kernel loaded, each with the build it was made by.
  void setComponents(Span<const ComponentInfo> loaded, Span<const ComponentInfo> imported);

  /// Starts mirroring what the loggers emit into a buffer the handler can read. Called once the
  /// logging configuration has been applied, since applying it replaces the loggers' sinks.
  void captureLogs();

  /// Adds the ring to one logger, for loggers made after captureLogs() has run.
  void captureLogsFrom(const std::shared_ptr<spdlog::logger>& logger);

  /// Gives the calling thread its own signal stack, so that a stack overflow on it still produces
  /// a dump: the fault handler needs somewhere to run that is not the stack that overflowed.
  /// Crashpad does this only for the thread that armed, so every thread Sen starts calls it.
  /// Does nothing if crash reporting was never armed.
  static void prepareCurrentThread();

  /// What the process was doing when it died, for a dump that can show where the code was but not
  /// what the kernel was in the middle of.
  void setPhase(const char* phase);

  /// Runs this process as the crash handler when it was started to be one. Returns the exit code
  /// the process should leave with, or nothing when this is an ordinary run.
  [[nodiscard]] static std::optional<int> runHandler(int argc, char* argv[]);

  /// Define a transport version for an eventual crash report.
  void setTransportVersion(uint32_t transportVersion);

private:
  CrashReporter() noexcept = default;
  ~CrashReporter() = default;

private:
  void addLog(std::chrono::system_clock::time_point time, std::string_view msg);

  /// True only while collectLogs() is replaying spdlog's backlog.
  [[nodiscard]] bool replayingBacklog() const noexcept;

  void collectLogs();
  void collectStackTrace(const cpptrace::stacktrace& trace);
  void collectExceptionData();
  void collectProcessData();
  void collectEnvironmentVars();
  void collectErrorData();
  void writeReport();

  /// The lines the report collected, framed on stderr for whoever is watching the process.
  void printErrorMessages() const;
  void doTermination();
  void configureKernelLogger(spdlog::logger* kernelLogger);
  UncaughtException makeExceptionData(std::string_view message, ExceptionKind kind);
  [[nodiscard]] std::filesystem::path computeCrashReportFile() const;
  [[noreturn]] static void terminationHandler();

  void startCrashpad(const std::filesystem::path& reportDirectory);

private:
  friend class LogRingSink;
  ErrorReport report_;

  /// Borrowed in install(), so a problem found while arming can be reported.
  spdlog::logger* kernelLogger_ {nullptr};

  /// What the configuration asked for, kept because it is known before anything is loaded and the
  /// inventory that replaces it is only known afterwards. A crash while loading gets this much.
  std::string configuredComponents_;

  /// Set only once the handler is up, so that a caller who fixes what stopped it can arm again.
  /// Atomic because every thread Sen starts reads it through prepareCurrentThread().
  std::atomic<bool> crashpadStarted_ {false};

  /// Where the handler was pointed when it started. The report goes here too, because a report and
  /// the dump of the same crash in different directories is worse than either being the wrong one.
  std::filesystem::path reportDirectory_;

  /// Whether any kernel has registered, so that a second one can say it is taking the record over.
  bool kernelRegistered_ {false};

  /// set_terminate cannot fail, so this is set unconditionally. Any handler already in place is
  /// dropped: install() neither restores it nor chains to it.
  bool terminateHandlerInstalled_ {false};

  /// Whether runHandler() was ever given main()'s arguments. Calling it is the only proof that
  /// this process will recognise being started as the handler, which Windows needs before it can
  /// start one.
  static bool handlerDispatchWired;
  /// Written by whichever thread logged the line and read as the process ends, so both sides
  /// take the lock. The threads do not stop when the terminate handler starts.
  std::multimap<std::chrono::system_clock::time_point, std::string> logs_;
  std::mutex logsMutex_;
  std::atomic<bool> replayingBacklog_ {false};

  /// Whether captureLogs() has run. Atomic because component threads read it through
  /// captureLogsFrom().
  std::atomic<bool> logsCaptured_ {false};
};

}  // namespace sen::kernel::impl

#endif  // SEN_LIBS_KERNEL_SRC_CRASH_REPORTER_H
