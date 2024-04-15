// SPDX-FileCopyrightText: 2024 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#include <gtest/gtest.h>

#include <cmath>

#include "framework/generator.h"
#include "framework/operation.h"
#include "framework/utils.h"
#include "kleidicv/kleidicv.h"

#define KLEIDICV_EXP(type, suffix) \
  KLEIDICV_API(exp, kleidicv_exp_##suffix, type)

KLEIDICV_EXP(float, f32);

static void check_1ulp_error(test::Array2D<float>& expected_array,
                             test::Array2D<float>& actual_array) {
  for (size_t i = 0; i < expected_array.height(); ++i) {
    for (size_t j = 0; j < expected_array.width(); ++j) {
      float expected = *(expected_array.at(i, j));
      float actual = *(actual_array.at(i, j));
      // Error of 1 ULP means that actual is either same as expected, or
      // the next float value in negative of positive direction
      EXPECT_EQ(std::nextafterf(actual, expected), expected);
    }
  }
}

template <typename ElementType>
class ExpTestSpecial;

template <>
class ExpTestSpecial<float> final : public UnaryOperationTest<float> {
  using ElementType = float;
  using Elements = typename UnaryOperationTest<ElementType>::Elements;

 public:
  ExpTestSpecial() : test_elements_(input_values().size()) {
    auto inputs = input_values();

    for (size_t i = 0; i < inputs.size(); ++i) {
      test_elements_[i].values[0] = inputs[i];
      // Expected values calculated as doubles to have 'perfect' references.
      // As the NEON implementation reuses the toolchain's expf implementation
      // for the tail path, the test expects that the error for expf is also
      // less than 1 ULP.
      test_elements_[i].values[1] =
          static_cast<float>(exp(static_cast<double>(inputs[i])));
    }
  }

 private:
  static const std::vector<ElementType>& input_values() {
    static const std::vector<ElementType> kInputValues = {
        -105.31, -100.07, -81.012, -47.66, -3.1088, -0.21,
        0.7,     6.2,     39.7201, 86.11,  88.947};

    return kInputValues;
  }

  kleidicv_error_t call_api() override {
    return exp<ElementType>()(
        this->inputs_[0].data(), this->inputs_[0].stride(),
        this->actual_[0].data(), this->actual_[0].stride(), this->width(),
        this->height());
  }

  void check(kleidicv_error_t err) override {
    EXPECT_EQ(KLEIDICV_OK, err);
    check_1ulp_error(this->expected_[0], this->actual_[0]);
  }

  const std::vector<Elements>& test_elements() override {
    return test_elements_;
  }

  void setup() override {
    // Default input value is 0.0, so the default expected value needs to be set
    // to 1.0
    ElementType expected = 1.0;
    this->expected_[0].fill(expected);
    UnaryOperationTest<ElementType>::setup();
  }

  std::vector<Elements> test_elements_;
};  // end of class ExpTestSpecial<float>

template <typename ElementType>
class ExpTestRandom;

template <>
class ExpTestRandom<float> {
 public:
  void test() {
    const size_t kWidth = test::Options::vector_length() * 16;
    constexpr size_t kHeight = 16;
    test::PseudoRandomNumberGenerator<float> generator;
    test::Array2D<float> input{kWidth, kHeight};
    test::Array2D<float> expected{kWidth, kHeight};
    test::Array2D<float> actual{kWidth, kHeight};
    input.fill(generator);

    fill_expected(input, expected);

    EXPECT_EQ(KLEIDICV_OK,
              exp<float>()(input.data(), input.stride(), actual.data(),
                           actual.stride(), input.width(), input.height()));

    check_1ulp_error(expected, actual);
  }

 private:
  void fill_expected(test::Array2D<float>& input,
                     test::Array2D<float>& expected) {
    for (size_t i = 0; i < input.height(); ++i) {
      for (size_t j = 0; j < input.width(); ++j) {
        // Expected values calculated as doubles to have 'perfect' references.
        // As the NEON implementation reuses the toolchain's expf implementation
        // for the tail path, the test expects that the error for expf is also
        // less than 1 ULP.
        // NOLINTBEGIN(clang-analyzer-core.CallAndMessage)
        *(expected.at(i, j)) =
            static_cast<float>(exp(static_cast<double>(*(input.at(i, j)))));
        // NOLINTEND(clang-analyzer-core.CallAndMessage)
      }
    }
  }
};  // end of class ExpTestRandom<float>

template <typename ElementType>
class Exp : public testing::Test {};
using ElementTypes = ::testing::Types<float>;
TYPED_TEST_SUITE(Exp, ElementTypes);

TYPED_TEST(Exp, SpecialValues) {
  ExpTestSpecial<TypeParam>{}.test();
  ExpTestSpecial<TypeParam>{}
      .with_padding(test::Options::vector_length())
      .test();
}

TYPED_TEST(Exp, RandomValues) { ExpTestRandom<TypeParam>{}.test(); }

TYPED_TEST(Exp, OversizeImage) {
  TypeParam src[1] = {}, dst[1];
  EXPECT_EQ(KLEIDICV_ERROR_RANGE,
            exp<TypeParam>()(src, sizeof(TypeParam), dst, sizeof(TypeParam),
                             KLEIDICV_MAX_IMAGE_PIXELS + 1, 1));
  EXPECT_EQ(
      KLEIDICV_ERROR_RANGE,
      exp<TypeParam>()(src, sizeof(TypeParam), dst, sizeof(TypeParam),
                       KLEIDICV_MAX_IMAGE_PIXELS, KLEIDICV_MAX_IMAGE_PIXELS));
}

TYPED_TEST(Exp, NullPointers) {
  TypeParam src[1] = {}, dst[1];
  test::test_null_args(exp<TypeParam>(), src, sizeof(TypeParam), dst,
                       sizeof(TypeParam), 1, 1);
}
