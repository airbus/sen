# === spdlog.cmake =====================================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================

include_guard()

find_package(spdlog REQUIRED)
find_package(fmt REQUIRED)

if(TARGET spdlog::spdlog)
  get_target_property(SPDLOG_INCLUDES spdlog::spdlog INTERFACE_INCLUDE_DIRECTORIES)
  string(
    REGEX
    REPLACE "\\$<\\$<CONFIG:[^>]+>:(.*)>"
            "\\1"
            SPDLOG_INCLUDES
            "${SPDLOG_INCLUDES}"
  )

  get_target_property(FMT_INCLUDES fmt::fmt INTERFACE_INCLUDE_DIRECTORIES)
  string(
    REGEX
    REPLACE "\\$<\\$<CONFIG:[^>]+>:(.*)>"
            "\\1"
            FMT_INCLUDES
            "${FMT_INCLUDES}"
  )

  # Not into include/, which every Sen target puts on a consumer's include path: a consumer with its
  # own spdlog would then have two. Reached through Findspdlog instead.
  if(SPDLOG_INCLUDES AND FMT_INCLUDES)
    install(DIRECTORY "${SPDLOG_INCLUDES}/spdlog" DESTINATION third_party/include)
    install(DIRECTORY "${FMT_INCLUDES}/fmt" DESTINATION third_party/include)
  endif()
endif()
