# === crashpad.cmake ===================================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================

include_guard()

# The recipe names its cmake config "crashpad" and its target "sentry-crashpad::sentry-crashpad";
# they do not match, so the find and the link use different names on purpose.
find_package(crashpad REQUIRED)

# crashpad::handler is the handler's code as a library; handler/handler_main.h exposes it so it
# "can be embedded into another binary". Sen links it and runs it, so nothing is installed beside
# the binaries and a program embedding the kernel needs no extra file.
#
# The annotations reach the handler through an ELF note that is the only thing in its object file.
# Nothing references it, so the linker leaves that object in the archive and the dump arrives with
# no annotations and no error. Naming the symbol takes it out.
# The recipe declares no requirements between its components, and its aggregate target carries
# neither the handler nor the tools it calls, so the order here is the link order: the handler
# first, then what it needs. Getting it wrong leaves libkernel.so linking cleanly with undefined
# symbols -- a shared library is allowed those -- and fails later on whatever links against it.
add_library(sen_crashpad INTERFACE)
target_link_libraries(
  sen_crashpad INTERFACE crashpad::handler crashpad::tools sentry-crashpad::sentry-crashpad
)
if(NOT WIN32)
  target_link_options(sen_crashpad INTERFACE "LINKER:-u,CRASHPAD_NOTE_REFERENCE")
else()
  # crashpad_util reads a module's version resource through GetFileVersionInfoW and VerQueryValueW,
  # which live in version.lib. The recipe does not declare it, so without this the kernel fails to
  # link with three unresolved externals and nothing points at their origin.
  target_link_libraries(sen_crashpad INTERFACE version)
endif()
