// SPDX-FileCopyrightText: 2024 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#include <gtest/gtest.h>

#include "framework/array.h"
#include "framework/generator.h"
#include "framework/kernel.h"
#include "framework/utils.h"
#include "intrinsiccv/intrinsiccv.h"

#define INTRINSICCV_GAUSSIAN_BLUR(type, kernel_suffix, type_suffix)          \
  INTRINSICCV_API(gaussian_blur_##kernel_suffix,                             \
                  intrinsiccv_gaussian_blur_##kernel_suffix##_##type_suffix, \
                  type)

INTRINSICCV_GAUSSIAN_BLUR(uint8_t, 3x3, u8);
INTRINSICCV_GAUSSIAN_BLUR(uint8_t, 5x5, u8);

// Implements KernelTestParams for Gaussian Blur operators.
template <typename ElementType, size_t KernelSize>
struct GaussianBlurKernelTestParams;

template <size_t KernelSize>
struct GaussianBlurKernelTestParams<uint8_t, KernelSize> {
  using InputType = uint8_t;
  using IntermediateType = uint16_t;
  using OutputType = uint8_t;

  static constexpr size_t kKernelSize = KernelSize;
};  // end of struct GaussianBlurKernelTestParams<uint8_t, KernelSize>

static constexpr std::array<intrinsiccv_border_type_t, 4> kSupportedBorders = {
    INTRINSICCV_BORDER_TYPE_REPLICATE,
    INTRINSICCV_BORDER_TYPE_REFLECT,
    INTRINSICCV_BORDER_TYPE_WRAP,
    INTRINSICCV_BORDER_TYPE_REVERSE,
};

// Test for GaussianBlur operator.
template <class KernelTestParams>
class GaussianBlurTest : public test::KernelTest<KernelTestParams> {
  using typename test::KernelTest<KernelTestParams>::InputType;
  using typename test::KernelTest<KernelTestParams>::IntermediateType;
  using typename test::KernelTest<KernelTestParams>::OutputType;

  intrinsiccv_error_t call_api(const test::Array2D<InputType> *input,
                               test::Array2D<OutputType> *output,
                               intrinsiccv_border_type_t border_type,
                               intrinsiccv_border_values_t) override {
    auto api = KernelTestParams::kKernelSize == 3
                   ? gaussian_blur_3x3<InputType>()
                   : gaussian_blur_5x5<InputType>();

    intrinsiccv_filter_context_t *context = nullptr;
    auto ret = intrinsiccv_filter_create(
        &context, input->channels(), sizeof(IntermediateType),
        intrinsiccv_rectangle_t{input->width() / input->channels(),
                                input->height()});
    if (ret != INTRINSICCV_OK) {
      return ret;
    }

    ret = api(input->data(), input->stride(), output->data(), output->stride(),
              input->width() / input->channels(), input->height(),
              input->channels(), border_type, context);
    auto releaseRet = intrinsiccv_filter_release(context);
    if (releaseRet != INTRINSICCV_OK) {
      return releaseRet;
    }

    return ret;
  }

  // Apply rounding to nearest integer division.
  IntermediateType scale_result(const test::Kernel<IntermediateType> &kernel,
                                IntermediateType result) override {
    return kernel.width() == 3 ? ((result + 8) / 16) : ((result + 128) / 256);
  }

 public:
  void test(test::Array2D<IntermediateType> mask) {
    test::Kernel kernel{mask};
    // Use the default array layouts for testing.
    auto array_layouts =
        test::default_array_layouts(mask.width(), mask.height());
    test::KernelTest<KernelTestParams>::with_debug();
    // Use the default border values for testing.
    auto kSupportedBorderValues = test::default_border_values();
    // Create generators and execute test.
    test::SequenceGenerator tested_array_layouts{array_layouts};
    test::SequenceGenerator tested_borders{kSupportedBorders};
    test::SequenceGenerator tested_border_values{kSupportedBorderValues};
    test::PseudoRandomNumberGenerator<InputType> element_generator;
    this->test::KernelTest<KernelTestParams>::test(
        kernel, tested_array_layouts, tested_borders, tested_border_values,
        element_generator);
  }
};  // end of class class GaussianBlur3x3Test<KernelTestParams>

using ElementTypes = ::testing::Types<uint8_t>;

template <typename ElementType>
class GaussianBlur : public testing::Test {};

TYPED_TEST_SUITE(GaussianBlur, ElementTypes);

// Tests gaussian_blur_3x3_<input_type> API.
TYPED_TEST(GaussianBlur, 3x3) {
  using KernelTestParams = GaussianBlurKernelTestParams<TypeParam, 3>;
  // 3x3 GaussianBlur operator.
  test::Array2D<typename KernelTestParams::IntermediateType> mask{3, 3};
  // clang-format off
  mask.set(0, 0, { 1, 2, 1});
  mask.set(1, 0, { 2, 4, 2});
  mask.set(2, 0, { 1, 2, 1});
  // clang-format on
  GaussianBlurTest<KernelTestParams>{}.test(mask);
}

// Tests gaussian_blur_5x5_<input_type> API.
TYPED_TEST(GaussianBlur, 5x5) {
  using KernelTestParams = GaussianBlurKernelTestParams<TypeParam, 5>;
  // 5x5 GaussianBlur operator.
  test::Array2D<typename KernelTestParams::IntermediateType> mask{5, 5};
  // clang-format off
  mask.set(0, 0, { 1,  4,  6,  4, 1});
  mask.set(1, 0, { 4, 16, 24, 16, 4});
  mask.set(2, 0, { 6, 24, 36, 24, 6});
  mask.set(3, 0, { 4, 16, 24, 16, 4});
  mask.set(4, 0, { 1,  4,  6,  4, 1});
  // clang-format on
  GaussianBlurTest<KernelTestParams>{}.test(mask);
}

TYPED_TEST(GaussianBlur, UnsupportedBorderType) {
  intrinsiccv_filter_context_t *context = nullptr;
  ASSERT_EQ(INTRINSICCV_OK,
            intrinsiccv_filter_create(&context, 1, 2 * sizeof(TypeParam),
                                      intrinsiccv_rectangle_t{1, 1}));
  TypeParam src[1] = {}, dst[1];
  for (intrinsiccv_border_type_t border : {
           INTRINSICCV_BORDER_TYPE_CONSTANT,
           INTRINSICCV_BORDER_TYPE_TRANSPARENT,
           INTRINSICCV_BORDER_TYPE_NONE,
       }) {
    EXPECT_EQ(INTRINSICCV_ERROR_NOT_IMPLEMENTED,
              gaussian_blur_3x3<TypeParam>()(src, sizeof(TypeParam), dst,
                                             sizeof(TypeParam), 1, 1, 1, border,
                                             context));
    EXPECT_EQ(INTRINSICCV_ERROR_NOT_IMPLEMENTED,
              gaussian_blur_5x5<TypeParam>()(src, sizeof(TypeParam), dst,
                                             sizeof(TypeParam), 1, 1, 1, border,
                                             context));
  }
  EXPECT_EQ(INTRINSICCV_OK, intrinsiccv_filter_release(context));
}

TYPED_TEST(GaussianBlur, NullPointer) {
  intrinsiccv_filter_context_t *context = nullptr;
  ASSERT_EQ(INTRINSICCV_OK,
            intrinsiccv_filter_create(&context, 1, 2 * sizeof(TypeParam),
                                      intrinsiccv_rectangle_t{1, 1}));
  TypeParam src[1] = {}, dst[1];
  test::test_null_args(gaussian_blur_3x3<TypeParam>(), src, sizeof(TypeParam),
                       dst, sizeof(TypeParam), 1, 1, 1,
                       INTRINSICCV_BORDER_TYPE_REPLICATE, context);
  test::test_null_args(gaussian_blur_5x5<TypeParam>(), src, sizeof(TypeParam),
                       dst, sizeof(TypeParam), 1, 1, 1,
                       INTRINSICCV_BORDER_TYPE_REPLICATE, context);
  EXPECT_EQ(INTRINSICCV_OK, intrinsiccv_filter_release(context));
}

TYPED_TEST(GaussianBlur, Misalignment) {
  if (sizeof(TypeParam) == 1) {
    // misalignment impossible
    return;
  }
  intrinsiccv_filter_context_t *context = nullptr;
  ASSERT_EQ(INTRINSICCV_OK,
            intrinsiccv_filter_create(&context, 1, 2 * sizeof(TypeParam),
                                      intrinsiccv_rectangle_t{1, 1}));
  TypeParam src[1] = {}, dst[1];
  EXPECT_EQ(INTRINSICCV_ERROR_ALIGNMENT,
            gaussian_blur_3x3<TypeParam>()(
                src, sizeof(TypeParam) + 1, dst, sizeof(TypeParam), 1, 1, 1,
                INTRINSICCV_BORDER_TYPE_REPLICATE, context));
  EXPECT_EQ(INTRINSICCV_ERROR_ALIGNMENT,
            gaussian_blur_3x3<TypeParam>()(
                src, sizeof(TypeParam), dst, sizeof(TypeParam) + 1, 1, 1, 1,
                INTRINSICCV_BORDER_TYPE_REPLICATE, context));
  EXPECT_EQ(INTRINSICCV_ERROR_ALIGNMENT,
            gaussian_blur_5x5<TypeParam>()(
                src, sizeof(TypeParam) + 1, dst, sizeof(TypeParam), 1, 1, 1,
                INTRINSICCV_BORDER_TYPE_REPLICATE, context));
  EXPECT_EQ(INTRINSICCV_ERROR_ALIGNMENT,
            gaussian_blur_5x5<TypeParam>()(
                src, sizeof(TypeParam), dst, sizeof(TypeParam) + 1, 1, 1, 1,
                INTRINSICCV_BORDER_TYPE_REPLICATE, context));
  EXPECT_EQ(INTRINSICCV_OK, intrinsiccv_filter_release(context));
}

TYPED_TEST(GaussianBlur, ImageSize) {
  intrinsiccv_filter_context_t *context = nullptr;
  ASSERT_EQ(INTRINSICCV_OK,
            intrinsiccv_filter_create(&context, 1, 2 * sizeof(TypeParam),
                                      intrinsiccv_rectangle_t{1, 1}));
  TypeParam src[1], dst[1];
  EXPECT_EQ(INTRINSICCV_ERROR_RANGE,
            gaussian_blur_3x3<TypeParam>()(
                src, sizeof(TypeParam), dst, sizeof(TypeParam),
                INTRINSICCV_MAX_IMAGE_PIXELS + 1, 1, 1,
                INTRINSICCV_BORDER_TYPE_REFLECT, context));
  EXPECT_EQ(INTRINSICCV_ERROR_RANGE,
            gaussian_blur_3x3<TypeParam>()(
                src, sizeof(TypeParam), dst, sizeof(TypeParam),
                INTRINSICCV_MAX_IMAGE_PIXELS, INTRINSICCV_MAX_IMAGE_PIXELS, 1,
                INTRINSICCV_BORDER_TYPE_REFLECT, context));
  EXPECT_EQ(INTRINSICCV_ERROR_RANGE,
            gaussian_blur_5x5<TypeParam>()(
                src, sizeof(TypeParam), dst, sizeof(TypeParam),
                INTRINSICCV_MAX_IMAGE_PIXELS + 1, 1, 1,
                INTRINSICCV_BORDER_TYPE_REFLECT, context));
  EXPECT_EQ(INTRINSICCV_ERROR_RANGE,
            gaussian_blur_5x5<TypeParam>()(
                src, sizeof(TypeParam), dst, sizeof(TypeParam),
                INTRINSICCV_MAX_IMAGE_PIXELS, INTRINSICCV_MAX_IMAGE_PIXELS, 1,
                INTRINSICCV_BORDER_TYPE_REFLECT, context));
  EXPECT_EQ(INTRINSICCV_OK, intrinsiccv_filter_release(context));
}

TYPED_TEST(GaussianBlur, ChannelNumber) {
  intrinsiccv_filter_context_t *context = nullptr;
  ASSERT_EQ(INTRINSICCV_OK,
            intrinsiccv_filter_create(&context, 1, 2 * sizeof(TypeParam),
                                      intrinsiccv_rectangle_t{1, 1}));
  TypeParam src[1], dst[1];
  EXPECT_EQ(INTRINSICCV_ERROR_RANGE,
            gaussian_blur_3x3<TypeParam>()(
                src, sizeof(TypeParam), dst, sizeof(TypeParam), 1, 1,
                INTRINSICCV_MAXIMUM_CHANNEL_COUNT + 1,
                INTRINSICCV_BORDER_TYPE_REFLECT, context));
  EXPECT_EQ(INTRINSICCV_ERROR_RANGE,
            gaussian_blur_5x5<TypeParam>()(
                src, sizeof(TypeParam), dst, sizeof(TypeParam), 1, 1,
                INTRINSICCV_MAXIMUM_CHANNEL_COUNT + 1,
                INTRINSICCV_BORDER_TYPE_REFLECT, context));
  EXPECT_EQ(INTRINSICCV_OK, intrinsiccv_filter_release(context));
}

TYPED_TEST(GaussianBlur, InvalidContextSizeType) {
  intrinsiccv_filter_context_t *context = nullptr;
  ASSERT_EQ(INTRINSICCV_OK,
            intrinsiccv_filter_create(&context, 1, 2 * sizeof(TypeParam) + 1,
                                      intrinsiccv_rectangle_t{1, 1}));
  TypeParam src[1], dst[1];
  EXPECT_EQ(INTRINSICCV_ERROR_CONTEXT_MISMATCH,
            gaussian_blur_3x3<TypeParam>()(
                src, sizeof(TypeParam), dst, sizeof(TypeParam), 1, 1, 1,
                INTRINSICCV_BORDER_TYPE_REFLECT, context));
  EXPECT_EQ(INTRINSICCV_ERROR_CONTEXT_MISMATCH,
            gaussian_blur_5x5<TypeParam>()(
                src, sizeof(TypeParam), dst, sizeof(TypeParam), 1, 1, 1,
                INTRINSICCV_BORDER_TYPE_REFLECT, context));
  EXPECT_EQ(INTRINSICCV_OK, intrinsiccv_filter_release(context));
}

TYPED_TEST(GaussianBlur, InvalidContextChannelNumber) {
  intrinsiccv_filter_context_t *context = nullptr;
  ASSERT_EQ(INTRINSICCV_OK,
            intrinsiccv_filter_create(&context, 2, 2 * sizeof(TypeParam),
                                      intrinsiccv_rectangle_t{1, 1}));
  TypeParam src[1], dst[1];
  EXPECT_EQ(INTRINSICCV_ERROR_CONTEXT_MISMATCH,
            gaussian_blur_3x3<TypeParam>()(
                src, sizeof(TypeParam), dst, sizeof(TypeParam), 1, 1, 1,
                INTRINSICCV_BORDER_TYPE_REFLECT, context));
  EXPECT_EQ(INTRINSICCV_ERROR_CONTEXT_MISMATCH,
            gaussian_blur_5x5<TypeParam>()(
                src, sizeof(TypeParam), dst, sizeof(TypeParam), 1, 1, 1,
                INTRINSICCV_BORDER_TYPE_REFLECT, context));
  EXPECT_EQ(INTRINSICCV_OK, intrinsiccv_filter_release(context));
}

TYPED_TEST(GaussianBlur, InvalidContextImageSize) {
  intrinsiccv_filter_context_t *context = nullptr;
  ASSERT_EQ(INTRINSICCV_OK,
            intrinsiccv_filter_create(&context, 1, 2 * sizeof(TypeParam),
                                      intrinsiccv_rectangle_t{1, 1}));
  TypeParam src[1], dst[1];
  EXPECT_EQ(INTRINSICCV_ERROR_CONTEXT_MISMATCH,
            gaussian_blur_3x3<TypeParam>()(
                src, sizeof(TypeParam), dst, sizeof(TypeParam), 2, 2, 1,
                INTRINSICCV_BORDER_TYPE_REFLECT, context));
  EXPECT_EQ(INTRINSICCV_ERROR_CONTEXT_MISMATCH,
            gaussian_blur_5x5<TypeParam>()(
                src, sizeof(TypeParam), dst, sizeof(TypeParam), 2, 2, 1,
                INTRINSICCV_BORDER_TYPE_REFLECT, context));
  EXPECT_EQ(INTRINSICCV_OK, intrinsiccv_filter_release(context));
}

TEST(FilterCreate, TooBigImage) {
  intrinsiccv_filter_context_t *context = nullptr;
  intrinsiccv_rectangle_t rect{INTRINSICCV_MAX_IMAGE_PIXELS, 1};
  EXPECT_EQ(INTRINSICCV_ERROR_ALLOCATION,
            intrinsiccv_filter_create(&context, 1, 1, rect));
}

TEST(FilterCreate, ImageSize) {
  intrinsiccv_filter_context_t *context = nullptr;

  for (intrinsiccv_rectangle_t rect : {
           intrinsiccv_rectangle_t{INTRINSICCV_MAX_IMAGE_PIXELS + 1, 1},
           intrinsiccv_rectangle_t{INTRINSICCV_MAX_IMAGE_PIXELS,
                                   INTRINSICCV_MAX_IMAGE_PIXELS},
       }) {
    EXPECT_EQ(INTRINSICCV_ERROR_RANGE,
              intrinsiccv_filter_create(&context, 1, 1, rect));
    ASSERT_EQ(nullptr, context);
  }
}

TEST(FilterCreate, TypeSize) {
  intrinsiccv_filter_context_t *context = nullptr;

  EXPECT_EQ(
      INTRINSICCV_ERROR_RANGE,
      intrinsiccv_filter_create(&context, 1, INTRINSICCV_MAXIMUM_TYPE_SIZE + 1,
                                intrinsiccv_rectangle_t{1, 1}));
  ASSERT_EQ(nullptr, context);
}

TEST(FilterCreate, ChannelNumber) {
  intrinsiccv_filter_context_t *context = nullptr;

  EXPECT_EQ(
      INTRINSICCV_ERROR_RANGE,
      intrinsiccv_filter_create(&context, INTRINSICCV_MAXIMUM_CHANNEL_COUNT + 1,
                                1, intrinsiccv_rectangle_t{1, 1}));
  ASSERT_EQ(nullptr, context);
}
