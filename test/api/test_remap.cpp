// SPDX-FileCopyrightText: 2024 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#include <gtest/gtest.h>

#include "framework/array.h"
#include "framework/generator.h"
#include "framework/utils.h"
#include "kleidicv/kleidicv.h"

template <class ScalarType>
class Remap16 : public testing::Test {
 public:
  static void test_random(size_t src_w, size_t src_h, size_t dst_w,
                          size_t dst_h, size_t channels, size_t padding) {
    test::Array2D<int16_t> mapxy{2 * dst_w, dst_h, padding, 2};
    test::PseudoRandomNumberGenerator<int16_t> coord_generator;
    mapxy.fill(coord_generator);
    execute_test(mapxy, src_w, src_h, dst_w, dst_h, channels, padding);
  }

  static void test_outside_random(size_t src_w, size_t src_h, size_t dst_w,
                                  size_t dst_h, size_t channels,
                                  size_t padding) {
    test::Array2D<int16_t> mapxy{2 * dst_w, dst_h, padding, 2};
    test::PseudoRandomNumberGeneratorIntRange<int16_t> coord_generator{
        static_cast<int16_t>(-src_w), static_cast<int16_t>(2 * src_w)};
    mapxy.fill(coord_generator);
    execute_test(mapxy, src_w, src_h, dst_w, dst_h, channels, padding);
  }

  static void test_blend(size_t src_w, size_t src_h, size_t dst_w, size_t dst_h,
                         size_t channels, size_t padding) {
    test::Array2D<int16_t> mapxy{2 * dst_w, dst_h, padding, 2};
    for (size_t row = 0; row < dst_h; ++row) {
      for (size_t column = 0; column < dst_w; ++column) {
        // Use a second degree function to add a nonlinear blend to the image
        *mapxy.at(row, column * 2) = std::max<int16_t>(
            0, std::min(
                   static_cast<int16_t>(src_w - 1),
                   static_cast<int16_t>(column * 2 - column * column / dst_w)));
        *mapxy.at(row, column * 2 + 1) = std::max<int16_t>(
            0, std::min(static_cast<int16_t>(src_h - 1),
                        static_cast<int16_t>(row * (dst_w - column) / dst_w +
                                             4 * row / dst_h)));
      }
    }
    execute_test(mapxy, src_w, src_h, dst_w, dst_h, channels, padding);
  }

 private:
  static void execute_test(test::Array2D<int16_t> &mapxy, size_t src_w,
                           size_t src_h, size_t dst_w, size_t dst_h,
                           size_t channels, size_t padding) {
    size_t src_total_width = channels * src_w;
    size_t dst_total_width = channels * dst_w;

    test::Array2D<ScalarType> source{src_total_width, src_h, padding, channels};
    test::Array2D<ScalarType> actual{dst_total_width, dst_h, padding, channels};
    test::Array2D<ScalarType> expected{dst_total_width, dst_h, padding,
                                       channels};

    test::PseudoRandomNumberGenerator<ScalarType> generator;
    source.fill(generator);
    actual.fill(42);

    calculate_expected(source, mapxy, expected);

    ASSERT_EQ(KLEIDICV_OK,
              kleidicv_remap_s16_u8(
                  source.data(), source.stride(), source.width(),
                  source.height(), actual.data(), actual.stride(),
                  actual.width(), actual.height(), channels, mapxy.data(),
                  mapxy.stride(), KLEIDICV_BORDER_TYPE_REPLICATE, {}));

    EXPECT_EQ_ARRAY2D(actual, expected);
  }
  static void calculate_expected(test::Array2D<ScalarType> &src,
                                 test::Array2D<int16_t> &mapxy,
                                 test::Array2D<ScalarType> &expected) {
    for (size_t row = 0; row < expected.height(); row++) {
      for (size_t column = 0; column < expected.width() / src.channels();
           ++column) {
        for (size_t ch = 0; ch < src.channels(); ++ch) {
          int16_t y = std::max<int16_t>(
              0, std::min<int16_t>(src.height() - 1,
                                   *mapxy.at(row, column * 2 + 1)));
          int16_t x = std::max<int16_t>(
              0,
              std::min<int16_t>(src.width() - 1, *mapxy.at(row, column * 2)));
          *expected.at(row, column * src.channels() + ch) =
              *src.at(y, x * src.channels() + ch);
        }
      }
    }
  }
};

using RemapElementTypes = ::testing::Types<uint8_t>;
TYPED_TEST_SUITE(Remap16, RemapElementTypes);

TYPED_TEST(Remap16, RandomNoPadding) {
  size_t src_w = 3 * test::Options::vector_lanes<TypeParam>() - 1;
  size_t src_h = 4;
  size_t dst_w = src_w;
  size_t dst_h = src_h;
  TestFixture::test_random(src_w, src_h, dst_w, dst_h, 1, 0);
}

TYPED_TEST(Remap16, OutsideRandomPadding) {
  size_t src_w = 3 * test::Options::vector_lanes<TypeParam>() - 1;
  size_t src_h = 4;
  size_t dst_w = src_w;
  size_t dst_h = src_h;
  TestFixture::test_outside_random(src_w, src_h, dst_w, dst_h, 1, 0);
}

TYPED_TEST(Remap16, BlendPadding) {
  size_t src_w = 3 * test::Options::vector_lanes<TypeParam>() - 1;
  size_t src_h = 4;
  size_t dst_w = src_w;
  size_t dst_h = src_h;
  TestFixture::test_blend(src_w, src_h, dst_w, dst_h, 1, 13);
}

TYPED_TEST(Remap16, NullPointer) {
  const TypeParam src[4] = {};
  TypeParam dst[1];
  int16_t mapxy[2] = {};
  test::test_null_args(kleidicv_remap_s16_u8, src, 2, 2, 2, dst, 1, 1, 1, 1,
                       mapxy, 4, KLEIDICV_BORDER_TYPE_REPLICATE,
                       kleidicv_border_values_t{});
}

TYPED_TEST(Remap16, ZeroImageSize) {
  const TypeParam src[1] = {};
  TypeParam dst[1];
  int16_t mapxy[2] = {};

  EXPECT_EQ(KLEIDICV_ERROR_NOT_IMPLEMENTED,
            kleidicv_remap_s16_u8(src, 1, 0, 1, dst, 1, 0, 1, 1, mapxy, 4,
                                  KLEIDICV_BORDER_TYPE_REPLICATE,
                                  kleidicv_border_values_t{}));
  EXPECT_EQ(KLEIDICV_ERROR_NOT_IMPLEMENTED,
            kleidicv_remap_s16_u8(src, 1, 1, 0, dst, 1, 1, 0, 1, mapxy, 4,
                                  KLEIDICV_BORDER_TYPE_REPLICATE,
                                  kleidicv_border_values_t{}));
}

TYPED_TEST(Remap16, InvalidImageSize) {
  const TypeParam src[1] = {};
  TypeParam dst[8];
  int16_t mapxy[16] = {};

  EXPECT_EQ(
      KLEIDICV_ERROR_RANGE,
      kleidicv_remap_s16_u8(src, 1, KLEIDICV_MAX_IMAGE_PIXELS + 1, 1, dst, 8, 8,
                            1, 1, mapxy, 4, KLEIDICV_BORDER_TYPE_REPLICATE,
                            kleidicv_border_values_t{}));

  EXPECT_EQ(KLEIDICV_ERROR_RANGE,
            kleidicv_remap_s16_u8(src, 1, KLEIDICV_MAX_IMAGE_PIXELS,
                                  KLEIDICV_MAX_IMAGE_PIXELS, dst, 8, 8, 1, 1,
                                  mapxy, 4, KLEIDICV_BORDER_TYPE_REPLICATE,
                                  kleidicv_border_values_t{}));

  EXPECT_EQ(
      KLEIDICV_ERROR_RANGE,
      kleidicv_remap_s16_u8(src, 1, 1, 1, dst, 1, KLEIDICV_MAX_IMAGE_PIXELS + 1,
                            1, 1, mapxy, 4, KLEIDICV_BORDER_TYPE_REPLICATE,
                            kleidicv_border_values_t{}));

  EXPECT_EQ(KLEIDICV_ERROR_RANGE,
            kleidicv_remap_s16_u8(
                src, 1, 1, 1, dst, 1, KLEIDICV_MAX_IMAGE_PIXELS,
                KLEIDICV_MAX_IMAGE_PIXELS, 1, mapxy, 4,
                KLEIDICV_BORDER_TYPE_REPLICATE, kleidicv_border_values_t{}));
}

TYPED_TEST(Remap16, NotSupported) {
  const TypeParam src[1] = {};
  TypeParam dst[8];
  int16_t mapxy[16] = {};

  EXPECT_EQ(KLEIDICV_ERROR_NOT_IMPLEMENTED,
            kleidicv_remap_s16_u8(src, 1, 1, 1, dst, 8, 8, 1, 2, mapxy, 4,
                                  KLEIDICV_BORDER_TYPE_REPLICATE,
                                  kleidicv_border_values_t{}));

  EXPECT_EQ(KLEIDICV_ERROR_NOT_IMPLEMENTED,
            kleidicv_remap_s16_u8(src, 1, 1, 1, dst, 8, 8, 1, 1, mapxy, 4,
                                  KLEIDICV_BORDER_TYPE_CONSTANT,
                                  kleidicv_border_values_t{}));
}
