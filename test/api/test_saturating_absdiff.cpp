// SPDX-FileCopyrightText: 2023 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#include <gtest/gtest.h>
#include <intrinsiccv.h>

#include <type_traits>

#include "framework/operation.h"

#define INTINSICCV_SATURATING_ABSDIFF(type, suffix)                           \
  INTINSICCV_API(saturating_absdiff, intrinsiccv_saturating_absdiff_##suffix, \
                 type)

INTINSICCV_SATURATING_ABSDIFF(int8_t, s8);
INTINSICCV_SATURATING_ABSDIFF(uint8_t, u8);
INTINSICCV_SATURATING_ABSDIFF(int16_t, s16);
INTINSICCV_SATURATING_ABSDIFF(uint16_t, u16);
INTINSICCV_SATURATING_ABSDIFF(int32_t, s32);

template <typename ElementType>
class SaturatingAbsDiffTest final : public BinaryOperationTest<ElementType> {
  /// Expose constructor of base class.
  using BinaryOperationTest<ElementType>::BinaryOperationTest;

 protected:
  using Elements = typename BinaryOperationTest<ElementType>::Elements;
  using BinaryOperationTest<ElementType>::min;
  using BinaryOperationTest<ElementType>::max;

  /// Calls the API-under-test in the appropriate way.
  void call_api() override {
    saturating_absdiff<ElementType>()(
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
          {    0,         0,     0},
          {    1,         1,     0},
          {    2,         1,     1},
          {    1,         2,     1},
          {    0,     max(), max()},
          {max(),     max(),     0},
          {max(), max() - 1,     1},
          {max(),         0, max()},
          // clang-format on
      };

      return kTestElements;
    } else {
      static const std::vector<Elements> kTestElements = {
          // clang-format off
          {min(),     max(), max()},
          {max(),     min(), max()},
          {   -1,     max(), max()},
          {max(),        -1, max()},
          {min(), min() + 1,     1},
          {   -1,        -2,     1},
          {   -2,        -1,     1},
          {   -1,        -1,     0},
          {    0,         0,     0},
          {    1,         1,     0},
          {    2,         1,     1},
          {    1,         2,     1},
          {    0,     max(), max()},
          {max(),     max(),     0},
          {max(), max() - 1,     1},
          {max(),         0, max()},
          // clang-format on
      };

      return kTestElements;
    }
  }
};  // end of class SaturatingAbsDiffTest<ElementType>

template <typename ElementType>
class SaturatingAbsDiff : public testing::Test {
 public:
  /// Dummy value, only used to get the type.
  ElementType value_;
};  // end of class SaturatingAbsDiff<ElementType>

using ElementTypes =
    ::testing::Types<int8_t, uint8_t, int16_t, uint16_t, int32_t>;
TYPED_TEST_SUITE(SaturatingAbsDiff, ElementTypes);

/// Tests \ref intrinsiccv_saturating_absdiff_<type> API.
TYPED_TEST(SaturatingAbsDiff, API) {
  using ElementType = decltype(this->value_);
  // Test without padding.
  SaturatingAbsDiffTest<ElementType>{}.test();
  // Test with padding.
  SaturatingAbsDiffTest<ElementType>{}
      .with_padding(test::Options::vector_length())
      .test();
}
