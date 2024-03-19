# SPDX-FileCopyrightText: 2023 - 2024 Arm Limited and/or its affiliates <open-source-office@arm.com>
#
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.16)

project("IntrinsicCV")

include(CheckCXXCompilerFlag)

if (CMAKE_SYSTEM_NAME MATCHES "^(Linux|Android)$")
  option(INTRINSICCV_ENABLE_SVE2 "Explicitly enables or disables SVE2 code paths for all supported algorithms" OFF)
  option(INTRINSICCV_ENABLE_SVE2_SELECTIVELY "Explicitly enables or disables SVE2 code paths for selected algorithms" ON)
  option(INTRINSICCV_ENABLE_SME2 "Explicitly enables or disables SME2 code paths for all supported algorithms" ON)
else()
  set(INTRINSICCV_ENABLE_SVE2 OFF)
  set(INTRINSICCV_ENABLE_SVE2_SELECTIVELY OFF)
  set(INTRINSICCV_ENABLE_SME2 OFF)
endif()
option(INTRINSICCV_CHECK_BANNED_FUNCTIONS "Check source for deprecated or obsolescent functions" OFF)
option(INTRINSICCV_ASSUME_128BIT_SVE2 "Internal - If turned ON 128-bit SVE2 vector length is assumed" OFF)
option(INTRINSICCV_PREFER_INTERLEAVING_LOAD_STORE "Internal - If turned ON interleaving loads and stores are preferred instead of continuous loads and stores" OFF)
option(INTRINSICCV_EXPERIMENTAL_FEATURE_CANNY "Internal - Enable experimental Canny algorithm" OFF)
option(INTRINSICCV_CANNY_ALGORITHM_CONFORM_OPENCV "Internal - If turned ON Canny algorithm creates bit exact result compared to OpenCV's original implementation" ON)

if (INTRINSICCV_ENABLE_SVE2 AND INTRINSICCV_ENABLE_SVE2_SELECTIVELY)
  message(FATAL_ERROR "[IntrinsicCV] INTRINSICCV_ENABLE_SVE2 and INTRINSICCV_ENABLE_SVE2_SELECTIVELY cannot be set at the same time")
endif()

# Compiler feature check for SVE2.
set(INTRINSICCV_BUILD_SVE2 OFF)
if (INTRINSICCV_ENABLE_SVE2 OR INTRINSICCV_ENABLE_SVE2_SELECTIVELY)
  check_cxx_compiler_flag("-march=armv8-a+sve2" INTRINSICCV_COMPILER_SUPPORTS_SVE2)
  if (INTRINSICCV_COMPILER_SUPPORTS_SVE2)
    set(INTRINSICCV_BUILD_SVE2 ON)
  else()
    message(STATUS "[IntrinsicCV] Compiler does not support SVE2")
  endif()
endif()

# Compiler feature check for SME2.
set(INTRINSICCV_BUILD_SME2 OFF)
if (INTRINSICCV_ENABLE_SME2)
  # Assuming clang
  if (CMAKE_CXX_COMPILER_VERSION VERSION_LESS 18)
    set(INTRINSICCV_COMPILER_SUPPORTS_SME2 OFF)
  else()
    check_cxx_compiler_flag("-march=armv9-a+sme2" INTRINSICCV_COMPILER_SUPPORTS_SME2)
  endif()

  if (INTRINSICCV_COMPILER_SUPPORTS_SME2)
    set(INTRINSICCV_BUILD_SME2 ON)
  else()
    message(STATUS "[IntrinsicCV] Compiler does not support SME2")
  endif()
endif()

if(INTRINSICCV_BUILD_SVE2 AND INTRINSICCV_ENABLE_SVE2)
  set(INTRINSICCV_ALWAYS_ENABLE_SVE2 ON)
endif()

configure_file("${CMAKE_CURRENT_LIST_DIR}/include/intrinsiccv/config.h.in" "include/intrinsiccv/config.h")

file(GLOB INTRINSICCV_API_SOURCES
  "${CMAKE_CURRENT_LIST_DIR}/src/*_api.cpp"
  "${CMAKE_CURRENT_LIST_DIR}/src/**/*_api.cpp"
)

file(GLOB INTRINSICCV_NEON_SOURCES
  "${CMAKE_CURRENT_LIST_DIR}/src/*_neon.cpp"
  "${CMAKE_CURRENT_LIST_DIR}/src/**/*_neon.cpp"
)

file(GLOB INTRINSICCV_SVE2_SOURCES
  "${CMAKE_CURRENT_LIST_DIR}/src/*_sve2.cpp"
  "${CMAKE_CURRENT_LIST_DIR}/src/**/*_sve2.cpp"
)

file(GLOB INTRINSICCV_SME2_SOURCES
  "${CMAKE_CURRENT_LIST_DIR}/src/*_sme2.cpp"
  "${CMAKE_CURRENT_LIST_DIR}/src/**/*_sme2.cpp"
)

set(INTRINSICCV_INCLUDE_DIRS
  "${CMAKE_CURRENT_LIST_DIR}/include"
  "${CMAKE_CURRENT_BINARY_DIR}/include"
)

set(INTRINSICCV_WARNING_FLAGS
  "-Werror"
  "-Wall"
  "-Wextra"
  "-Wold-style-cast"
  "-Wno-shadow" # GCC's shadow declaration check is too sensitive for the library
)

set(INTRINSICCV_CXX_FLAGS
  "-O2"
  "-g0"
  "-fomit-frame-pointer"
  "-fno-stack-protector"
  "-fno-exceptions"
  "-fno-rtti"
  "-fno-unroll-loops"
  ${INTRINSICCV_WARNING_FLAGS}
)

if(CMAKE_CXX_COMPILER_ID MATCHES ".*Clang")
  list(APPEND INTRINSICCV_CXX_FLAGS
    "-mllvm"
    "-inline-threshold=10000"
  )
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  list(APPEND INTRINSICCV_CXX_FLAGS
    "-flax-vector-conversions"
    "-Wno-unused-label"
  )
endif()

if (INTRINSICCV_CHECK_BANNED_FUNCTIONS)
  # The `SHELL:` prefix is used to turn off de-duplication of compiler flags,
  # it is necessary if other headers are need to be force included.
  # https://cmake.org/cmake/help/latest/command/target_compile_options.html#option-de-duplication
  list(APPEND INTRINSICCV_CXX_FLAGS "SHELL:-include intrinsiccv/unsafe.h")
endif()

if (CMAKE_BUILD_TYPE STREQUAL "Debug")
  list(APPEND INTRINSICCV_CXX_FLAGS "-O0" "-g")
else()
  list(APPEND INTRINSICCV_CXX_FLAGS "-O2" "-g0")
endif()

add_library(intrinsiccv_neon OBJECT ${INTRINSICCV_NEON_SOURCES})
target_include_directories(intrinsiccv_neon PRIVATE ${INTRINSICCV_INCLUDE_DIRS})
set_target_properties(intrinsiccv_neon PROPERTIES CXX_STANDARD 17)
target_compile_options(intrinsiccv_neon PRIVATE
  ${INTRINSICCV_CXX_FLAGS}
  "-march=armv8-a"
  "-DINTRINSICCV_TARGET_NEON=1"
)

if(INTRINSICCV_BUILD_SVE2)
  add_library(intrinsiccv_sve2 OBJECT ${INTRINSICCV_SVE2_SOURCES})
  target_include_directories(intrinsiccv_sve2 PRIVATE ${INTRINSICCV_INCLUDE_DIRS})
  set_target_properties(intrinsiccv_sve2 PROPERTIES CXX_STANDARD 17)
  target_compile_options(intrinsiccv_sve2 PRIVATE
    ${INTRINSICCV_CXX_FLAGS}
    "-march=armv8-a+sve2"
    "-DINTRINSICCV_TARGET_SVE2=1"
  )
endif()

if(INTRINSICCV_BUILD_SME2)
  add_library(intrinsiccv_sme2 OBJECT ${INTRINSICCV_SME2_SOURCES})
  target_include_directories(intrinsiccv_sme2 PRIVATE ${INTRINSICCV_INCLUDE_DIRS})
  set_target_properties(intrinsiccv_sme2 PROPERTIES CXX_STANDARD 17)
  target_compile_options(intrinsiccv_sme2 PRIVATE
    ${INTRINSICCV_CXX_FLAGS}
    "-march=armv9-a+sve2+sme2"
    "-DINTRINSICCV_TARGET_SME2=1"
  )
endif()

add_library(intrinsiccv STATIC ${INTRINSICCV_API_SOURCES})
target_include_directories(intrinsiccv PRIVATE ${INTRINSICCV_INCLUDE_DIRS})
set_target_properties(intrinsiccv PROPERTIES CXX_STANDARD 17)
target_compile_options(intrinsiccv PRIVATE ${INTRINSICCV_CXX_FLAGS})
target_link_libraries(intrinsiccv PRIVATE intrinsiccv_neon)

if(INTRINSICCV_BUILD_SVE2)
  target_compile_definitions(intrinsiccv PRIVATE INTRINSICCV_HAVE_SVE2)
  target_link_libraries(intrinsiccv PRIVATE intrinsiccv_sve2)
endif()

if(INTRINSICCV_BUILD_SME2)
  target_compile_definitions(intrinsiccv PRIVATE INTRINSICCV_HAVE_SME2)
  target_link_libraries(intrinsiccv PRIVATE intrinsiccv_sme2)
endif()
