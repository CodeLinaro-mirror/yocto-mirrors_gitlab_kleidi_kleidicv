// SPDX-FileCopyrightText: 2024 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#include <benchmark/benchmark.h>

#include <algorithm>
#include <cstdint>
#include <random>

#include "kleidicv/kleidicv.h"

extern size_t image_width, image_height;

// Setting up buffers can be time-consuming and is not of interest when
// benchmarking. Therefore endeavour to reuse buffers where possible. This
// function creates a single shared buffer for each pair of template arguments.
// All bytes of the buffer are filled with the Value argument.
template <int PixelSize, int Value>
uint8_t* get_buffer() {
  static std::vector<uint8_t> result(image_width * image_height * PixelSize,
                                     Value);
  return result.data();
}

// Get a buffer suitable for using as the first input buffer.
template <typename T, int Channels = 1>
const T* get_source_buffer_a() {
  return reinterpret_cast<T*>(get_buffer<sizeof(T) * Channels, 0xA3>());
}

// Get a buffer suitable for using as the second input buffer.
template <typename T, int Channels = 1>
const T* get_source_buffer_b() {
  return reinterpret_cast<T*>(get_buffer<sizeof(T) * Channels, 0x9E>());
}

// Get a buffer suitable for using as the destination buffer.
template <typename T, int Channels = 1>
T* get_destination_buffer() {
  // Value argument is only used here to differentiate from the source buffers.
  return reinterpret_cast<T*>(get_buffer<sizeof(T) * Channels, 0xC1>());
}

// Warms up the functor then benchmarks it.
template <typename F>
void bench_functor(benchmark::State& state, F functor) {
  // warm up
  functor();
  for (auto _ : state) {
    // This code gets benchmarked
    functor();
  }
}

template <typename T, typename Function>
static void bench_binary_op(Function f, benchmark::State& state) {
  bench_functor(state, [f]() {
    (void)f(get_source_buffer_a<T>(), image_width * sizeof(T),
            get_source_buffer_b<T>(), image_width * sizeof(T),
            get_destination_buffer<T>(), image_width * sizeof(T), image_width,
            image_height);
  });
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

template <typename I, typename O, int Channels, typename Function>
static void bench_unary_op(Function f, benchmark::State& state) {
  bench_functor(state, [f]() {
    (void)f(get_source_buffer_a<I, Channels>(),
            image_width * Channels * sizeof(I),
            get_destination_buffer<O, Channels>(),
            image_width * Channels * sizeof(O), image_width, image_height);
  });
}

#define BENCH_UNARY_OP(name, channels, type)                      \
  static void name(benchmark::State& state) {                     \
    bench_unary_op<type, type, channels>(kleidicv_##name, state); \
  }                                                               \
  BENCHMARK(name)

BENCH_UNARY_OP(rgb_to_yuv_u8, 3, uint8_t);
BENCH_UNARY_OP(rgba_to_yuv_u8, 4, uint8_t);
BENCH_UNARY_OP(bgr_to_yuv_u8, 3, uint8_t);
BENCH_UNARY_OP(bgra_to_yuv_u8, 4, uint8_t);
BENCH_UNARY_OP(exp_f32, 1, float);

#define BENCH_UNARY_OP_DIFFERENT_IO_TYPES(name, itype, otype) \
  static void name(benchmark::State& state) {                 \
    bench_unary_op<itype, otype, 1>(kleidicv_##name, state);  \
  }                                                           \
  BENCHMARK(name)

BENCH_UNARY_OP_DIFFERENT_IO_TYPES(float_conversion_f32_s8, float, int8_t);
BENCH_UNARY_OP_DIFFERENT_IO_TYPES(float_conversion_f32_u8, float, uint8_t);
BENCH_UNARY_OP_DIFFERENT_IO_TYPES(float_conversion_s8_f32, int8_t, float);
BENCH_UNARY_OP_DIFFERENT_IO_TYPES(float_conversion_u8_f32, uint8_t, float);

static void min_max_loc_u8(benchmark::State& state) {
  bench_functor(state, []() {
    size_t min_offset, max_offset;
    (void)kleidicv_min_max_loc_u8(get_source_buffer_a<uint8_t>(),
                                  image_width * sizeof(uint8_t), image_width,
                                  image_height, &min_offset, &max_offset);
  });
}
BENCHMARK(min_max_loc_u8);

template <typename T, typename Function>
static void scale(Function f, float factor, float shift,
                  benchmark::State& state) {
  bench_functor(state, [f, factor, shift]() {
    (void)f(get_source_buffer_a<T>(), image_width * sizeof(T),
            get_destination_buffer<T>(), image_width * sizeof(T), image_width,
            image_height, factor, shift);
  });
}

#define BENCH_SCALE(benchname, name, factor, shift, type) \
  static void benchname(benchmark::State& state) {        \
    scale<type>(kleidicv_##name, factor, shift, state);   \
  }                                                       \
  BENCHMARK(benchname)

BENCH_SCALE(scale_u8_1, scale_u8, 1.0, 4.567, uint8_t);
BENCH_SCALE(scale_u8_generic, scale_u8, 1.234, 4.567, uint8_t);
BENCH_SCALE(scale_f32_1, scale_f32, 1.0, 4.567, float);
BENCH_SCALE(scale_f32_generic, scale_f32, 1.234, 4.567, float);

template <typename T, typename F>
static void min_max(F f, benchmark::State& state) {
  // Setup
  std::vector<T> src;
  src.resize(image_width * image_height);
  std::mt19937 generator;
  std::generate(src.begin(), src.end(), generator);

  T min_value = 0, max_value = 0;

  for (auto _ : state) {
    // This code gets benchmarked
    auto unused = f(src.data(), image_width * sizeof(T), image_width,
                    image_height, &min_value, &max_value);
    (void)unused;
  }
}

#define BENCH_MIN_MAX(name, type)             \
  static void name(benchmark::State& state) { \
    min_max<type>(kleidicv_##name, state);    \
  }                                           \
  BENCHMARK(name)

BENCH_MIN_MAX(min_max_s8, int8_t);
BENCH_MIN_MAX(min_max_u8, uint8_t);
BENCH_MIN_MAX(min_max_s16, int16_t);
BENCH_MIN_MAX(min_max_u16, uint16_t);
BENCH_MIN_MAX(min_max_s32, int32_t);
BENCH_MIN_MAX(min_max_f32, float);

template <typename T, typename F>
static void resize(F f, size_t src_width, size_t src_height, size_t dst_width,
                   size_t dst_height, benchmark::State& state) {
  bench_functor(state, [f, src_width, src_height, dst_width, dst_height]() {
    (void)f(get_source_buffer_a<T>(), src_width * sizeof(T), src_width,
            src_height, get_destination_buffer<T>(), dst_width * sizeof(T),
            dst_width, dst_height);
  });
}

template <typename T, typename F>
static void resize_upscale(F f, size_t scale_x, size_t scale_y,
                           benchmark::State& state) {
  size_t src_width = image_width / scale_x;
  size_t src_height = image_height / scale_y;
  resize<T>(f, src_width, src_height, src_width * scale_x, src_height * scale_y,
            state);
}

template <typename T, typename F>
static void resize_downscale(F f, size_t scale_x, size_t scale_y,
                             benchmark::State& state) {
  size_t dst_width = image_width / scale_x;
  size_t dst_height = image_height / scale_y;
  resize<T>(f, dst_width * scale_x, dst_height * scale_y, dst_width, dst_height,
            state);
}

static void resize_quarter_u8(benchmark::State& state) {
  resize_downscale<uint8_t>(kleidicv_resize_to_quarter_u8, 2, 2, state);
}
BENCHMARK(resize_quarter_u8);

static void resize_linear_2x2_u8(benchmark::State& state) {
  resize_upscale<uint8_t>(kleidicv_resize_linear_u8, 2, 2, state);
}
BENCHMARK(resize_linear_2x2_u8);

static void resize_linear_4x4_u8(benchmark::State& state) {
  resize_upscale<uint8_t>(kleidicv_resize_linear_u8, 4, 4, state);
}
BENCHMARK(resize_linear_4x4_u8);

static void resize_linear_2x2_f32(benchmark::State& state) {
  resize_upscale<float>(kleidicv_resize_linear_f32, 2, 2, state);
}
BENCHMARK(resize_linear_2x2_f32);

static void resize_linear_4x4_f32(benchmark::State& state) {
  resize_upscale<float>(kleidicv_resize_linear_f32, 4, 4, state);
}
BENCHMARK(resize_linear_4x4_f32);

template <typename T, int Channels, size_t WideningMultiplier,
          typename Function>
static void gaussian_blur(Function f, benchmark::State& state) {
  kleidicv_filter_context_t* context;
  kleidicv_error_t err =
      kleidicv_filter_create(&context, Channels, WideningMultiplier * sizeof(T),
                             kleidicv_rectangle_t{image_width, image_height});
  if (err != KLEIDICV_OK) {
    state.SkipWithError("Could not initialize Gaussian blur filter.");
    return;
  }

  bench_functor(state, [f, context]() {
    (void)f(get_source_buffer_a<T, Channels>(),
            image_width * Channels * sizeof(T),
            get_destination_buffer<T, Channels>(),
            image_width * Channels * sizeof(T), image_width, image_height,
            Channels, KLEIDICV_BORDER_TYPE_REFLECT, context);
  });

  (void)kleidicv_filter_release(context);
}

static void gaussian_blur_7x7_u8_1ch(benchmark::State& state) {
  gaussian_blur<uint8_t, 1, 2>(kleidicv_gaussian_blur_7x7_u8, state);
}
BENCHMARK(gaussian_blur_7x7_u8_1ch);

static void gaussian_blur_7x7_u8_3ch(benchmark::State& state) {
  gaussian_blur<uint8_t, 3, 2>(kleidicv_gaussian_blur_7x7_u8, state);
}
BENCHMARK(gaussian_blur_7x7_u8_3ch);

static void gaussian_blur_15x15_u8_1ch(benchmark::State& state) {
  gaussian_blur<uint8_t, 1, 4>(kleidicv_gaussian_blur_15x15_u8, state);
}
BENCHMARK(gaussian_blur_15x15_u8_1ch);

static void gaussian_blur_15x15_u8_3ch(benchmark::State& state) {
  gaussian_blur<uint8_t, 3, 4>(kleidicv_gaussian_blur_15x15_u8, state);
}
BENCHMARK(gaussian_blur_15x15_u8_3ch);
