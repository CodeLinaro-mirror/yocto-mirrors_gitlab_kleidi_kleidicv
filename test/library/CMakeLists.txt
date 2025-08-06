# SPDX-FileCopyrightText: 2025 Arm Limited and/or its affiliates <open-source-office@arm.com>
#
# SPDX-License-Identifier: Apache-2.0

# Choose the default value for the KLEIDICV_ENABLE_SVE2 option
# according to the compiler version. The list of compiler versions
# recognised as supporting SVE may be extended in future.
# check_cxx_compiler_flag is not used to test whether the compiler
# supports +sve2 since this may succeed for compilers that have only
# partial SVE support.
if ((CMAKE_CXX_COMPILER_ID STREQUAL "Clang" AND
    CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 12) OR
    (CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND
    CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 10)
)
  set(KLEIDICV_ENABLE_SVE2 ON)
else()
  set(KLEIDICV_ENABLE_SVE2 OFF)
endif()

message(STATUS "KLEIDICV_ENABLE_SVE2 ${KLEIDICV_ENABLE_SVE2}")

file(GLOB kleidicv_library_test_sources CONFIGURE_DEPENDS "*.h" "*.cpp")

if (KLEIDICV_ENABLE_SVE2)
  list(APPEND KLEIDICV_TEST_CXX_FLAGS "-march=armv8-a+sve2")
else()
  list(FILTER kleidicv_library_test_sources EXCLUDE REGEX "_sve2\\.cpp$")
endif()

list(APPEND kleidicv_library_test_sources ${KLEIDICV_TEST_FRAMEWORK_SOURCES})

set_source_files_properties(
  ${kleidicv_library_test_sources}
  PROPERTIES COMPILE_OPTIONS "${KLEIDICV_TEST_CXX_FLAGS}"
)

add_executable(
  kleidicv-library-test
  ${kleidicv_library_test_sources}
)

set_target_properties(
  kleidicv-library-test
  PROPERTIES CXX_STANDARD 17
)

target_include_directories(
  kleidicv-library-test
  PRIVATE ${KLEIDICV_INCLUDE_DIR}
  PRIVATE ${KLEIDICV_TEST_INCLUDE_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/../../kleidicv_thread/include
)

if (KLEIDICV_ALLOCATION_TESTS)
  target_link_options(
    kleidicv-library-test
    PRIVATE -Wl,--wrap,malloc
  )
endif()


target_link_libraries(
  kleidicv-library-test
  kleidicv
  kleidicv_thread
  gtest
  gmock
)
