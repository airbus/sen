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
#include "sen/kernel/kernel_config.h"

// generated code
#include "stl/sen/kernel/kernel_objects.stl.h"

// spdlog
#include <spdlog/logger.h>

// cpptrace
#include "cpptrace/cpptrace.hpp"

// std
#include <chrono>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <string_view>

#ifdef __linux__
#  include <csignal>
#endif

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
  /// Install crash reporting mechanisms into the current process.
  void install(spdlog::logger* kernelLogger);

  /// Register the kernel information into the crash reporter.
  void registerKernel(const KernelConfig& config);

  /// Which part of its life the kernel is in. A crash while stopping and a crash while running point
  /// at different code, and the report is the only place that difference survives.
  enum class Phase : int
  {
    starting = 0,
    running = 1,
    stopping = 2,
    stopped = 3,
  };

  /// Records the phase for an eventual crash. Safe to call from anywhere and safe to read in a
  /// handler, which is why it is a plain value rather than anything that locks.
  static void setPhase(Phase phase) noexcept;

  /// Uninstall any previously-installed crash reporting mechanisms.
  void uninstall();

  /// Define a transport version for an eventual crash report.
  void setTransportVersion(uint32_t transportVersion);

private:
  CrashReporter() noexcept = default;
  ~CrashReporter() = default;

private:
  void addLog(std::chrono::system_clock::time_point time, std::string_view msg);
  void collectLogs();
  void collectStackTrace(const cpptrace::stacktrace& trace);
  void collectExceptionData();
  void collectProcessData();
  void collectEnvironmentVars();
  void collectErrorData();
  void writeReport();

  /// Writes what is already known to a file while formatting is still allowed, since a handler
  /// cannot. Removed on a clean shutdown; the handler's own record sits beside it.
  void writeCrashContext();
  void doTermination();
  void configureKernelLogger(spdlog::logger* kernelLogger);
  UncaughtException makeExceptionData(std::string_view message, ExceptionKind kind);
  [[nodiscard]] std::filesystem::path computeCrashReportFile() const;
  [[noreturn]] static void terminationHandler();

#ifdef __linux__

  /// Records the signal and the raw return addresses, and nothing else. Allocating here would wait
  /// on a lock the interrupted thread may hold, so turning addresses into names happens elsewhere.
  static void linuxSignalHandler(int signum, siginfo_t* info, void* context);

  /// Prepares everything the handler needs while allocating is still legal: the alternate stack,
  /// the record's path, and one backtrace() call to force its lazy loading.
  void prepareSignalHandling();
#endif

private:
  friend class RedirectionSink;
  ErrorReport report_;
  std::string crashContextPath_;
  std::multimap<std::chrono::system_clock::time_point, std::string> logs_;
  std::optional<std::terminate_handler> previousHandler_;
};

}  // namespace sen::kernel::impl

#endif  // SEN_LIBS_KERNEL_SRC_CRASH_REPORTER_H
