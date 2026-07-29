// SPDX-FileCopyrightText: 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#include <gtest/gtest.h>

#include "framework/array.h"
#include "framework/generator.h"
#include "framework/utils.h"
#include "kleidicv/ctypes.h"
#include "kleidicv/kleidicv.h"

// Using TestWithParam to run tests with different inputs
class Flip : public testing::TestWithParam<size_t> {
 public:
  size_t scalars_in_full_vec() const {
    switch (GetParam()) {
      case sizeof(uint8_t):
      case sizeof(uint8_t) * 3:
        return test::Options::vector_lanes<uint8_t>();
      case sizeof(uint16_t):
      case sizeof(uint16_t) * 3:
        return test::Options::vector_lanes<uint16_t>();
      case sizeof(uint32_t):
        return test::Options::vector_lanes<uint32_t>();
      case sizeof(uint64_t):
        return test::Options::vector_lanes<uint64_t>();
      default:
        ADD_FAILURE() << "Unsupported pixel size for flip test.";
        return 1;
    }
  }

  // Width 1 is especially important as it's a no-op
  void minimum_widths_test(size_t padding) {
    constexpr size_t height = 3;
    test_horizontal(1, height, padding);
    test_horizontal(2, height, padding);
  }

  void around_vector_length_test(size_t padding) {
    constexpr size_t height = 3;
    const size_t vector_lanes = scalars_in_full_vec();
    test_horizontal(vector_lanes - 1, height, padding);
    test_horizontal(vector_lanes, height, padding);
    test_horizontal(vector_lanes + 1, height, padding);
  }

  void around_two_vector_boundary_test(size_t padding) {
    constexpr size_t height = 3;
    const size_t boundary = 2 * scalars_in_full_vec();
    test_horizontal(boundary - 1, height, padding);
    test_horizontal(boundary, height, padding);
    test_horizontal(boundary + 1, height, padding);
  }

  void around_four_vector_boundary_test(size_t padding) {
    constexpr size_t height = 3;
    const size_t boundary = 4 * scalars_in_full_vec();
    test_horizontal(boundary - 1, height, padding);
    test_horizontal(boundary, height, padding);
    test_horizontal(boundary + 1, height, padding);
  }

 protected:
  void test_horizontal(size_t width, size_t height, size_t padding) {
    test(width, height, padding, false);
    test(width, height, padding, true);
  }

  // Calculates byte-by-byte the expected result
  void calculate_expected(const uint8_t *src, size_t src_stride, size_t width,
                          size_t height, uint8_t *dst, size_t dst_stride,
                          int flip_mode, size_t pixel_size) const {
    if (flip_mode <= 0) {
      FAIL() << "Only horizontal flip is supported at this time.";
    }

    for (size_t i = 0; i < height; i++) {
      for (size_t j = 0; j < width; j++) {
        // src[i][j] goes to dst[i][width - j - 1]
        memcpy(dst + i * dst_stride + (width - j - 1) * pixel_size,
               src + i * src_stride + j * pixel_size, pixel_size);
      }
    }
  }

  // Actual test implementation
  template <typename ScalarType, size_t kScalarPerPixel>
  void test_impl(size_t width, size_t height, size_t padding,
                 bool in_place) const {
    const size_t width_scalars = width * kScalarPerPixel;
    const size_t padding_scalars = padding * kScalarPerPixel;
    const size_t pixel_size = sizeof(ScalarType) * kScalarPerPixel;

    // Generate a random source image
    test::Array2D<ScalarType> source(width_scalars, height, padding_scalars,
                                     kScalarPerPixel);
    test::PseudoRandomNumberGenerator<ScalarType> input_value_random_range;
    source.fill(input_value_random_range);

    // Just test horizontal flip for now
    const int flip_mode = 1;

    test::Array2D<ScalarType> expected(width_scalars, height, padding_scalars,
                                       kScalarPerPixel);
    calculate_expected(reinterpret_cast<const uint8_t *>(source.data()),
                       source.stride(), width, height,
                       reinterpret_cast<uint8_t *>(expected.data()),
                       expected.stride(), flip_mode, pixel_size);

    test::Array2D<ScalarType> actual;
    if (!in_place) {
      actual = test::Array2D<ScalarType>(width_scalars, height, padding_scalars,
                                         kScalarPerPixel);
      ASSERT_EQ(
          KLEIDICV_OK,
          kleidicv_flip(source.data(), source.stride(), width, height,
                        actual.data(), actual.stride(), flip_mode, pixel_size));
    } else {
      actual = source;  // Deep copy
      ASSERT_EQ(
          KLEIDICV_OK,
          kleidicv_flip(actual.data(), actual.stride(), width, height,
                        actual.data(), actual.stride(), flip_mode, pixel_size));
    }

    // Dump source, expected, actual upon mismatch if image is small
    if (width <= 100 && height <= 100 && expected.compare_to(actual)) {
      std::cout << "source:\n";
      dump(&source);
      std::cout << "expected:\n";
      dump(&expected);
      std::cout << "actual:\n";
      dump(&actual);
    }

    // Proper gtest check of whether the images match
    EXPECT_EQ_ARRAY2D(expected, actual);
  }

  void test(size_t width, size_t height, size_t padding, bool in_place) const {
    switch (GetParam()) {
      case sizeof(uint8_t):
        test_impl<uint8_t, 1>(width, height, padding, in_place);
        break;
      case sizeof(uint16_t):
        test_impl<uint16_t, 1>(width, height, padding, in_place);
        break;
      case sizeof(uint32_t):
        test_impl<uint32_t, 1>(width, height, padding, in_place);
        break;
      case sizeof(uint64_t):
        test_impl<uint64_t, 1>(width, height, padding, in_place);
        break;
      case sizeof(uint8_t) * 3:
        test_impl<uint8_t, 3>(width, height, padding, in_place);
        break;
      case sizeof(uint16_t) * 3:
        test_impl<uint16_t, 3>(width, height, padding, in_place);
        break;
      default:
        FAIL() << "Unsupported pixel size for flip test.";
    }
  }
};

TEST_P(Flip, MinimumWidthsNoPadding) { minimum_widths_test(0); }

TEST_P(Flip, MinimumWidthsWithPadding) { minimum_widths_test(1); }

TEST_P(Flip, AroundVectorLengthNoPadding) { around_vector_length_test(0); }

TEST_P(Flip, AroundVectorLengthWithPadding) { around_vector_length_test(1); }

TEST_P(Flip, AroundTwoVectorBoundaryNoPadding) {
  around_two_vector_boundary_test(0);
}

TEST_P(Flip, AroundTwoVectorBoundaryWithPadding) {
  around_two_vector_boundary_test(1);
}

TEST_P(Flip, AroundFourVectorBoundaryNoPadding) {
  around_four_vector_boundary_test(0);
}

TEST_P(Flip, AroundFourVectorBoundaryWithPadding) {
  around_four_vector_boundary_test(1);
}

TEST_P(Flip, VerticalNotImplemented) {
  std::vector<uint8_t> src(1, 0);
  std::vector<uint8_t> dst(1, 0);
  size_t pixel_size = GetParam();

  EXPECT_EQ(KLEIDICV_ERROR_NOT_IMPLEMENTED,
            kleidicv_flip(src.data(), pixel_size, 1, 1, dst.data(), pixel_size,
                          0, pixel_size));
}

TEST_P(Flip, BothAxesNotImplemented) {
  std::vector<uint8_t> src(1, 0);
  std::vector<uint8_t> dst(1, 0);
  size_t pixel_size = GetParam();

  EXPECT_EQ(KLEIDICV_ERROR_NOT_IMPLEMENTED,
            kleidicv_flip(src.data(), pixel_size, 1, 1, dst.data(), pixel_size,
                          -1, pixel_size));
}

TEST_P(Flip, NullPointer) {
  std::vector<uint8_t> src(1, 0);
  std::vector<uint8_t> dst(1, 0);
  size_t pixel_size = GetParam();

  // Just testing horizontal flip for now
  const int flip_mode = 1;

  test::test_null_args(kleidicv_flip, src.data(), pixel_size, 1, 1, dst.data(),
                       pixel_size, flip_mode, pixel_size);
}

TEST_P(Flip, ZeroImageSize) {
  std::vector<uint8_t> src(1, 0);
  std::vector<uint8_t> dst(1, 0);
  size_t pixel_size = GetParam();

  // Just testing horizontal flip for now
  const int flip_mode = 1;

  EXPECT_EQ(KLEIDICV_OK, kleidicv_flip(src.data(), pixel_size, 0, 1, dst.data(),
                                       pixel_size, flip_mode, pixel_size));
  EXPECT_EQ(KLEIDICV_OK, kleidicv_flip(src.data(), pixel_size, 1, 0, dst.data(),
                                       pixel_size, flip_mode, pixel_size));
}

TEST_P(Flip, OversizeImage) {
  std::vector<uint8_t> src(1, 0);
  std::vector<uint8_t> dst(1, 0);
  size_t pixel_size = GetParam();

  // Just testing horizontal flip for now
  const int flip_mode = 1;

  EXPECT_EQ(
      KLEIDICV_ERROR_RANGE,
      kleidicv_flip(src.data(), pixel_size, 1, KLEIDICV_MAX_IMAGE_PIXELS + 1,
                    dst.data(), pixel_size, flip_mode, pixel_size));
  EXPECT_EQ(KLEIDICV_ERROR_RANGE,
            kleidicv_flip(src.data(), pixel_size, KLEIDICV_MAX_IMAGE_PIXELS + 1,
                          1, dst.data(), pixel_size, flip_mode, pixel_size));
}

TEST_P(Flip, Misalignment) {
  size_t pixel_size = GetParam();
  if (pixel_size == 1 || pixel_size == 3) {
    // Not possible to be misaligned
    return;
  }

  const size_t buffer_size = pixel_size * 10;
  std::vector<uint8_t> src(buffer_size, 0);
  std::vector<uint8_t> dst(buffer_size, 0);

  // Just testing horizontal flip for now
  const int flip_mode = 1;

  // Misaligned source pointer
  EXPECT_EQ(KLEIDICV_ERROR_ALIGNMENT,
            kleidicv_flip(src.data() + 1, pixel_size, 1, 1, dst.data(),
                          pixel_size, flip_mode, pixel_size));

  // Misaligned source stride
  EXPECT_EQ(KLEIDICV_ERROR_ALIGNMENT,
            kleidicv_flip(src.data(), pixel_size + 1, 1, 2, dst.data(),
                          pixel_size, flip_mode, pixel_size));

  // Misaligned dest pointer
  EXPECT_EQ(KLEIDICV_ERROR_ALIGNMENT,
            kleidicv_flip(src.data(), pixel_size, 1, 1, dst.data() + 1,
                          pixel_size, flip_mode, pixel_size));

  // Misaligned dest stride
  EXPECT_EQ(KLEIDICV_ERROR_ALIGNMENT,
            kleidicv_flip(src.data(), pixel_size, 1, 2, dst.data(),
                          pixel_size + 1, flip_mode, pixel_size));

  // Ignore stride if there's only one row
  EXPECT_EQ(KLEIDICV_OK,
            kleidicv_flip(src.data(), pixel_size + 1, 1, 1, dst.data(),
                          pixel_size, flip_mode, pixel_size));
  EXPECT_EQ(KLEIDICV_OK, kleidicv_flip(src.data(), pixel_size, 1, 1, dst.data(),
                                       pixel_size + 1, flip_mode, pixel_size));
}

INSTANTIATE_TEST_SUITE_P(, Flip, testing::Values(1, 2, 3, 4, 6, 8),
                         testing::PrintToStringParamName());

// Test a pixel size that isn't implemented (16 bytes per pixel)
TEST(FlipNotImplemented, ElementSize) {
  const size_t width = 1;
  const size_t height = 1;
  const size_t pixel_size = 16;
  const size_t stride = width * pixel_size;
  const int flip_mode = 1;

  std::vector<uint8_t> src(width * pixel_size * height, 0);
  std::vector<uint8_t> dst(width * pixel_size * height, 0);

  ASSERT_EQ(KLEIDICV_ERROR_NOT_IMPLEMENTED,
            kleidicv_flip(src.data(), stride, width, height, dst.data(), stride,
                          flip_mode, pixel_size));
}
