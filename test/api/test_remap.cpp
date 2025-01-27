// SPDX-FileCopyrightText: 2024 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#include <gtest/gtest.h>

#include "framework/array.h"
#include "framework/generator.h"
#include "framework/utils.h"
#include "kleidicv/ctypes.h"
#include "kleidicv/kleidicv.h"

#define KLEIDICV_REMAP_S16(type, type_suffix) \
  KLEIDICV_API(remap_s16, kleidicv_remap_s16_##type_suffix, type)

KLEIDICV_REMAP_S16(uint8_t, u8);
KLEIDICV_REMAP_S16(uint16_t, u16);

#define KLEIDICV_REMAP_S16POINT5(type, type_suffix) \
  KLEIDICV_API(remap_s16point5, kleidicv_remap_s16point5_##type_suffix, type)

KLEIDICV_REMAP_S16POINT5(uint8_t, u8);
KLEIDICV_REMAP_S16POINT5(uint16_t, u16);

template <typename ScalarType>
static const ScalarType *get_array2d_element_or_border(
    const test::Array2D<ScalarType> &src, ptrdiff_t x, ptrdiff_t y,
    kleidicv_border_type_t border_type, const ScalarType *border_value) {
  if (border_type == KLEIDICV_BORDER_TYPE_REPLICATE) {
    x = std::clamp<ptrdiff_t>(x, 0, static_cast<ptrdiff_t>(src.width()) - 1);
    y = std::clamp<ptrdiff_t>(y, 0, static_cast<ptrdiff_t>(src.height()) - 1);
  } else {
    assert(border_type == KLEIDICV_BORDER_TYPE_CONSTANT);
    if (x >= static_cast<ptrdiff_t>(src.width()) ||
        y >= static_cast<ptrdiff_t>(src.height()) || x < 0 || y < 0) {
      return border_value;
    }
  }
  return src.at(y, x * src.channels());
}

template <class ScalarType>
class RemapS16 : public testing::Test {
 public:
  static void test_random(size_t src_w, size_t src_h, size_t dst_w,
                          size_t dst_h, size_t channels,
                          kleidicv_border_type_t border_type,
                          const ScalarType *border_value, size_t padding) {
    test::Array2D<int16_t> mapxy{2 * dst_w, dst_h, padding, 2};
    test::PseudoRandomNumberGenerator<int16_t> coord_generator;
    mapxy.fill(coord_generator);
    execute_test(mapxy, src_w, src_h, dst_w, dst_h, channels, border_type,
                 border_value, padding);
  }

  static void test_outside_random(size_t src_w, size_t src_h, size_t dst_w,
                                  size_t dst_h, size_t channels,
                                  kleidicv_border_type_t border_type,
                                  const ScalarType *border_value,
                                  size_t padding) {
    test::Array2D<int16_t> mapxy{2 * dst_w, dst_h, padding, 2};
    test::PseudoRandomNumberGeneratorIntRange<int16_t> coord_generator{
        static_cast<int16_t>(-src_w), static_cast<int16_t>(2 * src_w)};
    mapxy.fill(coord_generator);
    execute_test(mapxy, src_w, src_h, dst_w, dst_h, channels, border_type,
                 border_value, padding);
  }

  static void test_blend(size_t src_w, size_t src_h, size_t dst_w, size_t dst_h,
                         size_t channels, kleidicv_border_type_t border_type,
                         const ScalarType *border_value, size_t padding) {
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
    execute_test(mapxy, src_w, src_h, dst_w, dst_h, channels, border_type,
                 border_value, padding);
  }

  // Test coordinates with edge values that may easily overflow
  static void test_corner_cases(size_t src_w, size_t src_h, size_t dst_w,
                                size_t dst_h, size_t channels,
                                kleidicv_border_type_t border_type,
                                const ScalarType *border_value,
                                size_t padding) {
    test::Array2D<int16_t> mapxy{2 * dst_w, dst_h, padding, 2};
    const int16_t corner_x_values[] = {-32768,
                                       -1,
                                       0,
                                       static_cast<int16_t>(src_w - 2),
                                       static_cast<int16_t>(src_w - 1),
                                       static_cast<int16_t>(src_w),
                                       32767};
    const int16_t corner_y_values[] = {-32768,
                                       -1,
                                       0,
                                       1,
                                       static_cast<int16_t>(src_h - 2),
                                       static_cast<int16_t>(src_h - 1),
                                       static_cast<int16_t>(src_h),
                                       32767};
    // Number of elements in corner_x_values and corner_y_values
    // One more y than x, so it will cycle through all combinations
    static const size_t nx = sizeof(corner_x_values) / sizeof(int16_t);
    static const size_t ny = sizeof(corner_y_values) / sizeof(int16_t);
    size_t counter = 0;
    for (size_t row = 0; row < dst_h; ++row) {
      for (size_t column = 0; column < dst_w; ++column) {
        *mapxy.at(row, column * 2) = corner_x_values[counter % nx];
        *mapxy.at(row, column * 2 + 1) = corner_y_values[counter % ny];
        ++counter;
      }
    }

    // This part is the same as execute_test() but without initializing source.
    // Corner Cases use the biggest possible source.
    size_t src_total_width = channels * src_w;
    size_t dst_total_width = channels * dst_w;

    test::Array2D<ScalarType> source{src_total_width, src_h, padding, channels};
    test::Array2D<ScalarType> actual{dst_total_width, dst_h, padding, channels};
    test::Array2D<ScalarType> expected{dst_total_width, dst_h, padding,
                                       channels};

    actual.fill(42);

    calculate_expected(source, mapxy, border_type, border_value, expected);

    ASSERT_EQ(
        KLEIDICV_OK,
        remap_s16<ScalarType>()(
            source.data(), source.stride(), source.width(), source.height(),
            actual.data(), actual.stride(), actual.width(), actual.height(),
            channels, mapxy.data(), mapxy.stride(), border_type, border_value));

    EXPECT_EQ_ARRAY2D(actual, expected);
  }

 private:
  static void execute_test(test::Array2D<int16_t> &mapxy, size_t src_w,
                           size_t src_h, size_t dst_w, size_t dst_h,
                           size_t channels, kleidicv_border_type_t border_type,
                           const ScalarType *border_value, size_t padding) {
    size_t src_total_width = channels * src_w;
    size_t dst_total_width = channels * dst_w;

    test::Array2D<ScalarType> source{src_total_width, src_h, padding, channels};
    test::Array2D<ScalarType> actual{dst_total_width, dst_h, padding, channels};
    test::Array2D<ScalarType> expected{dst_total_width, dst_h, padding,
                                       channels};

    test::PseudoRandomNumberGenerator<ScalarType> generator;
    source.fill(generator);
    actual.fill(42);

    calculate_expected(source, mapxy, border_type, border_value, expected);

    ASSERT_EQ(
        KLEIDICV_OK,
        remap_s16<ScalarType>()(
            source.data(), source.stride(), source.width(), source.height(),
            actual.data(), actual.stride(), actual.width(), actual.height(),
            channels, mapxy.data(), mapxy.stride(), border_type, border_value));

    EXPECT_EQ_ARRAY2D(actual, expected);
  }
  static void calculate_expected(test::Array2D<ScalarType> &src,
                                 test::Array2D<int16_t> &mapxy,
                                 kleidicv_border_type_t border_type,
                                 const ScalarType *border_value,
                                 test::Array2D<ScalarType> &expected) {
    auto get_src = [&](ptrdiff_t x, ptrdiff_t y) {
      return get_array2d_element_or_border(src, x, y, border_type,
                                           border_value);
    };

    for (size_t row = 0; row < expected.height(); row++) {
      for (size_t column = 0; column < expected.width() / src.channels();
           ++column) {
        for (size_t ch = 0; ch < src.channels(); ++ch) {
          const int16_t *coords = mapxy.at(row, column * 2);
          int16_t x = coords[0], y = coords[1];
          *expected.at(row, column * src.channels() + ch) = get_src(x, y)[ch];
        }
      }
    }
  }
};

using RemapElementTypes = ::testing::Types<uint8_t, uint16_t>;
TYPED_TEST_SUITE(RemapS16, RemapElementTypes);

template <typename T>
static const auto &get_borders() {
  using P = std::pair<kleidicv_border_type_t, const T *>;
  static const T border_value[KLEIDICV_MAXIMUM_CHANNEL_COUNT] = {4, 5, 6, 7};
  static const std::array borders{
      P{KLEIDICV_BORDER_TYPE_REPLICATE, nullptr},
      P{KLEIDICV_BORDER_TYPE_REPLICATE, border_value},
      P{KLEIDICV_BORDER_TYPE_CONSTANT, border_value},
  };
  return borders;
}

TYPED_TEST(RemapS16, RandomNoPadding) {
  size_t src_w = 3 * test::Options::vector_lanes<TypeParam>() - 1;
  size_t src_h = 4;
  size_t dst_w = src_w;
  size_t dst_h = src_h;
  size_t channels = 1;
  size_t padding = 0;
  for (auto [border_type, border_value] : get_borders<TypeParam>()) {
    TestFixture::test_random(src_w, src_h, dst_w, dst_h, channels, border_type,
                             border_value, padding);
  }
}

TYPED_TEST(RemapS16, OutsideRandomPadding) {
  size_t src_w = 3 * test::Options::vector_lanes<TypeParam>() - 1;
  size_t src_h = 4;
  size_t dst_w = src_w;
  size_t dst_h = src_h;
  size_t channels = 1;
  size_t padding = 13;
  for (auto [border_type, border_value] : get_borders<TypeParam>()) {
    TestFixture::test_outside_random(src_w, src_h, dst_w, dst_h, channels,
                                     border_type, border_value, padding);
  }
}

TYPED_TEST(RemapS16, BlendPadding) {
  size_t src_w = 3 * test::Options::vector_lanes<TypeParam>() - 1;
  size_t src_h = 4;
  size_t dst_w = src_w;
  size_t dst_h = src_h;
  size_t channels = 1;
  size_t padding = 13;
  for (auto [border_type, border_value] : get_borders<TypeParam>()) {
    TestFixture::test_blend(src_w, src_h, dst_w, dst_h, channels, border_type,
                            border_value, padding);
  }
}

TYPED_TEST(RemapS16, BlendBigStride) {
  size_t src_w = 3 * test::Options::vector_lanes<TypeParam>() - 1;
  size_t src_h = 16;
  size_t dst_w = src_w;
  size_t dst_h = src_h;
  size_t channels = 1;
  size_t padding = std::numeric_limits<uint16_t>::max() - src_w;
  for (auto [border_type, border_value] : get_borders<TypeParam>()) {
    TestFixture::test_blend(src_w, src_h, dst_w, dst_h, channels, border_type,
                            border_value, padding);
  }
}

TYPED_TEST(RemapS16, CornerCases) {
  size_t src_w = std::numeric_limits<int16_t>::max() + 1;
  size_t src_h = std::numeric_limits<int16_t>::max() + 1;
  size_t dst_w = 3 * test::Options::vector_lanes<TypeParam>() - 1;
  size_t dst_h = 4;
  size_t channels = 1;
  size_t padding = 17;
  for (auto [border_type, border_value] : get_borders<TypeParam>()) {
    TestFixture::test_corner_cases(src_w, src_h, dst_w, dst_h, channels,
                                   border_type, border_value, padding);
  }
}

TYPED_TEST(RemapS16, NullPointer) {
  const TypeParam src[4] = {};
  TypeParam dst[1];
  int16_t mapxy[2] = {};
  test::test_null_args(remap_s16<TypeParam>(), src, 2 * sizeof(TypeParam), 2, 2,
                       dst, 1 * sizeof(TypeParam), 1, 1, 1, mapxy, 4,
                       KLEIDICV_BORDER_TYPE_CONSTANT, src);
}

TYPED_TEST(RemapS16, ZeroImageSize) {
  const TypeParam src[1] = {};
  TypeParam dst[1];
  int16_t mapxy[2] = {};

  EXPECT_EQ(KLEIDICV_ERROR_NOT_IMPLEMENTED,
            remap_s16<TypeParam>()(src, 0, 0, 1, dst, 0, 0, 1, 1, mapxy, 4,
                                   KLEIDICV_BORDER_TYPE_REPLICATE, nullptr));
  EXPECT_EQ(KLEIDICV_ERROR_NOT_IMPLEMENTED,
            remap_s16<TypeParam>()(src, 1 * sizeof(TypeParam), 1, 0, dst,
                                   1 * sizeof(TypeParam), 1, 0, 1, mapxy, 4,
                                   KLEIDICV_BORDER_TYPE_REPLICATE, nullptr));
}

TYPED_TEST(RemapS16, InvalidImageSize) {
  const TypeParam src[1] = {};
  TypeParam dst[8];
  int16_t mapxy[16] = {};

  // Source too wide
  EXPECT_EQ(KLEIDICV_ERROR_NOT_IMPLEMENTED,
            remap_s16<TypeParam>()(src, 1 * sizeof(TypeParam),
                                   std::numeric_limits<int16_t>::max() + 2, 1,
                                   dst, 8, 8, 1, 1, mapxy, 4,
                                   KLEIDICV_BORDER_TYPE_REPLICATE, nullptr));

  // Source too high
  EXPECT_EQ(KLEIDICV_ERROR_NOT_IMPLEMENTED,
            remap_s16<TypeParam>()(src, 1 * sizeof(TypeParam), 1,
                                   std::numeric_limits<int16_t>::max() + 2, dst,
                                   8, 8, 1, 1, mapxy, 4,
                                   KLEIDICV_BORDER_TYPE_REPLICATE, nullptr));

  // Source extremely wide
  EXPECT_EQ(KLEIDICV_ERROR_RANGE,
            remap_s16<TypeParam>()(
                src, (KLEIDICV_MAX_IMAGE_PIXELS + 1) * sizeof(TypeParam),
                KLEIDICV_MAX_IMAGE_PIXELS + 1, 1, dst, 8 * sizeof(TypeParam), 8,
                1, 1, mapxy, 4, KLEIDICV_BORDER_TYPE_REPLICATE, nullptr));

  // Source extremely high
  EXPECT_EQ(KLEIDICV_ERROR_RANGE,
            remap_s16<TypeParam>()(src, sizeof(TypeParam), 1,
                                   KLEIDICV_MAX_IMAGE_PIXELS + 1, dst,
                                   8 * sizeof(TypeParam), 8, 1, 1, mapxy, 4,
                                   KLEIDICV_BORDER_TYPE_REPLICATE, nullptr));

  // Source too large
  EXPECT_EQ(KLEIDICV_ERROR_RANGE,
            remap_s16<TypeParam>()(
                src, KLEIDICV_MAX_IMAGE_PIXELS * sizeof(TypeParam),
                KLEIDICV_MAX_IMAGE_PIXELS, KLEIDICV_MAX_IMAGE_PIXELS, dst,
                8 * sizeof(TypeParam), 8, 1, 1, mapxy, 4,
                KLEIDICV_BORDER_TYPE_REPLICATE, nullptr));

  // Destination too wide
  EXPECT_EQ(KLEIDICV_ERROR_RANGE,
            remap_s16<TypeParam>()(
                src, 1 * sizeof(TypeParam), 1, 1, dst,
                (KLEIDICV_MAX_IMAGE_PIXELS + 1) * sizeof(TypeParam),
                KLEIDICV_MAX_IMAGE_PIXELS + 1, 1, 1, mapxy, 4,
                KLEIDICV_BORDER_TYPE_REPLICATE, nullptr));

  // Destination too large
  EXPECT_EQ(KLEIDICV_ERROR_RANGE,
            remap_s16<TypeParam>()(
                src, 1 * sizeof(TypeParam), 1, 1, dst,
                KLEIDICV_MAX_IMAGE_PIXELS * sizeof(TypeParam),
                KLEIDICV_MAX_IMAGE_PIXELS, KLEIDICV_MAX_IMAGE_PIXELS, 1, mapxy,
                4, KLEIDICV_BORDER_TYPE_REPLICATE, nullptr));
}

TYPED_TEST(RemapS16, UnsupportedBigStride) {
  const TypeParam src[1] = {};
  TypeParam dst[8];
  int16_t mapxy[16] = {};

  EXPECT_EQ(
      KLEIDICV_ERROR_NOT_IMPLEMENTED,
      remap_s16<TypeParam>()(
          src, (std::numeric_limits<uint16_t>::max() + 1L) * sizeof(TypeParam),
          1, 1, dst, 8 * sizeof(TypeParam), 8, 1, 1, mapxy, 4,
          KLEIDICV_BORDER_TYPE_REPLICATE, nullptr));

  EXPECT_EQ(KLEIDICV_OK,
            remap_s16<TypeParam>()(
                src, std::numeric_limits<uint16_t>::max() * sizeof(TypeParam),
                1, 1, dst, 8 * sizeof(TypeParam), 8, 1, 1, mapxy, 4,
                KLEIDICV_BORDER_TYPE_REPLICATE, nullptr));
}

TYPED_TEST(RemapS16, UnsupportedTwoChannels) {
  const TypeParam src[1] = {};
  TypeParam dst[8];
  int16_t mapxy[16] = {};

  EXPECT_EQ(KLEIDICV_ERROR_NOT_IMPLEMENTED,
            remap_s16<TypeParam>()(src, 1 * sizeof(TypeParam), 1, 1, dst,
                                   8 * sizeof(TypeParam), 8, 1, 2, mapxy, 4,
                                   KLEIDICV_BORDER_TYPE_REPLICATE, nullptr));
}

TYPED_TEST(RemapS16, UnsupportedBorderType) {
  const TypeParam src[1] = {};
  TypeParam dst[8];
  int16_t mapxy[16] = {};

  EXPECT_EQ(KLEIDICV_ERROR_NOT_IMPLEMENTED,
            remap_s16<TypeParam>()(src, 1 * sizeof(TypeParam), 1, 1, dst,
                                   8 * sizeof(TypeParam), 8, 1, 1, mapxy, 4,
                                   KLEIDICV_BORDER_TYPE_REFLECT, src));
}

TYPED_TEST(RemapS16, UnsupportedTooSmallImage) {
  const TypeParam src[1] = {};
  TypeParam dst[8];
  int16_t mapxy[16] = {};

  EXPECT_EQ(KLEIDICV_ERROR_NOT_IMPLEMENTED,
            remap_s16<TypeParam>()(src, 1 * sizeof(TypeParam), 1, 1, dst,
                                   8 * sizeof(TypeParam), 7, 1, 1, mapxy, 4,
                                   KLEIDICV_BORDER_TYPE_REPLICATE, nullptr));
}

template <class ScalarType>
class RemapS16Point5 : public testing::Test {
 public:
  static const uint16_t FRAC_BITS = 5;
  static const uint16_t FRAC_MAX = 1 << FRAC_BITS;
  static const uint16_t FRAC_MAX_SQUARE = FRAC_MAX * FRAC_MAX;

  static void test_random(size_t src_w, size_t src_h, size_t dst_w,
                          size_t dst_h, size_t channels,
                          kleidicv_border_type_t border_type,
                          const ScalarType *border_value, size_t padding) {
    test::Array2D<int16_t> mapxy(2 * dst_w, dst_h, padding, 2);
    test::PseudoRandomNumberGenerator<int16_t> coord_generator;
    mapxy.fill(coord_generator);
    test::Array2D<uint16_t> mapfrac(dst_w, dst_h, padding);
    test::PseudoRandomNumberGenerator<uint16_t> frac_generator;
    mapfrac.fill(frac_generator);
    execute_test(mapxy, mapfrac, src_w, src_h, dst_w, dst_h, channels,
                 border_type, border_value, padding);
  }

  static void test_outside_random(size_t src_w, size_t src_h, size_t dst_w,
                                  size_t dst_h, size_t channels,
                                  kleidicv_border_type_t border_type,
                                  const ScalarType *border_value,
                                  size_t padding) {
    test::Array2D<int16_t> mapxy(2 * dst_w, dst_h, padding, 2);
    test::PseudoRandomNumberGeneratorIntRange<int16_t> coord_generator{
        static_cast<int16_t>(-src_w), static_cast<int16_t>(2 * src_w)};
    mapxy.fill(coord_generator);
    test::Array2D<uint16_t> mapfrac(dst_w, dst_h, padding);
    test::PseudoRandomNumberGeneratorIntRange<uint16_t> frac_generator(
        0, static_cast<uint16_t>(FRAC_MAX_SQUARE * 3 / 2));
    mapfrac.fill(frac_generator);
    execute_test(mapxy, mapfrac, src_w, src_h, dst_w, dst_h, channels,
                 border_type, border_value, padding);
  }

  static void test_blend(size_t src_w, size_t src_h, size_t dst_w, size_t dst_h,
                         size_t channels, kleidicv_border_type_t border_type,
                         const ScalarType *border_value, size_t padding) {
    test::Array2D<int16_t> mapxy{2 * dst_w, dst_h, padding, 2};
    test::Array2D<uint16_t> mapfrac(dst_w, dst_h, padding);
    for (size_t row = 0; row < dst_h; ++row) {
      for (size_t column = 0; column < dst_w; ++column) {
        // Use a second degree function to add a nonlinear blend to the image
        auto r = static_cast<double>(row), c = static_cast<double>(column),
             w = static_cast<double>(dst_w), h = static_cast<double>(dst_h);
        double x = c * 2 - c * c / w;
        double y = r * (w - c) / w + 4 * r / h;
        *mapxy.at(row, column * 2) = std::max<int16_t>(
            0,
            std::min(static_cast<int16_t>(src_w - 1), static_cast<int16_t>(x)));
        *mapxy.at(row, column * 2 + 1) = std::max<int16_t>(
            0,
            std::min(static_cast<int16_t>(src_h - 1), static_cast<int16_t>(y)));
        *mapfrac.at(row, column) =
            static_cast<uint16_t>(FRAC_MAX * (x - static_cast<int16_t>(x))) |
            (static_cast<uint16_t>(FRAC_MAX * (y - static_cast<int16_t>(y)))
             << FRAC_BITS);
      }
    }
    execute_test(mapxy, mapfrac, src_w, src_h, dst_w, dst_h, channels,
                 border_type, border_value, padding);
  }

  // Test coordinates with edge values that may easily overflow
  static void test_corner_cases(size_t src_w, size_t src_h, size_t dst_w,
                                size_t dst_h, size_t channels,
                                kleidicv_border_type_t border_type,
                                const ScalarType *border_value,
                                size_t padding) {
    test::Array2D<int16_t> mapxy{2 * dst_w, dst_h, padding, 2};
    test::Array2D<uint16_t> mapfrac(dst_w, dst_h, padding);
    // One more y than x so we'll see many combinations
    const int16_t corner_x_values[] = {-32768,
                                       -1,
                                       0,
                                       static_cast<int16_t>(src_w - 2),
                                       static_cast<int16_t>(src_w - 1),
                                       static_cast<int16_t>(src_w),
                                       32767};
    const int16_t corner_y_values[] = {-32768,
                                       -1,
                                       0,
                                       1,
                                       static_cast<int16_t>(src_h - 2),
                                       static_cast<int16_t>(src_h - 1),
                                       static_cast<int16_t>(src_h),
                                       32767};
    static const size_t nx = sizeof(corner_x_values) / sizeof(int16_t);
    static const size_t ny = sizeof(corner_y_values) / sizeof(int16_t);
    size_t counter = 0;
    for (size_t row = 0; row < dst_h; ++row) {
      for (size_t column = 0; column < dst_w; ++column) {
        *mapxy.at(row, column * 2) = corner_x_values[counter % nx];
        *mapxy.at(row, column * 2 + 1) = corner_y_values[counter % ny];
        *mapfrac.at(row, column) =
            static_cast<uint16_t>((counter * 3) % FRAC_MAX) |
            (static_cast<uint16_t>((counter * 5) % FRAC_MAX) << FRAC_BITS);
        ++counter;
      }
    }

    // This part is the same as execute_test() but without initializing source.
    // Corner Cases use the biggest possible source.
    size_t src_total_width = channels * src_w;
    size_t dst_total_width = channels * dst_w;

    test::Array2D<ScalarType> source{src_total_width, src_h, padding, channels};
    test::Array2D<ScalarType> actual{dst_total_width, dst_h, padding, channels};
    test::Array2D<ScalarType> expected{dst_total_width, dst_h, padding,
                                       channels};

    actual.fill(42);

    calculate_expected(source, mapxy, mapfrac, border_type, border_value,
                       expected);

    ASSERT_EQ(KLEIDICV_OK, remap_s16point5<ScalarType>()(
                               source.data(), source.stride(), source.width(),
                               source.height(), actual.data(), actual.stride(),
                               actual.width(), actual.height(), channels,
                               mapxy.data(), mapxy.stride(), mapfrac.data(),
                               mapfrac.stride(), border_type, border_value));

    EXPECT_EQ_ARRAY2D(actual, expected);
  }

 private:
  static void execute_test(test::Array2D<int16_t> &mapxy,
                           test::Array2D<uint16_t> &mapfrac, size_t src_w,
                           size_t src_h, size_t dst_w, size_t dst_h,
                           size_t channels, kleidicv_border_type_t border_type,
                           const ScalarType *border_value, size_t padding) {
    size_t src_total_width = channels * src_w;
    size_t dst_total_width = channels * dst_w;

    test::Array2D<ScalarType> source{src_total_width, src_h, padding, channels};
    test::Array2D<ScalarType> actual{dst_total_width, dst_h, padding, channels};
    test::Array2D<ScalarType> expected{dst_total_width, dst_h, padding,
                                       channels};
    test::PseudoRandomNumberGenerator<ScalarType> generator;
    source.fill(generator);
    actual.fill(42);

    calculate_expected(source, mapxy, mapfrac, border_type, border_value,
                       expected);

    ASSERT_EQ(KLEIDICV_OK, remap_s16point5<ScalarType>()(
                               source.data(), source.stride(), source.width(),
                               source.height(), actual.data(), actual.stride(),
                               actual.width(), actual.height(), channels,
                               mapxy.data(), mapxy.stride(), mapfrac.data(),
                               mapfrac.stride(), border_type, border_value));

    EXPECT_EQ_ARRAY2D(actual, expected);
  }

  static ScalarType lerp2d(size_t cx, size_t cy, ScalarType a, ScalarType b,
                           ScalarType c, ScalarType d) {
    size_t inv_cx = FRAC_MAX - cx, inv_cy = FRAC_MAX - cy;
    ScalarType r = static_cast<ScalarType>((inv_cx * inv_cy * a +
                                            cx * inv_cy * b + inv_cx * cy * c +
                                            cx * cy * d + FRAC_MAX_SQUARE / 2) /
                                           FRAC_MAX_SQUARE);
    return r;
  }

  static void calculate_expected(test::Array2D<ScalarType> &src,
                                 test::Array2D<int16_t> &mapxy,
                                 test::Array2D<uint16_t> &mapfrac,
                                 kleidicv_border_type_t border_type,
                                 const ScalarType *border_value,
                                 test::Array2D<ScalarType> &expected) {
    auto get_src = [&](ptrdiff_t x, ptrdiff_t y) {
      return get_array2d_element_or_border(src, x, y, border_type,
                                           border_value);
    };

    for (size_t row = 0; row < expected.height(); row++) {
      for (size_t column = 0; column < expected.width() / src.channels();
           ++column) {
        for (size_t ch = 0; ch < src.channels(); ++ch) {
          // Clang-tidy thinks mapfrac may contain garbage, but it is fully
          // initialized at all code paths and the map size always equals dst
          // map pixel size
          // NOLINTBEGIN(clang-analyzer-core.UndefinedBinaryOperatorResult)
          uint8_t x_frac = *mapfrac.at(row, column) & (FRAC_MAX - 1);
          uint8_t y_frac =
              (*mapfrac.at(row, column) >> FRAC_BITS) & (FRAC_MAX - 1);
          // NOLINTEND(clang-analyzer-core.UndefinedBinaryOperatorResult)
          const int16_t *coords = mapxy.at(row, column * 2);
          int16_t x = coords[0], y = coords[1];
          *expected.at(row, column * src.channels() + ch) =
              lerp2d(x_frac, y_frac, get_src(x, y)[ch], get_src(x + 1, y)[ch],
                     get_src(x, y + 1)[ch], get_src(x + 1, y + 1)[ch]);
        }
      }
    }
  }
};

using RemapS16Point5ElementTypes = ::testing::Types<uint8_t, uint16_t>;
TYPED_TEST_SUITE(RemapS16Point5, RemapS16Point5ElementTypes);

TYPED_TEST(RemapS16Point5, RandomNoPadding) {
  size_t src_w = 3 * test::Options::vector_lanes<TypeParam>() - 1;
  size_t src_h = 4;
  size_t dst_w = src_w;
  size_t dst_h = src_h;
  size_t channels = 1;
  size_t padding = 0;
  for (auto [border_type, border_value] : get_borders<TypeParam>()) {
    TestFixture::test_random(src_w, src_h, dst_w, dst_h, channels, border_type,
                             border_value, padding);
  }
}

TYPED_TEST(RemapS16Point5, BlendPadding) {
  size_t src_w = 3 * test::Options::vector_lanes<TypeParam>() - 1;
  size_t src_h = 4;
  size_t dst_w = src_w;
  size_t dst_h = src_h;
  size_t channels = 1;
  size_t padding = 13;
  for (auto [border_type, border_value] : get_borders<TypeParam>()) {
    TestFixture::test_blend(src_w, src_h, dst_w, dst_h, channels, border_type,
                            border_value, padding);
  }
}

TYPED_TEST(RemapS16Point5, OutsideRandomPadding) {
  size_t src_w = 3 * test::Options::vector_lanes<TypeParam>() - 1;
  size_t src_h = 4;
  size_t dst_w = src_w;
  size_t dst_h = src_h;
  size_t channels = 1;
  size_t padding = 13;
  for (auto [border_type, border_value] : get_borders<TypeParam>()) {
    TestFixture::test_outside_random(src_w, src_h, dst_w, dst_h, channels,
                                     border_type, border_value, padding);
  }
}

TYPED_TEST(RemapS16Point5, BlendBigStride) {
  size_t src_w = 3 * test::Options::vector_lanes<TypeParam>() - 1;
  size_t src_h = 16;
  size_t dst_w = src_w;
  size_t dst_h = src_h;
  size_t channels = 1;
  size_t padding = std::numeric_limits<uint16_t>::max() - src_w;
  for (auto [border_type, border_value] : get_borders<TypeParam>()) {
    TestFixture::test_blend(src_w, src_h, dst_w, dst_h, channels, border_type,
                            border_value, padding);
  }
}

TYPED_TEST(RemapS16Point5, CornerCases) {
  size_t src_w = std::numeric_limits<int16_t>::max() + 1;
  size_t src_h = std::numeric_limits<int16_t>::max() + 1;
  size_t dst_w = 3 * test::Options::vector_lanes<TypeParam>() - 1;
  size_t dst_h = 4;
  size_t channels = 1;
  size_t padding = 17;
  for (auto [border_type, border_value] : get_borders<TypeParam>()) {
    TestFixture::test_corner_cases(src_w, src_h, dst_w, dst_h, channels,
                                   border_type, border_value, padding);
  }
}

TYPED_TEST(RemapS16Point5, NullPointer) {
  const TypeParam src[4] = {};
  TypeParam dst[1];
  int16_t mapxy[2] = {};
  uint16_t mapfrac[1] = {};
  test::test_null_args(remap_s16point5<TypeParam>(), src, 2 * sizeof(TypeParam),
                       2, 2, dst, 1 * sizeof(TypeParam), 1, 1, 1, mapxy, 4,
                       mapfrac, 2, KLEIDICV_BORDER_TYPE_CONSTANT, src);
}

TYPED_TEST(RemapS16Point5, ZeroImageSize) {
  const TypeParam src[1] = {};
  TypeParam dst[1];
  int16_t mapxy[2] = {};
  uint16_t mapfrac[1] = {};

  EXPECT_EQ(KLEIDICV_ERROR_NOT_IMPLEMENTED,
            remap_s16point5<TypeParam>()(
                src, 0, 0, 1, dst, 0, 0, 1, 1, mapxy, 4, mapfrac, 2,
                KLEIDICV_BORDER_TYPE_REPLICATE, nullptr));
  EXPECT_EQ(
      KLEIDICV_ERROR_NOT_IMPLEMENTED,
      remap_s16point5<TypeParam>()(
          src, 1 * sizeof(TypeParam), 1, 0, dst, 1 * sizeof(TypeParam), 1, 0, 1,
          mapxy, 4, mapfrac, 2, KLEIDICV_BORDER_TYPE_REPLICATE, nullptr));
}

TYPED_TEST(RemapS16Point5, InvalidImageSize) {
  const TypeParam src[1] = {};
  TypeParam dst[1];
  int16_t mapxy[2] = {};
  uint16_t mapfrac[1] = {};

  const size_t kTooBig = std::numeric_limits<int16_t>::max() + 2L;
  // Source too wide
  EXPECT_EQ(KLEIDICV_ERROR_NOT_IMPLEMENTED,
            remap_s16point5<TypeParam>()(
                src, kTooBig * sizeof(TypeParam), kTooBig, 1, dst,
                8 * sizeof(TypeParam), 8, 1, 1, mapxy, 4, mapfrac, 2,
                KLEIDICV_BORDER_TYPE_REPLICATE, nullptr));

  // Source too high
  EXPECT_EQ(
      KLEIDICV_ERROR_NOT_IMPLEMENTED,
      remap_s16point5<TypeParam>()(
          src, sizeof(TypeParam), 1, kTooBig, dst, 8 * sizeof(TypeParam), 8, 1,
          1, mapxy, 4, mapfrac, 2, KLEIDICV_BORDER_TYPE_REPLICATE, nullptr));

  // Source extremely wide
  EXPECT_EQ(
      KLEIDICV_ERROR_RANGE,
      remap_s16point5<TypeParam>()(
          src, (KLEIDICV_MAX_IMAGE_PIXELS + 1) * sizeof(TypeParam),
          KLEIDICV_MAX_IMAGE_PIXELS + 1, 1, dst, 8 * sizeof(TypeParam), 8, 1, 1,
          mapxy, 4, mapfrac, 2, KLEIDICV_BORDER_TYPE_REPLICATE, nullptr));

  // Source extremely high
  EXPECT_EQ(KLEIDICV_ERROR_RANGE,
            remap_s16point5<TypeParam>()(
                src, sizeof(TypeParam), 1, KLEIDICV_MAX_IMAGE_PIXELS + 1, dst,
                8 * sizeof(TypeParam), 8, 1, 1, mapxy, 4, mapfrac, 2,
                KLEIDICV_BORDER_TYPE_REPLICATE, nullptr));

  // Source too large
  EXPECT_EQ(KLEIDICV_ERROR_RANGE,
            remap_s16point5<TypeParam>()(
                src, KLEIDICV_MAX_IMAGE_PIXELS * sizeof(TypeParam),
                KLEIDICV_MAX_IMAGE_PIXELS, KLEIDICV_MAX_IMAGE_PIXELS, dst,
                8 * sizeof(TypeParam), 8, 1, 1, mapxy, 4, mapfrac, 2,
                KLEIDICV_BORDER_TYPE_REPLICATE, nullptr));

  // Destination too wide
  EXPECT_EQ(KLEIDICV_ERROR_RANGE,
            remap_s16point5<TypeParam>()(
                src, sizeof(TypeParam), 1, 1, dst,
                (KLEIDICV_MAX_IMAGE_PIXELS + 1) * sizeof(TypeParam),
                KLEIDICV_MAX_IMAGE_PIXELS + 1, 1, 1, mapxy, 4, mapfrac, 2,
                KLEIDICV_BORDER_TYPE_REPLICATE, nullptr));

  // Destination too large
  EXPECT_EQ(KLEIDICV_ERROR_RANGE,
            remap_s16point5<TypeParam>()(
                src, 1 * sizeof(TypeParam), 1, 1, dst,
                (KLEIDICV_MAX_IMAGE_PIXELS + 1) * sizeof(TypeParam),
                KLEIDICV_MAX_IMAGE_PIXELS, KLEIDICV_MAX_IMAGE_PIXELS, 1, mapxy,
                4, mapfrac, 2, KLEIDICV_BORDER_TYPE_REPLICATE, nullptr));
}

TYPED_TEST(RemapS16Point5, UnsupportedBigStride) {
  const TypeParam src[1] = {};
  TypeParam dst[8];
  int16_t mapxy[16] = {};
  uint16_t mapfrac[8] = {};

  EXPECT_EQ(
      KLEIDICV_ERROR_NOT_IMPLEMENTED,
      remap_s16point5<TypeParam>()(
          src, (std::numeric_limits<uint16_t>::max() + 1L) * sizeof(TypeParam),
          1, 1, dst, 8 * sizeof(TypeParam), 8, 1, 1, mapxy, 4, mapfrac, 2,
          KLEIDICV_BORDER_TYPE_REPLICATE, nullptr));

  EXPECT_EQ(KLEIDICV_OK,
            remap_s16point5<TypeParam>()(
                src, std::numeric_limits<uint16_t>::max() * sizeof(TypeParam),
                1, 1, dst, 8 * sizeof(TypeParam), 8, 1, 1, mapxy, 4, mapfrac, 2,
                KLEIDICV_BORDER_TYPE_REPLICATE, nullptr));
}

TYPED_TEST(RemapS16Point5, UnsupportedTwoChannels) {
  const TypeParam src[1] = {};
  TypeParam dst[8];
  int16_t mapxy[16] = {};
  uint16_t mapfrac[8] = {};

  EXPECT_EQ(
      KLEIDICV_ERROR_NOT_IMPLEMENTED,
      remap_s16point5<TypeParam>()(
          src, 1 * sizeof(TypeParam), 1, 1, dst, 8 * sizeof(TypeParam), 8, 1, 2,
          mapxy, 4, mapfrac, 2, KLEIDICV_BORDER_TYPE_REPLICATE, nullptr));
}

TYPED_TEST(RemapS16Point5, UnsupportedBorderType) {
  const TypeParam src[1] = {};
  TypeParam dst[8];
  int16_t mapxy[16] = {};
  uint16_t mapfrac[8] = {};

  EXPECT_EQ(KLEIDICV_ERROR_NOT_IMPLEMENTED,
            remap_s16point5<TypeParam>()(
                src, 1 * sizeof(TypeParam), 1, 1, dst, 8 * sizeof(TypeParam), 8,
                1, 1, mapxy, 4, mapfrac, 2, KLEIDICV_BORDER_TYPE_REFLECT, src));
}

TYPED_TEST(RemapS16Point5, UnsupportedTooSmallImage) {
  const TypeParam src[1] = {};
  TypeParam dst[8];
  int16_t mapxy[16] = {};
  uint16_t mapfrac[8] = {};

  EXPECT_EQ(
      KLEIDICV_ERROR_NOT_IMPLEMENTED,
      remap_s16point5<TypeParam>()(
          src, 1 * sizeof(TypeParam), 1, 1, dst, 8 * sizeof(TypeParam), 7, 1, 1,
          mapxy, 4, mapfrac, 2, KLEIDICV_BORDER_TYPE_REPLICATE, nullptr));
}
