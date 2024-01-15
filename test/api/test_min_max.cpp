// SPDX-FileCopyrightText: 2023 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#include <gtest/gtest.h>
#include <intrinsiccv.h>

#include "framework/array.h"
#include "framework/utils.h"

#define INTRINSICCV_MIN_MAX(type, suffix) \
  INTRINSICCV_API(min_max, intrinsiccv_min_max_##suffix, type)

INTRINSICCV_MIN_MAX(int8_t, s8);
INTRINSICCV_MIN_MAX(uint8_t, u8);
INTRINSICCV_MIN_MAX(int16_t, s16);
INTRINSICCV_MIN_MAX(uint16_t, u16);
INTRINSICCV_MIN_MAX(int32_t, s32);

template <typename ElementType>
class MinMaxTest {
  using ArrayType = test::Array2D<ElementType>;

  /// Returns the number of padding bytes at the end of rows.
  size_t padding() const { return padding_; }

  /// Returns the minimum value for ElementType.
  static constexpr ElementType min() {
    return std::numeric_limits<ElementType>::min();
  }

  /// Returns the maximum value for ElementType.
  static constexpr ElementType max() {
    return std::numeric_limits<ElementType>::max();
  }

  /// Returns the number of vector lanes.
  size_t lanes() const { return test::Options::vector_lanes<ElementType>(); }

  /// Number of padding bytes at the end of rows.
  size_t padding_{0};

  struct Elements {
    std::initializer_list<ElementType> source_row0_vector;
    std::initializer_list<ElementType> source_row0_scalar;
    std::initializer_list<ElementType> source_row1_vector;
    std::initializer_list<ElementType> source_row1_scalar;
    ElementType filler_value;
    ElementType expected_min;
    ElementType expected_max;
  };

  // clang-format off
  static constexpr Elements test_elements[] = {
    {
      {},  {},
      {},  {},
      10,
      10, 10
    },
    {
      {},  {},
      {},  {},
      min(),
      min(), min()
    },
    {
      {},  {},
      {},  {},
      0,
      0, 0
    },
    {
      {},  {},
      {},  {},
      max(),
      max(), max()
    },
    {
      {min()+10},    {},
      {max()}, {},
      min()+20,
      min()+10, max()
    },
    {
      {},    {min()},
      {},    {max() - 10},
      min()+20,
      min(), max()-10
    },
    {
      {9},  {},
      {},  {11},
      10,
      9, 11
    },
    {
      {},   {9},
      {11}, {},
      10,
      9, 11
    },
    {
      {11}, {},
      {},  {9},
      10,
      9, 11
    },
    {
      {10, 9, 11},  {},
      {},           {},
      10,
      9, 11
    },
    {
      {9, 10, 11},  {},
      {},           {},
      10,
      9, 11
    },
    {
      { 3,  4,  5,  6},  {15, 16, 17},
      {20, 21, 22, 23},  {35,  2, 33},
      10,
      2, 35
    },
    {
      { 3,  4,  5,  6},  {15, 16, 17},
      {20,  2, 22, 23},  {35, 36, 33},
      10,
      2, 36
    },
    {
      { 1,  2,  3,  4},  {15, 16, 42},
      { 1,  2,  3,  4},  {15, 16, 42},
      10,
      1, 42
    },

  };
  // clang-format on

  // We have 2 rows, source_row0 and source_row1
  size_t height() const { return 2; }

  /// Tested number of elements in a row.
  size_t width() const {
    // Sufficient number of elements to exercise both vector and scalar paths.
    return 3 * lanes() - 1;
  }

 public:
  MinMaxTest<ElementType>& with_padding(size_t padding) {
    padding_ = padding;
    return *this;
  }

  void test() {
    for (auto testData : test_elements) {
      ArrayType source{width(), height(), padding()};
      ASSERT_TRUE(source.valid());
      source.fill(testData.filler_value);

      // Fill elements one vector length apart.
      for (size_t column_index = 0; column_index + lanes() < width();
           column_index += lanes()) {
        source.set(0, column_index, testData.source_row0_vector);
        source.set(1, column_index, testData.source_row1_vector);
      }
      source.set(0, (width() / lanes()) * lanes(), testData.source_row0_scalar);
      source.set(1, (width() / lanes()) * lanes(), testData.source_row1_scalar);

      ElementType expected_min = testData.expected_min;
      ElementType expected_max = testData.expected_max;

      ElementType actual_min = max();
      ElementType actual_max = min();

      min_max<ElementType>()(source.data(), source.stride(), width(), height(),
                             nullptr, nullptr);
      EXPECT_EQ(actual_min, max());
      EXPECT_EQ(actual_max, min());

      actual_min = max();
      actual_max = min();
      min_max<ElementType>()(source.data(), source.stride(), width(), height(),
                             &actual_min, nullptr);
      EXPECT_EQ(actual_min, expected_min);
      EXPECT_EQ(actual_max, min());

      actual_min = max();
      actual_max = min();
      min_max<ElementType>()(source.data(), source.stride(), width(), height(),
                             nullptr, &actual_max);
      EXPECT_EQ(actual_min, max());
      EXPECT_EQ(actual_max, expected_max);

      actual_min = max();
      actual_max = min();
      min_max<ElementType>()(source.data(), source.stride(), width(), height(),
                             &actual_min, &actual_max);
      EXPECT_EQ(actual_min, expected_min);
      EXPECT_EQ(actual_max, expected_max);
    }
  }
};  // end of class MinMaxTest<ElementType>

template <typename ElementType>
class MinMax : public testing::Test {};

using ElementTypes =
    ::testing::Types<int8_t, uint8_t, int16_t, uint16_t, int32_t>;
TYPED_TEST_SUITE(MinMax, ElementTypes);

TYPED_TEST(MinMax, API) {
  MinMaxTest<TypeParam>{}.test();
  MinMaxTest<TypeParam>{}.with_padding(test::Options::vector_length()).test();
}
