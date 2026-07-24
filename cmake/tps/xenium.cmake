# === xenium.cmake =====================================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================

include_guard()

# NOTE: This dependency is fetched directly from GitHub because it
# is currently unavailable for download in Conan.

include(FetchContent)

FetchContent_Declare(
  xenium
  URL https://github.com/mpoeter/xenium/archive/refs/tags/v0.0.2.tar.gz
  # Pinned: this is the only dependency outside conan, so conan.lock's drift check
  # does not cover it. GitHub regenerates release tarballs, so an unpinned fetch can
  # change underneath us silently.
  URL_HASH SHA256=05da5e15c4bd600ebd3b5e999a138f39fe8b1a322da899555cd66c4996168d0e
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE
  PATCH_COMMAND ${CMAKE_COMMAND} -E env python3 ${CMAKE_CURRENT_LIST_DIR}/patch_xenium.py <SOURCE_DIR>
)

FetchContent_GetProperties(xenium)
if(NOT xenium_POPULATED)
  FetchContent_Populate(xenium)
endif()

add_library(xenium::xenium INTERFACE IMPORTED)
target_include_directories(xenium::xenium SYSTEM INTERFACE ${xenium_SOURCE_DIR})
