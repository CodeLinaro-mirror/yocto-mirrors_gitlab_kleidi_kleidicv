# SPDX-FileCopyrightText: 2024 Arm Limited and/or its affiliates <open-source-office@arm.com>
#
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.16)

# The requirements to build Google Benchmark are higher than the
# rest of the project so disable benchmarks by default.
# Another reason to disable it by default is that the configure time
# for Google Benchmark is quite noticeable.
option(INTRINSICCV_BENCHMARK "Enable IntrinsicCV benchmarks" OFF)

if(INTRINSICCV_BENCHMARK)

include(FetchContent)

# Please update SECURITY.md if adding, removing or changing the version of third party content.
FetchContent_Declare(
  benchmark
  URL https://github.com/google/benchmark/archive/refs/tags/v1.8.3.tar.gz
  URL_HASH SHA256=6bc180a57d23d4d9515519f92b0c83d61b05b5bab188961f36ac7b06b0d9e9ce
)
FetchContent_MakeAvailable(benchmark)


set(INTRINSICCV_INCLUDE_DIR
  ${CMAKE_CURRENT_SOURCE_DIR}/../intrinsiccv/include
  ${CMAKE_CURRENT_BINARY_DIR}/../intrinsiccv/include
)
set(INTRINSICCV_BENCHMARK_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR})

set(INTRINSICCV_BENCHMARK_CXX_FLAGS
  "-Werror"
  "-Wall"
  "-Wextra"
  "-Wold-style-cast"
)

if (CMAKE_BUILD_TYPE STREQUAL "Debug")
  list(APPEND INTRINSICCV_BENCHMARK_CXX_FLAGS "-O0" "-g")
else()
  list(APPEND INTRINSICCV_BENCHMARK_CXX_FLAGS "-O2" "-g0")
endif()

file(GLOB intrinsiccv_benchmark_sources CONFIGURE_DEPENDS "*.h" "*.cpp")

set_source_files_properties(
  ${intrinsiccv_benchmark_sources}
  PROPERTIES COMPILE_OPTIONS "${INTRINSICCV_BENCHMARK_CXX_FLAGS}"
)

add_executable(
  intrinsiccv-benchmark
  ${intrinsiccv_benchmark_sources}
)

set_target_properties(
  intrinsiccv-benchmark
  PROPERTIES CXX_STANDARD 17
)

target_include_directories(
  intrinsiccv-benchmark
  PRIVATE ${INTRINSICCV_INCLUDE_DIR}
  PRIVATE ${INTRINSICCV_BENCHMARK_INCLUDE_DIR}
)

target_link_libraries(
  intrinsiccv-benchmark
  intrinsiccv
  benchmark::benchmark
)

endif(INTRINSICCV_BENCHMARK)
