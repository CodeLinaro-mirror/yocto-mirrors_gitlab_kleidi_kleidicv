// SPDX-FileCopyrightText: 2024 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#include <benchmark/benchmark.h>

#include <algorithm>
#include <cstdint>
#include <random>

#include "kleidicv/kleidicv.h"

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
    auto unused = f(src_a.data(), image_width * sizeof(T), src_b.data(),
                    image_width * sizeof(T), dst.data(),
                    image_width * sizeof(T), image_width, image_height);
    (void)unused;
  }
}

#define BENCH_BINARY_OP(name, type)                \
  static void name(benchmark::State& state) {      \
    bench_binary_op<type>(kleidicv_##name, state); \
  }                                                \
  BENCHMARK(name)

BENCH_BINARY_OP(saturating_add_s8, int8_t);
BENCH_BINARY_OP(saturating_sub_u16, uint16_t);
BENCH_BINARY_OP(saturating_absdiff_s32, int32_t);
BENCH_BINARY_OP(bitwise_and, uint8_t);
BENCH_BINARY_OP(compare_equal_u8, uint8_t);
BENCH_BINARY_OP(compare_greater_u8, uint8_t);

template <typename T, typename Function>
static void bench_unary_op(Function f, size_t channels,
                           benchmark::State& state) {
  // Setup
  std::vector<T> src, dst;
  src.resize(image_width * image_height * channels);
  dst.resize(image_width * image_height * channels);

  std::mt19937 generator;
  std::generate(src.begin(), src.end(), generator);

  for (auto _ : state) {
    // This code gets benchmarked
    auto unused = f(src.data(), image_width * sizeof(T), dst.data(),
                    image_width * sizeof(T), image_width, image_height);
    (void)unused;
  }
}

#define BENCH_UNARY_OP(name, channels, type)                \
  static void name(benchmark::State& state) {               \
    bench_unary_op<type>(kleidicv_##name, channels, state); \
  }                                                         \
  BENCHMARK(name)

BENCH_UNARY_OP(rgb_to_yuv_u8, 3, uint8_t);
BENCH_UNARY_OP(rgba_to_yuv_u8, 4, uint8_t);
BENCH_UNARY_OP(bgr_to_yuv_u8, 3, uint8_t);
BENCH_UNARY_OP(bgra_to_yuv_u8, 4, uint8_t);

static void min_max_loc_u8(benchmark::State& state) {
  // Setup
  std::vector<uint8_t> src;
  src.resize(image_width * image_height);
  std::mt19937 generator;
  std::generate(src.begin(), src.end(), generator);

  size_t min_offset = 0, max_offset = 0;

  for (auto _ : state) {
    // This code gets benchmarked
    auto unused = kleidicv_min_max_loc_u8(
        src.data(), image_width * sizeof(uint8_t), image_width, image_height,
        &min_offset, &max_offset);
    (void)unused;
  }
}
BENCHMARK(min_max_loc_u8);

template <typename T, typename F>
static void resize_linear(F f, size_t scale_x, size_t scale_y,
                          benchmark::State& state) {
  // Setup
  size_t src_width = image_width / scale_x;
  size_t src_height = image_height / scale_y;
  size_t dst_width = src_width * scale_x;
  size_t dst_height = src_height * scale_y;
  std::vector<T> src, dst;
  src.resize(src_width * src_height);
  dst.resize(dst_width * dst_height);
  std::mt19937 generator;
  std::generate(src.begin(), src.end(), generator);

  for (auto _ : state) {
    // This code gets benchmarked
    auto unused = f(src.data(), src_width * sizeof(T), src_width, src_height,
                    dst.data(), dst_width * sizeof(T), dst_width, dst_height);
    (void)unused;
  }
}

static void resize_linear_2x2_u8(benchmark::State& state) {
  resize_linear<uint8_t>(kleidicv_resize_linear_u8, 2, 2, state);
}
BENCHMARK(resize_linear_2x2_u8);

static void resize_linear_4x4_u8(benchmark::State& state) {
  resize_linear<uint8_t>(kleidicv_resize_linear_u8, 4, 4, state);
}
BENCHMARK(resize_linear_4x4_u8);

static void resize_linear_2x2_f32(benchmark::State& state) {
  resize_linear<float>(kleidicv_resize_linear_f32, 2, 2, state);
}
BENCHMARK(resize_linear_2x2_f32);

static void resize_linear_4x4_f32(benchmark::State& state) {
  resize_linear<float>(kleidicv_resize_linear_f32, 4, 4, state);
}
BENCHMARK(resize_linear_4x4_f32);

template <typename T, typename Function>
static void gaussian_blur(Function f, size_t channels,
                          benchmark::State& state) {
  // Setup
  std::vector<T> src, dst;
  src.resize(image_width * image_height * channels);
  dst.resize(image_width * image_height * channels);

  std::mt19937 generator;
  std::generate(src.begin(), src.end(), generator);

  kleidicv_filter_context_t* context;
  kleidicv_error_t err =
      kleidicv_filter_create(&context, channels, 2 * sizeof(T),
                             kleidicv_rectangle_t{image_width, image_height});
  if (err != KLEIDICV_OK) {
    state.SkipWithError("Could not initialize Gaussian blur filter.");
    return;
  }

  for (auto _ : state) {
    // This code gets benchmarked
    auto unused = f(src.data(), image_width * sizeof(T), dst.data(),
                    image_width * sizeof(T), image_width, image_height,
                    channels, KLEIDICV_BORDER_TYPE_REFLECT, context);
    (void)unused;
  }

  (void)kleidicv_filter_release(context);
}

static void gaussian_blur_7x7_u8_1ch(benchmark::State& state) {
  gaussian_blur<uint8_t>(kleidicv_gaussian_blur_7x7_u8, 1, state);
}
BENCHMARK(gaussian_blur_7x7_u8_1ch);

static void gaussian_blur_7x7_u8_3ch(benchmark::State& state) {
  gaussian_blur<uint8_t>(kleidicv_gaussian_blur_7x7_u8, 3, state);
}
BENCHMARK(gaussian_blur_7x7_u8_3ch);

static void exp_f32(benchmark::State& state) {
  // Setup
  std::vector<float> src, dst;
  src.resize(image_width * image_height);
  dst.resize(image_width * image_height);

  std::mt19937 generator;
  std::generate(src.begin(), src.end(), generator);

  for (auto _ : state) {
    // This code gets benchmarked
    auto unused = kleidicv_exp_f32(src.data(), image_width * sizeof(float),
                                   dst.data(), image_width * sizeof(float),
                                   image_width, image_height);
    (void)unused;
  }
}
BENCHMARK(exp_f32);
