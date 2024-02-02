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
  explicit TestTranspose(size_t padding) : padding_(padding) {}

  void scalar_test() {
    // Scalar path will be exercised only if width is smaller than number of
    // lanes in a vector
    size_t src_width = test::Options::vector_lanes<ElementType>() - 1;
    // Set height to be different than width but still smaller than vector_lanes
    size_t src_height =
        inplace ? src_width : test::Options::vector_lanes<ElementType>() - 2;
    test(src_width, src_height);
  }

  void vector_test() {
    // Vector path will be exercised even for +1 because of
    // try_avoid_tail_loop()
    size_t src_width = test::Options::vector_lanes<ElementType>() + 1;
    // Set height to be different from width but still bigger than vector_lanes
    size_t src_height =
        inplace ? src_width : test::Options::vector_lanes<ElementType>() + 2;
    test(src_width, src_height);
  }

 protected:
  void test(size_t src_width, size_t src_height) const {
    const size_t dst_width = src_height;
    const size_t dst_height = src_width;

    test::Array2D<ElementType> source(src_width, src_height, padding_, 1);
    test::Array2D<ElementType> expected(dst_width, dst_height, padding_, 1);
    test::Array2D<ElementType> actual;
    test::Array2D<ElementType> *p_actual = &source;

    // Only allocate actual array if needed
    if constexpr (!inplace) {
      actual = test::Array2D<ElementType>(dst_width, dst_height, padding_, 1);
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

  void calculate_expected(const test::Array2D<ElementType> &source,
                          test::Array2D<ElementType> &expected) const {
    for (size_t hindex = 0; hindex < source.height(); ++hindex) {
      for (size_t vindex = 0; vindex < source.width(); ++vindex) {
        // NOLINTBEGIN(clang-analyzer-core.uninitialized.Assign)
        *expected.at(vindex, hindex) = *source.at(hindex, vindex);
        // NOLINTEND(clang-analyzer-core.uninitialized.Assign)
      }
    }
  }

  size_t padding_;
};

template <typename ElementType>
class Transpose : public testing::Test {};

using ElementTypes = ::testing::Types<uint8_t, uint16_t, uint32_t, uint64_t>;
TYPED_TEST_SUITE(Transpose, ElementTypes);

TYPED_TEST(Transpose, ScalarToNewBufferNoPadding) {
  TestTranspose<TypeParam, false> test(0);
  test.scalar_test();
}

TYPED_TEST(Transpose, VectorToNewBufferNoPadding) {
  TestTranspose<TypeParam, false> test(0);
  test.vector_test();
}

TYPED_TEST(Transpose, ScalarToNewBufferWithPadding) {
  TestTranspose<TypeParam, false> test(1);
  test.scalar_test();
}

TYPED_TEST(Transpose, VectorToNewBufferWithPadding) {
  TestTranspose<TypeParam, false> test(1);
  test.vector_test();
}

TYPED_TEST(Transpose, ScalarInplaceNoPadding) {
  TestTranspose<TypeParam, true> test(0);
  test.scalar_test();
}

TYPED_TEST(Transpose, VectorInplaceNoPadding) {
  TestTranspose<TypeParam, true> test(0);
  test.vector_test();
}

TYPED_TEST(Transpose, ScalarInplaceWithPadding) {
  TestTranspose<TypeParam, true> test(1);
  test.scalar_test();
}

TYPED_TEST(Transpose, VectorInplaceWithPadding) {
  TestTranspose<TypeParam, true> test(1);
  test.vector_test();
}

TYPED_TEST(Transpose, NullPointer) {
  TypeParam src[1] = {}, dst[1];
  test::test_null_args(intrinsiccv_transpose, src, sizeof(TypeParam), dst,
                       sizeof(TypeParam), 1, 1, sizeof(TypeParam));
}
