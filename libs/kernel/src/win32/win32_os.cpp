// === win32_os.cpp ====================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifdef _WIN32

#  include "./win32_os.h"

// sen
#  include "sen/core/base/assert.h"

// std
#  include <filesystem>

namespace sen::kernel
{

Win32OS::Win32OS(std::shared_ptr<Win32API> api) noexcept: api_(std::move(api)) {}

void Win32OS::closeSharedLibrary(SharedLibrary& library) noexcept
{
  if (library.nativeHandle == nullptr)
  {
    return;
  }

  // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
  std::ignore = api_->FreeLibrary(reinterpret_cast<HMODULE>(library.nativeHandle));  // NOSONAR
  library.nativeHandle = nullptr;
}

SharedLibrary Win32OS::openSharedLibrary(std::string_view path)
{
  auto libraryPath = std::filesystem::path(path);
  if (!libraryPath.has_extension())
  {
    libraryPath.replace_extension(".dll");
  }

  void* handle = api_->LoadLibraryA(libraryPath.string().c_str());
  if (!handle)
  {
    std::string err;
    err.append("could not open shared library ");
    err.append(libraryPath.string());
    ::sen::throwRuntimeError(err);
  }

  return {handle};
}

void* Win32OS::getSymbol(SharedLibrary library, std::string_view symbolName)
{
  if (library.nativeHandle == nullptr)
  {
    return nullptr;
  }

  // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
  return static_cast<void*>(
    api_->GetProcAddress(reinterpret_cast<HMODULE>(library.nativeHandle), symbolName.data()));  // NOSONAR
}

Result<Thread, ThreadCreateErr> Win32OS::createThread(const ThreadConfig& config)
{
  if (config.function == nullptr)
  {
    return Err(ThreadCreateErr::invalidThreadFunction);
  }

  if (config.arg == nullptr)
  {
    return Err(ThreadCreateErr::invalidThreadFunctionArgument);
  }

  threads_.push_back(std::make_unique<ThreadImpl>(config, api_));
  auto runResult = threads_.back()->run();
  if (runResult.isError())
  {
    return Err(runResult.getError());
  }

  return Ok(Thread {threads_.back().get()});
}

bool Win32OS::joinThread(Thread thread) noexcept
{
  if (thread.nativeHandle == nullptr)
  {
    return false;
  }

  auto threadImpl = static_cast<ThreadImpl*>(thread.nativeHandle);
  auto result = threadImpl->join();
  destroyThread(thread);
  return result;
}

bool Win32OS::killThread(Thread thread) noexcept
{
  if (thread.nativeHandle == nullptr)
  {
    return false;
  }

  auto threadImpl = static_cast<ThreadImpl*>(thread.nativeHandle);
  auto result = threadImpl->kill();
  destroyThread(thread);
  return result;
}

bool Win32OS::detachThread(Thread& thread) noexcept
{
  if (thread.nativeHandle == nullptr)
  {
    return false;
  }

  auto threadImpl = static_cast<ThreadImpl*>(thread.nativeHandle);
  auto result = threadImpl->detach();
  destroyThread(thread);
  return result;
}

void Win32OS::destroyThread(Thread& thread) noexcept
{
  auto threadImpl = static_cast<ThreadImpl*>(thread.nativeHandle);
  for (auto itr = threads_.begin(); itr != threads_.end(); ++itr)
  {
    if (itr->get() == threadImpl)
    {
      threads_.erase(itr);
      break;
    }
  }
  thread.nativeHandle = nullptr;
}

std::shared_ptr<OperatingSystem> makeNativeOS()
{
  return std::make_shared<Win32OS>(std::make_shared<NativeWin32API>());
}

}  // namespace sen::kernel

#endif
