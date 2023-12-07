// SPDX-FileCopyrightText: 2023 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#include <gtest/gtest.h>
#include <intrinsiccv.h>

#include <type_traits>

#include "framework/operation.h"

#define INTINSICCV_SATURATING_SUB(type, suffix) \
  INTINSICCV_API(saturating_sub, intrinsiccv_saturating_sub_##suffix, type)

INTINSICCV_SATURATING_SUB(int8_t, s8);
INTINSICCV_SATURATING_SUB(uint8_t, u8);
INTINSICCV_SATURATING_SUB(int16_t, s16);
INTINSICCV_SATURATING_SUB(uint16_t, u16);
INTINSICCV_SATURATING_SUB(int32_t, s32);
INTINSICCV_SATURATING_SUB(uint32_t, u32);
INTINSICCV_SATURATING_SUB(int64_t, s64);
INTINSICCV_SATURATING_SUB(uint64_t, u64);

template <typename ElementType>
class SaturatingSubTest final : public BinaryOperationTest<ElementType> {
  /// Expose constructor of base class.
  using BinaryOperationTest<ElementType>::BinaryOperationTest;

 protected:
  using Elements = typename BinaryOperationTest<ElementType>::Elements;
  using BinaryOperationTest<ElementType>::min;
  using BinaryOperationTest<ElementType>::max;

  /// Calls the API-under-test in the appropriate way.
  void call_api() override {
    saturating_sub<ElementType>()(
        this->inputs_[0].data(), this->inputs_[0].stride(),
        this->inputs_[1].data(), this->inputs_[1].stride(),
        this->actual_[0].data(), this->actual_[0].stride(), this->width(),
        this->height());
  }

  /// Returns different test data for signed and unsigned element types.
  const std::vector<Elements>& test_elements() override {
    if constexpr (std::is_unsigned_v<ElementType>) {
      static const std::vector<Elements> kTestElements = {
          // clang-format off
          {    0,     0, 0},
          {    2,     1, 1},
          {    1,     1, 0},
          {    1,     2, 0},
          {max(), max(), 0},
          // clang-format on
      };

      return kTestElements;
    } else {
      static const std::vector<Elements> kTestElements = {
          // clang-format off
          {    min(), max(),     min()},
          {    min(),     1,     min()},
          {min() + 1,     2,     min()},
          {    min(),    -1, min() + 1},
          {    min(), min(),         0},
          {        0,     0,         0},
          {        2,     1,         1},
          {        1,     1,         0},
          {        1,     2,        -1},
          {    max(), max(),         0},
          {        2, min(),     max()},
          // clang-format on
      };

      return kTestElements;
    }
  }
};  // end of class SaturatingSubTest<ElementType>

template <typename ElementType>
class SaturatingSub : public testing::Test {
 public:
  /// Dummy value, only used to get the type.
  ElementType value_;
};  // end of class SaturatingSub<ElementType>

using ElementTypes = ::testing::Types<int8_t, uint8_t, int16_t, uint16_t,
                                      int32_t, uint32_t, int64_t, uint64_t>;
TYPED_TEST_SUITE(SaturatingSub, ElementTypes);

/// Tests \ref intrinsiccv_saturating_sub_<type> API.
TYPED_TEST(SaturatingSub, API) {
  using ElementType = decltype(this->value_);
  // Test without padding.
  SaturatingSubTest<ElementType>{}.test();
  // Test with padding.
  SaturatingSubTest<ElementType>{}
      .with_padding(test::Options::vector_length())
      .test();
}
