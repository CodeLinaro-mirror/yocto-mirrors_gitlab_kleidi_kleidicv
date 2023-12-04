# SPDX-FileCopyrightText: 2023 Arm Limited and/or its affiliates <open-source-office@arm.com>
#
# SPDX-License-Identifier: Apache-2.0

file(GLOB intrinsiccv_api_test_sources CONFIGURE_DEPENDS "*.h" "*.cpp")

list(APPEND intrinsiccv_api_test_sources ${INTRINSICCV_TEST_MAIN})

set_source_files_properties(
  ${intrinsiccv_api_test_sources}
  PROPERTIES COMPILE_OPTIONS "-O2;${INTRINSICCV_WARNING_FLAGS}"
)

add_executable(
  intrinsiccv-api-test
  ${intrinsiccv_api_test_sources}
)

target_include_directories(
  intrinsiccv-api-test
  PRIVATE ${INTRINSICCV_INCLUDE_DIR}
  PRIVATE ${INTRINSICCV_TEST_INCLUDE_DIR}
)

target_link_libraries(
  intrinsiccv-api-test
  intrinsiccv
  gtest_main
)
