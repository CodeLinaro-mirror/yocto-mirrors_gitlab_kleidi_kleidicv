// SPDX-FileCopyrightText: 2024 - 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#include <gtest/gtest.h>

#include <vector>

#include "framework/array.h"
#include "framework/generator.h"
#include "kleidicv/kleidicv.h"

class Transpose : public testing::TestWithParam<size_t> {
 public:
  void scalar_test(size_t padding) {
    const size_t lanes = lanes_for_element_size();
    size_t first_dim = lanes - 1;
    size_t second_dim = lanes + 1;
    // Exercise scalar paths
    test_out_of_place(first_dim, second_dim, padding);
    test_out_of_place(second_dim, first_dim, padding);
    test_in_place(first_dim, padding);
  }

  void vector_test(size_t padding) {
    const size_t lanes = lanes_for_element_size();
    // Make at least two full vector passes
    size_t src_width = 2 * lanes;
    size_t src_height = 3 * lanes;
    test_out_of_place(src_width, src_height, padding);
    test_in_place(src_width, padding);
  }

  void vector_plus_scalar_test(size_t padding) {
    const size_t lanes = lanes_for_element_size();
    size_t first_dim = 3 * lanes - 1;
    size_t second_dim = 3 * lanes + 1;
    test_out_of_place(first_dim, second_dim, padding);
    test_out_of_place(second_dim, first_dim, padding);
    test_in_place(first_dim, padding);
  }

 protected:
  size_t lanes_for_element_size() const {
    const size_t element_size = GetParam();
    switch (element_size) {
      case sizeof(uint8_t):
        return test::Options::vector_lanes<uint8_t>();
      case sizeof(uint16_t):
        return test::Options::vector_lanes<uint16_t>();
      case sizeof(uint32_t):
        return test::Options::vector_lanes<uint32_t>();
      case sizeof(uint64_t):
        return test::Options::vector_lanes<uint64_t>();
      case sizeof(uint8_t) * 3:
        return test::Options::vector_lanes<uint8_t>();
      case sizeof(uint16_t) * 3:
        return test::Options::vector_lanes<uint16_t>();
      default:
        return test::Options::vector_lanes<uint8_t>();
    }
  }

  template <typename ScalarType, size_t kChannels>
  void test_out_of_place_impl(size_t src_width, size_t src_height,
                              size_t padding) const {
    const size_t dst_width = src_height;
    const size_t dst_height = src_width;
    const size_t element_size = sizeof(ScalarType) * kChannels;
    const size_t src_width_elements = src_width * kChannels;
    const size_t dst_width_elements = dst_width * kChannels;
    const size_t padding_elements = padding * kChannels;

    test::Array2D<ScalarType> source(src_width_elements, src_height,
                                     padding_elements, kChannels);
    test::Array2D<ScalarType> expected(dst_width_elements, dst_height,
                                       padding_elements, kChannels);
    test::Array2D<ScalarType> actual(dst_width_elements, dst_height,
                                     padding_elements, kChannels);

    test::PseudoRandomNumberGenerator<ScalarType> input_value_random_range;
    source.fill(input_value_random_range);

    calculate_expected(reinterpret_cast<const uint8_t *>(source.data()),
                       reinterpret_cast<uint8_t *>(expected.data()), src_width,
                       src_height, source.stride(), expected.stride(),
                       element_size);

    ASSERT_EQ(KLEIDICV_OK,
              kleidicv_transpose(source.data(), source.stride(), actual.data(),
                                 actual.stride(), src_width, src_height,
                                 element_size));

    EXPECT_EQ_ARRAY2D(actual, expected);
  }

  template <typename ScalarType, size_t kChannels>
  void test_in_place_impl(size_t size, size_t padding) const {
    const size_t element_size = sizeof(ScalarType) * kChannels;
    const size_t width_elements = size * kChannels;
    const size_t padding_elements = padding * kChannels;

    test::Array2D<ScalarType> source(width_elements, size, padding_elements,
                                     kChannels);
    test::Array2D<ScalarType> expected(width_elements, size, padding_elements,
                                       kChannels);
    test::Array2D<ScalarType> actual;

    test::PseudoRandomNumberGenerator<ScalarType> input_value_random_range;
    source.fill(input_value_random_range);

    calculate_expected(reinterpret_cast<const uint8_t *>(source.data()),
                       reinterpret_cast<uint8_t *>(expected.data()), size, size,
                       source.stride(), source.stride(), element_size);
    actual = source;

    ASSERT_EQ(KLEIDICV_OK,
              kleidicv_transpose(actual.data(), actual.stride(), actual.data(),
                                 actual.stride(), size, size, element_size));

    EXPECT_EQ_ARRAY2D(actual, expected);
  }

  void test_out_of_place(size_t src_width, size_t src_height,
                         size_t padding) const {
    switch (GetParam()) {
      case sizeof(uint8_t):
        test_out_of_place_impl<uint8_t, 1>(src_width, src_height, padding);
        break;
      case sizeof(uint16_t):
        test_out_of_place_impl<uint16_t, 1>(src_width, src_height, padding);
        break;
      case sizeof(uint32_t):
        test_out_of_place_impl<uint32_t, 1>(src_width, src_height, padding);
        break;
      case sizeof(uint64_t):
        test_out_of_place_impl<uint64_t, 1>(src_width, src_height, padding);
        break;
      case sizeof(uint8_t) * 3:
        test_out_of_place_impl<uint8_t, 3>(src_width, src_height, padding);
        break;
      case sizeof(uint16_t) * 3:
        test_out_of_place_impl<uint16_t, 3>(src_width, src_height, padding);
        break;
      default:
        FAIL() << "Unsupported element size for transpose test.";
    }
  }

  void test_in_place(size_t size, size_t padding) const {
    switch (GetParam()) {
      case sizeof(uint8_t):
        test_in_place_impl<uint8_t, 1>(size, padding);
        break;
      case sizeof(uint16_t):
        test_in_place_impl<uint16_t, 1>(size, padding);
        break;
      case sizeof(uint32_t):
        test_in_place_impl<uint32_t, 1>(size, padding);
        break;
      case sizeof(uint64_t):
        test_in_place_impl<uint64_t, 1>(size, padding);
        break;
      case sizeof(uint8_t) * 3:
        test_in_place_impl<uint8_t, 3>(size, padding);
        break;
      case sizeof(uint16_t) * 3:
        test_in_place_impl<uint16_t, 3>(size, padding);
        break;
      default:
        FAIL() << "Unsupported element size for transpose test.";
    }
  }

  void calculate_expected(const uint8_t *source, uint8_t *expected,
                          size_t src_width, size_t src_height,
                          size_t src_stride, size_t dst_stride,
                          size_t element_size) const {
    for (size_t i = 0; i < src_height; i++) {
      for (size_t j = 0; j < src_width; j++) {
        std::memcpy(expected + j * dst_stride + i * element_size,
                    source + i * src_stride + j * element_size, element_size);
      }
    }
  }
};

TEST_P(Transpose, ScalarNoPadding) { scalar_test(0); }

TEST_P(Transpose, VectorNoPadding) { vector_test(0); }

TEST_P(Transpose, ScalarWithPadding) { scalar_test(1); }

TEST_P(Transpose, VectorWithPadding) { vector_test(1); }

TEST_P(Transpose, VectorPlusScalarNoPadding) { vector_plus_scalar_test(0); }

TEST_P(Transpose, VectorPlusScalarWithPadding) { vector_plus_scalar_test(1); }

TEST_P(Transpose, NullPointer) {
  std::vector<uint8_t> src(1, 0);
  std::vector<uint8_t> dst(1, 0);
  size_t element_size = GetParam();
  test::test_null_args(kleidicv_transpose, src.data(), element_size, dst.data(),
                       element_size, 1, 1, element_size);
}

TEST_P(Transpose, NotImplementedDims) {
  std::vector<uint8_t> src(2, 0);
  size_t element_size = GetParam();
  size_t stride = element_size;
  ASSERT_EQ(KLEIDICV_ERROR_NOT_IMPLEMENTED,
            kleidicv_transpose(src.data(), stride, src.data(), stride, 1, 2,
                               element_size));
}

TEST(TransposeNotImplemented, ElementSize) {
  const size_t width = 1;
  const size_t height = 1;
  const size_t element_size = 16;
  const size_t stride = width * element_size;

  std::vector<uint8_t> source(width * element_size * height, 0);
  std::vector<uint8_t> dst(width * element_size * height, 0);
  ASSERT_EQ(KLEIDICV_ERROR_NOT_IMPLEMENTED,
            kleidicv_transpose(source.data(), stride, dst.data(), stride, width,
                               height, element_size));
}

TEST_P(Transpose, Misalignment) {
  size_t element_size = GetParam();
  if (element_size == 1 || element_size == 3) {
    // misalignment impossible
    return;
  }

  const size_t kBufSize = element_size * 10;
  std::vector<uint8_t> src(kBufSize, 0);
  std::vector<uint8_t> dst(kBufSize, 0);

  EXPECT_EQ(KLEIDICV_ERROR_ALIGNMENT,
            kleidicv_transpose(src.data() + 1, element_size, dst.data(),
                               element_size, 1, 1, element_size));
  EXPECT_EQ(KLEIDICV_ERROR_ALIGNMENT,
            kleidicv_transpose(src.data(), element_size + 1, dst.data(),
                               element_size, 1, 2, element_size));
  EXPECT_EQ(KLEIDICV_ERROR_ALIGNMENT,
            kleidicv_transpose(src.data(), element_size, dst.data() + 1,
                               element_size, 1, 1, element_size));
  EXPECT_EQ(KLEIDICV_ERROR_ALIGNMENT,
            kleidicv_transpose(src.data(), element_size, dst.data(),
                               element_size + 1, 2, 1, element_size));
  // Ignore stride if there's only one row
  EXPECT_EQ(KLEIDICV_OK,
            kleidicv_transpose(src.data(), element_size, dst.data(),
                               element_size + 1, 1, 1, element_size));
}

TEST_P(Transpose, ZeroImageSize) {
  std::vector<uint8_t> src(1, 0);
  std::vector<uint8_t> dst(1, 0);
  size_t element_size = GetParam();
  EXPECT_EQ(KLEIDICV_OK,
            kleidicv_transpose(src.data(), element_size, dst.data(),
                               element_size, 0, 1, element_size));
  EXPECT_EQ(KLEIDICV_OK,
            kleidicv_transpose(src.data(), element_size, dst.data(),
                               element_size, 1, 0, element_size));
}

TEST_P(Transpose, OversizeImage) {
  std::vector<uint8_t> src(1, 0);
  std::vector<uint8_t> dst(1, 0);
  size_t element_size = GetParam();
  EXPECT_EQ(
      KLEIDICV_ERROR_RANGE,
      kleidicv_transpose(src.data(), element_size, dst.data(), element_size,
                         KLEIDICV_MAX_IMAGE_PIXELS + 1, 1, element_size));
  EXPECT_EQ(KLEIDICV_ERROR_RANGE,
            kleidicv_transpose(src.data(), element_size, dst.data(),
                               element_size, KLEIDICV_MAX_IMAGE_PIXELS,
                               KLEIDICV_MAX_IMAGE_PIXELS, element_size));
}

INSTANTIATE_TEST_SUITE_P(, Transpose, testing::Values(1, 2, 3, 4, 6, 8),
                         testing::PrintToStringParamName());
