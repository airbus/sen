// === posix_os.cpp ====================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "./posix_os.h"

// implementation
#include "operating_system.h"
#include "posix/posix_api.h"
#include "posix/thread_impl.h"
#include "shared_library.h"
#include "thread.h"

// sen
#include "sen/core/base/assert.h"
#include "sen/core/base/result.h"

// generated code
#include "stl/sen/kernel/basic_types.stl.h"

// os
#include <dlfcn.h>

// std
#include <filesystem>
#include <memory>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>

namespace sen::kernel
{

std::shared_ptr<OperatingSystem> makeNativeOS()
{
  return std::make_shared<PosixOS>(std::make_shared<NativePosixAPI>());
}

PosixOS::PosixOS(std::shared_ptr<PosixAPI> api) noexcept: api_(std::move(api)) {}

void* PosixOS::installDeathHandler() { return nullptr; }

SharedLibrary PosixOS::openSharedLibrary(std::string_view path)
{
  std::filesystem::path nativePath(path);

  // set 'libXYZ.so' as the library name
  std::string libName = "lib";
  libName.append(nativePath.stem());
  libName.append(".so");

  nativePath.replace_filename(libName);

  int loadFlags = RTLD_GLOBAL | RTLD_LAZY | RTLD_NODELETE;  // NOLINT(hicpp-signed-bitwise)
  void* handle = api_->dlopen(nativePath.c_str(), loadFlags);
  if (!handle)
  {
    std::string err;
    err.append(nativePath);
    err.append(": ");
    err.append(dlerror());
    throwRuntimeError(err);
  }

  return {handle};
}

void PosixOS::closeSharedLibrary(SharedLibrary& library) noexcept
{
  if (library.nativeHandle == nullptr)
  {
    return;
  }

  // We cannot close the library in Posix due to pointers to the .so files
  // dangling in our code base.

  library.nativeHandle = nullptr;
}

void* PosixOS::getSymbol(SharedLibrary library, std::string_view symbolName)
{
  if (library.nativeHandle == nullptr)
  {
    return nullptr;
  }

  return api_->dlsym(library.nativeHandle, symbolName.data());
}

Result<Thread, ThreadCreateErr> PosixOS::createThread(const ThreadConfig& config) noexcept
{
  if (config.function == nullptr)
  {
    return Err(ThreadCreateErr::invalidThreadFunction);
  }

  if (config.arg == nullptr)
  {
    return Err(ThreadCreateErr::invalidThreadFunctionArgument);
  }

  auto result = ThreadImpl::make(config, api_);
  if (result.isError())
  {
    return Err(result.getError());
  }

  threads_.emplace_back(std::move(result).getValue());
  std::ignore = threads_.back().run();
  return Ok(Thread {&threads_.back()});
}

bool PosixOS::joinThread(Thread thread) noexcept
{
  if (thread.nativeHandle == nullptr)
  {
    return false;
  }

  const auto* threadImpl = static_cast<ThreadImpl*>(thread.nativeHandle);
  return threadImpl->join();
}

bool PosixOS::killThread(Thread thread) noexcept
{
  if (thread.nativeHandle == nullptr)
  {
    return false;
  }

  const auto* threadImpl = static_cast<ThreadImpl*>(thread.nativeHandle);
  return threadImpl->kill();
}

bool PosixOS::detachThread(Thread& thread) noexcept
{
  if (thread.nativeHandle == nullptr)
  {
    return false;
  }

  const auto* threadImpl = static_cast<ThreadImpl*>(thread.nativeHandle);
  auto result = threadImpl->detach();
  destroyThread(thread);
  return result;
}

void PosixOS::destroyThread(Thread& thread) noexcept
{
  const auto* threadImpl = static_cast<const ThreadImpl*>(thread.nativeHandle);
  for (auto itr = threads_.begin(); itr != threads_.end(); ++itr)
  {
    if (&(*itr) == threadImpl)
    {
      threads_.erase(itr);
      break;
    }
  }
  thread.nativeHandle = nullptr;
}

}  // namespace sen::kernel
