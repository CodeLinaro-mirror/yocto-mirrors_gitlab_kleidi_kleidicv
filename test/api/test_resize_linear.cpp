// SPDX-FileCopyrightText: 2024 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <random>

#include "framework/array.h"
#include "framework/utils.h"
#include "intrinsiccv/intrinsiccv.h"
#include "test_config.h"

TEST(ResizeLinear, NotImplemented) {
  const uint8_t src[1] = {};
  uint8_t dst[4];

  EXPECT_EQ(INTRINSICCV_ERROR_NOT_IMPLEMENTED,
            intrinsiccv_resize_linear_u8(src, 1, 1, 1, dst, 2, 2, 1));
  EXPECT_EQ(INTRINSICCV_ERROR_NOT_IMPLEMENTED,
            intrinsiccv_resize_linear_u8(src, 1, 1, 1, dst, 1, 1, 2));
  EXPECT_EQ(INTRINSICCV_ERROR_NOT_IMPLEMENTED,
            intrinsiccv_resize_linear_u8(src, 1, 1, 1, dst, 4, 4, 2));
  EXPECT_EQ(INTRINSICCV_ERROR_NOT_IMPLEMENTED,
            intrinsiccv_resize_linear_u8(src, 1, 1, 1, dst, 2, 2, 4));
}

TEST(ResizeLinear, NullPointer) {
  const uint8_t src[1] = {};
  uint8_t dst[4];
  test::test_null_args(intrinsiccv_resize_linear_u8, src, 1, 1, 1, dst, 2, 2,
                       2);
}

TEST(ResizeLinear, InvalidImageSize) {
  const uint8_t src[1] = {};
  uint8_t dst[4];

  EXPECT_EQ(INTRINSICCV_ERROR_RANGE,
            intrinsiccv_resize_linear_u8(src, 1, 1, 1, dst,
                                         INTRINSICCV_MAX_IMAGE_PIXELS + 1,
                                         INTRINSICCV_MAX_IMAGE_PIXELS + 1, 1));

  EXPECT_EQ(INTRINSICCV_ERROR_RANGE,
            intrinsiccv_resize_linear_u8(
                src, 1, 1, 1, dst, INTRINSICCV_MAX_IMAGE_PIXELS,
                INTRINSICCV_MAX_IMAGE_PIXELS, INTRINSICCV_MAX_IMAGE_PIXELS));
}

TEST(ResizeLinear, ZeroImageSize) {
  const uint8_t src[1] = {};
  uint8_t dst[1];
  EXPECT_EQ(INTRINSICCV_OK,
            intrinsiccv_resize_linear_u8(src, 0, 0, 0, dst, 0, 0, 0));
  EXPECT_EQ(INTRINSICCV_OK,
            intrinsiccv_resize_linear_u8(src, 1, 1, 0, dst, 2, 2, 0));
  EXPECT_EQ(INTRINSICCV_OK,
            intrinsiccv_resize_linear_u8(src, 0, 0, 1, dst, 0, 0, 2));
}

static void resize_linear_unaccelerated_2x2_u8(
    const uint8_t *src, size_t src_stride, size_t src_width, size_t src_height,
    uint8_t *dst, size_t dst_stride, size_t dst_width, size_t dst_height) {
  auto lerp1d = [](uint8_t near, uint8_t far) {
    return (near * 3 + far + 2) >> 2;
  };

  auto lerp2d = [](uint8_t near, uint8_t mid_a, uint8_t mid_b, uint8_t far) {
    return (near * 9 + (mid_a + mid_b) * 3 + far + 8) >> 4;
  };

  auto process_row = [src_width, dst_width, lerp1d, lerp2d](
                         const uint8_t *src_row0, const uint8_t *src_row1,
                         uint8_t *dst_row0, uint8_t *dst_row1) {
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
      const uint8_t src_tl = src_row0[src_x], src_tr = src_row0[src_x + 1],
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
    const uint8_t *src_row0 = src + src_stride * src_y;
    const uint8_t *src_row1 = src_row0 + src_stride;
    uint8_t *dst_row0 = dst + dst_stride * dst_y;
    uint8_t *dst_row1 = dst_row0 + dst_stride;

    process_row(src_row0, src_row1, dst_row0, dst_row1);
  }

  // Bottom row
  const uint8_t *last_src_row = src + src_stride * (src_height - 1);
  uint8_t *last_dst_row = dst + dst_stride * (dst_height - 1);
  process_row(last_src_row, last_src_row, last_dst_row, last_dst_row);
}

static void resize_linear_unaccelerated_4x4_u8(
    const uint8_t *src, size_t src_stride, size_t src_width, size_t src_height,
    uint8_t *dst, size_t dst_stride, size_t dst_width, size_t dst_height) {
  auto lerp1d_scalar = [](uint8_t coeff_a, uint8_t a, uint8_t coeff_b,
                          uint8_t b) {
    return (coeff_a * a + coeff_b * b + 4) >> 3;
  };
  auto lerp2d_scalar = [](uint8_t coeff_a, uint8_t a, uint8_t coeff_b,
                          uint8_t b, uint8_t coeff_c, uint8_t c,
                          uint8_t coeff_d, uint8_t d) {
    return (coeff_a * a + coeff_b * b + coeff_c * c + coeff_d * d + 32) >> 6;
  };
  // Handle top or bottom edge
  auto process_edge_row = [src_width, dst_width,
                           lerp1d_scalar /*, lerp1d_vector*/](
                              const uint8_t *src_row, uint8_t *dst_row) {
    // Left elements
    dst_row[1] = dst_row[0] = src_row[0];

    // Right elements
    dst_row[dst_width - 1] = dst_row[dst_width - 2] = src_row[src_width - 1];

    // Middle elements
    for (size_t src_x = 0; src_x + 1 < src_width; ++src_x) {
      size_t dst_x = src_x * 4 + 2;
      const uint8_t a = src_row[src_x], b = src_row[src_x + 1];
      dst_row[dst_x + 0] = lerp1d_scalar(7, a, 1, b);
      dst_row[dst_x + 1] = lerp1d_scalar(5, a, 3, b);
      dst_row[dst_x + 2] = lerp1d_scalar(3, a, 5, b);
      dst_row[dst_x + 3] = lerp1d_scalar(1, a, 7, b);
    }
  };

  auto process_row = [src_width, dst_width, lerp1d_scalar, lerp2d_scalar](
                         const uint8_t *src_row0, const uint8_t *src_row1,
                         uint8_t *dst_row0, uint8_t *dst_row1,
                         uint8_t *dst_row2, uint8_t *dst_row3) {
    // Left elements
    const uint8_t s0l = src_row0[0], s1l = src_row1[0];
    dst_row0[0] = dst_row0[1] = lerp1d_scalar(7, s0l, 1, s1l);
    dst_row1[0] = dst_row1[1] = lerp1d_scalar(5, s0l, 3, s1l);
    dst_row2[0] = dst_row2[1] = lerp1d_scalar(3, s0l, 5, s1l);
    dst_row3[0] = dst_row3[1] = lerp1d_scalar(1, s0l, 7, s1l);

    // Right elements
    const size_t s0r = src_row0[src_width - 1], s1r = src_row1[src_width - 1];
    const size_t dr0 = dst_width - 2;
    const size_t dr1 = dst_width - 1;
    dst_row0[dr0] = dst_row0[dr1] = lerp1d_scalar(7, s0r, 1, s1r);
    dst_row1[dr0] = dst_row1[dr1] = lerp1d_scalar(5, s0r, 3, s1r);
    dst_row2[dr0] = dst_row2[dr1] = lerp1d_scalar(3, s0r, 5, s1r);
    dst_row3[dr0] = dst_row3[dr1] = lerp1d_scalar(1, s0r, 7, s1r);

    // Middle elements
    for (size_t src_x = 0; src_x + 1 < src_width; ++src_x) {
      size_t dst_x = src_x * 4 + 2;
      const uint8_t a = src_row0[src_x], b = src_row0[src_x + 1],
                    c = src_row1[src_x], d = src_row1[src_x + 1];

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
  memcpy(dst + dst_stride, dst, dst_stride);

  // Middle rows
  for (size_t src_y = 0; src_y + 1 < src_height; ++src_y) {
    size_t dst_y = src_y * 4 + 2;
    const uint8_t *src_row0 = src + src_stride * src_y;
    const uint8_t *src_row1 = src_row0 + src_stride;
    uint8_t *dst_row0 = dst + dst_stride * dst_y;
    uint8_t *dst_row1 = dst_row0 + dst_stride;
    uint8_t *dst_row2 = dst_row1 + dst_stride;
    uint8_t *dst_row3 = dst_row2 + dst_stride;

    process_row(src_row0, src_row1, dst_row0, dst_row1, dst_row2, dst_row3);
  }

  // Bottom rows
  process_edge_row(src + src_stride * (src_height - 1),
                   dst + dst_stride * (dst_height - 2));
  memcpy(dst + dst_stride * (dst_height - 1),
         dst + dst_stride * (dst_height - 2), dst_stride);
}

static void resize_linear_unaccelerated_u8(const uint8_t *src,
                                           size_t src_stride, size_t src_width,
                                           size_t src_height, uint8_t *dst,
                                           size_t dst_stride, size_t dst_width,
                                           size_t dst_height) {
  if (src_width * 2 == dst_width && src_height * 2 == dst_height) {
    resize_linear_unaccelerated_2x2_u8(src, src_stride, src_width, src_height,
                                       dst, dst_stride, dst_width, dst_height);
  }
  if (src_width * 4 == dst_width && src_height * 4 == dst_height) {
    resize_linear_unaccelerated_4x4_u8(src, src_stride, src_width, src_height,
                                       dst, dst_stride, dst_width, dst_height);
  }
}

static void do_large_dimensions_test(size_t x_scale, size_t y_scale) {
  size_t src_width = 2049;
  size_t src_height = 5;
  size_t src_stride = src_width + 6;
  size_t dst_width = src_width * x_scale;
  size_t dst_height = src_height * y_scale;
  size_t dst_stride = dst_width + 3;

  std::vector<uint8_t> src, dst, expected_data;
  src.resize(src_stride * src_height);
  dst.resize(dst_stride * dst_height);
  expected_data.resize(dst_stride * dst_height);
  std::mt19937 generator(test::Options::seed());
  std::generate(src.begin(), src.end(), generator);
  resize_linear_unaccelerated_u8(src.data(), src_stride, src_width, src_height,
                                 expected_data.data(), dst_stride, dst_width,
                                 dst_height);
  ASSERT_EQ(INTRINSICCV_OK, intrinsiccv_resize_linear_u8(
                                src.data(), src_stride, src_width, src_height,
                                dst.data(), dst_stride, dst_width, dst_height));

  for (size_t y = 0; y < dst_height; ++y) {
    // Compare as int to avoid test framework displaying values as chars.
    std::vector<intmax_t> actual{
        dst.begin() + static_cast<ptrdiff_t>(y * dst_stride),
        dst.begin() + static_cast<ptrdiff_t>(y * dst_stride + dst_width)},
        expected{expected_data.begin() + static_cast<ptrdiff_t>(y * dst_stride),
                 expected_data.begin() +
                     static_cast<ptrdiff_t>(y * dst_stride + dst_width)};
    EXPECT_THAT(actual, ::testing::ElementsAreArray(expected)) << "Row #" << y;
  }
}

TEST(ResizeLinear, LargeDimensions2x2) { do_large_dimensions_test(2, 2); }

TEST(ResizeLinear, LargeDimensions4x4) { do_large_dimensions_test(4, 4); }

// Parameterised tests
struct ResizeTestParams {
  std::vector<std::vector<uint8_t>> src;
  std::vector<std::vector<uint8_t>> expected;

  friend void PrintTo(const ResizeTestParams &v, std::ostream *os) {
    *os << "([\n";
    for (size_t y = 0; y < v.src.size(); ++y) {
      const auto &row = v.src[y];
      *os << "  [";
      for (size_t x = 0; x < row.size(); ++x) {
        *os << std::setw(3) << int{row[x]};
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

void do_linear_resize_test(const ResizeTestParams &param, size_t src_padding,
                           size_t dst_padding) {
  size_t src_width = param.src[0].size();
  size_t src_height = param.src.size();
  size_t src_stride = src_width + src_padding;
  size_t dst_width = param.expected[0].size();
  size_t dst_height = param.expected.size();
  size_t dst_stride = dst_width + dst_padding;

  auto flatten = [](const std::vector<std::vector<uint8_t>> &vec2d,
                    size_t padding) {
    std::vector<uint8_t> result;
    for (const auto &row : vec2d) {
      result.insert(result.end(), row.begin(), row.end());
      result.resize(result.size() + padding);
    }
    return result;
  };
  std::vector<uint8_t> src = flatten(param.src, src_padding), dst;
  dst.resize(dst_stride * dst_height);

  ASSERT_EQ(INTRINSICCV_OK, intrinsiccv_resize_linear_u8(
                                src.data(), src_stride, src_width, src_height,
                                dst.data(), dst_stride, dst_width, dst_height));
  for (size_t y = 0; y < dst_height; ++y) {
    // Compare as int to avoid test framework displaying values as chars.
    std::vector<intmax_t> actual{
        dst.begin() + static_cast<ptrdiff_t>(y * dst_stride),
        dst.begin() + static_cast<ptrdiff_t>(y * dst_stride + dst_width)},
        expected{param.expected[y].begin(), param.expected[y].end()};
    EXPECT_THAT(actual, ::testing::ElementsAreArray(expected)) << "Row #" << y;
  }
}

class ResizeLinear : public testing::TestWithParam<ResizeTestParams> {};

TEST_P(ResizeLinear, ResizeNoPadding) {
  do_linear_resize_test(GetParam(), 0, 0);
}

TEST_P(ResizeLinear, ResizeWithPadding) {
  do_linear_resize_test(GetParam(), 1, 2);
}

TEST_P(ResizeLinear, ResizePadDst) { do_linear_resize_test(GetParam(), 0, 3); }

TEST_P(ResizeLinear, ResizePadSrc) { do_linear_resize_test(GetParam(), 4, 0); }

using P = ResizeTestParams;

INSTANTIATE_TEST_SUITE_P(
    ResizeLinear, ResizeLinear,
    testing::Values(
        // 1*1 -> 2*2
        P{{{123}}, {{123, 123}, {123, 123}}},
        // 2*1 -> 4*2
        P{{{0, 255}}, {{0, 64, 191, 255}, {0, 64, 191, 255}}},
        // 2*1 -> 4*2. Check rounding behaviour.
        P{{{1, 63}}, {{1, 17, 48, 63}, {1, 17, 48, 63}}},
        // 2*2 -> 4*4
        P{{{0, 255}, {100, 8}},
          {{0, 64, 191, 255},
           {25, 67, 151, 193},
           {75, 74, 71, 70},
           {100, 77, 31, 8}}},
        // 3*3 -> 6*6
        P{{{1, 63, 164}, {28, 251, 35}, {218, 64, 99}},
          {{1, 17, 48, 88, 139, 164},
           {8, 33, 84, 115, 126, 132},
           {21, 67, 158, 170, 101, 67},
           {76, 108, 172, 166, 89, 51},
           {171, 156, 126, 104, 90, 83},
           {218, 180, 103, 73, 90, 99}}},
        // 4*4 -> 8*8
        P{{{10, 30, 5, 70},
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
        P{{{0,   1,   2,   3,   4,   5,   6,   7,   8,   9,   10,  82,
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
           {3,   3,   4,   4,   5,  5,  6,   6,   7,   7,   8,   8,   9,   9,
            10,  10,  11,  11,  12, 12, 13,  51,  127, 142, 95,  80,  97,  121,
            154, 162, 145, 119, 84, 88, 130, 117, 49,  55,  136, 169, 154, 126,
            83,  54,  38,  43,  69, 78, 70,  90,  138, 153, 135, 114, 89,  81,
            89,  74,  35,  25,  45, 75, 116, 144, 160, 167, 165, 154, 134, 125},
           {4,  4,  5,   5,   6,   6,   7,   7,   8,   8,  9,   9,
            10, 10, 11,  11,  12,  12,  13,  13,  14,  59, 148, 156,
            81, 59, 90,  127, 170, 170, 127, 98,  81,  92, 129, 114,
            47, 50, 124, 150, 129, 94,  45,  17,  8,   11, 26,  36,
            39, 68, 123, 143, 128, 107, 81,  70,  73,  60, 29,  18,
            27, 54, 101, 148, 197, 219, 216, 197, 163, 146}}},
        // 2*2 -> 8*8
        P{{{0, 255}, {128, 124}},
          {{0, 0, 32, 96, 159, 223, 255, 255},
           {0, 0, 32, 96, 159, 223, 255, 255},
           {16, 16, 44, 99, 155, 211, 239, 239},
           {48, 48, 68, 107, 147, 186, 206, 206},
           {80, 80, 92, 115, 138, 161, 173, 173},
           {112, 112, 116, 123, 130, 137, 140, 140},
           {128, 128, 128, 127, 126, 125, 124, 124},
           {128, 128, 128, 127, 126, 125, 124, 124}}},
        // 35*2 -> 140*8
        P{{{0,   1,   2,   3,   4,   5,   6,   7,   8,   9,   10,  82,
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
