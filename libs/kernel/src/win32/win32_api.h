// === win32_api.h =====================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_LIBS_KERNEL_SRC_WIN32_WIN32_API_H
#define SEN_LIBS_KERNEL_SRC_WIN32_WIN32_API_H

#ifdef _WIN32

#  include "sen/core/base/class_helpers.h"

#  define WIN32_LEAN_AND_MEAN
#  define NOMINMAX
#  include "windows.h"

namespace sen::kernel
{

/// All calls we do to the Win32 system. This class allows us to mock and
/// instrument system calls and evaluate our usage of the system.
class Win32API
{
  SEN_NOCOPY_NOMOVE(Win32API)

public:  // special members.
  Win32API() noexcept = default;
  virtual ~Win32API() noexcept = default;

public:
  /// Same as ::LoadLibraryA. NOLINTNEXTLINE
  [[nodiscard]] virtual HMODULE LoadLibraryA(LPCSTR lpLibFileName) noexcept = 0;

  /// Same as ::FreeLibrary. NOLINTNEXTLINE
  [[nodiscard]] virtual BOOL FreeLibrary(HMODULE hLibModule) noexcept = 0;

  /// Same as ::GetProcAddress. NOLINTNEXTLINE
  [[nodiscard]] virtual FARPROC GetProcAddress(HMODULE hModule, LPCSTR lpProcName) noexcept = 0;

  /// Same as ::CreateThread. NOLINTNEXTLINE
  [[nodiscard]] virtual HANDLE CreateThread(LPSECURITY_ATTRIBUTES lpThreadAttributes,
                                            SIZE_T dwStackSize,
                                            LPTHREAD_START_ROUTINE lpStartAddress,
                                            LPVOID lpParameter,
                                            DWORD dwCreationFlags,
                                            LPDWORD lpThreadId) noexcept = 0;

  /// Same as ::TerminateThread. NOLINTNEXTLINE
  [[nodiscard]] virtual BOOL TerminateThread(HANDLE hThread, DWORD dwExitCode) noexcept = 0;

  /// Same as ::SetThreadPriority. NOLINTNEXTLINE
  [[nodiscard]] virtual BOOL SetThreadPriority(HANDLE hThread, int nPriority) noexcept = 0;

  /// Same as ::SetThreadAffinityMask. NOLINTNEXTLINE
  [[nodiscard]] virtual DWORD_PTR SetThreadAffinityMask(HANDLE hThread, DWORD_PTR dwThreadAffinityMask) noexcept = 0;

  /// Same as ::WaitForSingleObject. NOLINTNEXTLINE
  [[nodiscard]] virtual DWORD WaitForSingleObject(HANDLE hHandle, DWORD dwMilliseconds) noexcept = 0;

  /// Same as ::CloseHandle. NOLINTNEXTLINE
  [[nodiscard]] virtual BOOL CloseHandle(HANDLE hObject) noexcept = 0;
};

/// Implements real calls to the WIN32 API
class NativeWin32API final: public Win32API
{
public:
  [[nodiscard]] HMODULE LoadLibraryA(LPCSTR lpLibFileName) noexcept final;
  [[nodiscard]] BOOL FreeLibrary(HMODULE hLibModule) noexcept final;
  [[nodiscard]] FARPROC GetProcAddress(HMODULE hModule, LPCSTR lpProcName) noexcept final;
  [[nodiscard]] HANDLE CreateThread(LPSECURITY_ATTRIBUTES lpThreadAttributes,
                                    SIZE_T dwStackSize,
                                    LPTHREAD_START_ROUTINE lpStartAddress,
                                    LPVOID lpParameter,
                                    DWORD dwCreationFlags,
                                    LPDWORD lpThreadId) noexcept final;
  [[nodiscard]] BOOL TerminateThread(HANDLE hThread, DWORD dwExitCode) noexcept final;
  [[nodiscard]] BOOL SetThreadPriority(HANDLE hThread, int nPriority) noexcept final;
  [[nodiscard]] DWORD_PTR SetThreadAffinityMask(HANDLE hThread, DWORD_PTR dwThreadAffinityMask) noexcept final;
  [[nodiscard]] DWORD WaitForSingleObject(HANDLE hHandle, DWORD dwMilliseconds) noexcept final;
  [[nodiscard]] BOOL CloseHandle(HANDLE hObject) noexcept final;
};

}  // namespace sen::kernel

#endif

#endif  // SEN_LIBS_KERNEL_SRC_WIN32_WIN32_API_H
