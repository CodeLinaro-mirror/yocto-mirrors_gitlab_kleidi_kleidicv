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

static constexpr std::array<intrinsiccv_border_type_t, 1> kDefaultBorder = {
    INTRINSICCV_BORDER_TYPE_REPLICATE};

static constexpr std::array<intrinsiccv_border_type_t, 1> kReflectBorder = {
    INTRINSICCV_BORDER_TYPE_REFLECT};

static constexpr std::array<intrinsiccv_border_type_t, 4> kAllBorders = {
    INTRINSICCV_BORDER_TYPE_REPLICATE,
    INTRINSICCV_BORDER_TYPE_REFLECT,
    INTRINSICCV_BORDER_TYPE_WRAP,
    INTRINSICCV_BORDER_TYPE_REVERSE,
};

template <typename IterableType>
std::unique_ptr<test::Generator<typename IterableType::value_type>>
make_generator_ptr(IterableType &elements) {
  test::Generator<typename IterableType::value_type> *pg =
      new test::SequenceGenerator(elements);
  return std::unique_ptr<test::Generator<typename IterableType::value_type>>(
      pg);
}

// Test for GaussianBlur operator.
template <class KernelTestParams>
class GaussianBlurTest : public test::KernelTest<KernelTestParams> {
  using Base = test::KernelTest<KernelTestParams>;
  using typename test::KernelTest<KernelTestParams>::InputType;
  using typename test::KernelTest<KernelTestParams>::IntermediateType;
  using typename test::KernelTest<KernelTestParams>::OutputType;

 public:
  GaussianBlurTest()
      : small_array_layouts_{test::small_array_layouts(
            KernelTestParams::kKernelSize, KernelTestParams::kKernelSize)} {
    array_layout_generator_ = make_generator_ptr(small_array_layouts_);
    border_type_generator_ = make_generator_ptr(kDefaultBorder);
  }

  GaussianBlurTest &with_array_layouts(
      std::unique_ptr<test::Generator<test::ArrayLayout>> g) {
    array_layout_generator_ = std::move(g);
    return *this;
  }

  GaussianBlurTest &with_border_types(
      std::unique_ptr<test::Generator<intrinsiccv_border_type_t>> g) {
    border_type_generator_ = std::move(g);
    return *this;
  }

  void test(test::Array2D<IntermediateType> mask) {
    test::Kernel kernel{mask};
    // Use the default border values for testing.
    auto kSupportedBorderValues = test::default_border_values();
    // Create generators and execute test.
    test::SequenceGenerator tested_border_values{kSupportedBorderValues};
    test::PseudoRandomNumberGenerator<InputType> element_generator;
    Base::test(kernel, *array_layout_generator_, *border_type_generator_,
               tested_border_values, element_generator);
  }

 protected:
  std::array<test::ArrayLayout, 6> small_array_layouts_;
  std::unique_ptr<test::Generator<test::ArrayLayout>> array_layout_generator_;
  std::unique_ptr<test::Generator<intrinsiccv_border_type_t>>
      border_type_generator_;

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
};  // end of class class GaussianBlur3x3Test<KernelTestParams>

using ElementTypes = ::testing::Types<uint8_t>;

template <typename ElementType>
class GaussianBlur : public testing::Test {};

TYPED_TEST_SUITE(GaussianBlur, ElementTypes);

// Tests gaussian_blur_3x3_<input_type> API.
TYPED_TEST(GaussianBlur, 3x3Small) {
  using KernelTestParams = GaussianBlurKernelTestParams<TypeParam, 3>;

  // 3x3 GaussianBlur operator.
  test::Array2D<typename KernelTestParams::IntermediateType> mask{3, 3};
  // clang-format off
  mask.set(0, 0, { 1, 2, 1});
  mask.set(1, 0, { 2, 4, 2});
  mask.set(2, 0, { 1, 2, 1});
  // clang-format on
  GaussianBlurTest<KernelTestParams>{}
      .with_border_types(make_generator_ptr(kAllBorders))
      .test(mask);
}

TYPED_TEST(GaussianBlur, 3x3Default) {
  using KernelTestParams = GaussianBlurKernelTestParams<TypeParam, 3>;
  std::array<test::ArrayLayout, 14> medium_array_layouts_3x3 =
      test::default_array_layouts(3, 3);
  // 3x3 GaussianBlur operator.
  test::Array2D<typename KernelTestParams::IntermediateType> mask{3, 3};
  // clang-format off
  mask.set(0, 0, { 1, 2, 1});
  mask.set(1, 0, { 2, 4, 2});
  mask.set(2, 0, { 1, 2, 1});
  // clang-format on
  GaussianBlurTest<KernelTestParams>{}
      .with_array_layouts(make_generator_ptr(medium_array_layouts_3x3))
      .with_border_types(make_generator_ptr(kReflectBorder))
      .test(mask);
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
  GaussianBlurTest<KernelTestParams>{}
      .with_border_types(make_generator_ptr(kAllBorders))
      .test(mask);
}

TYPED_TEST(GaussianBlur, UnsupportedBorderType3x3) {
  using KernelTestParams = GaussianBlurKernelTestParams<TypeParam, 3>;
  intrinsiccv_filter_context_t *context = nullptr;
  size_t validSize = KernelTestParams::kKernelSize - 1;
  ASSERT_EQ(INTRINSICCV_OK, intrinsiccv_filter_create(
                                &context, 1, 2 * sizeof(TypeParam),
                                intrinsiccv_rectangle_t{validSize, validSize}));
  TypeParam src[1] = {}, dst[1];
  for (intrinsiccv_border_type_t border : {
           INTRINSICCV_BORDER_TYPE_CONSTANT,
           INTRINSICCV_BORDER_TYPE_TRANSPARENT,
           INTRINSICCV_BORDER_TYPE_NONE,
       }) {
    EXPECT_EQ(INTRINSICCV_ERROR_NOT_IMPLEMENTED,
              gaussian_blur_3x3<TypeParam>()(src, sizeof(TypeParam), dst,
                                             sizeof(TypeParam), validSize,
                                             validSize, 1, border, context));
  }
  EXPECT_EQ(INTRINSICCV_OK, intrinsiccv_filter_release(context));
}

TYPED_TEST(GaussianBlur, UnsupportedBorderType5x5) {
  using KernelTestParams = GaussianBlurKernelTestParams<TypeParam, 5>;
  intrinsiccv_filter_context_t *context = nullptr;
  size_t validSize = KernelTestParams::kKernelSize - 1;
  ASSERT_EQ(INTRINSICCV_OK, intrinsiccv_filter_create(
                                &context, 1, 2 * sizeof(TypeParam),
                                intrinsiccv_rectangle_t{validSize, validSize}));
  TypeParam src[1] = {}, dst[1];
  for (intrinsiccv_border_type_t border : {
           INTRINSICCV_BORDER_TYPE_CONSTANT,
           INTRINSICCV_BORDER_TYPE_TRANSPARENT,
           INTRINSICCV_BORDER_TYPE_NONE,
       }) {
    EXPECT_EQ(INTRINSICCV_ERROR_NOT_IMPLEMENTED,
              gaussian_blur_5x5<TypeParam>()(src, sizeof(TypeParam), dst,
                                             sizeof(TypeParam), validSize,
                                             validSize, 1, border, context));
  }
  EXPECT_EQ(INTRINSICCV_OK, intrinsiccv_filter_release(context));
}

TYPED_TEST(GaussianBlur, NullPointer) {
  using KernelTestParams3x3 = GaussianBlurKernelTestParams<TypeParam, 3>;
  using KernelTestParams5x5 = GaussianBlurKernelTestParams<TypeParam, 5>;
  intrinsiccv_filter_context_t *context = nullptr;
  size_t validSize = KernelTestParams3x3::kKernelSize - 1;
  ASSERT_EQ(INTRINSICCV_OK, intrinsiccv_filter_create(
                                &context, 1, 2 * sizeof(TypeParam),
                                intrinsiccv_rectangle_t{validSize, validSize}));
  TypeParam src[1] = {}, dst[1];
  test::test_null_args(gaussian_blur_3x3<TypeParam>(), src, sizeof(TypeParam),
                       dst, sizeof(TypeParam), validSize, validSize, 1,
                       INTRINSICCV_BORDER_TYPE_REPLICATE, context);
  validSize = KernelTestParams5x5::kKernelSize - 1;
  test::test_null_args(gaussian_blur_5x5<TypeParam>(), src, sizeof(TypeParam),
                       dst, sizeof(TypeParam), validSize, validSize, 1,
                       INTRINSICCV_BORDER_TYPE_REPLICATE, context);
  EXPECT_EQ(INTRINSICCV_OK, intrinsiccv_filter_release(context));
}

TYPED_TEST(GaussianBlur, Misalignment) {
  if (sizeof(TypeParam) == 1) {
    // misalignment impossible
    return;
  }
  using KernelTestParams3x3 = GaussianBlurKernelTestParams<TypeParam, 3>;
  using KernelTestParams5x5 = GaussianBlurKernelTestParams<TypeParam, 5>;
  intrinsiccv_filter_context_t *context = nullptr;
  size_t validSize = KernelTestParams3x3::kKernelSize - 1;
  ASSERT_EQ(INTRINSICCV_OK, intrinsiccv_filter_create(
                                &context, 1, 2 * sizeof(TypeParam),
                                intrinsiccv_rectangle_t{validSize, validSize}));
  TypeParam src[1] = {}, dst[1];
  EXPECT_EQ(INTRINSICCV_ERROR_ALIGNMENT,
            gaussian_blur_3x3<TypeParam>()(
                src, sizeof(TypeParam) + 1, dst, sizeof(TypeParam), validSize,
                validSize, 1, INTRINSICCV_BORDER_TYPE_REPLICATE, context));
  EXPECT_EQ(INTRINSICCV_ERROR_ALIGNMENT,
            gaussian_blur_3x3<TypeParam>()(
                src, sizeof(TypeParam), dst, sizeof(TypeParam) + 1, validSize,
                validSize, 1, INTRINSICCV_BORDER_TYPE_REPLICATE, context));
  validSize = KernelTestParams5x5::kKernelSize - 1;
  EXPECT_EQ(INTRINSICCV_ERROR_ALIGNMENT,
            gaussian_blur_5x5<TypeParam>()(
                src, sizeof(TypeParam) + 1, dst, sizeof(TypeParam), validSize,
                validSize, 1, INTRINSICCV_BORDER_TYPE_REPLICATE, context));
  EXPECT_EQ(INTRINSICCV_ERROR_ALIGNMENT,
            gaussian_blur_5x5<TypeParam>()(
                src, sizeof(TypeParam), dst, sizeof(TypeParam) + 1, validSize,
                validSize, 1, INTRINSICCV_BORDER_TYPE_REPLICATE, context));
  EXPECT_EQ(INTRINSICCV_OK, intrinsiccv_filter_release(context));
}

TYPED_TEST(GaussianBlur, ZeroImageSize3x3) {
  TypeParam src[1] = {}, dst[1];
  intrinsiccv_filter_context_t *context = nullptr;
  ASSERT_EQ(INTRINSICCV_OK,
            intrinsiccv_filter_create(&context, 1, 2 * sizeof(TypeParam),
                                      intrinsiccv_rectangle_t{0, 1}));
  EXPECT_EQ(INTRINSICCV_ERROR_NOT_IMPLEMENTED,
            gaussian_blur_3x3<TypeParam>()(
                src, sizeof(TypeParam), dst, sizeof(TypeParam), 0, 1, 1,
                INTRINSICCV_BORDER_TYPE_REFLECT, context));
  EXPECT_EQ(INTRINSICCV_OK, intrinsiccv_filter_release(context));

  ASSERT_EQ(INTRINSICCV_OK,
            intrinsiccv_filter_create(&context, 1, 2 * sizeof(TypeParam),
                                      intrinsiccv_rectangle_t{1, 0}));
  EXPECT_EQ(INTRINSICCV_ERROR_NOT_IMPLEMENTED,
            gaussian_blur_3x3<TypeParam>()(
                src, sizeof(TypeParam), dst, sizeof(TypeParam), 1, 0, 1,
                INTRINSICCV_BORDER_TYPE_REFLECT, context));
  EXPECT_EQ(INTRINSICCV_OK, intrinsiccv_filter_release(context));
}

TYPED_TEST(GaussianBlur, ZeroImageSize5x5) {
  TypeParam src[1] = {}, dst[1];
  intrinsiccv_filter_context_t *context = nullptr;
  ASSERT_EQ(INTRINSICCV_OK,
            intrinsiccv_filter_create(&context, 1, 2 * sizeof(TypeParam),
                                      intrinsiccv_rectangle_t{0, 1}));
  EXPECT_EQ(INTRINSICCV_ERROR_NOT_IMPLEMENTED,
            gaussian_blur_5x5<TypeParam>()(
                src, sizeof(TypeParam), dst, sizeof(TypeParam), 0, 1, 1,
                INTRINSICCV_BORDER_TYPE_REFLECT, context));
  EXPECT_EQ(INTRINSICCV_OK, intrinsiccv_filter_release(context));

  ASSERT_EQ(INTRINSICCV_OK,
            intrinsiccv_filter_create(&context, 1, 2 * sizeof(TypeParam),
                                      intrinsiccv_rectangle_t{1, 0}));
  EXPECT_EQ(INTRINSICCV_ERROR_NOT_IMPLEMENTED,
            gaussian_blur_5x5<TypeParam>()(
                src, sizeof(TypeParam), dst, sizeof(TypeParam), 1, 0, 1,
                INTRINSICCV_BORDER_TYPE_REFLECT, context));
  EXPECT_EQ(INTRINSICCV_OK, intrinsiccv_filter_release(context));
}

TYPED_TEST(GaussianBlur, ValidImageSize3x3) {
  using KernelTestParams = GaussianBlurKernelTestParams<TypeParam, 3>;
  intrinsiccv_filter_context_t *context = nullptr;
  size_t validSize = KernelTestParams::kKernelSize - 1;
  ASSERT_EQ(INTRINSICCV_OK, intrinsiccv_filter_create(
                                &context, 1, 2 * sizeof(TypeParam),
                                intrinsiccv_rectangle_t{validSize, validSize}));
  test::Array2D<TypeParam> src{validSize, validSize,
                               test::Options::vector_length()};
  src.set(0, 0, {1, 2});
  src.set(1, 0, {1, 2});

  test::Array2D<TypeParam> dst{validSize, validSize,
                               test::Options::vector_length()};
  EXPECT_EQ(INTRINSICCV_OK,
            gaussian_blur_3x3<TypeParam>()(
                src.data(), src.stride(), dst.data(), dst.stride(), validSize,
                validSize, 1, INTRINSICCV_BORDER_TYPE_REVERSE, context));
  EXPECT_EQ(INTRINSICCV_OK, intrinsiccv_filter_release(context));
}

TYPED_TEST(GaussianBlur, ValidImageSize5x5) {
  using KernelTestParams = GaussianBlurKernelTestParams<TypeParam, 5>;
  intrinsiccv_filter_context_t *context = nullptr;
  size_t validSize = KernelTestParams::kKernelSize - 1;
  ASSERT_EQ(INTRINSICCV_OK, intrinsiccv_filter_create(
                                &context, 1, 2 * sizeof(TypeParam),
                                intrinsiccv_rectangle_t{validSize, validSize}));
  test::Array2D<TypeParam> src{validSize, validSize,
                               test::Options::vector_length()};
  src.set(0, 0, {1, 2, 3, 4});
  src.set(1, 0, {1, 2, 3, 4});
  src.set(2, 0, {1, 2, 3, 4});
  src.set(3, 0, {1, 2, 3, 4});

  test::Array2D<TypeParam> dst{validSize, validSize,
                               test::Options::vector_length()};
  EXPECT_EQ(INTRINSICCV_OK,
            gaussian_blur_5x5<TypeParam>()(
                src.data(), src.stride(), dst.data(), dst.stride(), validSize,
                validSize, 1, INTRINSICCV_BORDER_TYPE_REVERSE, context));
  EXPECT_EQ(INTRINSICCV_OK, intrinsiccv_filter_release(context));
}

TYPED_TEST(GaussianBlur, UndersizeImage3x3) {
  using KernelTestParams = GaussianBlurKernelTestParams<TypeParam, 3>;
  intrinsiccv_filter_context_t *context = nullptr;
  size_t underSize = KernelTestParams::kKernelSize - 2;
  size_t validWidth = KernelTestParams::kKernelSize + 10;
  size_t validHeight = KernelTestParams::kKernelSize + 5;
  TypeParam src[1], dst[1];
  ASSERT_EQ(INTRINSICCV_OK, intrinsiccv_filter_create(
                                &context, 1, 2 * sizeof(TypeParam),
                                intrinsiccv_rectangle_t{underSize, underSize}));
  EXPECT_EQ(INTRINSICCV_ERROR_NOT_IMPLEMENTED,
            gaussian_blur_3x3<TypeParam>()(
                src, sizeof(TypeParam), dst, sizeof(TypeParam), underSize,
                underSize, 1, INTRINSICCV_BORDER_TYPE_REPLICATE, context));
  EXPECT_EQ(INTRINSICCV_OK, intrinsiccv_filter_release(context));
  ASSERT_EQ(INTRINSICCV_OK,
            intrinsiccv_filter_create(
                &context, 1, 2 * sizeof(TypeParam),
                intrinsiccv_rectangle_t{underSize, validHeight}));
  EXPECT_EQ(INTRINSICCV_ERROR_NOT_IMPLEMENTED,
            gaussian_blur_3x3<TypeParam>()(
                src, sizeof(TypeParam), dst, sizeof(TypeParam), underSize,
                validHeight, 1, INTRINSICCV_BORDER_TYPE_REPLICATE, context));
  EXPECT_EQ(INTRINSICCV_OK, intrinsiccv_filter_release(context));
  ASSERT_EQ(INTRINSICCV_OK,
            intrinsiccv_filter_create(
                &context, 1, 2 * sizeof(TypeParam),
                intrinsiccv_rectangle_t{validWidth, underSize}));
  EXPECT_EQ(INTRINSICCV_ERROR_NOT_IMPLEMENTED,
            gaussian_blur_3x3<TypeParam>()(
                src, sizeof(TypeParam), dst, sizeof(TypeParam), validWidth,
                underSize, 1, INTRINSICCV_BORDER_TYPE_REPLICATE, context));
  EXPECT_EQ(INTRINSICCV_OK, intrinsiccv_filter_release(context));
}

TYPED_TEST(GaussianBlur, UndersizeImage5x5) {
  using KernelTestParams = GaussianBlurKernelTestParams<TypeParam, 5>;
  intrinsiccv_filter_context_t *context = nullptr;
  size_t underSize = KernelTestParams::kKernelSize - 2;
  size_t width = KernelTestParams::kKernelSize + 8;
  size_t height = KernelTestParams::kKernelSize + 3;

  ASSERT_EQ(INTRINSICCV_OK, intrinsiccv_filter_create(
                                &context, 1, 2 * sizeof(TypeParam),
                                intrinsiccv_rectangle_t{underSize, underSize}));
  TypeParam src[1] = {}, dst[1];
  EXPECT_EQ(INTRINSICCV_ERROR_NOT_IMPLEMENTED,
            gaussian_blur_5x5<TypeParam>()(
                src, sizeof(TypeParam), dst, sizeof(TypeParam), underSize,
                underSize, 1, INTRINSICCV_BORDER_TYPE_REPLICATE, context));
  EXPECT_EQ(INTRINSICCV_OK, intrinsiccv_filter_release(context));
  ASSERT_EQ(INTRINSICCV_OK, intrinsiccv_filter_create(
                                &context, 1, 2 * sizeof(TypeParam),
                                intrinsiccv_rectangle_t{underSize, height}));
  EXPECT_EQ(INTRINSICCV_ERROR_NOT_IMPLEMENTED,
            gaussian_blur_5x5<TypeParam>()(
                src, sizeof(TypeParam), dst, sizeof(TypeParam), underSize,
                height, 1, INTRINSICCV_BORDER_TYPE_REPLICATE, context));
  EXPECT_EQ(INTRINSICCV_OK, intrinsiccv_filter_release(context));
  ASSERT_EQ(INTRINSICCV_OK, intrinsiccv_filter_create(
                                &context, 1, 2 * sizeof(TypeParam),
                                intrinsiccv_rectangle_t{width, underSize}));
  EXPECT_EQ(INTRINSICCV_ERROR_NOT_IMPLEMENTED,
            gaussian_blur_5x5<TypeParam>()(
                src, sizeof(TypeParam), dst, sizeof(TypeParam), width,
                underSize, 1, INTRINSICCV_BORDER_TYPE_REPLICATE, context));
  EXPECT_EQ(INTRINSICCV_OK, intrinsiccv_filter_release(context));
}

TYPED_TEST(GaussianBlur, OversizeImage) {
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
  using KernelTestParams3x3 = GaussianBlurKernelTestParams<TypeParam, 3>;
  using KernelTestParams5x5 = GaussianBlurKernelTestParams<TypeParam, 5>;
  intrinsiccv_filter_context_t *context = nullptr;
  size_t validSize = KernelTestParams3x3::kKernelSize - 1;

  ASSERT_EQ(INTRINSICCV_OK, intrinsiccv_filter_create(
                                &context, 1, 2 * sizeof(TypeParam),
                                intrinsiccv_rectangle_t{validSize, validSize}));
  TypeParam src[1], dst[1];
  EXPECT_EQ(INTRINSICCV_ERROR_RANGE,
            gaussian_blur_3x3<TypeParam>()(
                src, sizeof(TypeParam), dst, sizeof(TypeParam), validSize,
                validSize, INTRINSICCV_MAXIMUM_CHANNEL_COUNT + 1,
                INTRINSICCV_BORDER_TYPE_REFLECT, context));

  validSize = KernelTestParams5x5::kKernelSize - 1;
  EXPECT_EQ(INTRINSICCV_ERROR_RANGE,
            gaussian_blur_5x5<TypeParam>()(
                src, sizeof(TypeParam), dst, sizeof(TypeParam), validSize,
                validSize, INTRINSICCV_MAXIMUM_CHANNEL_COUNT + 1,
                INTRINSICCV_BORDER_TYPE_REFLECT, context));
  EXPECT_EQ(INTRINSICCV_OK, intrinsiccv_filter_release(context));
}

TYPED_TEST(GaussianBlur, InvalidContextSizeType) {
  using KernelTestParams3x3 = GaussianBlurKernelTestParams<TypeParam, 3>;
  using KernelTestParams5x5 = GaussianBlurKernelTestParams<TypeParam, 5>;
  intrinsiccv_filter_context_t *context = nullptr;
  size_t validSize = KernelTestParams3x3::kKernelSize - 1;

  ASSERT_EQ(INTRINSICCV_OK, intrinsiccv_filter_create(
                                &context, 1, 2 * sizeof(TypeParam) + 1,
                                intrinsiccv_rectangle_t{validSize, validSize}));
  TypeParam src[1], dst[1];
  EXPECT_EQ(INTRINSICCV_ERROR_CONTEXT_MISMATCH,
            gaussian_blur_3x3<TypeParam>()(
                src, sizeof(TypeParam), dst, sizeof(TypeParam), validSize,
                validSize, 1, INTRINSICCV_BORDER_TYPE_REFLECT, context));
  validSize = KernelTestParams5x5::kKernelSize - 1;
  EXPECT_EQ(INTRINSICCV_ERROR_CONTEXT_MISMATCH,
            gaussian_blur_5x5<TypeParam>()(
                src, sizeof(TypeParam), dst, sizeof(TypeParam), validSize,
                validSize, 1, INTRINSICCV_BORDER_TYPE_REFLECT, context));
  EXPECT_EQ(INTRINSICCV_OK, intrinsiccv_filter_release(context));
}

TYPED_TEST(GaussianBlur, InvalidContextChannelNumber) {
  using KernelTestParams3x3 = GaussianBlurKernelTestParams<TypeParam, 3>;
  using KernelTestParams5x5 = GaussianBlurKernelTestParams<TypeParam, 5>;
  intrinsiccv_filter_context_t *context = nullptr;
  size_t validSize = KernelTestParams3x3::kKernelSize - 1;

  ASSERT_EQ(INTRINSICCV_OK, intrinsiccv_filter_create(
                                &context, 2, 2 * sizeof(TypeParam),
                                intrinsiccv_rectangle_t{validSize, validSize}));
  TypeParam src[1], dst[1];
  EXPECT_EQ(INTRINSICCV_ERROR_CONTEXT_MISMATCH,
            gaussian_blur_3x3<TypeParam>()(
                src, sizeof(TypeParam), dst, sizeof(TypeParam), validSize,
                validSize, 1, INTRINSICCV_BORDER_TYPE_REFLECT, context));

  validSize = KernelTestParams5x5::kKernelSize - 1;
  EXPECT_EQ(INTRINSICCV_ERROR_CONTEXT_MISMATCH,
            gaussian_blur_5x5<TypeParam>()(
                src, sizeof(TypeParam), dst, sizeof(TypeParam), validSize,
                validSize, 1, INTRINSICCV_BORDER_TYPE_REFLECT, context));
  EXPECT_EQ(INTRINSICCV_OK, intrinsiccv_filter_release(context));
}

TYPED_TEST(GaussianBlur, InvalidContextImageSize) {
  using KernelTestParams3x3 = GaussianBlurKernelTestParams<TypeParam, 3>;
  using KernelTestParams5x5 = GaussianBlurKernelTestParams<TypeParam, 5>;
  intrinsiccv_filter_context_t *context = nullptr;
  size_t validSize = KernelTestParams3x3::kKernelSize - 1;

  ASSERT_EQ(INTRINSICCV_OK, intrinsiccv_filter_create(
                                &context, 1, 2 * sizeof(TypeParam),
                                intrinsiccv_rectangle_t{validSize, validSize}));
  TypeParam src[1], dst[1];
  EXPECT_EQ(INTRINSICCV_ERROR_CONTEXT_MISMATCH,
            gaussian_blur_3x3<TypeParam>()(
                src, sizeof(TypeParam), dst, sizeof(TypeParam), validSize + 1,
                validSize + 1, 1, INTRINSICCV_BORDER_TYPE_REFLECT, context));

  validSize = KernelTestParams5x5::kKernelSize - 1;
  EXPECT_EQ(INTRINSICCV_ERROR_CONTEXT_MISMATCH,
            gaussian_blur_5x5<TypeParam>()(
                src, sizeof(TypeParam), dst, sizeof(TypeParam), validSize + 1,
                validSize + 1, 1, INTRINSICCV_BORDER_TYPE_REFLECT, context));
  EXPECT_EQ(INTRINSICCV_OK, intrinsiccv_filter_release(context));
}

TEST(FilterCreate, CannotAllocateFilter) {
  MockMallocToFail::enable();
  intrinsiccv_filter_context_t *context = nullptr;
  intrinsiccv_rectangle_t rect{INTRINSICCV_MAX_IMAGE_PIXELS, 1};
  EXPECT_EQ(INTRINSICCV_ERROR_ALLOCATION,
            intrinsiccv_filter_create(&context, 1, 1, rect));
  MockMallocToFail::disable();
}

TEST(FilterCreate, OversizeImage) {
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

TEST(FilterCreate, NullPointer) {
  EXPECT_EQ(
      INTRINSICCV_ERROR_NULL_POINTER,
      intrinsiccv_filter_create(nullptr, 1, 1, intrinsiccv_rectangle_t{1, 1}));
}
TEST(FilterRelease, NullPointer) {
  EXPECT_EQ(INTRINSICCV_ERROR_NULL_POINTER,
            intrinsiccv_filter_release(nullptr));
}
