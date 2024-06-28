// SPDX-FileCopyrightText: 2024 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <cfloat>
#include <cstdint>
#include <random>
#include <type_traits>

#include "framework/utils.h"
#include "kleidicv/kleidicv.h"

namespace {
// Google Mock custom matcher to check that two numbers are the same as each
// other to customisable precision. This is similar to gmock's standard
// FloatNear except that maximum absolute error is calculated relative to the
// magnitude of the arguments instead of being a fixed value.
MATCHER_P(FloatSimilar, max_relative_error, "") {
  auto [a, b] = arg;
  float max_abs_error = std::max(std::abs(a), std::abs(b)) * max_relative_error;
  return std::abs(a - b) <= max_abs_error;
}
}  // namespace

// Check that FloatSimilar behaves as designed.
TEST(FloatSimilar, FloatSimilar) {
  std::vector<float> eq_a = {0.0F, 1.000009F, 0.001000009F, 1e10F};
  std::vector<float> eq_b = {0.0F, 1.0F, 0.001F, 1.000009e10F};
  EXPECT_THAT(eq_a, ::testing::Pointwise(FloatSimilar(1e-5F), eq_b));

  std::vector<float> ne_a = {1.0F, 1.000011F, 0.001F, 1.0001e10F};
  std::vector<float> ne_b = {-1.0F, 1.0F, 0.001000011F, 1e10F};
  EXPECT_THAT(ne_a,
              ::testing::Pointwise(::testing::Not(FloatSimilar(1e-5F)), ne_b));
}

template <typename T>
static void resize_linear_unaccelerated_2x2(const T *src, size_t src_stride,
                                            size_t src_width, size_t src_height,
                                            T *dst, size_t dst_stride,
                                            size_t dst_width,
                                            size_t dst_height) {
  src_stride /= sizeof(T);
  dst_stride /= sizeof(T);

  auto lerp1d = [](T near, T far) {
    constexpr T bias = std::is_floating_point_v<T> ? 0 : 2;
    return (near * 3 + far + bias) / 4;
  };

  auto lerp2d = [](T near, T mid_a, T mid_b, T far) {
    constexpr T bias = std::is_floating_point_v<T> ? 0 : 8;
    return (near * 9 + (mid_a + mid_b) * 3 + far + bias) / 16;
  };

  auto process_row = [src_width, dst_width, lerp1d, lerp2d](
                         const T *src_row0, const T *src_row1, T *dst_row0,
                         T *dst_row1) {
    // Left element
    dst_row0[0] = lerp1d(src_row0[0], src_row1[0]);
    dst_row1[0] = lerp1d(src_row1[0], src_row0[0]);

    // Right element
    dst_row0[dst_width - 1] =
        lerp1d(src_row0[src_width - 1], src_row1[src_width - 1]);
    dst_row1[dst_width - 1] =
        lerp1d(src_row1[src_width - 1], src_row0[src_width - 1]);

    for (size_t src_x = 0; src_x + 1 < src_width; ++src_x) {
      size_t dst_x = src_x * 2 + 1;
      const T src_tl = src_row0[src_x], src_tr = src_row0[src_x + 1],
              src_bl = src_row1[src_x], src_br = src_row1[src_x + 1];
      dst_row0[dst_x] = lerp2d(src_tl, src_tr, src_bl, src_br);
      dst_row0[dst_x + 1] = lerp2d(src_tr, src_tl, src_br, src_bl);
      dst_row1[dst_x] = lerp2d(src_bl, src_tl, src_br, src_tr);
      dst_row1[dst_x + 1] = lerp2d(src_br, src_tr, src_bl, src_tl);
    }
  };

  // Top row
  process_row(src, src, dst, dst);

  // Middle rows
  for (size_t src_y = 0; src_y + 1 < src_height; ++src_y) {
    size_t dst_y = src_y * 2 + 1;
    const T *src_row0 = src + src_stride * src_y;
    const T *src_row1 = src_row0 + src_stride;
    T *dst_row0 = dst + dst_stride * dst_y;
    T *dst_row1 = dst_row0 + dst_stride;

    process_row(src_row0, src_row1, dst_row0, dst_row1);
  }

  // Bottom row
  const T *last_src_row = src + src_stride * (src_height - 1);
  T *last_dst_row = dst + dst_stride * (dst_height - 1);
  process_row(last_src_row, last_src_row, last_dst_row, last_dst_row);
}

template <typename T>
static void resize_linear_unaccelerated_4x4(const T *src, size_t src_stride,
                                            size_t src_width, size_t src_height,
                                            T *dst, size_t dst_stride,
                                            size_t dst_width,
                                            size_t dst_height) {
  src_stride /= sizeof(T);
  dst_stride /= sizeof(T);

  auto lerp1d_scalar = [](T coeff_a, T a, T coeff_b, T b) {
    constexpr T bias = std::is_floating_point_v<T> ? 0 : 4;
    return (coeff_a * a + coeff_b * b + bias) / 8;
  };
  auto lerp2d_scalar = [](T coeff_a, T a, T coeff_b, T b, T coeff_c, T c,
                          T coeff_d, T d) {
    constexpr T bias = std::is_floating_point_v<T> ? 0 : 32;
    return (coeff_a * a + coeff_b * b + coeff_c * c + coeff_d * d + bias) / 64;
  };
  // Handle top or bottom edge
  auto process_edge_row = [src_width, dst_width, lerp1d_scalar](
                              const T *src_row, T *dst_row) {
    // Left elements
    dst_row[1] = dst_row[0] = src_row[0];

    // Right elements
    dst_row[dst_width - 1] = dst_row[dst_width - 2] = src_row[src_width - 1];

    // Middle elements
    for (size_t src_x = 0; src_x + 1 < src_width; ++src_x) {
      size_t dst_x = src_x * 4 + 2;
      const T a = src_row[src_x], b = src_row[src_x + 1];
      dst_row[dst_x + 0] = lerp1d_scalar(7, a, 1, b);
      dst_row[dst_x + 1] = lerp1d_scalar(5, a, 3, b);
      dst_row[dst_x + 2] = lerp1d_scalar(3, a, 5, b);
      dst_row[dst_x + 3] = lerp1d_scalar(1, a, 7, b);
    }
  };

  auto process_row = [src_width, dst_width, lerp1d_scalar, lerp2d_scalar](
                         const T *src_row0, const T *src_row1, T *dst_row0,
                         T *dst_row1, T *dst_row2, T *dst_row3) {
    // Left elements
    const T s0l = src_row0[0], s1l = src_row1[0];
    dst_row0[0] = dst_row0[1] = lerp1d_scalar(7, s0l, 1, s1l);
    dst_row1[0] = dst_row1[1] = lerp1d_scalar(5, s0l, 3, s1l);
    dst_row2[0] = dst_row2[1] = lerp1d_scalar(3, s0l, 5, s1l);
    dst_row3[0] = dst_row3[1] = lerp1d_scalar(1, s0l, 7, s1l);

    // Right elements
    const T s0r = src_row0[src_width - 1], s1r = src_row1[src_width - 1];
    const size_t dr0 = dst_width - 2;
    const size_t dr1 = dst_width - 1;
    dst_row0[dr0] = dst_row0[dr1] = lerp1d_scalar(7, s0r, 1, s1r);
    dst_row1[dr0] = dst_row1[dr1] = lerp1d_scalar(5, s0r, 3, s1r);
    dst_row2[dr0] = dst_row2[dr1] = lerp1d_scalar(3, s0r, 5, s1r);
    dst_row3[dr0] = dst_row3[dr1] = lerp1d_scalar(1, s0r, 7, s1r);

    // Middle elements
    for (size_t src_x = 0; src_x + 1 < src_width; ++src_x) {
      size_t dst_x = src_x * 4 + 2;
      const T a = src_row0[src_x], b = src_row0[src_x + 1], c = src_row1[src_x],
              d = src_row1[src_x + 1];

      dst_row0[dst_x + 0] = lerp2d_scalar(49, a, 7, b, 7, c, 1, d);
      dst_row0[dst_x + 1] = lerp2d_scalar(35, a, 21, b, 5, c, 3, d);
      dst_row0[dst_x + 2] = lerp2d_scalar(21, a, 35, b, 3, c, 5, d);
      dst_row0[dst_x + 3] = lerp2d_scalar(7, a, 49, b, 1, c, 7, d);
      dst_row1[dst_x + 0] = lerp2d_scalar(35, a, 5, b, 21, c, 3, d);
      dst_row1[dst_x + 1] = lerp2d_scalar(25, a, 15, b, 15, c, 9, d);
      dst_row1[dst_x + 2] = lerp2d_scalar(15, a, 25, b, 9, c, 15, d);
      dst_row1[dst_x + 3] = lerp2d_scalar(5, a, 35, b, 3, c, 21, d);
      dst_row2[dst_x + 0] = lerp2d_scalar(21, a, 3, b, 35, c, 5, d);
      dst_row2[dst_x + 1] = lerp2d_scalar(15, a, 9, b, 25, c, 15, d);
      dst_row2[dst_x + 2] = lerp2d_scalar(9, a, 15, b, 15, c, 25, d);
      dst_row2[dst_x + 3] = lerp2d_scalar(3, a, 21, b, 5, c, 35, d);
      dst_row3[dst_x + 0] = lerp2d_scalar(7, a, 1, b, 49, c, 7, d);
      dst_row3[dst_x + 1] = lerp2d_scalar(5, a, 3, b, 35, c, 21, d);
      dst_row3[dst_x + 2] = lerp2d_scalar(3, a, 5, b, 21, c, 35, d);
      dst_row3[dst_x + 3] = lerp2d_scalar(1, a, 7, b, 7, c, 49, d);
    }
  };

  // Top rows
  process_edge_row(src, dst);
  memcpy(dst + dst_stride, dst, dst_stride * sizeof(T));

  // Middle rows
  for (size_t src_y = 0; src_y + 1 < src_height; ++src_y) {
    size_t dst_y = src_y * 4 + 2;
    const T *src_row0 = src + src_stride * src_y;
    const T *src_row1 = src_row0 + src_stride;
    T *dst_row0 = dst + dst_stride * dst_y;
    T *dst_row1 = dst_row0 + dst_stride;
    T *dst_row2 = dst_row1 + dst_stride;
    T *dst_row3 = dst_row2 + dst_stride;

    process_row(src_row0, src_row1, dst_row0, dst_row1, dst_row2, dst_row3);
  }

  // Bottom rows
  process_edge_row(src + src_stride * (src_height - 1),
                   dst + dst_stride * (dst_height - 2));
  memcpy(dst + dst_stride * (dst_height - 1),
         dst + dst_stride * (dst_height - 2), dst_stride * sizeof(T));
}

template <typename T>
static void resize_linear_unaccelerated_generic_upscale(
    const T *src, size_t src_stride, size_t src_width, size_t src_height,
    T *dst, size_t dst_stride, size_t dst_width, size_t dst_height) {
  src_stride /= sizeof(T);
  dst_stride /= sizeof(T);
  size_t scale_width = dst_width / src_width;
  size_t scale_height = dst_height / src_height;

  auto lerp1d_horizontal = [scale_width](T coeff_a, T a, T coeff_b, T b) {
    T bias = std::is_floating_point_v<T> ? 0 : scale_width;
    return (coeff_a * a + coeff_b * b + bias) / (2 * scale_width);
  };

  auto lerp1d_vertical = [scale_height](T coeff_a, T a, T coeff_b, T b) {
    T bias = std::is_floating_point_v<T> ? 0 : scale_height;
    return (coeff_a * a + coeff_b * b + bias) / (2 * scale_height);
  };

  auto lerp2d = [scale_width, scale_height](T coeff_a, T a, T coeff_b, T b,
                                            T coeff_c, T c, T coeff_d, T d) {
    T bias = std::is_floating_point_v<T> ? 0 : (2 * scale_height * scale_width);
    return (coeff_a * a + coeff_b * b + coeff_c * c + coeff_d * d + bias) /
           (4 * scale_height * scale_width);
  };
  // Handle top or bottom edge
  auto process_edge_row = [src_width, dst_width, scale_width, scale_height,
                           lerp1d_horizontal](const T *src_row, T *dst_row,
                                              size_t dst_stride) {
    for (size_t y = 0; y < scale_height / 2; ++y) {
      for (size_t x = 0; x < scale_width / 2; ++x) {
        // Left elements
        dst_row[x] = src_row[0];
        // Right elements
        dst_row[dst_width - scale_width / 2 + x] = src_row[src_width - 1];
      }
      // Middle elements
      for (size_t src_x = 0; src_x + 1 < src_width; ++src_x) {
        size_t dst_x = src_x * scale_width + scale_width / 2;
        const T a = src_row[src_x], b = src_row[src_x + 1];
        for (size_t x = 0; x < scale_width; ++x) {
          dst_row[dst_x + x] =
              lerp1d_horizontal((scale_width - x) * 2 - 1, a, x * 2 + 1, b);
        }
      }
      dst_row += dst_stride;
    }
  };

  auto process_row = [src_width, dst_width, scale_width, scale_height,
                      lerp1d_vertical,
                      lerp2d](const T *src_row0, const T *src_row1, T *dst_row,
                              size_t dst_stride) {
    for (size_t y = 0; y < scale_height; ++y) {
      for (size_t x = 0; x < scale_width / 2; ++x) {
        // Left elements
        dst_row[x] = lerp1d_vertical((scale_height - y) * 2 - 1, src_row0[0],
                                     y * 2 + 1, src_row1[0]);
        // Right elements
        dst_row[dst_width - scale_width / 2 + x] =
            lerp1d_vertical((scale_height - y) * 2 - 1, src_row0[src_width - 1],
                            y * 2 + 1, src_row1[src_width - 1]);
      }
      // Middle elements
      for (size_t src_x = 0; src_x + 1 < src_width; ++src_x) {
        size_t dst_x = src_x * scale_width + scale_width / 2;
        const T a = src_row0[src_x], b = src_row0[src_x + 1];
        const T c = src_row1[src_x], d = src_row1[src_x + 1];
        for (size_t x = 0; x < scale_width; ++x) {
          dst_row[dst_x + x] =
              lerp2d(((scale_width - x) * 2 - 1) * ((scale_height - y) * 2 - 1),
                     a, (x * 2 + 1) * ((scale_height - y) * 2 - 1), b,
                     ((scale_width - x) * 2 - 1) * (y * 2 + 1), c,
                     (x * 2 + 1) * (y * 2 + 1), d);
        }
      }
      dst_row += dst_stride;
    }
  };

  // Top rows
  process_edge_row(src, dst, dst_stride);

  // Middle rows
  for (size_t src_y = 0; src_y + 1 < src_height; ++src_y) {
    size_t dst_y = src_y * scale_height + scale_height / 2;
    const T *src_row0 = src + src_stride * src_y;
    const T *src_row1 = src_row0 + src_stride;
    process_row(src_row0, src_row1, dst + dst_stride * dst_y, dst_stride);
  }

  // Bottom rows
  process_edge_row(src + src_stride * (src_height - 1),
                   dst + dst_stride * (dst_height - scale_height / 2),
                   dst_stride);
}

template <typename T>
static void resize_linear_unaccelerated(const T *src, size_t src_stride,
                                        size_t src_width, size_t src_height,
                                        T *dst, size_t dst_stride,
                                        size_t dst_width, size_t dst_height) {
  if (src_width * 2 == dst_width && src_height * 2 == dst_height) {
    resize_linear_unaccelerated_2x2(src, src_stride, src_width, src_height, dst,
                                    dst_stride, dst_width, dst_height);
  } else if (src_width * 4 == dst_width && src_height * 4 == dst_height) {
    resize_linear_unaccelerated_4x4(src, src_stride, src_width, src_height, dst,
                                    dst_stride, dst_width, dst_height);
  } else if (src_width > 0 && src_height > 0 && dst_width % src_width == 0 &&
             dst_height % src_height == 0 && (dst_width / src_width) % 2 == 0 &&
             (dst_height / src_height) % 2 == 0) {
    // even integer scaling only
    resize_linear_unaccelerated_generic_upscale(src, src_stride, src_width,
                                                src_height, dst, dst_stride,
                                                dst_width, dst_height);
  }
}

// Provides an alternative type instead of char to avoid values being printed as
// char.
template <typename T>
struct PrintTypeGetter {
  using type = T;
};

template <>
struct PrintTypeGetter<int8_t> {
  using type = int;
};

template <>
struct PrintTypeGetter<uint8_t> {
  using type = unsigned;
};

inline kleidicv_error_t kleidicv_resize_linear(
    const uint8_t *src, size_t src_stride, size_t src_width, size_t src_height,
    uint8_t *dst, size_t dst_stride, size_t dst_width, size_t dst_height) {
  return kleidicv_resize_linear_u8(src, src_stride, src_width, src_height, dst,
                                   dst_stride, dst_width, dst_height);
}

inline kleidicv_error_t kleidicv_resize_linear(
    const float *src, size_t src_stride, size_t src_width, size_t src_height,
    float *dst, size_t dst_stride, size_t dst_width, size_t dst_height) {
  return kleidicv_resize_linear_f32(src, src_stride, src_width, src_height, dst,
                                    dst_stride, dst_width, dst_height);
}

template <typename T>
class ResizeLinear : public testing::Test {};

using ResizeLinearTypes = testing::Types<uint8_t, float>;
TYPED_TEST_SUITE(ResizeLinear, ResizeLinearTypes);

TYPED_TEST(ResizeLinear, NotImplemented) {
  const TypeParam src[1] = {};
  TypeParam dst[4];

  EXPECT_EQ(KLEIDICV_ERROR_NOT_IMPLEMENTED,
            kleidicv_resize_linear(src, sizeof(TypeParam), 1, 1, dst,
                                   sizeof(TypeParam) * 2, 2, 1));
  EXPECT_EQ(KLEIDICV_ERROR_NOT_IMPLEMENTED,
            kleidicv_resize_linear(src, sizeof(TypeParam), 1, 1, dst,
                                   sizeof(TypeParam), 1, 2));
  EXPECT_EQ(KLEIDICV_ERROR_NOT_IMPLEMENTED,
            kleidicv_resize_linear(src, sizeof(TypeParam), 1, 1, dst,
                                   sizeof(TypeParam) * 4, 4, 2));
  EXPECT_EQ(KLEIDICV_ERROR_NOT_IMPLEMENTED,
            kleidicv_resize_linear(src, sizeof(TypeParam), 1, 1, dst,
                                   sizeof(TypeParam) * 2, 2, 4));
}

TYPED_TEST(ResizeLinear, NullPointer) {
  const TypeParam src[1] = {};
  TypeParam dst[4];
  kleidicv_error_t (*f)(const TypeParam *, size_t, size_t, size_t, TypeParam *,
                        size_t, size_t, size_t) = kleidicv_resize_linear;
  test::test_null_args(f, src, sizeof(TypeParam), 1, 1, dst,
                       sizeof(TypeParam) * 2, 2, 2);
}

TYPED_TEST(ResizeLinear, InvalidImageSize) {
  const TypeParam src[1] = {};
  TypeParam dst[4];

  EXPECT_EQ(KLEIDICV_ERROR_RANGE,
            kleidicv_resize_linear(
                src, sizeof(TypeParam), 1, 1, dst,
                sizeof(TypeParam) * (KLEIDICV_MAX_IMAGE_PIXELS + 1),
                KLEIDICV_MAX_IMAGE_PIXELS + 1, 1));

  EXPECT_EQ(KLEIDICV_ERROR_RANGE,
            kleidicv_resize_linear(
                src, sizeof(TypeParam), 1, 1, dst,
                sizeof(TypeParam) * KLEIDICV_MAX_IMAGE_PIXELS,
                KLEIDICV_MAX_IMAGE_PIXELS, KLEIDICV_MAX_IMAGE_PIXELS));
}

TYPED_TEST(ResizeLinear, ZeroImageSize) {
  const TypeParam src[1] = {};
  TypeParam dst[1];
  EXPECT_EQ(KLEIDICV_OK, kleidicv_resize_linear(src, 0, 0, 0, dst, 0, 0, 0));
  EXPECT_EQ(KLEIDICV_OK,
            kleidicv_resize_linear(src, sizeof(TypeParam), 1, 0, dst,
                                   sizeof(TypeParam) * 2, 2, 0));
  EXPECT_EQ(KLEIDICV_OK, kleidicv_resize_linear(src, 0, 0, 1, dst, 0, 0, 2));
}

template <typename T>
static void do_large_dimensions_test(size_t x_scale, size_t y_scale) {
  size_t src_width = 2049;
  size_t src_height = 5;
  size_t src_stride_pixels = src_width + 6;
  size_t dst_width = src_width * x_scale;
  size_t dst_height = src_height * y_scale;
  size_t dst_stride_pixels = dst_width + 3;

  std::vector<T> src, dst, expected_data;
  src.resize(src_stride_pixels * src_height);
  dst.resize(dst_stride_pixels * dst_height);
  expected_data.resize(dst_stride_pixels * dst_height);
  std::mt19937 generator{
      static_cast<std::mt19937::result_type>(test::Options::seed())};
  std::generate(src.begin(), src.end(), generator);
  resize_linear_unaccelerated(src.data(), src_stride_pixels * sizeof(T),
                              src_width, src_height, expected_data.data(),
                              dst_stride_pixels * sizeof(T), dst_width,
                              dst_height);
  ASSERT_EQ(KLEIDICV_OK,
            kleidicv_resize_linear(src.data(), src_stride_pixels * sizeof(T),
                                   src_width, src_height, dst.data(),
                                   dst_stride_pixels * sizeof(T), dst_width,
                                   dst_height));

  for (size_t y = 0; y < dst_height; ++y) {
    std::vector<typename PrintTypeGetter<T>::type> actual{
        dst.begin() + static_cast<ptrdiff_t>(y * dst_stride_pixels),
        dst.begin() +
            static_cast<ptrdiff_t>(y * dst_stride_pixels + dst_width)},
        expected{expected_data.begin() +
                     static_cast<ptrdiff_t>(y * dst_stride_pixels),
                 expected_data.begin() +
                     static_cast<ptrdiff_t>(y * dst_stride_pixels + dst_width)};
    if constexpr (std::is_floating_point_v<T>) {
      EXPECT_THAT(actual, ::testing::Pointwise(FloatSimilar(2e-6F), expected))
          << "Row #" << y;
    } else {
      EXPECT_THAT(actual, ::testing::ElementsAreArray(expected))
          << "Row #" << y;
    }
  }
}

TYPED_TEST(ResizeLinear, LargeDimensions2x2) {
  do_large_dimensions_test<TypeParam>(2, 2);
}

TYPED_TEST(ResizeLinear, LargeDimensions4x4) {
  do_large_dimensions_test<TypeParam>(4, 4);
}

TEST(ResizeLinearFloat, LargeDimensions8x8) {
  do_large_dimensions_test<float>(8, 8);
}

TEST(ResizeLinearU8, NotImplemented_8x8) {
  const uint8_t src[1] = {};
  uint8_t dst[4];

  EXPECT_EQ(KLEIDICV_ERROR_NOT_IMPLEMENTED,
            kleidicv_resize_linear(src, sizeof(uint8_t), 1, 1, dst,
                                   sizeof(uint8_t) * 8, 8, 8));
}

// Parameterised tests
template <typename T>
struct ResizeTestParams {
  std::vector<std::vector<T>> src;
  std::vector<std::vector<T>> expected;

  friend void PrintTo(const ResizeTestParams &v, std::ostream *os) {
    *os << "([\n";
    for (size_t y = 0; y < v.src.size(); ++y) {
      const auto &row = v.src[y];
      *os << "  [";
      for (size_t x = 0; x < row.size(); ++x) {
        *os << std::setw(3) << typename PrintTypeGetter<T>::type{row[x]};
        if (x + 1 != row.size()) {
          *os << ", ";
        }
      }
      *os << "]";
      if (y + 1 != v.src.size()) {
        *os << ",\n";
      }
    }
    *os << "], " << v.expected.size() << ")";
  }
};

template <typename T>
void do_linear_resize_test(const ResizeTestParams<T> &param, size_t src_padding,
                           size_t dst_padding) {
  size_t src_width = param.src[0].size();
  size_t src_height = param.src.size();
  size_t src_stride_pixels = src_width + src_padding;
  size_t dst_width = param.expected[0].size();
  size_t dst_height = param.expected.size();
  size_t dst_stride_pixels = dst_width + dst_padding;

  auto flatten = [](const std::vector<std::vector<T>> &vec2d, size_t padding) {
    std::vector<T> result;
    for (const auto &row : vec2d) {
      result.insert(result.end(), row.begin(), row.end());
      result.resize(result.size() + padding);
    }
    return result;
  };
  std::vector<T> src = flatten(param.src, src_padding), dst;
  dst.resize(dst_stride_pixels * dst_height);

  ASSERT_EQ(KLEIDICV_OK,
            kleidicv_resize_linear(src.data(), src_stride_pixels * sizeof(T),
                                   src_width, src_height, dst.data(),
                                   dst_stride_pixels * sizeof(T), dst_width,
                                   dst_height));
  for (size_t y = 0; y < dst_height; ++y) {
    std::vector<typename PrintTypeGetter<T>::type> actual{
        dst.begin() + static_cast<ptrdiff_t>(y * dst_stride_pixels),
        dst.begin() +
            static_cast<ptrdiff_t>(y * dst_stride_pixels + dst_width)},
        expected{param.expected[y].begin(), param.expected[y].end()};
    if constexpr (std::is_floating_point_v<T>) {
      EXPECT_THAT(actual, ::testing::Pointwise(FloatSimilar(2e-6F), expected))
          << "Row #" << y;
    } else {
      EXPECT_THAT(actual, ::testing::ElementsAreArray(expected))
          << "Row #" << y;
    }
  }
}

class ResizeLinearU8
    : public testing::TestWithParam<ResizeTestParams<uint8_t>> {};

TEST_P(ResizeLinearU8, ResizeNoPadding) {
  do_linear_resize_test(GetParam(), 0, 0);
}

TEST_P(ResizeLinearU8, ResizeWithPadding) {
  do_linear_resize_test(GetParam(), 1, 2);
}

TEST_P(ResizeLinearU8, ResizePadDst) {
  do_linear_resize_test(GetParam(), 0, 3);
}

TEST_P(ResizeLinearU8, ResizePadSrc) {
  do_linear_resize_test(GetParam(), 4, 0);
}

using Pu8 = ResizeTestParams<uint8_t>;

INSTANTIATE_TEST_SUITE_P(
    ResizeLinear, ResizeLinearU8,
    testing::Values(
        // 1*1 -> 2*2
        Pu8{{{123}}, {{123, 123}, {123, 123}}},
        // 2*1 -> 4*2
        Pu8{{{0, 255}}, {{0, 64, 191, 255}, {0, 64, 191, 255}}},
        // 2*1 -> 4*2. Check rounding behaviour.
        Pu8{{{1, 63}}, {{1, 17, 48, 63}, {1, 17, 48, 63}}},
        // 2*2 -> 4*4
        Pu8{{{0, 255}, {100, 8}},
            {{0, 64, 191, 255},
             {25, 67, 151, 193},
             {75, 74, 71, 70},
             {100, 77, 31, 8}}},
        // 3*3 -> 6*6
        Pu8{{{1, 63, 164}, {28, 251, 35}, {218, 64, 99}},
            {{1, 17, 48, 88, 139, 164},
             {8, 33, 84, 115, 126, 132},
             {21, 67, 158, 170, 101, 67},
             {76, 108, 172, 166, 89, 51},
             {171, 156, 126, 104, 90, 83},
             {218, 180, 103, 73, 90, 99}}},
        // 4*4 -> 8*8
        Pu8{{{10, 30, 5, 70},
             {255, 11, 11, 12},
             {127, 127, 128, 0},
             {200, 100, 150, 50}},
            {{10, 15, 25, 24, 11, 21, 54, 70},
             {71, 60, 37, 21, 11, 19, 43, 56},
             {194, 149, 60, 14, 11, 14, 22, 27},
             {223, 177, 86, 40, 40, 32, 17, 9},
             {159, 144, 113, 98, 99, 75, 27, 3},
             {145, 139, 127, 124, 130, 103, 43, 13},
             {182, 163, 126, 116, 135, 118, 64, 38},
             {200, 175, 125, 113, 138, 125, 75, 50}}},
        // 35*2 -> 70*4
        Pu8{{{0,   1,   2,   3,   4,   5,   6,   7,   8,   9,   10,  82,
              155, 104, 108, 227, 46,  162, 21,  220, 235, 183, 113, 225,
              146, 196, 144, 104, 148, 19,  126, 172, 9,   12,  61},
             {4,  5,   6,   7,   8,  9,   10, 11,  12,  13,  14, 193,
              44, 105, 191, 106, 73, 148, 13, 161, 118, 21,  3,  34,
              40, 150, 120, 68,  75, 14,  31, 124, 221, 214, 146}},
            {{0,   0,   1,   1,   2,   2,   3,   3,   4,   4,   5,   5,
              6,   6,   7,   7,   8,   8,   9,   9,   10,  28,  64,  100,
              137, 142, 117, 105, 107, 138, 197, 182, 91,  75,  133, 127,
              56,  71,  170, 224, 231, 222, 196, 166, 131, 141, 197, 205,
              166, 159, 184, 183, 157, 134, 114, 115, 137, 116, 51,  46,
              99,  138, 161, 131, 50,  10,  11,  24,  49,  61},
             {1,   1,   2,   2,   3,   3,   4,   4,   5,   5,   6,   6,
              7,   7,   8,   8,   9,   9,   10,  10,  11,  36,  85,  114,
              123, 122, 110, 110, 123, 146, 180, 161, 89,  79,  132, 124,
              54,  66,  159, 205, 206, 190, 158, 128, 100, 108, 154, 163,
              134, 136, 168, 173, 150, 127, 106, 104, 121, 102, 46,  39,
              81,  117, 146, 136, 87,  62,  62,  67,  77,  82},
             {3,  3,  4,   4,   5,   5,   6,   6,   7,   7,  8,   8,
              9,  9,  10,  10,  11,  11,  12,  12,  13,  51, 127, 142,
              95, 80, 97,  121, 154, 162, 145, 119, 84,  88, 130, 117,
              49, 55, 136, 169, 154, 126, 83,  54,  38,  43, 69,  78,
              70, 90, 138, 153, 135, 114, 89,  81,  89,  74, 35,  25,
              45, 75, 116, 144, 160, 167, 165, 154, 134, 125},
             {4,  4,  5,   5,   6,   6,   7,   7,   8,   8,  9,   9,
              10, 10, 11,  11,  12,  12,  13,  13,  14,  59, 148, 156,
              81, 59, 90,  127, 170, 170, 127, 98,  81,  92, 129, 114,
              47, 50, 124, 150, 129, 94,  45,  17,  8,   11, 26,  36,
              39, 68, 123, 143, 128, 107, 81,  70,  73,  60, 29,  18,
              27, 54, 101, 148, 197, 219, 216, 197, 163, 146}}},
        // 2*2 -> 8*8
        Pu8{{{0, 255}, {128, 124}},
            {{0, 0, 32, 96, 159, 223, 255, 255},
             {0, 0, 32, 96, 159, 223, 255, 255},
             {16, 16, 44, 99, 155, 211, 239, 239},
             {48, 48, 68, 107, 147, 186, 206, 206},
             {80, 80, 92, 115, 138, 161, 173, 173},
             {112, 112, 116, 123, 130, 137, 140, 140},
             {128, 128, 128, 127, 126, 125, 124, 124},
             {128, 128, 128, 127, 126, 125, 124, 124}}},
        // 35*2 -> 140*8
        Pu8{{{0,   1,   2,   3,   4,   5,   6,   7,   8,   9,   10,  82,
              155, 104, 108, 227, 46,  162, 21,  220, 235, 183, 113, 225,
              146, 196, 144, 104, 148, 19,  126, 172, 9,   12,  61},
             {4,  5,   6,   7,   8,  9,   10, 11,  12,  13,  14, 193,
              44, 105, 191, 106, 73, 148, 13, 161, 118, 21,  3,  34,
              40, 150, 120, 68,  75, 14,  31, 124, 221, 214, 146}},
            {{0,   0,   0,   0,   1,   1,   1,   1,   2,   2,   2,   2,   3,
              3,   3,   3,   4,   4,   4,   4,   5,   5,   5,   5,   6,   6,
              6,   6,   7,   7,   7,   7,   8,   8,   8,   8,   9,   9,   9,
              9,   10,  10,  19,  37,  55,  73,  91,  109, 128, 146, 149, 136,
              123, 110, 105, 106, 107, 108, 123, 153, 182, 212, 204, 159, 114,
              69,  61,  90,  119, 148, 144, 109, 74,  39,  46,  96,  145, 195,
              222, 226, 229, 233, 229, 216, 203, 190, 174, 157, 139, 122, 127,
              155, 183, 211, 215, 195, 176, 156, 152, 165, 177, 190, 190, 177,
              164, 151, 139, 129, 119, 109, 110, 121, 132, 143, 132, 100, 67,
              35,  32,  59,  86,  113, 132, 143, 155, 166, 152, 111, 70,  29,
              9,   10,  11,  12,  18,  30,  43,  55,  61,  61},
             {0,   0,   0,   0,   1,   1,   1,   1,   2,   2,   2,   2,   3,
              3,   3,   3,   4,   4,   4,   4,   5,   5,   5,   5,   6,   6,
              6,   6,   7,   7,   7,   7,   8,   8,   8,   8,   9,   9,   9,
              9,   10,  10,  19,  37,  55,  73,  91,  109, 128, 146, 149, 136,
              123, 110, 105, 106, 107, 108, 123, 153, 182, 212, 204, 159, 114,
              69,  61,  90,  119, 148, 144, 109, 74,  39,  46,  96,  145, 195,
              222, 226, 229, 233, 229, 216, 203, 190, 174, 157, 139, 122, 127,
              155, 183, 211, 215, 195, 176, 156, 152, 165, 177, 190, 190, 177,
              164, 151, 139, 129, 119, 109, 110, 121, 132, 143, 132, 100, 67,
              35,  32,  59,  86,  113, 132, 143, 155, 166, 152, 111, 70,  29,
              9,   10,  11,  12,  18,  30,  43,  55,  61,  61},
             {1,   1,   1,   1,   1,   1,   2,   2,   2,   2,   3,   3,   3,
              3,   4,   4,   4,   4,   5,   5,   5,   5,   6,   6,   6,   6,
              7,   7,   7,   7,   8,   8,   8,   8,   9,   9,   9,   9,   10,
              10,  10,  10,  21,  43,  64,  85,  102, 113, 124, 135, 137, 127,
              118, 109, 106, 109, 113, 117, 130, 153, 177, 200, 192, 151, 110,
              70,  63,  91,  119, 146, 143, 108, 73,  38,  44,  92,  140, 189,
              214, 216, 217, 219, 213, 199, 184, 170, 155, 139, 123, 107, 112,
              137, 163, 188, 193, 175, 158, 141, 140, 154, 169, 183, 184, 172,
              159, 147, 136, 125, 115, 105, 104, 114, 124, 134, 124, 94,  64,
              33,  30,  54,  78,  102, 121, 134, 147, 160, 150, 117, 84,  52,
              36,  36,  37,  37,  42,  50,  59,  67,  72,  72},
             {2,   2,   2,   2,   2,   2,   3,   3,   3,   3,   4,   4,   4,
              4,   5,   5,   5,   5,   6,   6,   6,   6,   7,   7,   7,   7,
              8,   8,   8,   8,   9,   9,   9,   9,   10,  10,  10,  10,  11,
              11,  11,  11,  26,  54,  82,  110, 122, 120, 117, 115, 112, 110,
              108, 106, 109, 117, 126, 135, 144, 155, 166, 176, 166, 135, 103,
              72,  69,  94,  119, 144, 139, 105, 70,  35,  40,  85,  130, 175,
              197, 195, 194, 192, 183, 165, 148, 131, 116, 103, 91,  78,  82,
              102, 123, 143, 147, 136, 124, 112, 115, 133, 152, 170, 173, 162,
              151, 140, 129, 118, 107, 96,  94,  102, 109, 117, 108, 82,  56,
              30,  26,  45,  63,  81,  98,  114, 130, 146, 146, 129, 113, 97,
              88,  88,  88,  88,  88,  90,  91,  92,  93,  93},
             {3,   3,   3,   3,   3,   3,   4,   4,   4,   4,   5,   5,   5,
              5,   6,   6,   6,   6,   7,   7,   7,   7,   8,   8,   8,   8,
              9,   9,   9,   9,   10,  10,  10,  10,  11,  11,  11,  11,  12,
              12,  12,  12,  30,  65,  99,  134, 143, 127, 110, 94,  88,  93,
              98,  102, 112, 125, 139, 153, 159, 157, 155, 152, 140, 118, 96,
              74,  74,  97,  119, 142, 136, 102, 67,  33,  37,  79,  120, 162,
              180, 175, 170, 165, 152, 132, 112, 92,  77,  68,  58,  49,  52,
              67,  83,  98,  102, 96,  89,  83,  91,  113, 134, 156, 162, 153,
              143, 134, 123, 111, 99,  87,  84,  89,  95,  100, 92,  70,  48,
              27,  22,  35,  48,  60,  76,  95,  114, 133, 142, 142, 142, 142,
              141, 140, 139, 139, 135, 129, 123, 117, 114, 114},
             {4,   4,   4,   4,   4,   4,   5,   5,   5,   5,   6,   6,   6,
              6,   7,   7,   7,   7,   8,   8,   8,   8,   9,   9,   9,   9,
              10,  10,  10,  10,  11,  11,  11,  11,  12,  12,  12,  12,  13,
              13,  13,  13,  34,  76,  117, 158, 164, 134, 103, 73,  64,  76,
              87,  99,  114, 133, 152, 171, 173, 158, 143, 129, 115, 102, 89,
              76,  80,  100, 120, 140, 133, 99,  65,  31,  33,  72,  110, 149,
              164, 155, 146, 137, 121, 98,  76,  53,  38,  32,  26,  20,  22,
              32,  42,  53,  57,  56,  55,  54,  66,  92,  117, 143, 152, 143,
              135, 127, 117, 104, 91,  79,  74,  77,  80,  83,  75,  58,  41,
              23,  18,  25,  32,  39,  54,  76,  97,  119, 138, 154, 170, 186,
              194, 192, 191, 189, 182, 169, 155, 142, 135, 135},
             {4,   4,   4,   4,   5,   5,   5,   5,   6,   6,   6,   6,   7,
              7,   7,   7,   8,   8,   8,   8,   9,   9,   9,   9,   10,  10,
              10,  10,  11,  11,  11,  11,  12,  12,  12,  12,  13,  13,  13,
              13,  14,  14,  36,  81,  126, 171, 174, 137, 100, 63,  52,  67,
              82,  97,  116, 137, 159, 180, 180, 159, 138, 117, 102, 94,  85,
              77,  82,  101, 120, 139, 131, 97,  64,  30,  32,  69,  106, 143,
              156, 145, 134, 123, 106, 82,  57,  33,  19,  14,  10,  5,   7,
              15,  22,  30,  35,  36,  38,  39,  54,  81,  109, 136, 146, 139,
              131, 124, 114, 101, 88,  75,  69,  71,  72,  74,  67,  52,  37,
              22,  16,  20,  25,  29,  43,  66,  89,  112, 136, 160, 185, 209,
              220, 218, 217, 215, 206, 189, 172, 155, 146, 146},
             {4,   4,   4,   4,   5,   5,   5,   5,   6,   6,   6,   6,   7,
              7,   7,   7,   8,   8,   8,   8,   9,   9,   9,   9,   10,  10,
              10,  10,  11,  11,  11,  11,  12,  12,  12,  12,  13,  13,  13,
              13,  14,  14,  36,  81,  126, 171, 174, 137, 100, 63,  52,  67,
              82,  97,  116, 137, 159, 180, 180, 159, 138, 117, 102, 94,  85,
              77,  82,  101, 120, 139, 131, 97,  64,  30,  32,  69,  106, 143,
              156, 145, 134, 123, 106, 82,  57,  33,  19,  14,  10,  5,   7,
              15,  22,  30,  35,  36,  38,  39,  54,  81,  109, 136, 146, 139,
              131, 124, 114, 101, 88,  75,  69,  71,  72,  74,  67,  52,  37,
              22,  16,  20,  25,  29,  43,  66,  89,  112, 136, 160, 185, 209,
              220, 218, 217, 215, 206, 189, 172, 155, 146, 146}}}));

class ResizeLinearF32 : public testing::TestWithParam<ResizeTestParams<float>> {
};

TEST_P(ResizeLinearF32, ResizeNoPadding) {
  do_linear_resize_test(GetParam(), 0, 0);
}

TEST_P(ResizeLinearF32, ResizeWithPadding) {
  do_linear_resize_test(GetParam(), 1, 2);
}

TEST_P(ResizeLinearF32, ResizePadDst) {
  do_linear_resize_test(GetParam(), 0, 3);
}

TEST_P(ResizeLinearF32, ResizePadSrc) {
  do_linear_resize_test(GetParam(), 4, 0);
}

using Pf32 = ResizeTestParams<float>;

INSTANTIATE_TEST_SUITE_P(
    ResizeLinear, ResizeLinearF32,
    testing::Values(
        // 1*1 -> 2*2
        Pf32{{{123}}, {{123, 123}, {123, 123}}},
        // 2*1 -> 4*2
        Pf32{{{0, 255}},
             {{0, 63.75F, 191.25F, 255}, {0, 63.75F, 191.25F, 255}}},
        // 2*2 -> 4*4
        Pf32{{{FLT_MAX, 1e38F}, {0, FLT_TRUE_MIN}},
             {{FLT_MAX, 2.8021173e38F, 1.6007058e38F, 1e38F},
              {2.5521173e38F, 2.101588e38F, 1.2005294e38F, 7.5e37F},
              {8.5070577e37F, 7.0052933e37F, 4.0017645e37F, 2.5e37F},
              {0, 0, FLT_TRUE_MIN, FLT_TRUE_MIN}}},
        // 3*3 -> 6*6
        Pf32{{{1, 63, 164}, {28, 251, 35}, {218, 64, 99}},
             {{1, 16.5F, 47.5F, 88.25F, 138.75F, 164},
              {7.75F, 33.3125F, 84.4375F, 115.4375F, 126.3125F, 131.75F},
              {21.25F, 66.9375F, 158.3125F, 169.8125F, 101.4375F, 67.25F},
              {75.5F, 107.6875F, 172.0625F, 165.9375F, 89.3125F, 51},
              {170.5F, 155.5625F, 125.6875F, 103.8125F, 89.9375F, 83},
              {218, 179.5F, 102.5F, 72.75F, 90.25F, 99}}},
        // 4*4 -> 8*8
        Pf32{
            {{10, 30, 5, 70},
             {255, 11, 11, 12},
             {127, 127, 128, 0},
             {200, 100, 150, 50}},
            {{10, 15, 25, 23.75F, 11.25F, 21.25F, 53.75F, 70},
             {71.25F, 59.75F, 36.75F, 20.5625F, 11.1875F, 18.75F, 43.25F,
              55.5F},
             {193.75F, 149.25F, 60.25F, 14.1875F, 11.0625F, 13.75F, 22.25F,
              26.5F},
             {223, 177.25F, 85.75F, 40.0625F, 40.1875F, 32.4375F, 16.8125F, 9},
             {159, 143.75F, 113.25F, 98.1875F, 98.5625F, 74.8125F, 26.9375F, 3},
             {145.25F, 139, 126.5F, 123.5625F, 130.1875F, 103.25F, 42.75F,
              12.5F},
             {181.75F, 163, 125.5F, 116.1875F, 135.0625F, 117.75F, 64.25F,
              37.5F},
             {200, 175, 125, 112.5F, 137.5F, 125, 75, 50}}},
        // 35*2 -> 70*4
        Pf32{{{0,   1,   2,       3,       4,   5,   6,   7,   8,
               9,   10,  FLT_MAX, FLT_MAX, 104, 108, 227, 46,  162,
               21,  220, 235,     183,     113, 225, 146, 196, 144,
               104, 148, 19,      126,     172, 9,   12,  61},
              {4,       5,   6,   7,   8,  9,   10, 11,  12,  13,  14, FLT_MAX,
               FLT_MAX, 105, 191, 106, 73, 148, 13, 161, 118, 21,  3,  34,
               40,      150, 120, 68,  75, 14,  31, 124, 221, 214, 146}},
             {{0.0F,         0.25F,         0.75F,         1.25F,
               1.75F,        2.25F,         2.75F,         3.25F,
               3.75F,        4.25F,         4.75F,         5.25F,
               5.75F,        6.25F,         6.75F,         7.25F,
               7.75F,        8.25F,         8.75F,         9.25F,
               9.75F,        8.50705e+37F,  2.552115e+38F, 3.40282e+38F,
               3.40282e+38F, 2.552115e+38F, 8.50705e+37F,  105.0F,
               107.0F,       137.75F,       197.25F,       181.75F,
               91.25F,       75.0F,         133.0F,        126.75F,
               56.25F,       70.75F,        170.25F,       223.75F,
               231.25F,      222.0F,        196.0F,        165.5F,
               130.5F,       141.0F,        197.0F,        205.25F,
               165.75F,      158.5F,        183.5F,        183.0F,
               157.0F,       134.0F,        114.0F,        115.0F,
               137.0F,       115.75F,       51.25F,        45.75F,
               99.25F,       137.5F,        160.5F,        131.25F,
               49.75F,       9.75F,         11.25F,        24.25F,
               48.75F,       61.0F},
              {1.0F,         1.25F,         1.75F,         2.25F,
               2.75F,        3.25F,         3.75F,         4.25F,
               4.75F,        5.25F,         5.75F,         6.25F,
               6.75F,        7.25F,         7.75F,         8.25F,
               8.75F,        9.25F,         9.75F,         10.25F,
               10.75F,       8.50705e+37F,  2.552115e+38F, 3.40282e+38F,
               3.40282e+38F, 2.552115e+38F, 8.50705e+37F,  110.375F,
               122.625F,     145.75F,       179.75F,       160.75F,
               88.75F,       79.1875F,      132.0625F,     123.625F,
               53.875F,      65.5625F,      158.6875F,     205.375F,
               205.625F,     189.9375F,     158.3125F,     128.25F,
               99.75F,       108.4375F,     154.3125F,     162.8125F,
               133.9375F,    135.75F,       168.25F,       172.875F,
               149.625F,     127.25F,       105.75F,       103.6875F,
               121.0625F,    101.75F,       45.75F,        38.875F,
               81.125F,      116.6875F,     145.5625F,     135.5F,
               86.5F,        62.125F,       62.375F,       67.4375F,
               77.3125F,     82.25F},
              {3.0F,         3.25F,         3.75F,         4.25F,
               4.75F,        5.25F,         5.75F,         6.25F,
               6.75F,        7.25F,         7.75F,         8.25F,
               8.75F,        9.25F,         9.75F,         10.25F,
               10.75F,       11.25F,        11.75F,        12.25F,
               12.75F,       8.50705e+37F,  2.552115e+38F, 3.40282e+38F,
               3.40282e+38F, 2.552115e+38F, 8.50705e+37F,  121.125F,
               153.875F,     161.75F,       144.75F,       118.75F,
               83.75F,       87.5625F,      130.1875F,     117.375F,
               49.125F,      55.1875F,      135.5625F,     168.625F,
               154.375F,     125.8125F,     82.9375F,      53.75F,
               38.25F,       43.3125F,      68.9375F,      77.9375F,
               70.3125F,     90.25F,        137.75F,       152.625F,
               134.875F,     113.75F,       89.25F,        81.0625F,
               89.1875F,     73.75F,        34.75F,        25.125F,
               44.875F,      75.0625F,      115.6875F,     144.0F,
               160.0F,       166.875F,      164.625F,      153.8125F,
               134.4375F,    124.75F},
              {4.0F,         4.25F,         4.75F,         5.25F,
               5.75F,        6.25F,         6.75F,         7.25F,
               7.75F,        8.25F,         8.75F,         9.25F,
               9.75F,        10.25F,        10.75F,        11.25F,
               11.75F,       12.25F,        12.75F,        13.25F,
               13.75F,       8.50705e+37F,  2.552115e+38F, 3.40282e+38F,
               3.40282e+38F, 2.552115e+38F, 8.50705e+37F,  126.5F,
               169.5F,       169.75F,       127.25F,       97.75F,
               81.25F,       91.75F,        129.25F,       114.25F,
               46.75F,       50.0F,         124.0F,        150.25F,
               128.75F,      93.75F,        45.25F,        16.5F,
               7.5F,         10.75F,        26.25F,        35.5F,
               38.5F,        67.5F,         122.5F,        142.5F,
               127.5F,       107.0F,        81.0F,         69.75F,
               73.25F,       59.75F,        29.25F,        18.25F,
               26.75F,       54.25F,        100.75F,       148.25F,
               196.75F,      219.25F,       215.75F,       197.0F,
               163.0F,       146.0F}}},
        // 2*2 -> 8*8
        Pf32{{{FLT_MAX, 1e38F}, {0, FLT_TRUE_MIN}},
             {{FLT_MAX, FLT_MAX, 3.10247e38F, 2.5017644e38F, 1.9010587e38F,
               1.3003528e38F, 1e38F, 1e38F},
              {FLT_MAX, FLT_MAX, 3.1024702e38F, 2.5017644e38F, 1.9010587e38F,
               1.3003528e38F, 1e38F, 1e38F},
              {2.9774701e38F, 2.9774701e38F, 2.7146614e38F, 2.189044e38F,
               1.6634263e38F, 1.1378087e38F, 8.75e37F, 8.75e37F},
              {2.1267644e38F, 2.1267644e38F, 1.9390438e38F, 1.5636028e38F,
               1.1881617e38F, 8.1272051e37F, 6.25e37F, 6.25e37F},
              {1.2760587e38F, 1.2760587e38F, 1.1634263e38F, 9.3816164e37F,
               7.1289698e37F, 4.8763233e37F, 3.75e37F, 3.75e37F},
              {4.2535288e37F, 4.2535288e37F, 3.8780877e37F, 3.1272055e37F,
               2.3763234e37F, 1.6254411e37F, 1.25e37F, 1.25e37F},
              {0, 0, 0, 0, FLT_TRUE_MIN, FLT_TRUE_MIN, FLT_TRUE_MIN,
               FLT_TRUE_MIN},
              {0, 0, 0, 0, FLT_TRUE_MIN, FLT_TRUE_MIN, FLT_TRUE_MIN,
               FLT_TRUE_MIN}}},
        // 35*2 -> 140*8
        Pf32{{{0,   1,   2,   3,   4,   5,   6,   7,   8,   9,   10,  82,
               155, 104, 108, 227, 46,  162, 21,  220, 235, 183, 113, 225,
               146, 196, 144, 104, 148, 19,  126, 172, 9,   12,  61},
              {4,  5,   6,   7,   8,  9,   10, 11,  12,  13,  14, 193,
               44, 105, 191, 106, 73, 148, 13, 161, 118, 21,  3,  34,
               40, 150, 120, 68,  75, 14,  31, 124, 221, 214, 146}},
             {{0,        0,        0.125F,   0.375F,   0.625F,   0.875F,
               1.125F,   1.375F,   1.625F,   1.875F,   2.125F,   2.375F,
               2.625F,   2.875F,   3.125F,   3.375F,   3.625F,   3.875F,
               4.125F,   4.375F,   4.625F,   4.875F,   5.125F,   5.375F,
               5.625F,   5.875F,   6.125F,   6.375F,   6.625F,   6.875F,
               7.125F,   7.375F,   7.625F,   7.875F,   8.125F,   8.375F,
               8.625F,   8.875F,   9.125F,   9.375F,   9.625F,   9.875F,
               19,       37,       55,       73,       91.125F,  109.375F,
               127.625F, 145.875F, 148.625F, 135.875F, 123.125F, 110.375F,
               104.5F,   105.5F,   106.5F,   107.5F,   122.875F, 152.625F,
               182.375F, 212.125F, 204.375F, 159.125F, 113.875F, 68.625F,
               60.5F,    89.5F,    118.5F,   147.5F,   144.375F, 109.125F,
               73.875F,  38.625F,  45.875F,  95.625F,  145.375F, 195.125F,
               221.875F, 225.625F, 229.375F, 233.125F, 228.5F,   215.5F,
               202.5F,   189.5F,   174.25F,  156.75F,  139.25F,  121.75F,
               127,      155,      183,      211,      215.125F, 195.375F,
               175.625F, 155.875F, 152.25F,  164.75F,  177.25F,  189.75F,
               189.5F,   176.5F,   163.5F,   150.5F,   139,      129,
               119,      109,      109.5F,   120.5F,   131.5F,   142.5F,
               131.875F, 99.625F,  67.375F,  35.125F,  32.375F,  59.125F,
               85.875F,  112.625F, 131.75F,  143.25F,  154.75F,  166.25F,
               151.625F, 110.875F, 70.125F,  29.375F,  9.375F,   10.125F,
               10.875F,  11.625F,  18.125F,  30.375F,  42.625F,  54.875F,
               61,       61},
              {0,        0,        0.125F,   0.375F,   0.625F,   0.875F,
               1.125F,   1.375F,   1.625F,   1.875F,   2.125F,   2.375F,
               2.625F,   2.875F,   3.125F,   3.375F,   3.625F,   3.875F,
               4.125F,   4.375F,   4.625F,   4.875F,   5.125F,   5.375F,
               5.625F,   5.875F,   6.125F,   6.375F,   6.625F,   6.875F,
               7.125F,   7.375F,   7.625F,   7.875F,   8.125F,   8.375F,
               8.625F,   8.875F,   9.125F,   9.375F,   9.625F,   9.875F,
               19,       37,       55,       73,       91.125F,  109.375F,
               127.625F, 145.875F, 148.625F, 135.875F, 123.125F, 110.375F,
               104.5F,   105.5F,   106.5F,   107.5F,   122.875F, 152.625F,
               182.375F, 212.125F, 204.375F, 159.125F, 113.875F, 68.625F,
               60.5F,    89.5F,    118.5F,   147.5F,   144.375F, 109.125F,
               73.875F,  38.625F,  45.875F,  95.625F,  145.375F, 195.125F,
               221.875F, 225.625F, 229.375F, 233.125F, 228.5F,   215.5F,
               202.5F,   189.5F,   174.25F,  156.75F,  139.25F,  121.75F,
               127,      155,      183,      211,      215.125F, 195.375F,
               175.625F, 155.875F, 152.25F,  164.75F,  177.25F,  189.75F,
               189.5F,   176.5F,   163.5F,   150.5F,   139,      129,
               119,      109,      109.5F,   120.5F,   131.5F,   142.5F,
               131.875F, 99.625F,  67.375F,  35.125F,  32.375F,  59.125F,
               85.875F,  112.625F, 131.75F,  143.25F,  154.75F,  166.25F,
               151.625F, 110.875F, 70.125F,  29.375F,  9.375F,   10.125F,
               10.875F,  11.625F,  18.125F,  30.375F,  42.625F,  54.875F,
               61,       61},
              {0.5F,        0.5F,        0.625F,      0.875F,      1.125F,
               1.375F,      1.625F,      1.875F,      2.125F,      2.375F,
               2.625F,      2.875F,      3.125F,      3.375F,      3.625F,
               3.875F,      4.125F,      4.375F,      4.625F,      4.875F,
               5.125F,      5.375F,      5.625F,      5.875F,      6.125F,
               6.375F,      6.625F,      6.875F,      7.125F,      7.375F,
               7.625F,      7.875F,      8.125F,      8.375F,      8.625F,
               8.875F,      9.125F,      9.375F,      9.625F,      9.875F,
               10.125F,     10.375F,     21.171875F,  42.515625F,  63.859375F,
               85.203125F,  101.53125F,  112.84375F,  124.15625F,  135.46875F,
               136.5F,      127.25F,     118,         108.75F,     105.90625F,
               109.46875F,  113.03125F,  116.59375F,  130.0625F,   153.4375F,
               176.8125F,   200.1875F,   191.5625F,   150.9375F,   110.3125F,
               69.6875F,    63.234375F,  90.953125F,  118.671875F, 146.39062F,
               142.71875F,  107.65625F,  72.59375F,   37.53125F,   44.078125F,
               92.234375F,  140.39062F,  188.54688F,  213.59375F,  215.53125F,
               217.46875F,  219.40625F,  213.17188F,  198.76562F,  184.35938F,
               169.95312F,  154.8125F,   138.9375F,   123.0625F,   107.1875F,
               111.984375F, 137.45312F,  162.92188F,  188.39062F,  192.57812F,
               175.48438F,  158.39062F,  141.29688F,  139.9375F,   154.3125F,
               168.6875F,   183.0625F,   184.09375F,  171.78125F,  159.46875F,
               147.15625F,  135.8125F,   125.4375F,   115.0625F,   104.6875F,
               104.421875F, 114.265625F, 124.109375F, 133.95312F,  123.8125F,
               93.6875F,    63.5625F,    33.4375F,    30.34375F,   54.28125F,
               78.21875F,   102.15625F,  120.609375F, 133.57812F,  146.54688F,
               159.51562F,  149.6875F,   117.0625F,   84.4375F,    51.8125F,
               35.71875F,   36.15625F,   36.59375F,   37.03125F,   41.546875F,
               50.140625F,  58.734375F,  67.328125F,  71.625F,     71.625F},
              {1.5F,        1.5F,        1.625F,      1.875F,      2.125F,
               2.375F,      2.625F,      2.875F,      3.125F,      3.375F,
               3.625F,      3.875F,      4.125F,      4.375F,      4.625F,
               4.875F,      5.125F,      5.375F,      5.625F,      5.875F,
               6.125F,      6.375F,      6.625F,      6.875F,      7.125F,
               7.375F,      7.625F,      7.875F,      8.125F,      8.375F,
               8.625F,      8.875F,      9.125F,      9.375F,      9.625F,
               9.875F,      10.125F,     10.375F,     10.625F,     10.875F,
               11.125F,     11.375F,     25.515625F,  53.546875F,  81.578125F,
               109.609375F, 122.34375F,  119.78125F,  117.21875F,  114.65625F,
               112.25F,     110,         107.75F,     105.5F,      108.71875F,
               117.40625F,  126.09375F,  134.78125F,  144.4375F,   155.0625F,
               165.6875F,   176.3125F,   165.9375F,   134.5625F,   103.1875F,
               71.8125F,    68.703125F,  93.859375F,  119.015625F, 144.17188F,
               139.40625F,  104.71875F,  70.03125F,   35.34375F,   40.484375F,
               85.453125F,  130.42188F,  175.39062F,  197.03125F,  195.34375F,
               193.65625F,  191.96875F,  182.51562F,  165.29688F,  148.07812F,
               130.85938F,  115.9375F,   103.3125F,   90.6875F,    78.0625F,
               81.953125F,  102.359375F, 122.765625F, 143.17188F,  147.48438F,
               135.70312F,  123.921875F, 112.140625F, 115.3125F,   133.4375F,
               151.5625F,   169.6875F,   173.28125F,  162.34375F,  151.40625F,
               140.46875F,  129.4375F,   118.3125F,   107.1875F,   96.0625F,
               94.265625F,  101.796875F, 109.328125F, 116.859375F, 107.6875F,
               81.8125F,    55.9375F,    30.0625F,    26.28125F,   44.59375F,
               62.90625F,   81.21875F,   98.328125F,  114.234375F, 130.14062F,
               146.04688F,  145.8125F,   129.4375F,   113.0625F,   96.6875F,
               88.40625F,   88.21875F,   88.03125F,   87.84375F,   88.390625F,
               89.671875F,  90.953125F,  92.234375F,  92.875F,     92.875F},
              {2.5F,       2.5F,        2.625F,      2.875F,      3.125F,
               3.375F,     3.625F,      3.875F,      4.125F,      4.375F,
               4.625F,     4.875F,      5.125F,      5.375F,      5.625F,
               5.875F,     6.125F,      6.375F,      6.625F,      6.875F,
               7.125F,     7.375F,      7.625F,      7.875F,      8.125F,
               8.375F,     8.625F,      8.875F,      9.125F,      9.375F,
               9.625F,     9.875F,      10.125F,     10.375F,     10.625F,
               10.875F,    11.125F,     11.375F,     11.625F,     11.875F,
               12.125F,    12.375F,     29.859375F,  64.578125F,  99.296875F,
               134.01562F, 143.15625F,  126.71875F,  110.28125F,  93.84375F,
               88,         92.75F,      97.5F,       102.25F,     111.53125F,
               125.34375F, 139.15625F,  152.96875F,  158.8125F,   156.6875F,
               154.5625F,  152.4375F,   140.3125F,   118.1875F,   96.0625F,
               73.9375F,   74.171875F,  96.765625F,  119.359375F, 141.95312F,
               136.09375F, 101.78125F,  67.46875F,   33.15625F,   36.890625F,
               78.671875F, 120.453125F, 162.23438F,  180.46875F,  175.15625F,
               169.84375F, 164.53125F,  151.85938F,  131.82812F,  111.796875F,
               91.765625F, 77.0625F,    67.6875F,    58.3125F,    48.9375F,
               51.921875F, 67.265625F,  82.609375F,  97.953125F,  102.390625F,
               95.921875F, 89.453125F,  82.984375F,  90.6875F,    112.5625F,
               134.4375F,  156.3125F,   162.46875F,  152.90625F,  143.34375F,
               133.78125F, 123.0625F,   111.1875F,   99.3125F,    87.4375F,
               84.109375F, 89.328125F,  94.546875F,  99.765625F,  91.5625F,
               69.9375F,   48.3125F,    26.6875F,    22.21875F,   34.90625F,
               47.59375F,  60.28125F,   76.046875F,  94.890625F,  113.734375F,
               132.57812F, 141.9375F,   141.8125F,   141.6875F,   141.5625F,
               141.09375F, 140.28125F,  139.46875F,  138.65625F,  135.23438F,
               129.20312F, 123.171875F, 117.140625F, 114.125F,    114.125F},
              {3.5F,        3.5F,        3.625F,      3.875F,      4.125F,
               4.375F,      4.625F,      4.875F,      5.125F,      5.375F,
               5.625F,      5.875F,      6.125F,      6.375F,      6.625F,
               6.875F,      7.125F,      7.375F,      7.625F,      7.875F,
               8.125F,      8.375F,      8.625F,      8.875F,      9.125F,
               9.375F,      9.625F,      9.875F,      10.125F,     10.375F,
               10.625F,     10.875F,     11.125F,     11.375F,     11.625F,
               11.875F,     12.125F,     12.375F,     12.625F,     12.875F,
               13.125F,     13.375F,     34.203125F,  75.609375F,  117.015625F,
               158.42188F,  163.96875F,  133.65625F,  103.34375F,  73.03125F,
               63.75F,      75.5F,       87.25F,      99,          114.34375F,
               133.28125F,  152.21875F,  171.15625F,  173.1875F,   158.3125F,
               143.4375F,   128.5625F,   114.6875F,   101.8125F,   88.9375F,
               76.0625F,    79.640625F,  99.671875F,  119.703125F, 139.73438F,
               132.78125F,  98.84375F,   64.90625F,   30.96875F,   33.296875F,
               71.890625F,  110.484375F, 149.07812F,  163.90625F,  154.96875F,
               146.03125F,  137.09375F,  121.203125F, 98.359375F,  75.515625F,
               52.671875F,  38.1875F,    32.0625F,    25.9375F,    19.8125F,
               21.890625F,  32.171875F,  42.453125F,  52.734375F,  57.296875F,
               56.140625F,  54.984375F,  53.828125F,  66.0625F,    91.6875F,
               117.3125F,   142.9375F,   151.65625F,  143.46875F,  135.28125F,
               127.09375F,  116.6875F,   104.0625F,   91.4375F,    78.8125F,
               73.953125F,  76.859375F,  79.765625F,  82.671875F,  75.4375F,
               58.0625F,    40.6875F,    23.3125F,    18.15625F,   25.21875F,
               32.28125F,   39.34375F,   53.765625F,  75.546875F,  97.328125F,
               119.109375F, 138.0625F,   154.1875F,   170.3125F,   186.4375F,
               193.78125F,  192.34375F,  190.90625F,  189.46875F,  182.07812F,
               168.73438F,  155.39062F,  142.04688F,  135.375F,    135.375F},
              {4,        4,        4.125F,   4.375F,   4.625F,   4.875F,
               5.125F,   5.375F,   5.625F,   5.875F,   6.125F,   6.375F,
               6.625F,   6.875F,   7.125F,   7.375F,   7.625F,   7.875F,
               8.125F,   8.375F,   8.625F,   8.875F,   9.125F,   9.375F,
               9.625F,   9.875F,   10.125F,  10.375F,  10.625F,  10.875F,
               11.125F,  11.375F,  11.625F,  11.875F,  12.125F,  12.375F,
               12.625F,  12.875F,  13.125F,  13.375F,  13.625F,  13.875F,
               36.375F,  81.125F,  125.875F, 170.625F, 174.375F, 137.125F,
               99.875F,  62.625F,  51.625F,  66.875F,  82.125F,  97.375F,
               115.75F,  137.25F,  158.75F,  180.25F,  180.375F, 159.125F,
               137.875F, 116.625F, 101.875F, 93.625F,  85.375F,  77.125F,
               82.375F,  101.125F, 119.875F, 138.625F, 131.125F, 97.375F,
               63.625F,  29.875F,  31.5F,    68.5F,    105.5F,   142.5F,
               155.625F, 144.875F, 134.125F, 123.375F, 105.875F, 81.625F,
               57.375F,  33.125F,  18.75F,   14.25F,   9.75F,    5.25F,
               6.875F,   14.625F,  22.375F,  30.125F,  34.75F,   36.25F,
               37.75F,   39.25F,   53.75F,   81.25F,   108.75F,  136.25F,
               146.25F,  138.75F,  131.25F,  123.75F,  113.5F,   100.5F,
               87.5F,    74.5F,    68.875F,  70.625F,  72.375F,  74.125F,
               67.375F,  52.125F,  36.875F,  21.625F,  16.125F,  20.375F,
               24.625F,  28.875F,  42.625F,  65.875F,  89.125F,  112.375F,
               136.125F, 160.375F, 184.625F, 208.875F, 220.125F, 218.375F,
               216.625F, 214.875F, 205.5F,   188.5F,   171.5F,   154.5F,
               146,      146},
              {4,        4,        4.125F,   4.375F,   4.625F,   4.875F,
               5.125F,   5.375F,   5.625F,   5.875F,   6.125F,   6.375F,
               6.625F,   6.875F,   7.125F,   7.375F,   7.625F,   7.875F,
               8.125F,   8.375F,   8.625F,   8.875F,   9.125F,   9.375F,
               9.625F,   9.875F,   10.125F,  10.375F,  10.625F,  10.875F,
               11.125F,  11.375F,  11.625F,  11.875F,  12.125F,  12.375F,
               12.625F,  12.875F,  13.125F,  13.375F,  13.625F,  13.875F,
               36.375F,  81.125F,  125.875F, 170.625F, 174.375F, 137.125F,
               99.875F,  62.625F,  51.625F,  66.875F,  82.125F,  97.375F,
               115.75F,  137.25F,  158.75F,  180.25F,  180.375F, 159.125F,
               137.875F, 116.625F, 101.875F, 93.625F,  85.375F,  77.125F,
               82.375F,  101.125F, 119.875F, 138.625F, 131.125F, 97.375F,
               63.625F,  29.875F,  31.5F,    68.5F,    105.5F,   142.5F,
               155.625F, 144.875F, 134.125F, 123.375F, 105.875F, 81.625F,
               57.375F,  33.125F,  18.75F,   14.25F,   9.75F,    5.25F,
               6.875F,   14.625F,  22.375F,  30.125F,  34.75F,   36.25F,
               37.75F,   39.25F,   53.75F,   81.25F,   108.75F,  136.25F,
               146.25F,  138.75F,  131.25F,  123.75F,  113.5F,   100.5F,
               87.5F,    74.5F,    68.875F,  70.625F,  72.375F,  74.125F,
               67.375F,  52.125F,  36.875F,  21.625F,  16.125F,  20.375F,
               24.625F,  28.875F,  42.625F,  65.875F,  89.125F,  112.375F,
               136.125F, 160.375F, 184.625F, 208.875F, 220.125F, 218.375F,
               216.625F, 214.875F, 205.5F,   188.5F,   171.5F,   154.5F,
               146,      146}}}));
