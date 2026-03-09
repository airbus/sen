// === util.cpp ========================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "sen/kernel/util.h"

// sen
#include "sen/core/base/hash32.h"
#include "sen/core/base/uuid.h"

// generated code
#include "stl/sen/kernel/basic_types.stl.h"

// os
#if defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__NT__)
#  pragma warning(disable: 4996)
#  include <windows.h>
#else
#  include <sys/utsname.h>
#  include <unistd.h>

// extra cstd
// NOLINTNEXTLINE(hicpp-deprecated-headers,modernize-deprecated-headers)
#  include "errno.h"  // program_invocation_short_name is not part of <cerrno>
#endif

// std
#include <array>
#include <cstdint>
#include <string>
#include <string_view>

namespace sen::kernel
{
//--------------------------------------------------------------------------------------------------------------
// Helpers
//--------------------------------------------------------------------------------------------------------------

namespace
{
[[nodiscard]] constexpr kernel::CpuArch getCpuArch() noexcept
{
#if defined(__x86_64__) || defined(_M_X64)
  return kernel::CpuArch::x64;
#elif defined(i386) || defined(__i386__) || defined(__i386) || defined(_M_IX86)
  return kernel::CpuArch::x8632;
#elif defined(__ARM_ARCH_2__)
  return kernel::CpuArch::arm2;
#elif defined(__ARM_ARCH_3__) || defined(__ARM_ARCH_3M__)
  return kernel::CpuArch::arm3;
#elif defined(__ARM_ARCH_4T__) || defined(__TARGET_ARM_4T)
  return kernel::CpuArch::arm4t;
#elif defined(__ARM_ARCH_5_) || defined(__ARM_ARCH_5E_)
  return kernel::CpuArch::arm5;
#elif defined(__ARM_ARCH_6T2_) || defined(__ARM_ARCH_6T2_)
  return kernel::CpuArch::arm6t2;
#elif defined(__ARM_ARCH_6__) || defined(__ARM_ARCH_6J__) || defined(__ARM_ARCH_6K__) || defined(__ARM_ARCH_6Z__) ||   \
  defined(__ARM_ARCH_6ZK__)
  return kernel::CpuArch::arm6;
#elif defined(__ARM_ARCH_7__) || defined(__ARM_ARCH_7A__) || defined(__ARM_ARCH_7R__) || defined(__ARM_ARCH_7M__) ||   \
  defined(__ARM_ARCH_7S__)
  return kernel::CpuArch::arm6;
#elif defined(__ARM_ARCH_7A__) || defined(__ARM_ARCH_7R__) || defined(__ARM_ARCH_7M__) || defined(__ARM_ARCH_7S__)
  return kernel::CpuArch::arm7a;
#elif defined(__ARM_ARCH_7R__) || defined(__ARM_ARCH_7M__) || defined(__ARM_ARCH_7S__)
  return kernel::CpuArch::arm7r;
#elif defined(__ARM_ARCH_7M__)
  return kernel::CpuArch::arm7m;
#elif defined(__ARM_ARCH_7S__)
  return kernel::CpuArch::arm7s;
#elif defined(__aarch64__) || defined(_M_ARM64)
  return kernel::CpuArch::arm64;
#elif defined(mips) || defined(__mips__) || defined(__mips)
  return kernel::CpuArch::mips;
#elif defined(__sh__)
  return kernel::CpuArch::superH;
#elif defined(__powerpc) || defined(__powerpc__) || defined(__powerpc64__) || defined(__POWERPC__) ||                  \
  defined(__ppc__) || defined(__PPC__) || defined(_ARCH_PPC)
  return kernel::CpuArch::ppc;
#elif defined(__PPC64__) || defined(__ppc64__) || defined(_ARCH_PPC64)
  return kernel::CpuArch::ppc64;
#elif defined(__sparc__) || defined(__sparc)
  return kernel::CpuArch::sparc;
#elif defined(__m68k__)
  return kernel::CpuArch::m68k;
#else
  return kernel::CpuArch::otherArch;
#endif
}

[[nodiscard]] constexpr kernel::OsKind getOsKind() noexcept
{
#if defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__NT__)
  return kernel::OsKind::windowsOs;
#elif __APPLE__
  return kernel::OsKind::appleOs;
#elif __ANDROID__
  return kernel::OsKind::androidOs;
#elif __linux__
  return kernel::OsKind::linuxOs;
#elif __unix__
  return kernel::OsKind::unixOs;
#elif defined(_POSIX_VERSION)
  return kernel::OsKind::posixOs;
#else
  return kernel::OsKind::otherOs;
#endif
}

[[nodiscard]] std::string getOsName()
{
#if defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__NT__)
  OSVERSIONINFOEX vi;  // OSVERSIONINFOEX is supported starting at Windows 2000
  vi.dwOSVersionInfoSize = sizeof(vi);
  if (GetVersionEx((OSVERSIONINFO*)&vi) == 0)  // NOLINT
  {
    throwRuntimeError("Cannot get OS version information");
  }

  switch (vi.dwMajorVersion)
  {
    case 10:
      switch (vi.dwMinorVersion)
      {
        case 0:
          if (vi.dwBuildNumber >= 22000)
          {
            return "Windows 11";
          }
          else if (vi.dwBuildNumber >= 20348 && vi.wProductType != VER_NT_WORKSTATION)
          {
            return "Windows Server 2022";
          }
          else if (vi.dwBuildNumber >= 17763 && vi.wProductType != VER_NT_WORKSTATION)
          {
            return "Windows Server 2019";
          }
          else
          {
            return vi.wProductType == VER_NT_WORKSTATION ? "Windows 10" : "Windows Server 2016";
          }
      }
    case 6:
      switch (vi.dwMinorVersion)
      {
        case 0:
          return vi.wProductType == VER_NT_WORKSTATION ? "Windows Vista" : "Windows Server 2008";
        case 1:
          return vi.wProductType == VER_NT_WORKSTATION ? "Windows 7" : "Windows Server 2008 R2";
        case 2:
          return vi.wProductType == VER_NT_WORKSTATION ? "Windows 8" : "Windows Server 2012";
        case 3:
          return vi.wProductType == VER_NT_WORKSTATION ? "Windows 8.1" : "Windows Server 2012 R2";
        default:
          return "Unknown";
      }
    case 5:
      switch (vi.dwMinorVersion)
      {
        case 0:
          return "Windows 2000";
        case 1:
          return "Windows XP";
        case 2:
          return "Windows Server 2003/Windows Server 2003 R2";
        default:
          return "Unknown";
      }
    default:
      return "Unknown";
  }
#else
  struct utsname uts = {};
  uname(&uts);
  return uts.sysname;
#endif
}

[[nodiscard]] std::string getProcessName()
{
#if defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__NT__)
  std::string buf;
  buf.resize(MAX_PATH);
  do
  {
    auto len = GetModuleFileNameA(NULL, &buf[0], static_cast<unsigned int>(buf.size()));
    if (len < buf.size())
    {
      buf.resize(len);
      break;
    }

    buf.resize(buf.size() * 2);
  } while (buf.size() < 65536);

  return buf;

#elif defined(__APPLE__)
  return getprogname();
#else
  return ::program_invocation_short_name;
#endif
}

[[nodiscard]] std::string getHostName()
{
  constexpr auto infoBufferSize = 32767;
#if defined(__linux) || defined(__APPLE__)
  std::array<char, infoBufferSize> hostName {};
  gethostname(hostName.data(), hostName.size());  // NOLINT(misc-include-cleaner)
  return {hostName.data()};
#elif defined(_WIN32)
  TCHAR infoBuffer[infoBufferSize];
  DWORD bufCharCount = infoBufferSize;

  // Get and display the name of the computer.
  GetComputerName(infoBuffer, &bufCharCount);
  return std::string(infoBuffer);
#endif
}

}  // namespace

//--------------------------------------------------------------------------------------------------------------
// Utility functions
//--------------------------------------------------------------------------------------------------------------

ProcessInfo getOwnProcessInfo(std::string_view sessionName)
{
  return {getHostId(),
          getUniqueSenProcessId(),
          crc32(sessionName),
          std::string(sessionName),
          getProcessName(),
          getHostName(),
          getOsKind(),
          getOsName(),
          getCpuArch()};
}

[[nodiscard]] uint32_t getHostId() { return crc32(getHostName()); }

[[nodiscard]] uint32_t getUniqueSenProcessId()
{
  static const uint32_t pid = UuidRandomGenerator()().getHash32();
  return pid;
}

}  // namespace sen::kernel
