// SPDX-FileCopyrightText: 2024 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#include <gtest/gtest.h>
#include <intrinsiccv.h>

#include "framework/operation.h"

template <typename ElementType>
class ThresholdBinaryTestBase : public UnaryOperationTest<ElementType> {
  // Needed to initialize value()
  using UnaryOperationTest<ElementType>::max;

 protected:
  // Calls the API-under-test in the appropriate way.
  intrinsiccv_error_t call_api() override {
    return intrinsiccv_threshold_binary_u8(
        this->inputs_[0].data(), this->inputs_[0].stride(),
        this->actual_[0].data(), this->actual_[0].stride(), this->width(),
        this->height(), this->threshold(), this->value());
  }

  virtual ElementType threshold() = 0;
  static constexpr ElementType value() { return max(); }
};  // end of class ThresholdBinaryTestBase<ElementType>

template <typename ElementType>
class ThresholdBinaryTest final : public ThresholdBinaryTestBase<ElementType> {
  using Elements = typename UnaryOperationTest<ElementType>::Elements;

  ElementType threshold() override { return 13; }

  const std::vector<Elements>& test_elements() override {
    static const std::vector<Elements> kTestElements = {
        // clang-format off
      { 12,             0},
      { 13,             0},
      { 14, this->value()},
        // clang-format on
    };
    return kTestElements;
  }
};

template <typename ElementType>
class ThresholdBinaryTestMin final
    : public ThresholdBinaryTestBase<ElementType> {
  using Elements = typename UnaryOperationTest<ElementType>::Elements;
  using UnaryOperationTest<ElementType>::min;

  ElementType threshold() override { return min(); }

  const std::vector<Elements>& test_elements() override {
    static const std::vector<Elements> kTestElements = {
        // clang-format off
      {    min(),             0},
      {min() + 1, this->value()},
        // clang-format on
    };
    return kTestElements;
  }
};

template <typename ElementType>
class ThresholdBinaryTestZero final
    : public ThresholdBinaryTestBase<ElementType> {
  using Elements = typename UnaryOperationTest<ElementType>::Elements;

  ElementType threshold() override { return 0; }

  const std::vector<Elements>& test_elements() override {
    static const std::vector<Elements> kTestElements = {
        // clang-format off
      { 0,             0},
      { 1, this->value()},
        // clang-format on
    };
    return kTestElements;
  }
};

template <typename ElementType>
class ThresholdBinaryTestMax final
    : public ThresholdBinaryTestBase<ElementType> {
  using Elements = typename UnaryOperationTest<ElementType>::Elements;
  using UnaryOperationTest<ElementType>::max;

  ElementType threshold() override { return max() - 1; }

  const std::vector<Elements>& test_elements() override {
    static const std::vector<Elements> kTestElements = {
        // clang-format off
      {max() - 1,             0},
      {    max(), this->value()},
        // clang-format on
    };
    return kTestElements;
  }
};

template <typename ElementType>
class ThresholdBinary : public testing::Test {};

using ElementTypes = ::testing::Types<uint8_t>;

TYPED_TEST_SUITE(ThresholdBinary, ElementTypes);

TYPED_TEST(ThresholdBinary, Test) { ThresholdBinaryTest<TypeParam>{}.test(); }

TYPED_TEST(ThresholdBinary, TestMin) {
  ThresholdBinaryTestMin<TypeParam>{}.test();
}

TYPED_TEST(ThresholdBinary, TestZero) {
  ThresholdBinaryTestZero<TypeParam>{}.test();
}

TYPED_TEST(ThresholdBinary, TestMax) {
  ThresholdBinaryTestMax<TypeParam>{}.test();
}

TYPED_TEST(ThresholdBinary, NullPointer) {
  const TypeParam src[1] = {};
  TypeParam dst[1];
  test::test_null_args(intrinsiccv_threshold_binary_u8, src, sizeof(TypeParam),
                       dst, sizeof(TypeParam), 1, 1, 1, 1);
}
