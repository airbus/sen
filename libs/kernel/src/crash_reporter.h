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
  void doTermination();
  void configureKernelLogger(spdlog::logger* kernelLogger);
  UncaughtException makeExceptionData(std::string_view message, ExceptionKind kind);
  [[nodiscard]] std::filesystem::path computeCrashReportFile() const;
  [[noreturn]] static void terminationHandler();

#ifdef __linux__
  void collectSignalData(int signum);
  static void linuxSignalHandler(int signum);
#endif

private:
  friend class RedirectionSink;
  ErrorReport report_;
  std::multimap<std::chrono::system_clock::time_point, std::string> logs_;
  std::optional<std::terminate_handler> previousHandler_;
};

}  // namespace sen::kernel::impl

#endif  // SEN_LIBS_KERNEL_SRC_CRASH_REPORTER_H
