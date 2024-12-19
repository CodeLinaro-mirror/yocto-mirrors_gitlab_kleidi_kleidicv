// SPDX-FileCopyrightText: 2024 - 2025 Arm Limited and/or its affiliates <open-source-office@arm.com>
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

template <class ScalarType>
class RemapF32 : public testing::Test {
 public:
  static void test_random(size_t src_w, size_t src_h, size_t dst_w,
                          size_t dst_h, size_t channels, size_t padding) {
    test::Array2D<float> mapx(dst_w, dst_h, padding);
    test::PseudoRandomNumberGenerator<float> xcoord_generator;
    mapx.fill(xcoord_generator);
    test::Array2D<float> mapy(dst_w, dst_h, padding);
    test::PseudoRandomNumberGenerator<float> ycoord_generator;
    mapy.fill(ycoord_generator);
    execute_test(mapx, mapy, src_w, src_h, dst_w, dst_h, channels, padding);
  }

  static void test_outside_random(size_t src_w, size_t src_h, size_t dst_w,
                                  size_t dst_h, size_t channels,
                                  size_t padding) {
    test::Array2D<float> mapx(dst_w, dst_h, padding);
    test::PseudoRandomNumberGeneratorFloatRange<float> xcoord_generator{
        // static_cast<float>(-src_w), static_cast<float>(2 * src_w)}; -src_w
        // overflow
        static_cast<float>(-static_cast<int64_t>(src_w)),
        static_cast<float>(2 * src_w)};
    mapx.fill(xcoord_generator);
    test::Array2D<float> mapy(dst_w, dst_h, padding);
    test::PseudoRandomNumberGeneratorFloatRange<float> ycoord_generator{
        static_cast<float>(-static_cast<int64_t>(src_h)),
        static_cast<float>(2 * src_h)};
    mapy.fill(ycoord_generator);
    execute_test(mapx, mapy, src_w, src_h, dst_w, dst_h, channels, padding);
  }

  static void test_blend(size_t src_w, size_t src_h, size_t dst_w, size_t dst_h,
                         size_t channels, size_t padding) {
    test::Array2D<float> mapx(dst_w, dst_h, padding);
    test::Array2D<float> mapy(dst_w, dst_h, padding);
    for (size_t row = 0; row < dst_h; ++row) {
      for (size_t column = 0; column < dst_w; ++column) {
        // Use a second degree function to add a nonlinear blend to the image
        auto r = static_cast<double>(row), c = static_cast<double>(column),
             w = static_cast<double>(dst_w), h = static_cast<double>(dst_h);
        double x = c * 2 - c * c / w;
        double y = r * (w - c) / w + 4 * r / h;
        *mapx.at(row, column) = std::max<float>(
            0, std::min(static_cast<float>(src_w - 1), static_cast<float>(x)));
        *mapy.at(row, column) = std::max<float>(
            0, std::min(static_cast<float>(src_h - 1), static_cast<float>(y)));
      }
    }
    execute_test(mapx, mapy, src_w, src_h, dst_w, dst_h, channels, padding);
  }

  // Test coordinates with edge values that may easily overflow
  static void test_corner_cases(size_t src_w, size_t src_h, size_t dst_w,
                                size_t dst_h, size_t channels, size_t padding) {
    test::Array2D<float> mapx(dst_w, dst_h, padding);
    test::Array2D<float> mapy(dst_w, dst_h, padding);
    const float corner_x_values[] = {std::numeric_limits<float>::min(),
                                     -1,
                                     0,
                                     static_cast<float>(src_w - 2),
                                     static_cast<float>(src_w - 1),
                                     static_cast<float>(src_w),
                                     std::numeric_limits<float>::max()};
    const float corner_y_values[] = {std::numeric_limits<float>::min(),
                                     -1,
                                     0,
                                     1,
                                     static_cast<float>(src_h - 2),
                                     static_cast<float>(src_h - 1),
                                     static_cast<float>(src_h),
                                     std::numeric_limits<float>::max()};
    const size_t nx = sizeof(corner_x_values) / sizeof(float);
    const size_t ny = sizeof(corner_y_values) / sizeof(float);
    size_t counter = 0;
    for (size_t row = 0; row < dst_h; ++row) {
      for (size_t column = 0; column < dst_w; ++column) {
        *mapx.at(row, column) = corner_x_values[counter % nx];
        *mapy.at(row, column) = corner_y_values[counter % ny];
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

    test::PseudoRandomNumberGenerator<ScalarType> generator;
    actual.fill(42);

    calculate_expected(source, mapx, mapy, expected);

    ASSERT_EQ(
        KLEIDICV_OK,
        kleidicv_remap_f32_u8(
            source.data(), source.stride(), source.width(), source.height(),
            actual.data(), actual.stride(), actual.width(), actual.height(),
            channels, mapx.data(), mapx.stride(), mapy.data(), mapy.stride(),
            KLEIDICV_INTERPOLATION_LINEAR, KLEIDICV_BORDER_TYPE_REPLICATE, {}));

    EXPECT_EQ_ARRAY2D(actual, expected);
  }

 private:
  static void execute_test(test::Array2D<float> &mapx,
                           test::Array2D<float> &mapy, size_t src_w,
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

    calculate_expected(source, mapx, mapy, expected);

    ASSERT_EQ(
        KLEIDICV_OK,
        kleidicv_remap_f32_u8(
            source.data(), source.stride(), source.width(), source.height(),
            actual.data(), actual.stride(), actual.width(), actual.height(),
            channels, mapx.data(), mapx.stride(), mapy.data(), mapy.stride(),
            KLEIDICV_INTERPOLATION_LINEAR, KLEIDICV_BORDER_TYPE_REPLICATE, {}));

    EXPECT_EQ_ARRAY2D(actual, expected);
    // EXPECT_EQ_ARRAY2D_WITH_TOLERANCE(1, actual, expected);  // Needed?
  }

  static void calculate_expected(test::Array2D<ScalarType> &src,
                                 test::Array2D<float> &mapx,
                                 test::Array2D<float> &mapy,
                                 test::Array2D<ScalarType> &expected) {
    for (size_t row = 0; row < expected.height(); row++) {
      for (size_t column = 0; column < expected.width() / src.channels();
           ++column) {
        // Should we use float or double during the calculation?
        float x = std::max<float>(
            0, std::min<float>(src.width() - 1, *mapx.at(row, column)));
        float y = std::max<float>(
            0, std::min<float>(src.height() - 1, *mapy.at(row, column)));
        // get floor(x) and floor(y)
        uint32_t x0 = static_cast<uint32_t>(x);
        uint32_t y0 = static_cast<uint32_t>(y);
        // get floor(x) + 1 and floor(y)
        uint32_t x1 = (x0 < src.width() - 1) ? x0 + 1 : x0;
        uint32_t y1 = (y0 < src.height() - 1) ? y0 + 1 : y0;
        // get frac part of x and y
        float xfrac = (x0 < src.width() - 1) ? x - static_cast<float>(x0) : 0.0;
        float yfrac =
            (y0 < src.height() - 1) ? y - static_cast<float>(y0) : 0.0;
        for (size_t ch = 0; ch < src.channels(); ++ch) {
          float a = *src.at(y0, x0 * src.channels() + ch);
          float b = *src.at(y0, x1 * src.channels() + ch);
          float c = *src.at(y1, x0 * src.channels() + ch);
          float d = *src.at(y1, x1 * src.channels() + ch);
          float line1 = (b - a) * xfrac + a;
          float line2 = (d - c) * xfrac + c;
          float float_result = (line2 - line1) * yfrac + line1;
          *expected.at(row, column * src.channels() + ch) =
              static_cast<ScalarType>(std::lround(float_result));
        }
      }
    }
  }
};

using RemapF32ElementTypes = ::testing::Types<uint8_t>;
TYPED_TEST_SUITE(RemapF32, RemapF32ElementTypes);

TYPED_TEST(RemapF32, RandomNoPadding) {
  size_t src_w = 3 * test::Options::vector_lanes<TypeParam>() - 1;
  size_t src_h = 4;
  size_t dst_w = src_w;
  size_t dst_h = src_h;
  TestFixture::test_random(src_w, src_h, dst_w, dst_h, 1, 0);
}

TYPED_TEST(RemapF32, BlendPadding) {
  size_t src_w = 3 * test::Options::vector_lanes<TypeParam>() - 1;
  size_t src_h = 4;
  size_t dst_w = src_w;
  size_t dst_h = src_h;
  TestFixture::test_blend(src_w, src_h, dst_w, dst_h, 1, 13);
}

TYPED_TEST(RemapF32, OutsideRandomPadding) {
  size_t src_w = 3 * test::Options::vector_lanes<TypeParam>() - 1;
  size_t src_h = 4;
  size_t dst_w = src_w;
  size_t dst_h = src_h;
  TestFixture::test_outside_random(src_w, src_h, dst_w, dst_h, 1, 13);
}

TYPED_TEST(RemapF32, BlendBigStride) {
  size_t src_w = 3 * test::Options::vector_lanes<TypeParam>() - 1;
  size_t src_h = 16;
  size_t dst_w = src_w;
  size_t dst_h = src_h;
  size_t channels = 1;
  // size_t padding = std::numeric_limits<uint32_t>::max() - src_w; //OOM
  size_t padding = std::numeric_limits<uint16_t>::max() - src_w;
  TestFixture::test_blend(src_w, src_h, dst_w, dst_h, channels, padding);
}

TYPED_TEST(RemapF32, CornerCases) {
  // size_t src_w = (1ULL << 24) - 1; //OOM
  // size_t src_h = (1ULL << 24) - 1; //OOM
  size_t src_w = (1ULL << 14) - 1;
  size_t src_h = (1ULL << 14) - 1;
  size_t dst_w = 4;
  size_t dst_h = 3 * test::Options::vector_lanes<TypeParam>() - 1;
  TestFixture::test_corner_cases(src_w, src_h, dst_w, dst_h, 1, 17);
}

// Test for load_src_into_floats_large
TYPED_TEST(RemapF32, CornerCasesLargeLoad) {
  // TODO: Take to long to run! Need fix
  size_t src_w = (1ULL << 16) - 1;
  size_t src_h = (1ULL << 16) - 1;
  size_t dst_w = 4;
  size_t dst_h = 3 * test::Options::vector_lanes<TypeParam>() - 1;
  TestFixture::test_corner_cases(src_w, src_h, dst_w, dst_h, 1, 17);
}

TYPED_TEST(RemapF32, NullPointer) {
  const size_t element_size = sizeof(TypeParam);
  const size_t src_width = 2;
  const size_t src_height = 2;
  const size_t src_stride = src_width * element_size;
  const size_t dst_width = 1;
  const size_t dst_height = 1;
  const size_t dst_stride = dst_width * element_size;
  const TypeParam src[4] = {};
  TypeParam dst[1];
  const size_t channels = 1;
  float mapx[1] = {};
  const size_t mapx_stride = dst_width * sizeof(float);
  float mapy[1] = {};
  const size_t mapy_stride = dst_width * sizeof(float);
  test::test_null_args(kleidicv_remap_f32_u8, src, src_stride, src_width,
                       src_height, dst, dst_stride, dst_width, dst_height,
                       channels, mapx, mapx_stride, mapy, mapy_stride,
                       KLEIDICV_INTERPOLATION_LINEAR,
                       KLEIDICV_BORDER_TYPE_REPLICATE, nullptr);
}

TYPED_TEST(RemapF32, ZeroImageSize) {
  const TypeParam src[1] = {};
  TypeParam dst[1];
  const size_t src_stride = sizeof(TypeParam);
  const size_t dst_stride = sizeof(TypeParam);
  float mapx[1] = {};
  float mapy[1] = {};
  const size_t mapx_stride = sizeof(float);
  const size_t mapy_stride = sizeof(float);

  EXPECT_EQ(KLEIDICV_ERROR_NOT_IMPLEMENTED,
            kleidicv_remap_f32_u8(src, src_stride, 0, 1, dst, dst_stride, 0, 1,
                                  1, mapx, mapx_stride, mapy, mapy_stride,
                                  KLEIDICV_INTERPOLATION_LINEAR,
                                  KLEIDICV_BORDER_TYPE_REPLICATE, nullptr));
  EXPECT_EQ(KLEIDICV_ERROR_NOT_IMPLEMENTED,
            kleidicv_remap_f32_u8(src, src_stride, 1, 0, dst, dst_stride, 1, 0,
                                  1, mapx, mapx_stride, mapy, mapy_stride,
                                  KLEIDICV_INTERPOLATION_LINEAR,
                                  KLEIDICV_BORDER_TYPE_REPLICATE, nullptr));
}

TYPED_TEST(RemapF32, InvalidImageSize) {
  const size_t element_size = sizeof(TypeParam);
  const TypeParam src[1] = {};
  TypeParam dst[1];
  float mapx[1] = {};
  float mapy[1] = {};

  // Q: what happens if `src_stride` less than `width * sizeof(type)` and row>1
  EXPECT_EQ(
      KLEIDICV_ERROR_RANGE,
      kleidicv_remap_f32_u8(src, element_size, KLEIDICV_MAX_IMAGE_PIXELS + 1, 1,
                            dst, element_size, 1, 1, 1, mapx, sizeof(float),
                            mapy, sizeof(float), KLEIDICV_INTERPOLATION_LINEAR,
                            KLEIDICV_BORDER_TYPE_REPLICATE, nullptr));

  EXPECT_EQ(KLEIDICV_ERROR_RANGE,
            kleidicv_remap_f32_u8(src, element_size, KLEIDICV_MAX_IMAGE_PIXELS,
                                  KLEIDICV_MAX_IMAGE_PIXELS, dst, element_size,
                                  1, 1, 1, mapx, sizeof(float), mapy,
                                  sizeof(float), KLEIDICV_INTERPOLATION_LINEAR,
                                  KLEIDICV_BORDER_TYPE_REPLICATE, nullptr));

  EXPECT_EQ(KLEIDICV_ERROR_RANGE,
            kleidicv_remap_f32_u8(src, element_size, 1, 1, dst, element_size,
                                  KLEIDICV_MAX_IMAGE_PIXELS + 1, 1, 1, mapx,
                                  sizeof(float), mapy, sizeof(float),
                                  KLEIDICV_INTERPOLATION_LINEAR,
                                  KLEIDICV_BORDER_TYPE_REPLICATE, nullptr));

  EXPECT_EQ(
      KLEIDICV_ERROR_RANGE,
      kleidicv_remap_f32_u8(src, element_size, 1, 1, dst, element_size,
                            KLEIDICV_MAX_IMAGE_PIXELS,
                            KLEIDICV_MAX_IMAGE_PIXELS, 1, mapx, sizeof(float),
                            mapy, sizeof(float), KLEIDICV_INTERPOLATION_LINEAR,
                            KLEIDICV_BORDER_TYPE_REPLICATE, nullptr));
}

TYPED_TEST(RemapF32, UnsupportedTwoChannels) {
  const size_t element_size = sizeof(TypeParam);
  const TypeParam src[1] = {};
  TypeParam dst[16];
  float mapx[16] = {};
  float mapy[16] = {};
  const size_t channels = 2;

  EXPECT_EQ(
      KLEIDICV_ERROR_NOT_IMPLEMENTED,
      kleidicv_remap_f32_u8(src, element_size, 1, 1, dst, 16 * element_size, 16,
                            1, channels, mapx, 16 * sizeof(float), mapy,
                            16 * sizeof(float), KLEIDICV_INTERPOLATION_LINEAR,
                            KLEIDICV_BORDER_TYPE_REPLICATE, nullptr));
}

TYPED_TEST(RemapF32, UnsupportedInterpolationTypeNEAREST) {
  const size_t element_size = sizeof(TypeParam);
  const TypeParam src[1] = {};
  TypeParam dst[16];
  float mapx[16] = {};
  float mapy[16] = {};

  EXPECT_EQ(
      KLEIDICV_ERROR_NOT_IMPLEMENTED,
      kleidicv_remap_f32_u8(src, element_size, 1, 1, dst, 16 * element_size, 16,
                            1, 1, mapx, 16 * sizeof(float), mapy,
                            16 * sizeof(float), KLEIDICV_INTERPOLATION_NEAREST,
                            KLEIDICV_BORDER_TYPE_REPLICATE, nullptr));
}

TYPED_TEST(RemapF32, UnsupportedBorderTypeConst) {
  const size_t element_size = sizeof(TypeParam);
  const TypeParam src[1] = {};
  TypeParam dst[16];
  float mapx[16] = {};
  float mapy[16] = {};

  EXPECT_EQ(
      KLEIDICV_ERROR_NOT_IMPLEMENTED,
      kleidicv_remap_f32_u8(src, element_size, 1, 1, dst, 16 * element_size, 16,
                            1, 1, mapx, 16 * sizeof(float), mapy,
                            16 * sizeof(float), KLEIDICV_INTERPOLATION_LINEAR,
                            KLEIDICV_BORDER_TYPE_CONSTANT, nullptr));
}

TYPED_TEST(RemapF32, UnsupportedTooSmallImage) {
  const size_t element_size = sizeof(TypeParam);
  const TypeParam src[1] = {};
  TypeParam dst[16];
  float mapx[16] = {};
  float mapy[16] = {};

  EXPECT_EQ(
      KLEIDICV_ERROR_NOT_IMPLEMENTED,
      kleidicv_remap_f32_u8(src, element_size, 1, 1, dst, 16 * element_size, 3,
                            1, 1, mapx, 16 * sizeof(float), mapy,
                            16 * sizeof(float), KLEIDICV_INTERPOLATION_LINEAR,
                            KLEIDICV_BORDER_TYPE_REPLICATE, nullptr));
}

TYPED_TEST(RemapF32, UnsupportedBigStride) {
  const size_t element_size = sizeof(TypeParam);
  const TypeParam src[1] = {};
  TypeParam dst[16];
  float mapx[16] = {};
  float mapy[16] = {};
  const uint64_t src_stride =
      static_cast<uint64_t>(std::numeric_limits<uint32_t>::max()) + 1;

  EXPECT_EQ(
      KLEIDICV_ERROR_NOT_IMPLEMENTED,
      kleidicv_remap_f32_u8(src, src_stride, 1, 1, dst, 16 * element_size, 16,
                            1, 1, mapx, 16 * sizeof(float), mapy,
                            16 * sizeof(float), KLEIDICV_INTERPOLATION_LINEAR,
                            KLEIDICV_BORDER_TYPE_REPLICATE, nullptr));
}

TYPED_TEST(RemapF32, UnsupportedBigSourceWidth) {
  const size_t element_size = sizeof(TypeParam);
  const TypeParam src[1] = {};
  TypeParam dst[16];
  float mapx[16] = {};
  float mapy[16] = {};

  EXPECT_EQ(KLEIDICV_ERROR_RANGE,
            kleidicv_remap_f32_u8(src, element_size, 1ULL << 24, 1, dst,
                                  16 * element_size, 16, 1, 1, mapx,
                                  16 * sizeof(float), mapy, 16 * sizeof(float),
                                  KLEIDICV_INTERPOLATION_LINEAR,
                                  KLEIDICV_BORDER_TYPE_REPLICATE, nullptr));
}

TYPED_TEST(RemapF32, UnsupportedBigSourceHeight) {
  const size_t element_size = sizeof(TypeParam);
  const TypeParam src[1] = {};
  TypeParam dst[16];
  float mapx[16] = {};
  float mapy[16] = {};

  EXPECT_EQ(KLEIDICV_ERROR_RANGE,
            kleidicv_remap_f32_u8(src, element_size, 1, 1ULL << 24, dst,
                                  16 * element_size, 16, 1, 1, mapx,
                                  16 * sizeof(float), mapy, 16 * sizeof(float),
                                  KLEIDICV_INTERPOLATION_LINEAR,
                                  KLEIDICV_BORDER_TYPE_REPLICATE, nullptr));
}

TYPED_TEST(RemapF32, UnsupportedBigDestinationWidth) {
  const size_t element_size = sizeof(TypeParam);
  const TypeParam src[1] = {};
  TypeParam dst[16];
  float mapx[16] = {};
  float mapy[16] = {};

  EXPECT_EQ(
      KLEIDICV_ERROR_RANGE,
      kleidicv_remap_f32_u8(src, element_size, 1, 1, dst, 16 * element_size,
                            1ULL << 24, 1, 1, mapx, 16 * sizeof(float), mapy,
                            16 * sizeof(float), KLEIDICV_INTERPOLATION_LINEAR,
                            KLEIDICV_BORDER_TYPE_REPLICATE, nullptr));
}

TYPED_TEST(RemapF32, UnsupportedBigDestinationHeight) {
  const size_t element_size = sizeof(TypeParam);
  const TypeParam src[1] = {};
  TypeParam dst[16];
  float mapx[16] = {};
  float mapy[16] = {};

  EXPECT_EQ(
      KLEIDICV_ERROR_RANGE,
      kleidicv_remap_f32_u8(src, element_size, 1, 1, dst, 16 * element_size, 16,
                            1ULL << 24, 1, mapx, 16 * sizeof(float), mapy,
                            16 * sizeof(float), KLEIDICV_INTERPOLATION_LINEAR,
                            KLEIDICV_BORDER_TYPE_REPLICATE, nullptr));
}

TYPED_TEST(RemapF32, Misalignment) {
  const size_t element_size = sizeof(TypeParam);
  if (element_size == 1) {
    // misalignment impossible
    GTEST_SKIP();
  }

  // Will be needed when supporting uint16_t
}
