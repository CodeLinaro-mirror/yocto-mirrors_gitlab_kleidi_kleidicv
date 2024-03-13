// SPDX-FileCopyrightText: 2024 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#include <benchmark/benchmark.h>

#include <algorithm>
#include <random>

#include "intrinsiccv/intrinsiccv.h"

extern size_t image_width, image_height;

template <typename T, typename Function>
static void bench_binary_op(Function f, benchmark::State& state) {
  // Setup
  std::vector<T> src_a, src_b, dst;
  src_a.resize(image_width * image_height);
  src_b.resize(image_width * image_height);
  dst.resize(image_width * image_height);

  std::mt19937 generator;
  std::generate(src_a.begin(), src_a.end(), generator);
  std::generate(src_b.begin(), src_b.end(), generator);

  for (auto _ : state) {
    // This code gets benchmarked
    auto unused = f(src_a.data(), image_width, src_a.data(), image_width,
                    dst.data(), image_width, image_width, image_height);
    (void)unused;
  }
}

#define BENCH_BINARY_OP(name, type)                   \
  static void name(benchmark::State& state) {         \
    bench_binary_op<type>(intrinsiccv_##name, state); \
  }                                                   \
  BENCHMARK(name)

BENCH_BINARY_OP(saturating_add_s8, int8_t);
BENCH_BINARY_OP(saturating_sub_u16, uint16_t);
BENCH_BINARY_OP(saturating_absdiff_s32, int32_t);

static void min_max_loc_u8(benchmark::State& state) {
  // Setup
  std::vector<uint8_t> src;
  src.resize(image_width * image_height);
  std::mt19937 generator;
  std::generate(src.begin(), src.end(), generator);

  size_t min_offset = 0, max_offset = 0;

  for (auto _ : state) {
    // This code gets benchmarked
    auto unused =
        intrinsiccv_min_max_loc_u8(src.data(), image_width, image_width,
                                   image_height, &min_offset, &max_offset);
    (void)unused;
  }
}
BENCHMARK(min_max_loc_u8);
