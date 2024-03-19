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

static void resize_linear_unaccelerated(const uint8_t *src, size_t src_stride,
                                        size_t src_width, size_t src_height,
                                        uint8_t *dst, size_t dst_stride,
                                        size_t dst_width, size_t dst_height) {
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

TEST(ResizeLinear, LargeDimensions) {
  size_t src_width = 2049;
  size_t src_height = 5;
  size_t src_stride = src_width + 6;
  size_t dst_width = src_width * 2;
  size_t dst_height = src_height * 2;
  size_t dst_stride = dst_width + 3;

  std::vector<uint8_t> src, dst, expected_data;
  src.resize(src_stride * src_height);
  dst.resize(dst_stride * dst_height);
  expected_data.resize(dst_stride * dst_height);
  std::mt19937 generator{test::Options::seed()};
  std::generate(src.begin(), src.end(), generator);
  resize_linear_unaccelerated(src.data(), src_stride, src_width, src_height,
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
            27, 54, 101, 148, 197, 219, 216, 197, 163, 146}}}));
