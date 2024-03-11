// SPDX-FileCopyrightText: 2024 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#include <gtest/gtest.h>

#include <cmath>
#include <vector>

#include "framework/array.h"
#include "framework/generator.h"
#include "framework/utils.h"
#include "intrinsiccv/intrinsiccv.h"
#include "intrinsiccv/traits.h"

template <typename Type>
struct CannyTestParams;

template <>
struct CannyTestParams<uint8_t> {
  using ElementType = uint8_t;
  using IntermediateType = int16_t;
};

template <class TestParams>
class CannyTestSuite {
 public:
  void test() {
    low_threshold_ = 0x200;
    high_threshold_ = 0x380;
    constexpr size_t width = 200;
    constexpr size_t height = 200;

    test::Array2D<ElementType> source(width, height);
    test::Array2D<ElementType> actual(width, height);
    test::Array2D<ElementType> expected(width, height);

    test::PseudoRandomNumberGenerator<ElementType> generator;
    source.fill(generator);

    // Make sure there is some edge in generated source.
    source.set(0, 0, {200, 200, 0, 0, 0});
    source.set(1, 0, {200, 200, 0, 0, 0});
    source.set(2, 0, {200, 0, 0, 200, 200});
    source.set(3, 0, {0, 0, 0, 200, 200});
    source.set(4, 0, {0, 0, 0, 200, 200});

    calculate_expected(source, expected);

    auto err = intrinsiccv_canny_u8(source.data(), source.stride(),
                                    actual.data(), actual.stride(), width,
                                    height, low_threshold_, high_threshold_);
    ASSERT_EQ(INTRINSICCV_OK, err);

    EXPECT_EQ_ARRAY2D(actual, expected);
  }

 private:
  using ElementType = typename TestParams::ElementType;
  using IntermediateType = typename TestParams::IntermediateType;

  struct Coords {
    size_t row;
    size_t column;
  };

  void calculate_expected(test::Array2D<ElementType> &src,
                          test::Array2D<ElementType> &dst) {
    // Allocate temp buffers for gradients
    size_t width = dst.width();
    size_t height = dst.height();
    test::Array2D<IntermediateType> horizontal_gradient(width, height);
    test::Array2D<IntermediateType> vertical_gradient(width, height);
    test::Array2D<IntermediateType> magnitudes(width, height);

    // Calculate horizontal gradient
    auto err = intrinsiccv_sobel_3x3_horizontal_s16_u8(
        src.data(), src.stride(), horizontal_gradient.data(),
        horizontal_gradient.stride(), width, height, 1);
    ASSERT_EQ(INTRINSICCV_OK, err);

    // Calculate vertical gradient
    err = intrinsiccv_sobel_3x3_vertical_s16_u8(
        src.data(), src.stride(), vertical_gradient.data(),
        vertical_gradient.stride(), width, height, 1);
    ASSERT_EQ(INTRINSICCV_OK, err);

    // Calculate magnitude from the horizontal and vertical derivatives, and
    // apply lower threshold.
    err = intrinsiccv_saturating_add_abs_with_threshold_s16(
        horizontal_gradient.data(), horizontal_gradient.stride(),
        vertical_gradient.data(), vertical_gradient.stride(), magnitudes.data(),
        magnitudes.stride(), width, height, low_threshold_);
    ASSERT_EQ(INTRINSICCV_OK, err);

    // Edge thining
    non_maxima_suppression(horizontal_gradient, vertical_gradient, magnitudes);

    // Promote weak pixels
    perform_hysteresis_supress_weak_edges(magnitudes, dst);
  }

  void promote_weak(test::Array2D<IntermediateType> &source,
                    std::vector<Coords> &strongPixels) {
    // Walk through all strong pixels including added in this function
    while (!strongPixels.empty()) {
      Coords current = strongPixels.back();
      strongPixels.pop_back();

      // Promote weak pixels in 3x3 window around current
      for (int8_t y = -1; y < 2; ++y) {
        for (int8_t x = -1; x < 2; ++x) {
          if (current.column + x >= 0 &&
              current.column + x < static_cast<__int128_t>(source.width()) &&
              current.row + y >= 0 &&
              current.row + y < static_cast<__int128_t>(source.height())) {
            if (x == y && y == 0) {
              continue;
            }
            if (source.at(current.row + y, current.column + x)[0] > 0 &&
                source.at(current.row + y, current.column + x)[0] <=
                    high_threshold_) {
              size_t strong_row = current.row + y;
              size_t strong_col = current.column + x;
              source.at(strong_row, strong_col)[0] = high_threshold_ + 1;
              strongPixels.push_back({strong_row, strong_col});
            }
          }
        }
      }
    }
  }

  void perform_hysteresis_supress_weak_edges(
      test::Array2D<IntermediateType> &source,
      test::Array2D<ElementType> &dst) {
    std::vector<Coords> strongPixels;

    // Find all strong pixels
    for (size_t row = 0; row < source.height(); ++row) {
      for (size_t column = 0; column < source.width(); ++column) {
        if (source.at(row, column)[0] > high_threshold_) {
          strongPixels.push_back({row, column});
        }
      }
    }

    // Promote weak edges
    promote_weak(source, strongPixels);

    // Fill destination
    for (size_t row = 0; row < dst.height(); ++row) {
      for (size_t column = 0; column < dst.width(); ++column) {
        if (source.at(row, column)[0] > high_threshold_) {
          dst.at(row, column)[0] = 0xff;
        }
      }
    }
  }

  void non_maxima_suppression(test::Array2D<IntermediateType> &Gx,
                              test::Array2D<IntermediateType> &Gy,
                              test::Array2D<IntermediateType> &magnitudes) {
    constexpr double c_pi = 3.14159265358979323846;
    test::Array2D<IntermediateType> temp = magnitudes;
    size_t row = 0;
    size_t column = 0;

    auto drop_weak_mag = [&](IntermediateType prev, IntermediateType next) {
      if (temp.at(row, column)[0] <= prev || temp.at(row, column)[0] < next) {
        magnitudes.at(row, column)[0] = 0;
      }
    };

    for (row = 0; row < magnitudes.height(); ++row) {
      for (column = 0; column < magnitudes.width(); ++column) {
        if (temp.at(row, column)[0] == 0) {
          continue;
        }

        // Calculate gradient direction and confine it to [0,c_pi]
        double angle = std::atan2(Gy.at(row, column)[0], Gx.at(row, column)[0]);
        if (angle < 0) {
          angle += c_pi;
        }

        if (angle > 7 * c_pi / 8 || angle < c_pi / 8) {
          // right and left
          auto left_mag = column == 0 ? 0 : temp.at(row, column - 1)[0];
          auto right_mag =
              column == temp.width() - 1 ? 0 : temp.at(row, column + 1)[0];
          drop_weak_mag(left_mag, right_mag);
        } else if (angle > 3 * c_pi / 8 && angle < 5 * c_pi / 8) {
          // top and bottom
          auto top_mag = row == 0 ? 0 : temp.at(row - 1, column)[0];
          auto bottom_mag =
              row == temp.height() - 1 ? 0 : temp.at(row + 1, column)[0];
          drop_weak_mag(top_mag, bottom_mag);
#if INTRINSICCV_CANNY_ALGORITHM_CONFORM_OPENCV
          // top-right and  bottom-left
          // diagonal directions are swapped and `prev >= current < next`
          // becomes `prev >= current <= next`
        } else if (angle > 5 * c_pi / 8 && angle < 7 * c_pi / 8) {
          auto top_right_mag = 1;
          auto bottom_left_mag = 0;
#else
        } else if (angle > c_pi / 8 && angle < 3 * c_pi / 8) {
          auto top_right_mag = 0;
          auto bottom_left_mag = 0;
#endif
          top_right_mag += (column == temp.width() - 1 || row == 0)
                               ? 0
                               : temp.at(row - 1, column + 1)[0];
          bottom_left_mag += (column == 0 || row == temp.height() - 1)
                                 ? 0
                                 : temp.at(row + 1, column - 1)[0];
          drop_weak_mag(bottom_left_mag, top_right_mag);
        } else {
          // top-left and bottom-right
#if INTRINSICCV_CANNY_ALGORITHM_CONFORM_OPENCV
          auto top_left_mag = 0;
          auto bottom_right_mag = 1;
#else
          auto top_left_mag = 0;
          auto bottom_right_mag = 0;
#endif
          top_left_mag +=
              (column == 0 || row == 0) ? 0 : temp.at(row - 1, column - 1)[0];
          bottom_right_mag +=
              (column == temp.width() - 1 || row == temp.height() - 1)
                  ? 0
                  : temp.at(row + 1, column + 1)[0];
          drop_weak_mag(top_left_mag, bottom_right_mag);
        }
      }
    }
  }

  int16_t low_threshold_, high_threshold_;
};  // end of class CannyTestSuite<TestParams>

#define INTRINSICCV_CANNY(type, suffix) \
  INTRINSICCV_API(canny, intrinsiccv_canny_##suffix, type)

INTRINSICCV_CANNY(uint8_t, u8);

using ElementTypes = ::testing::Types<uint8_t>;

template <typename ElementType>
class CannyTest : public testing::Test {};

TYPED_TEST_SUITE(CannyTest, ElementTypes);

TYPED_TEST(CannyTest, NullPointer) {
  TypeParam src[1], dst[1];
  test::test_null_args(canny<TypeParam>(), src, sizeof(TypeParam), dst,
                       sizeof(TypeParam), 1, 1, 0.0, 1.0);
}

TYPED_TEST(CannyTest, Values) {
  using KernelTestParams = CannyTestParams<TypeParam>;
  CannyTestSuite<KernelTestParams> test{};
  test.test();
}

TYPED_TEST(CannyTest, Misalignment) {
  if (sizeof(TypeParam) == 1) {
    // misalignment impossible
    return;
  }
  TypeParam src[1], dst[1];
  EXPECT_EQ(INTRINSICCV_ERROR_ALIGNMENT,
            canny<TypeParam>()(src, sizeof(TypeParam) + 1, dst,
                               sizeof(TypeParam), 1, 1, 0.0, 1.0));
  EXPECT_EQ(INTRINSICCV_ERROR_ALIGNMENT,
            canny<TypeParam>()(src, sizeof(TypeParam), dst,
                               sizeof(TypeParam) + 1, 1, 1, 0.0, 1.0));
}

TYPED_TEST(CannyTest, ZeroImageSize) {
  TypeParam src[1] = {}, dst[1];
  EXPECT_EQ(INTRINSICCV_ERROR_NOT_IMPLEMENTED,
            canny<TypeParam>()(src, sizeof(TypeParam), dst, sizeof(TypeParam),
                               0, 1, 0.0, 1.0));
  EXPECT_EQ(INTRINSICCV_ERROR_NOT_IMPLEMENTED,
            canny<TypeParam>()(src, sizeof(TypeParam), dst, sizeof(TypeParam),
                               1, 0, 0.0, 1.0));
}

TYPED_TEST(CannyTest, OversizeImage) {
  TypeParam src[1] = {}, dst[1];
  EXPECT_EQ(INTRINSICCV_ERROR_RANGE,
            canny<TypeParam>()(src, sizeof(TypeParam), dst, sizeof(TypeParam),
                               INTRINSICCV_MAX_IMAGE_PIXELS + 1, 1, 0.0, 1.0));
  EXPECT_EQ(INTRINSICCV_ERROR_RANGE,
            canny<TypeParam>()(src, sizeof(TypeParam), dst, sizeof(TypeParam),
                               INTRINSICCV_MAX_IMAGE_PIXELS,
                               INTRINSICCV_MAX_IMAGE_PIXELS, 0.0, 1.0));
}

TYPED_TEST(CannyTest, CannotAllocateBuffers) {
  MockMallocToFail::enable();
  TypeParam src[5] = {}, dst[5] = {};
  EXPECT_EQ(INTRINSICCV_ERROR_ALLOCATION,
            canny<TypeParam>()(src, sizeof(TypeParam), dst, sizeof(TypeParam),
                               5, 5, 0.0, 1.0));
  MockMallocToFail::disable();
}
