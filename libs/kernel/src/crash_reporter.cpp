// === crash_reporter.cpp ==============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "crash_reporter.h"

// sen
#include "sen/core/base/compiler_macros.h"
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
#include "cpptrace/cpptrace.hpp"

// linux
#ifdef __linux__
#  include <climits>
#  include <csignal>
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
#include <utility>

namespace sen::kernel::impl
{

//--------------------------------------------------------------------------------------------------------------
// Constants
//--------------------------------------------------------------------------------------------------------------

namespace
{

#ifdef __linux__
// NOLINTNEXTLINE(misc-include-cleaner)
constexpr std::array<int, 5U> signalsToHandle = {SIGSEGV, SIGABRT, SIGFPE, SIGILL, SIGBUS};
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
  for (auto sig: signalsToHandle)
  {
    signal(sig, linuxSignalHandler);
  }
#endif
}

void CrashReporter::registerKernel(const KernelConfig& config)
{
  report_.senData.kernelParams = config.getParams();
  report_.senData.kernelProtocol = getKernelProtocolVersion();

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
  for (auto env = environ; *env;  // NOLINT(misc-include-cleaner
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
    collectStackTrace(cpptrace::generate_trace());
    report_.errorData.errorMessage.push_back("Terminate called without an active exception");
  }
}

void CrashReporter::collectErrorData()
{
  report_.errorData.time = TimeStamp(std::chrono::system_clock::now().time_since_epoch());
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
void CrashReporter::collectSignalData(int signum)
{
  SignalData sigData;
  sigData.signalName = strsignal(signum);  // NOLINT(misc-include-cleaner)
  sigData.signalNumber = signum;
  report_.errorData.signalData = std::move(sigData);
}

void CrashReporter::linuxSignalHandler(int signum)
{
  get().collectSignalData(signum);
  terminationHandler();
}

#endif

void CrashReporter::configureKernelLogger(spdlog::logger* kernelLogger)
{
  // set a high logging level but filter at the sinks
  kernelLogger->set_level(spdlog::level::trace);
}

}  // namespace sen::kernel::impl
