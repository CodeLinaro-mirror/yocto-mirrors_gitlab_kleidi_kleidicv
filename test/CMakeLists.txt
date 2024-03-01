# SPDX-FileCopyrightText: 2023 - 2024 Arm Limited and/or its affiliates <open-source-office@arm.com>
#
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.16)

set(INTRINSICCV_INCLUDE_DIR
  ${CMAKE_CURRENT_SOURCE_DIR}/../intrinsiccv/include
  ${CMAKE_CURRENT_BINARY_DIR}/../intrinsiccv/include
)
set(INTRINSICCV_TEST_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR})

set(INTRINSICCV_TEST_CXX_FLAGS
  "-Werror"
  "-Wall"
  "-Wextra"
  "-Wold-style-cast"
)

if (CMAKE_BUILD_TYPE STREQUAL "Debug")
  list(APPEND INTRINSICCV_TEST_CXX_FLAGS "-O0" "-g")
else()
  list(APPEND INTRINSICCV_TEST_CXX_FLAGS "-O2" "-g0")
endif()

set(INTRINSICCV_TEST_FRAMEWORK_SOURCES
  ${CMAKE_CURRENT_SOURCE_DIR}/framework/border.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/framework/test_main.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/framework/utils.cpp
)

include(FetchContent)
# Please update SECURITY.md if adding, removing or changing the version of third party content.
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/refs/tags/release-1.12.1.tar.gz
  URL_HASH SHA256=81964fe578e9bd7c94dfdb09c8e4d6e6759e19967e397dbea48d1c10e45d0df2
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
