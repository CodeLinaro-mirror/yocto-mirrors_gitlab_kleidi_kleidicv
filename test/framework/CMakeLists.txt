# SPDX-FileCopyrightText: 2023 - 2024 Arm Limited and/or its affiliates <open-source-office@arm.com>
#
# SPDX-License-Identifier: Apache-2.0

file(GLOB intrinsiccv_framework_test_sources CONFIGURE_DEPENDS "*.h" "test_*.cpp")

list(APPEND intrinsiccv_framework_test_sources ${INTRINSICCV_TEST_FRAMEWORK_SOURCES})

set_source_files_properties(
  ${intrinsiccv_framework_test_sources}
  PROPERTIES COMPILE_OPTIONS "-O2;${INTRINSICCV_WARNING_FLAGS}"
)

add_executable(
  intrinsiccv-framework-test
  ${intrinsiccv_framework_test_sources}
)

set_target_properties(
  intrinsiccv-framework-test
  PROPERTIES CXX_STANDARD 17
)

target_include_directories(
  intrinsiccv-framework-test
  PRIVATE ${INTRINSICCV_INCLUDE_DIR}
  PRIVATE ${INTRINSICCV_TEST_INCLUDE_DIR}
)

target_link_libraries(
  intrinsiccv-framework-test
  intrinsiccv
  gtest_main
)
