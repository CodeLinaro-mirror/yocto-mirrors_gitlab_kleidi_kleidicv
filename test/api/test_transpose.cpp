// SPDX-FileCopyrightText: 2024 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#include <gtest/gtest.h>
#include <intrinsiccv.h>

#include "framework/array.h"
#include "framework/generator.h"

template <typename ElementType, bool inplace>
class TestTranspose final {
 public:
  static void test(size_t padding) {
    // Width is set to execute 2 vector paths and 1 scalar path
    const size_t src_width = test::Options::vector_length() * 2 + 1;
    // Only square data is supported inplace
    // Otherwise execute 3 vector paths and 1 scalar path (not to match width)
    const size_t src_height =
        inplace ? src_width : test::Options::vector_length() * 3 + 1;
    const size_t dst_width = src_height;
    const size_t dst_height = src_width;

    test::Array2D<ElementType> source(src_width, src_height, padding, 1);
    test::Array2D<ElementType> expected(dst_width, dst_height, padding, 1);
    test::Array2D<ElementType> actual;
    test::Array2D<ElementType> *p_actual = &source;

    // Only allocate actual array if needed
    if constexpr (!inplace) {
      actual = test::Array2D<ElementType>(dst_width, dst_height, padding, 1);
      p_actual = &actual;
    }

    // No specific values to test
    test::PseudoRandomNumberGenerator<ElementType> generator;
    source.fill(&generator);

    calculate_expected(source, expected);

    ASSERT_EQ(INTRINSICCV_OK, intrinsiccv_transpose(
                                  source.data(), source.stride(),
                                  p_actual->data(), p_actual->stride(),
                                  src_width, src_height, sizeof(ElementType)));

    EXPECT_EQ_ARRAY2D(expected, *p_actual);
  }

 protected:
  static void calculate_expected(const test::Array2D<ElementType> &source,
                                 test::Array2D<ElementType> &expected) {
    for (size_t hindex = 0; hindex < source.height(); ++hindex) {
      for (size_t vindex = 0; vindex < source.width(); ++vindex) {
        // NOLINTBEGIN(clang-analyzer-core.uninitialized.Assign)
        *expected.at(vindex, hindex) = *source.at(hindex, vindex);
        // NOLINTEND(clang-analyzer-core.uninitialized.Assign)
      }
    }
  }
};

template <typename ElementType>
class Transpose : public testing::Test {};

using ElementTypes = ::testing::Types<uint8_t, uint16_t, uint32_t, uint64_t>;
TYPED_TEST_SUITE(Transpose, ElementTypes);

TYPED_TEST(Transpose, ToNewBufferNoPadding) {
  TestTranspose<TypeParam, false>::test(0);
}

TYPED_TEST(Transpose, ToNewBufferWithPadding) {
  TestTranspose<TypeParam, false>::test(1);
}

TYPED_TEST(Transpose, InplaceNoPadding) {
  TestTranspose<TypeParam, true>::test(0);
}

TYPED_TEST(Transpose, InplaceWithPadding) {
  TestTranspose<TypeParam, true>::test(1);
}
