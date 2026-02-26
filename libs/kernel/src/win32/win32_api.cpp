// === win32_api.cpp ===================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifdef _WIN32

#  include "win32_api.h"

namespace sen::kernel
{

HMODULE NativeWin32API::LoadLibraryA(LPCSTR lpLibFileName) noexcept { return ::LoadLibraryA(lpLibFileName); }

BOOL NativeWin32API::FreeLibrary(HMODULE hLibModule) noexcept { return ::FreeLibrary(hLibModule); }

FARPROC NativeWin32API::GetProcAddress(HMODULE hModule, LPCSTR lpProcName) noexcept
{
  return ::GetProcAddress(hModule, lpProcName);
}

HANDLE NativeWin32API::CreateThread(LPSECURITY_ATTRIBUTES lpThreadAttributes,
                                    SIZE_T dwStackSize,
                                    LPTHREAD_START_ROUTINE lpStartAddress,
                                    LPVOID lpParameter,
                                    DWORD dwCreationFlags,
                                    LPDWORD lpThreadId) noexcept
{
  return ::CreateThread(lpThreadAttributes, dwStackSize, lpStartAddress, lpParameter, dwCreationFlags, lpThreadId);
}

BOOL NativeWin32API::TerminateThread(HANDLE hThread, DWORD dwExitCode) noexcept
{
  return ::TerminateThread(hThread, dwExitCode);
}

BOOL NativeWin32API::SetThreadPriority(HANDLE hThread, int nPriority) noexcept
{
  return ::SetThreadPriority(hThread, nPriority);
}

DWORD_PTR NativeWin32API::SetThreadAffinityMask(HANDLE hThread, DWORD_PTR dwThreadAffinityMask) noexcept
{
  return ::SetThreadAffinityMask(hThread, dwThreadAffinityMask);
}

DWORD NativeWin32API::WaitForSingleObject(HANDLE hHandle, DWORD dwMilliseconds) noexcept
{
  return ::WaitForSingleObject(hHandle, dwMilliseconds);
}

BOOL NativeWin32API::CloseHandle(HANDLE hObject) noexcept { return ::CloseHandle(hObject); }

}  // namespace sen::kernel

#endif
