# SPDX-FileCopyrightText: 2023 - 2024 Arm Limited and/or its affiliates <open-source-office@arm.com>
#
# SPDX-License-Identifier: Apache-2.0

file(GLOB intrinsiccv_framework_test_sources CONFIGURE_DEPENDS "*.h" "test_*.cpp")

list(APPEND intrinsiccv_framework_test_sources ${KLEIDICV_TEST_FRAMEWORK_SOURCES})

set_source_files_properties(
  ${intrinsiccv_framework_test_sources}
  PROPERTIES COMPILE_OPTIONS "${KLEIDICV_TEST_CXX_FLAGS}"
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
  PRIVATE ${KLEIDICV_INCLUDE_DIR}
  PRIVATE ${KLEIDICV_TEST_INCLUDE_DIR}
)

if (KLEIDICV_ALLOCATION_TESTS)
  target_link_options(
    intrinsiccv-framework-test
    PRIVATE -Wl,--wrap,malloc
  )
endif()


target_link_libraries(
  intrinsiccv-framework-test
  intrinsiccv
  gtest_main
)
