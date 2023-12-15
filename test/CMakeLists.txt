# SPDX-FileCopyrightText: 2023 Arm Limited and/or its affiliates <open-source-office@arm.com>
#
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.16)

set(INTRINSICCV_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../intrinsiccv/include)
set(INTRINSICCV_TEST_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR})

set(INTRINSICCV_WARNING_FLAGS
  "-Werror"
  "-Wall"
  "-Wextra"
  "-Wold-style-cast"
)

set(INTRINSICCV_TEST_FRAMEWORK_SOURCES
  ${CMAKE_CURRENT_SOURCE_DIR}/framework/border.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/framework/test_main.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/framework/utils.cpp
)

include(FetchContent)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/refs/tags/release-1.12.1.tar.gz
  # Disable death test feature otherwise tests may crash when run under emulation.
  PATCH_COMMAND cd <SOURCE_DIR> && patch --strip=1 --input=${CMAKE_CURRENT_SOURCE_DIR}/patches/googletest.patch
)
FetchContent_MakeAvailable(googletest)

add_subdirectory(api)
add_subdirectory(framework)

# Target to build all tests.
add_custom_target(
  intrinsiccv-test
  DEPENDS intrinsiccv-framework-test intrinsiccv-api-test
)

# Target to build and run all tests.
add_custom_target(
  check-intrinsiccv
  COMMAND intrinsiccv-framework-test
  COMMAND intrinsiccv-api-test
  DEPENDS intrinsiccv-test
  USES_TERMINAL
)
