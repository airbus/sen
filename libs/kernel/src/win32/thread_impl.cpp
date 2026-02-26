// === thread_impl.cpp =================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifdef _WIN32

#  include "./thread_impl.h"

#  include "./win32_api.h"

namespace sen::kernel
{

ThreadImpl::ThreadImpl(ThreadConfig configuration, std::shared_ptr<Win32API> api) noexcept
  : config_(std::move(configuration)), api_(std::move(api))
{
}

Result<void, ThreadCreateErr> ThreadImpl::run() noexcept
{
  DWORD threadId;
  thread_ = api_->CreateThread(nullptr,          // default security attributes
                               0,                // use default stack size
                               &threadFunction,  // thread function name
                               this,             // argument to thread function
                               0,                // use default creation flags
                               &threadId);

  if (thread_ == nullptr)
  {
    return Err(ThreadCreateErr::internalOsError);
  }

  // now that the thread is created, we have to set the affinity (if needed).
  auto affinityResult = configureAffinity();
  if (affinityResult.isError())
  {
    return Err(affinityResult.getError());
  }

  constexpr DWORD exceptionCode = 0x406D1388;
#  pragma pack(push, 8)
  typedef struct tagTHREADNAME_INFO
  {
    DWORD dwType;      // must be 0x1000
    LPCSTR szName;     // pointer to name (in user addr space)
    DWORD dwThreadID;  // thread ID (-1=caller thread)
    DWORD dwFlags;     // reserved for future use, must be zero
  } THREADNAME_INFO;
#  pragma pack(pop)

  THREADNAME_INFO info;
  info.dwType = 0x1000;
  info.szName = config_.name.data();
  info.dwThreadID = threadId;
  info.dwFlags = 0;

#  pragma warning(push)
#  pragma warning(disable: 6320 6322)

  __try
  {
    RaiseException(exceptionCode, 0, sizeof(info) / sizeof(ULONG_PTR), (ULONG_PTR*)&info);
  }
  __except (EXCEPTION_EXECUTE_HANDLER)
  {
    // the doc says to swallow this exception https://msdn.microsoft.com/en-us/library/xcb2z8hs.aspx
  }
#  pragma warning(pop)

  return configurePriority();
}

bool ThreadImpl::join() noexcept
{
  if (thread_ == nullptr)
  {
    return false;
  }

  return (api_->WaitForSingleObject(thread_, INFINITE) != WAIT_FAILED);
}

bool ThreadImpl::kill() noexcept
{
  if (thread_ == nullptr)
  {
    return false;
  }

  return api_->TerminateThread(thread_, 1) == TRUE;
}

bool ThreadImpl::detach() noexcept
{
  if (thread_ == nullptr)
  {
    return false;
  }

  return api_->CloseHandle(thread_) == TRUE;
}

Result<void, ThreadCreateErr> ThreadImpl::configurePriority() noexcept
{
  if (config_.priority == Priority::nominalMin)
  {
    return Ok();
  }

  int nPriority = THREAD_PRIORITY_NORMAL;

  switch (config_.priority)
  {
    case Priority::lowest:
      nPriority = THREAD_PRIORITY_LOWEST;
      break;
    case Priority::nominalMin:
      nPriority = THREAD_PRIORITY_NORMAL;
      break;
    case Priority::nominalMax:
      nPriority = THREAD_PRIORITY_ABOVE_NORMAL;
      break;
    case Priority::highest:
      nPriority = THREAD_PRIORITY_HIGHEST;
      break;
  }

  if (api_->SetThreadPriority(thread_, nPriority) == FALSE)
  {
    return Err(ThreadCreateErr::internalOsError);
  }

  return Ok();
}

Result<void, ThreadCreateErr> ThreadImpl::configureAffinity() noexcept
{
  // 0 means no particular affinity
  if (config_.affinity == 0U)
  {
    return Ok();
  }

  DWORD affinity = DWORD(config_.affinity);
  if (api_->SetThreadAffinityMask(thread_, affinity) != 0U)
  {
    return Err(ThreadCreateErr::invalidAffinity);
  }

  return Ok();
}

DWORD WINAPI ThreadImpl::threadFunction(LPVOID lpParam)
{
  auto control = static_cast<ThreadImpl*>(lpParam);
  control->config_.function(control->config_.arg);
  return 0;
}

}  // namespace sen::kernel

#endif
