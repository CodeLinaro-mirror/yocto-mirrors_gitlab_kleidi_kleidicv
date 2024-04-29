# SPDX-FileCopyrightText: 2023 - 2024 Arm Limited and/or its affiliates <open-source-office@arm.com>
#
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.16)

project("KleidiCV")

include(CheckCXXCompilerFlag)

if (CMAKE_SYSTEM_NAME MATCHES "^(Linux|Android)$")
  option(KLEIDICV_ENABLE_SVE2 "Enable SVE2 code paths" ON)
  option(KLEIDICV_ENABLE_SME2 "Enable SME2 code paths" ON)
else()
  set(KLEIDICV_ENABLE_SVE2 OFF)
  set(KLEIDICV_ENABLE_SME2 OFF)
endif()
option(
  KLEIDICV_LIMIT_SVE2_TO_SELECTED_ALGORITHMS
  "Enables or disables SVE2 code paths for selected algorithms. Has no effect if KLEIDICV_ENABLE_SVE2 is false."
  ON
)
option(KLEIDICV_CHECK_BANNED_FUNCTIONS "Internal - Check source for deprecated or obsolescent functions" OFF)
option(KLEIDICV_ASSUME_128BIT_SVE2 "Internal - If turned ON 128-bit SVE2 vector length is assumed" OFF)
option(KLEIDICV_PREFER_INTERLEAVING_LOAD_STORE "Internal - If turned ON interleaving loads and stores are preferred instead of continuous loads and stores" OFF)
option(KLEIDICV_EXPERIMENTAL_FEATURE_CANNY "Internal - Enable experimental Canny algorithm" OFF)
option(KLEIDICV_CANNY_ALGORITHM_CONFORM_OPENCV "Internal - If turned ON Canny algorithm creates bit exact result compared to OpenCV's original implementation" ON)

# Compiler feature check for SVE2.
set(KLEIDICV_BUILD_SVE2 OFF)
if (KLEIDICV_ENABLE_SVE2)
  check_cxx_compiler_flag("-march=armv8-a+sve2" KLEIDICV_COMPILER_SUPPORTS_SVE2)
  if (KLEIDICV_COMPILER_SUPPORTS_SVE2)
    set(KLEIDICV_BUILD_SVE2 ON)
  else()
    message(STATUS "[KleidiCV] Compiler does not support SVE2")
  endif()
endif()

# Compiler feature check for SME2.
set(KLEIDICV_BUILD_SME2 OFF)
if (KLEIDICV_ENABLE_SME2)
  # Assuming clang
  if (CMAKE_CXX_COMPILER_VERSION VERSION_LESS 18)
    set(KLEIDICV_COMPILER_SUPPORTS_SME2 OFF)
  else()
    check_cxx_compiler_flag("-march=armv9-a+sme2" KLEIDICV_COMPILER_SUPPORTS_SME2)
  endif()

  if (KLEIDICV_COMPILER_SUPPORTS_SME2)
    set(KLEIDICV_BUILD_SME2 ON)
  else()
    message(STATUS "[KleidiCV] Compiler does not support SME2")
  endif()
endif()

if(KLEIDICV_BUILD_SVE2 AND NOT KLEIDICV_LIMIT_SVE2_TO_SELECTED_ALGORITHMS)
  set(KLEIDICV_ALWAYS_ENABLE_SVE2 ON)
endif()

configure_file("${CMAKE_CURRENT_LIST_DIR}/include/kleidicv/config.h.in" "include/kleidicv/config.h")

file(GLOB KLEIDICV_API_SOURCES
  "${CMAKE_CURRENT_LIST_DIR}/src/*_api.cpp"
  "${CMAKE_CURRENT_LIST_DIR}/src/**/*_api.cpp"
)

file(GLOB KLEIDICV_NEON_SOURCES
  "${CMAKE_CURRENT_LIST_DIR}/src/*_neon.cpp"
  "${CMAKE_CURRENT_LIST_DIR}/src/**/*_neon.cpp"
)

file(GLOB KLEIDICV_SVE2_SOURCES
  "${CMAKE_CURRENT_LIST_DIR}/src/*_sve2.cpp"
  "${CMAKE_CURRENT_LIST_DIR}/src/**/*_sve2.cpp"
)

file(GLOB KLEIDICV_SME2_SOURCES
  "${CMAKE_CURRENT_LIST_DIR}/src/*_sme2.cpp"
  "${CMAKE_CURRENT_LIST_DIR}/src/**/*_sme2.cpp"
)

set(KLEIDICV_INCLUDE_DIRS
  "${CMAKE_CURRENT_LIST_DIR}/include"
  "${CMAKE_CURRENT_BINARY_DIR}/include"
)

set(KLEIDICV_WARNING_FLAGS
  "-Werror"
  "-Wall"
  "-Wextra"
  "-Wold-style-cast"
  "-Wno-shadow" # GCC's shadow declaration check is too sensitive for the library
)

set(KLEIDICV_CXX_FLAGS
  "-O2"
  "-g0"
  "-fomit-frame-pointer"
  "-fno-stack-protector"
  "-fno-exceptions"
  "-fno-rtti"
  "-fno-unroll-loops"
  ${KLEIDICV_WARNING_FLAGS}
)

if(CMAKE_CXX_COMPILER_ID MATCHES ".*Clang")
  list(APPEND KLEIDICV_CXX_FLAGS
    "-mllvm"
    "-inline-threshold=10000"
  )
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  list(APPEND KLEIDICV_CXX_FLAGS
    "-flax-vector-conversions"
    "-Wno-unused-label"
  )
endif()

if (KLEIDICV_CHECK_BANNED_FUNCTIONS)
  # The `SHELL:` prefix is used to turn off de-duplication of compiler flags,
  # it is necessary if other headers are need to be force included.
  # https://cmake.org/cmake/help/latest/command/target_compile_options.html#option-de-duplication
  list(APPEND KLEIDICV_CXX_FLAGS "SHELL:-include kleidicv/unsafe.h")
endif()

if (CMAKE_BUILD_TYPE STREQUAL "Debug")
  list(APPEND KLEIDICV_CXX_FLAGS "-O0" "-g")
else()
  list(APPEND KLEIDICV_CXX_FLAGS "-O2" "-g0")
endif()

add_library(kleidicv_neon OBJECT ${KLEIDICV_NEON_SOURCES})
target_include_directories(kleidicv_neon PRIVATE ${KLEIDICV_INCLUDE_DIRS})
set_target_properties(kleidicv_neon PROPERTIES CXX_STANDARD 17)
target_compile_options(kleidicv_neon PRIVATE
  ${KLEIDICV_CXX_FLAGS}
  "-march=armv8-a"
  "-DKLEIDICV_TARGET_NEON=1"
)

if(KLEIDICV_BUILD_SVE2)
  add_library(kleidicv_sve2 OBJECT ${KLEIDICV_SVE2_SOURCES})
  target_include_directories(kleidicv_sve2 PRIVATE ${KLEIDICV_INCLUDE_DIRS})
  set_target_properties(kleidicv_sve2 PROPERTIES CXX_STANDARD 17)
  target_compile_options(kleidicv_sve2 PRIVATE
    ${KLEIDICV_CXX_FLAGS}
    "-march=armv8-a+sve2"
    "-DKLEIDICV_TARGET_SVE2=1"
  )
endif()

if(KLEIDICV_BUILD_SME2)
  add_library(kleidicv_sme2 OBJECT ${KLEIDICV_SME2_SOURCES})
  target_include_directories(kleidicv_sme2 PRIVATE ${KLEIDICV_INCLUDE_DIRS})
  set_target_properties(kleidicv_sme2 PROPERTIES CXX_STANDARD 17)
  target_compile_options(kleidicv_sme2 PRIVATE
    ${KLEIDICV_CXX_FLAGS}
    "-march=armv9-a+sve2+sme2"
    "-DKLEIDICV_TARGET_SME2=1"
  )
endif()

add_library(kleidicv STATIC ${KLEIDICV_API_SOURCES})
target_include_directories(kleidicv PRIVATE ${KLEIDICV_INCLUDE_DIRS})
set_target_properties(kleidicv PROPERTIES CXX_STANDARD 17)
target_compile_options(kleidicv PRIVATE ${KLEIDICV_CXX_FLAGS})
target_link_libraries(kleidicv PRIVATE kleidicv_neon)

if(KLEIDICV_BUILD_SVE2)
  target_compile_definitions(kleidicv PRIVATE KLEIDICV_HAVE_SVE2)
  target_link_libraries(kleidicv PRIVATE kleidicv_sve2)
endif()

if(KLEIDICV_BUILD_SME2)
  target_compile_definitions(kleidicv PRIVATE KLEIDICV_HAVE_SME2)
  target_link_libraries(kleidicv PRIVATE kleidicv_sme2)
endif()
