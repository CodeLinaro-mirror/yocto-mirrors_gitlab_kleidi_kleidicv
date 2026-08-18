# SPDX-FileCopyrightText: 2023 - 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
#
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.16)

set(KLEIDICV_INCLUDE_DIR
  ${CMAKE_CURRENT_SOURCE_DIR}/../kleidicv/include
  ${CMAKE_CURRENT_BINARY_DIR}/../kleidicv/include
)
set(KLEIDICV_TEST_INCLUDE_DIR
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_BINARY_DIR}
)

set(KLEIDICV_TEST_CXX_FLAGS
  "-Wall"
  "-Wextra"
  "-Wold-style-cast"
)

if (CMAKE_BUILD_TYPE STREQUAL "Debug")
  list(APPEND KLEIDICV_TEST_CXX_FLAGS "-O0" "-g")
else()
  list(APPEND KLEIDICV_TEST_CXX_FLAGS "-O2" "-g0")
endif()

# Only perform allocation tests on Linux and Android
if (CMAKE_SYSTEM_NAME MATCHES "^(Linux|Android)$")
  set(KLEIDICV_ALLOCATION_TESTS TRUE)
endif()

set(KLEIDICV_TEST_FRAMEWORK_SOURCES
  ${CMAKE_CURRENT_SOURCE_DIR}/framework/border.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/framework/test_main.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/framework/utils.cpp
)

if (KLEIDICV_ALLOCATION_TESTS)
  list(APPEND KLEIDICV_TEST_CXX_FLAGS
    "-fno-builtin-malloc"
    "-fno-builtin-calloc"
  )
  list(APPEND KLEIDICV_TEST_FRAMEWORK_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/framework/wrap_allocations.cpp
  )
endif()

configure_file("${CMAKE_CURRENT_LIST_DIR}/test_config.h.in" "test_config.h")

include(FetchContent)
# Please update SECURITY.md if adding, removing or changing the version of third party content.
set(KLEIDICV_GOOGLETEST_GIT_REPOSITORY "$ENV{KLEIDICV_GOOGLETEST_GIT_REPOSITORY}" CACHE STRING
  "Git repository from which to fetch googletest instead of the public release archive")
set(KLEIDICV_GOOGLETEST_GIT_TAG_PREFIX "$ENV{KLEIDICV_GOOGLETEST_GIT_TAG_PREFIX}" CACHE STRING
  "Prefix for the googletest Git tag when fetching from a Git repository")
set(KLEIDICV_GOOGLETEST_VERSION "release-1.12.1")

if (KLEIDICV_GOOGLETEST_GIT_REPOSITORY)
  message(STATUS "Fetching googletest from ${KLEIDICV_GOOGLETEST_GIT_REPOSITORY} at tag ${KLEIDICV_GOOGLETEST_GIT_TAG_PREFIX}${KLEIDICV_GOOGLETEST_VERSION}")
  FetchContent_Declare(
    googletest
    GIT_REPOSITORY "${KLEIDICV_GOOGLETEST_GIT_REPOSITORY}"
    GIT_TAG "${KLEIDICV_GOOGLETEST_GIT_TAG_PREFIX}${KLEIDICV_GOOGLETEST_VERSION}"
    GIT_SHALLOW TRUE
    # Disable death test feature otherwise tests may crash when run under emulation.
    PATCH_COMMAND cd <SOURCE_DIR> && patch --strip=1 --input=${CMAKE_CURRENT_SOURCE_DIR}/patches/googletest.patch
  )
else()
  message(STATUS "Fetching googletest from https://github.com/google/googletest/archive/refs/tags/${KLEIDICV_GOOGLETEST_VERSION}.tar.gz")
  FetchContent_Declare(
    googletest
    URL "https://github.com/google/googletest/archive/refs/tags/${KLEIDICV_GOOGLETEST_VERSION}.tar.gz"
    URL_HASH SHA256=81964fe578e9bd7c94dfdb09c8e4d6e6759e19967e397dbea48d1c10e45d0df2
    # Disable death test feature otherwise tests may crash when run under emulation.
    PATCH_COMMAND cd <SOURCE_DIR> && patch --strip=1 --input=${CMAKE_CURRENT_SOURCE_DIR}/patches/googletest.patch
  )
endif()
FetchContent_MakeAvailable(googletest)

add_subdirectory(api)
add_subdirectory(framework)
add_subdirectory(unit_neon)
add_subdirectory(unit_thread)

# Target to build all tests.
add_custom_target(
  kleidicv-test
  DEPENDS kleidicv-framework-test kleidicv-api-test kleidicv-neon-unit-test
          kleidicv-thread-unit-test
)

# Target to build and run all tests.
add_custom_target(
  check-kleidicv
  COMMAND kleidicv-framework-test
  COMMAND kleidicv-api-test
  COMMAND kleidicv-neon-unit-test
  COMMAND kleidicv-thread-unit-test
  DEPENDS kleidicv-test
  USES_TERMINAL
)
