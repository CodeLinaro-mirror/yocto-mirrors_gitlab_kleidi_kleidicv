// SPDX-FileCopyrightText: 2023 - 2024 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#include <gtest/gtest.h>
#include <intrinsiccv.h>

#include <array>

#include "framework/array.h"
#include "framework/generator.h"
#include "framework/kernel.h"

#define INTRINSICCV_SOBEL_3X3_HORIZONTAL(type, suffix) \
  INTRINSICCV_API(sobel_3x3_horizontal,                \
                  intrinsiccv_sobel_3x3_horizontal_##suffix, type)

#define INTRINSICCV_SOBEL_3X3_VERTICAL(type, suffix)                           \
  INTRINSICCV_API(sobel_3x3_vertical, intrinsiccv_sobel_3x3_vertical_##suffix, \
                  type)

INTRINSICCV_SOBEL_3X3_HORIZONTAL(uint8_t, s16_u8);

INTRINSICCV_SOBEL_3X3_VERTICAL(uint8_t, s16_u8);

// Implements KernelTestParams for Sobel operators.
template <typename ElementType, bool IsHorizontal>
struct SobelKernelTestParams;

template <bool IsHorizontal>
struct SobelKernelTestParams<uint8_t, IsHorizontal> {
  using InputType = uint8_t;
  using IntermediateType = int16_t;
  using OutputType = int16_t;

  static constexpr bool kIsHorizontal = IsHorizontal;
};  // end of struct SobelKernelTestParams<uint8_t, IsHorizontal>

static constexpr std::array<intrinsiccv_border_type_t, 1> kSupportedBorders = {
    INTRINSICCV_BORDER_TYPE_REPLICATE,
};

// Test for Sobel 3x3 operator.
template <class KernelTestParams>
class Sobel3x3Test : public test::KernelTest<KernelTestParams> {
  using typename test::KernelTest<KernelTestParams>::InputType;
  using typename test::KernelTest<KernelTestParams>::IntermediateType;
  using typename test::KernelTest<KernelTestParams>::OutputType;

  intrinsiccv_error_t call_api(const test::Array2D<InputType> *input,
                               test::Array2D<OutputType> *output,
                               intrinsiccv_border_type_t) override {
    auto api = KernelTestParams::kIsHorizontal
                   ? sobel_3x3_horizontal<InputType>()
                   : sobel_3x3_vertical<InputType>();
    return api(input->data(), input->stride(), output->data(), output->stride(),
               input->width() / input->channels(), input->height(),
               input->channels());
  }

 public:
  void test(test::Array2D<IntermediateType> mask) {
    test::Kernel kernel{mask};
    // Use the default array layouts for testing.
    auto array_layouts =
        test::default_array_layouts(mask.width(), mask.height());
    // Create generators and execute test.
    test::SequenceGenerator tested_borders{kSupportedBorders};
    test::SequenceGenerator tested_array_layouts{array_layouts};
    test::PseudoRandomNumberGenerator<InputType> element_generator;
    this->test::KernelTest<KernelTestParams>::test(
        kernel, &tested_array_layouts, &tested_borders, &element_generator);
  }
};  // end of class Sobel3x3Test<KernelTestParams>

template <typename TypeParam>
class Sobel : public testing::Test {};

using ElementTypes = ::testing::Types<uint8_t>;
TYPED_TEST_SUITE(Sobel, ElementTypes);

// Tests sobel_3x3_horizontal_<output_type>_<input_type> API.
TYPED_TEST(Sobel, Horizontal3x3) {
  using KernelTestParams = SobelKernelTestParams<TypeParam, true>;
  // Horizontal 3x3 Sobel operator.
  test::Array2D<typename KernelTestParams::IntermediateType> mask{3, 3};
  mask.set(0, 0, {-1, 0, 1});
  mask.set(1, 0, {-2, 0, 2});
  mask.set(2, 0, {-1, 0, 1});
  Sobel3x3Test<KernelTestParams>{}.test(mask);

  typename KernelTestParams::InputType src[1] = {};
  typename KernelTestParams::OutputType dst[1];
  test::test_null_args(sobel_3x3_horizontal<TypeParam>(), src, sizeof(src), dst,
                       sizeof(dst), 1, 1, 1);
}

// Tests sobel_3x3_vertical_<output_type>_<input_type> API.
TYPED_TEST(Sobel, Vertical3x3) {
  using KernelTestParams = SobelKernelTestParams<TypeParam, false>;
  // Vertical 3x3 Sobel operator.
  test::Array2D<typename KernelTestParams::IntermediateType> mask{3, 3};
  // clang-format off
  mask.set(0, 0, {-1, -2, -1});
  mask.set(1, 0, { 0,  0,  0});
  mask.set(2, 0, { 1,  2,  1});
  // clang-format on
  Sobel3x3Test<KernelTestParams>{}.test(mask);

  typename KernelTestParams::InputType src[1] = {};
  typename KernelTestParams::OutputType dst[1];
  test::test_null_args(sobel_3x3_vertical<TypeParam>(), src, sizeof(src), dst,
                       sizeof(dst), 1, 1, 1);
}

TYPED_TEST(Sobel, MisalignmentHorizontal) {
  using KernelTestParams = SobelKernelTestParams<TypeParam, true>;
  typename KernelTestParams::InputType src[1] = {};
  typename KernelTestParams::OutputType dst[1];
  if (sizeof(typename KernelTestParams::InputType) != 1) {
    EXPECT_EQ(INTRINSICCV_ERROR_ALIGNMENT,
              sobel_3x3_horizontal<TypeParam>()(src, sizeof(src) + 1, dst,
                                                sizeof(dst), 1, 1, 1));
  }
  if (sizeof(typename KernelTestParams::OutputType) != 1) {
    EXPECT_EQ(INTRINSICCV_ERROR_ALIGNMENT,
              sobel_3x3_horizontal<TypeParam>()(src, sizeof(src), dst,
                                                sizeof(dst) + 1, 1, 1, 1));
  }
}

TYPED_TEST(Sobel, MisalignmentVertical) {
  using KernelTestParams = SobelKernelTestParams<TypeParam, false>;
  typename KernelTestParams::InputType src[1] = {};
  typename KernelTestParams::OutputType dst[1];
  if (sizeof(typename KernelTestParams::InputType) != 1) {
    EXPECT_EQ(INTRINSICCV_ERROR_ALIGNMENT,
              sobel_3x3_vertical<TypeParam>()(src, sizeof(src) + 1, dst,
                                              sizeof(dst), 1, 1, 1));
  }
  if (sizeof(typename KernelTestParams::OutputType) != 1) {
    EXPECT_EQ(INTRINSICCV_ERROR_ALIGNMENT,
              sobel_3x3_vertical<TypeParam>()(src, sizeof(src), dst,
                                              sizeof(dst) + 1, 1, 1, 1));
  }
}
