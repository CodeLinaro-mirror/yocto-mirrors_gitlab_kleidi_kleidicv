// SPDX-FileCopyrightText: 2024 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#include <gtest/gtest.h>

#include "framework/array.h"
#include "framework/generator.h"
#include "framework/kernel.h"
#include "framework/utils.h"
#include "kleidicv/kleidicv.h"
#include "test_config.h"

#define KLEIDICV_GAUSSIAN_BLUR(type, kernel_suffix, type_suffix) \
  KLEIDICV_API(gaussian_blur_##kernel_suffix,                    \
               kleidicv_gaussian_blur_##kernel_suffix##_##type_suffix, type)

KLEIDICV_GAUSSIAN_BLUR(uint8_t, 3x3, u8);
KLEIDICV_GAUSSIAN_BLUR(uint8_t, 5x5, u8);
KLEIDICV_GAUSSIAN_BLUR(uint8_t, 7x7, u8);
KLEIDICV_GAUSSIAN_BLUR(uint8_t, 15x15, u8);

// Implements KernelTestParams for Gaussian Blur operators.
template <typename ElementType, size_t KernelSize>
struct GaussianBlurKernelTestParams;

template <size_t KernelSize>
struct GaussianBlurKernelTestParams<uint8_t, KernelSize> {
  using InputType = uint8_t;
  using IntermediateType = uint64_t;
  using OutputType = uint8_t;

  static constexpr size_t kKernelSize = KernelSize;
};  // end of struct GaussianBlurKernelTestParams<uint8_t, KernelSize>

static constexpr std::array<kleidicv_border_type_t, 1> kDefaultBorder = {
    KLEIDICV_BORDER_TYPE_REPLICATE};

static constexpr std::array<kleidicv_border_type_t, 1> kReflectBorder = {
    KLEIDICV_BORDER_TYPE_REFLECT};

static constexpr std::array<kleidicv_border_type_t, 4> kAllBorders = {
    KLEIDICV_BORDER_TYPE_REPLICATE,
    KLEIDICV_BORDER_TYPE_REFLECT,
    KLEIDICV_BORDER_TYPE_WRAP,
    KLEIDICV_BORDER_TYPE_REVERSE,
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
      std::unique_ptr<test::Generator<kleidicv_border_type_t>> g) {
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
  std::unique_ptr<test::Generator<kleidicv_border_type_t>>
      border_type_generator_;

  kleidicv_error_t call_api(const test::Array2D<InputType> *input,
                            test::Array2D<OutputType> *output,
                            kleidicv_border_type_t border_type,
                            kleidicv_border_values_t) override {
    // NOLINTBEGIN(readability-avoid-nested-conditional-operator)
    auto api =
        KernelTestParams::kKernelSize == 3   ? gaussian_blur_3x3<InputType>()
        : KernelTestParams::kKernelSize == 5 ? gaussian_blur_5x5<InputType>()
        : KernelTestParams::kKernelSize == 7 ? gaussian_blur_7x7<InputType>()
                                             : gaussian_blur_15x15<InputType>();
    // NOLINTEND(readability-avoid-nested-conditional-operator)

    size_t intermediate_size = 2 * sizeof(InputType);
    if constexpr (KernelTestParams::kKernelSize == 15) {
      intermediate_size = 4 * sizeof(InputType);
    }

    kleidicv_filter_context_t *context = nullptr;
    auto ret = kleidicv_filter_create(
        &context, input->channels(), intermediate_size,
        kleidicv_rectangle_t{input->width() / input->channels(),
                             input->height()});
    if (ret != KLEIDICV_OK) {
      return ret;
    }

    ret = api(input->data(), input->stride(), output->data(), output->stride(),
              input->width() / input->channels(), input->height(),
              input->channels(), border_type, context);
    auto releaseRet = kleidicv_filter_release(context);
    if (releaseRet != KLEIDICV_OK) {
      return releaseRet;
    }

    return ret;
  }

  // Apply rounding to nearest integer division.
  IntermediateType scale_result(const test::Kernel<IntermediateType> &kernel,
                                IntermediateType result) override {
    // NOLINTBEGIN(readability-avoid-nested-conditional-operator)
    return kernel.width() == 3   ? ((result + 8) / 16)
           : kernel.width() == 5 ? ((result + 128) / 256)
           : kernel.width() == 7 ? ((result + 2048) / 4096)
                                 : ((result + 524288) / 1048576);
    // NOLINTEND(readability-avoid-nested-conditional-operator)
  }
};  // end of class GaussianBlurTest<KernelTestParams>

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

// Tests gaussian_blur_7x7_<input_type> API.
TYPED_TEST(GaussianBlur, 7x7) {
  using KernelTestParams = GaussianBlurKernelTestParams<TypeParam, 7>;
  // 7x7 GaussianBlur operator.
  test::Array2D<typename KernelTestParams::IntermediateType> mask{7, 7};
  // clang-format off
  mask.set(0, 0, {  4,  14,  28,  36,  28,  14,  4 });
  mask.set(1, 0, { 14,  49,  98, 126,  98,  49, 14 });
  mask.set(2, 0, { 28,  98, 196, 252, 196,  98, 28 });
  mask.set(3, 0, { 36, 126, 252, 324, 252, 126, 36 });
  mask.set(4, 0, { 28,  98, 196, 252, 196,  98, 28 });
  mask.set(5, 0, { 14,  49,  98, 126,  98,  49, 14 });
  mask.set(6, 0, {  4,  14,  28,  36,  28,  14,  4 });
  // clang-format on
  GaussianBlurTest<KernelTestParams>{}
      .with_border_types(make_generator_ptr(kAllBorders))
      .test(mask);
}

// Tests gaussian_blur_15x15_<input_type> API.
TYPED_TEST(GaussianBlur, 15x15) {
  using KernelTestParams = GaussianBlurKernelTestParams<TypeParam, 15>;
  // 15x15 GaussianBlur operator.
  test::Array2D<typename KernelTestParams::IntermediateType> mask{15, 15};
  // clang-format off
  mask.set(0, 0,  {  16,   44,  100,  192,   324,   472,   584,   632,   584,   472,   324,  192,  100,   44,  16 });
  mask.set(1, 0,  {  44,  121,  275,  528,   891,  1298,  1606,  1738,  1606,  1298,   891,  528,  275,  121,  44 });
  mask.set(2, 0,  { 100,  275,  625, 1200,  2025,  2950,  3650,  3950,  3650,  2950,  2025, 1200,  625,  275, 100 });
  mask.set(3, 0,  { 192,  528, 1200, 2304,  3888,  5664,  7008,  7584,  7008,  5664,  3888, 2304, 1200,  528, 192 });
  mask.set(4, 0,  { 324,  891, 2025, 3888,  6561,  9558, 11826, 12798, 11826,  9558,  6561, 3888, 2025,  891, 324 });
  mask.set(5, 0,  { 472, 1298, 2950, 5664,  9558, 13924, 17228, 18644, 17228, 13924,  9558, 5664, 2950, 1298, 472 });
  mask.set(6, 0,  { 584, 1606, 3650, 7008, 11826, 17228, 21316, 23068, 21316, 17228, 11826, 7008, 3650, 1606, 584 });
  mask.set(7, 0,  { 632, 1738, 3950, 7584, 12798, 18644, 23068, 24964, 23068, 18644, 12798, 7584, 3950, 1738, 632 });
  mask.set(8, 0,  { 584, 1606, 3650, 7008, 11826, 17228, 21316, 23068, 21316, 17228, 11826, 7008, 3650, 1606, 584 });
  mask.set(9, 0,  { 472, 1298, 2950, 5664,  9558, 13924, 17228, 18644, 17228, 13924,  9558, 5664, 2950, 1298, 472 });
  mask.set(10, 0, { 324,  891, 2025, 3888,  6561,  9558, 11826, 12798, 11826,  9558,  6561, 3888, 2025,  891, 324 });
  mask.set(11, 0, { 192,  528, 1200, 2304,  3888,  5664,  7008,  7584,  7008,  5664,  3888, 2304, 1200,  528, 192 });
  mask.set(12, 0, { 100,  275,  625, 1200,  2025,  2950,  3650,  3950,  3650,  2950,  2025, 1200,  625,  275, 100 });
  mask.set(13, 0, {  44,  121,  275,  528,   891,  1298,  1606,  1738,  1606,  1298,   891,  528,  275,  121,  44 });
  mask.set(14, 0, {  16,   44,  100,  192,   324,   472,   584,   632,   584,   472,   324,  192,  100,   44,  16 });
  // clang-format on
  GaussianBlurTest<KernelTestParams>{}
      .with_border_types(make_generator_ptr(kAllBorders))
      .test(mask);
}

TYPED_TEST(GaussianBlur, UnsupportedBorderType3x3) {
  using KernelTestParams = GaussianBlurKernelTestParams<TypeParam, 3>;
  kleidicv_filter_context_t *context = nullptr;
  size_t validSize = KernelTestParams::kKernelSize - 1;
  ASSERT_EQ(KLEIDICV_OK,
            kleidicv_filter_create(&context, 1, 2 * sizeof(TypeParam),
                                   kleidicv_rectangle_t{validSize, validSize}));
  TypeParam src[1] = {}, dst[1];
  for (kleidicv_border_type_t border : {
           KLEIDICV_BORDER_TYPE_CONSTANT,
           KLEIDICV_BORDER_TYPE_TRANSPARENT,
           KLEIDICV_BORDER_TYPE_NONE,
       }) {
    EXPECT_EQ(KLEIDICV_ERROR_NOT_IMPLEMENTED,
              gaussian_blur_3x3<TypeParam>()(src, sizeof(TypeParam), dst,
                                             sizeof(TypeParam), validSize,
                                             validSize, 1, border, context));
  }
  EXPECT_EQ(KLEIDICV_OK, kleidicv_filter_release(context));
}

TYPED_TEST(GaussianBlur, UnsupportedBorderType5x5) {
  using KernelTestParams = GaussianBlurKernelTestParams<TypeParam, 5>;
  kleidicv_filter_context_t *context = nullptr;
  size_t validSize = KernelTestParams::kKernelSize - 1;
  ASSERT_EQ(KLEIDICV_OK,
            kleidicv_filter_create(&context, 1, 2 * sizeof(TypeParam),
                                   kleidicv_rectangle_t{validSize, validSize}));
  TypeParam src[1] = {}, dst[1];
  for (kleidicv_border_type_t border : {
           KLEIDICV_BORDER_TYPE_CONSTANT,
           KLEIDICV_BORDER_TYPE_TRANSPARENT,
           KLEIDICV_BORDER_TYPE_NONE,
       }) {
    EXPECT_EQ(KLEIDICV_ERROR_NOT_IMPLEMENTED,
              gaussian_blur_5x5<TypeParam>()(src, sizeof(TypeParam), dst,
                                             sizeof(TypeParam), validSize,
                                             validSize, 1, border, context));
  }
  EXPECT_EQ(KLEIDICV_OK, kleidicv_filter_release(context));
}

TYPED_TEST(GaussianBlur, UnsupportedBorderType7x7) {
  using KernelTestParams = GaussianBlurKernelTestParams<TypeParam, 7>;
  kleidicv_filter_context_t *context = nullptr;
  size_t validSize = KernelTestParams::kKernelSize - 1;
  ASSERT_EQ(KLEIDICV_OK,
            kleidicv_filter_create(&context, 1, 2 * sizeof(TypeParam),
                                   kleidicv_rectangle_t{validSize, validSize}));
  TypeParam src[1] = {}, dst[1];
  for (kleidicv_border_type_t border : {
           KLEIDICV_BORDER_TYPE_CONSTANT,
           KLEIDICV_BORDER_TYPE_TRANSPARENT,
           KLEIDICV_BORDER_TYPE_NONE,
       }) {
    EXPECT_EQ(KLEIDICV_ERROR_NOT_IMPLEMENTED,
              gaussian_blur_7x7<TypeParam>()(src, sizeof(TypeParam), dst,
                                             sizeof(TypeParam), validSize,
                                             validSize, 1, border, context));
  }
  EXPECT_EQ(KLEIDICV_OK, kleidicv_filter_release(context));
}

TYPED_TEST(GaussianBlur, UnsupportedBorderType15x15) {
  using KernelTestParams = GaussianBlurKernelTestParams<TypeParam, 15>;
  kleidicv_filter_context_t *context = nullptr;
  size_t validSize = KernelTestParams::kKernelSize - 1;
  ASSERT_EQ(KLEIDICV_OK,
            kleidicv_filter_create(&context, 1, 4 * sizeof(TypeParam),
                                   kleidicv_rectangle_t{validSize, validSize}));
  TypeParam src[1] = {}, dst[1];
  for (kleidicv_border_type_t border : {
           KLEIDICV_BORDER_TYPE_CONSTANT,
           KLEIDICV_BORDER_TYPE_TRANSPARENT,
           KLEIDICV_BORDER_TYPE_NONE,
       }) {
    EXPECT_EQ(KLEIDICV_ERROR_NOT_IMPLEMENTED,
              gaussian_blur_15x15<TypeParam>()(src, sizeof(TypeParam), dst,
                                               sizeof(TypeParam), validSize,
                                               validSize, 1, border, context));
  }
  EXPECT_EQ(KLEIDICV_OK, kleidicv_filter_release(context));
}

TYPED_TEST(GaussianBlur, NullPointer) {
  using KernelTestParams3x3 = GaussianBlurKernelTestParams<TypeParam, 3>;
  using KernelTestParams5x5 = GaussianBlurKernelTestParams<TypeParam, 5>;
  using KernelTestParams7x7 = GaussianBlurKernelTestParams<TypeParam, 7>;
  using KernelTestParams15x15 = GaussianBlurKernelTestParams<TypeParam, 15>;
  kleidicv_filter_context_t *context = nullptr;
  size_t validSize = KernelTestParams3x3::kKernelSize - 1;
  ASSERT_EQ(KLEIDICV_OK,
            kleidicv_filter_create(&context, 1, 2 * sizeof(TypeParam),
                                   kleidicv_rectangle_t{validSize, validSize}));
  TypeParam src[1] = {}, dst[1];
  test::test_null_args(gaussian_blur_3x3<TypeParam>(), src, sizeof(TypeParam),
                       dst, sizeof(TypeParam), validSize, validSize, 1,
                       KLEIDICV_BORDER_TYPE_REPLICATE, context);
  validSize = KernelTestParams5x5::kKernelSize - 1;
  test::test_null_args(gaussian_blur_5x5<TypeParam>(), src, sizeof(TypeParam),
                       dst, sizeof(TypeParam), validSize, validSize, 1,
                       KLEIDICV_BORDER_TYPE_REPLICATE, context);
  validSize = KernelTestParams7x7::kKernelSize - 1;
  test::test_null_args(gaussian_blur_7x7<TypeParam>(), src, sizeof(TypeParam),
                       dst, sizeof(TypeParam), validSize, validSize, 1,
                       KLEIDICV_BORDER_TYPE_REPLICATE, context);
  validSize = KernelTestParams15x15::kKernelSize - 1;
  test::test_null_args(gaussian_blur_15x15<TypeParam>(), src, sizeof(TypeParam),
                       dst, sizeof(TypeParam), validSize, validSize, 1,
                       KLEIDICV_BORDER_TYPE_REPLICATE, context);
  EXPECT_EQ(KLEIDICV_OK, kleidicv_filter_release(context));
}

TYPED_TEST(GaussianBlur, Misalignment) {
  if (sizeof(TypeParam) == 1) {
    // misalignment impossible
    return;
  }
  using KernelTestParams3x3 = GaussianBlurKernelTestParams<TypeParam, 3>;
  using KernelTestParams5x5 = GaussianBlurKernelTestParams<TypeParam, 5>;
  using KernelTestParams7x7 = GaussianBlurKernelTestParams<TypeParam, 7>;
  using KernelTestParams15x15 = GaussianBlurKernelTestParams<TypeParam, 15>;
  kleidicv_filter_context_t *context = nullptr;
  size_t validSize = KernelTestParams3x3::kKernelSize - 1;
  ASSERT_EQ(KLEIDICV_OK,
            kleidicv_filter_create(&context, 1, 2 * sizeof(TypeParam),
                                   kleidicv_rectangle_t{validSize, validSize}));
  TypeParam src[1] = {}, dst[1];
  EXPECT_EQ(KLEIDICV_ERROR_ALIGNMENT,
            gaussian_blur_3x3<TypeParam>()(
                src, sizeof(TypeParam) + 1, dst, sizeof(TypeParam), validSize,
                validSize, 1, KLEIDICV_BORDER_TYPE_REPLICATE, context));
  EXPECT_EQ(KLEIDICV_ERROR_ALIGNMENT,
            gaussian_blur_3x3<TypeParam>()(
                src, sizeof(TypeParam), dst, sizeof(TypeParam) + 1, validSize,
                validSize, 1, KLEIDICV_BORDER_TYPE_REPLICATE, context));
  validSize = KernelTestParams5x5::kKernelSize - 1;
  EXPECT_EQ(KLEIDICV_ERROR_ALIGNMENT,
            gaussian_blur_5x5<TypeParam>()(
                src, sizeof(TypeParam) + 1, dst, sizeof(TypeParam), validSize,
                validSize, 1, KLEIDICV_BORDER_TYPE_REPLICATE, context));
  EXPECT_EQ(KLEIDICV_ERROR_ALIGNMENT,
            gaussian_blur_5x5<TypeParam>()(
                src, sizeof(TypeParam), dst, sizeof(TypeParam) + 1, validSize,
                validSize, 1, KLEIDICV_BORDER_TYPE_REPLICATE, context));
  validSize = KernelTestParams7x7::kKernelSize - 1;
  EXPECT_EQ(KLEIDICV_ERROR_ALIGNMENT,
            gaussian_blur_7x7<TypeParam>()(
                src, sizeof(TypeParam) + 1, dst, sizeof(TypeParam), validSize,
                validSize, 1, KLEIDICV_BORDER_TYPE_REPLICATE, context));
  EXPECT_EQ(KLEIDICV_ERROR_ALIGNMENT,
            gaussian_blur_7x7<TypeParam>()(
                src, sizeof(TypeParam), dst, sizeof(TypeParam) + 1, validSize,
                validSize, 1, KLEIDICV_BORDER_TYPE_REPLICATE, context));
  validSize = KernelTestParams15x15::kKernelSize - 1;
  EXPECT_EQ(KLEIDICV_ERROR_ALIGNMENT,
            gaussian_blur_15x15<TypeParam>()(
                src, sizeof(TypeParam) + 1, dst, sizeof(TypeParam), validSize,
                validSize, 1, KLEIDICV_BORDER_TYPE_REPLICATE, context));
  EXPECT_EQ(KLEIDICV_ERROR_ALIGNMENT,
            gaussian_blur_15x15<TypeParam>()(
                src, sizeof(TypeParam), dst, sizeof(TypeParam) + 1, validSize,
                validSize, 1, KLEIDICV_BORDER_TYPE_REPLICATE, context));
  EXPECT_EQ(KLEIDICV_OK, kleidicv_filter_release(context));
}

TYPED_TEST(GaussianBlur, ZeroImageSize3x3) {
  TypeParam src[1] = {}, dst[1];
  kleidicv_filter_context_t *context = nullptr;
  ASSERT_EQ(KLEIDICV_OK,
            kleidicv_filter_create(&context, 1, 2 * sizeof(TypeParam),
                                   kleidicv_rectangle_t{0, 1}));
  EXPECT_EQ(KLEIDICV_ERROR_NOT_IMPLEMENTED,
            gaussian_blur_3x3<TypeParam>()(
                src, sizeof(TypeParam), dst, sizeof(TypeParam), 0, 1, 1,
                KLEIDICV_BORDER_TYPE_REFLECT, context));
  EXPECT_EQ(KLEIDICV_OK, kleidicv_filter_release(context));

  ASSERT_EQ(KLEIDICV_OK,
            kleidicv_filter_create(&context, 1, 2 * sizeof(TypeParam),
                                   kleidicv_rectangle_t{1, 0}));
  EXPECT_EQ(KLEIDICV_ERROR_NOT_IMPLEMENTED,
            gaussian_blur_3x3<TypeParam>()(
                src, sizeof(TypeParam), dst, sizeof(TypeParam), 1, 0, 1,
                KLEIDICV_BORDER_TYPE_REFLECT, context));
  EXPECT_EQ(KLEIDICV_OK, kleidicv_filter_release(context));
}

TYPED_TEST(GaussianBlur, ZeroImageSize5x5) {
  TypeParam src[1] = {}, dst[1];
  kleidicv_filter_context_t *context = nullptr;
  ASSERT_EQ(KLEIDICV_OK,
            kleidicv_filter_create(&context, 1, 2 * sizeof(TypeParam),
                                   kleidicv_rectangle_t{0, 1}));
  EXPECT_EQ(KLEIDICV_ERROR_NOT_IMPLEMENTED,
            gaussian_blur_5x5<TypeParam>()(
                src, sizeof(TypeParam), dst, sizeof(TypeParam), 0, 1, 1,
                KLEIDICV_BORDER_TYPE_REFLECT, context));
  EXPECT_EQ(KLEIDICV_OK, kleidicv_filter_release(context));

  ASSERT_EQ(KLEIDICV_OK,
            kleidicv_filter_create(&context, 1, 2 * sizeof(TypeParam),
                                   kleidicv_rectangle_t{1, 0}));
  EXPECT_EQ(KLEIDICV_ERROR_NOT_IMPLEMENTED,
            gaussian_blur_5x5<TypeParam>()(
                src, sizeof(TypeParam), dst, sizeof(TypeParam), 1, 0, 1,
                KLEIDICV_BORDER_TYPE_REFLECT, context));
  EXPECT_EQ(KLEIDICV_OK, kleidicv_filter_release(context));
}

TYPED_TEST(GaussianBlur, ZeroImageSize7x7) {
  TypeParam src[1] = {}, dst[1];
  kleidicv_filter_context_t *context = nullptr;
  ASSERT_EQ(KLEIDICV_OK,
            kleidicv_filter_create(&context, 1, 2 * sizeof(TypeParam),
                                   kleidicv_rectangle_t{0, 1}));
  EXPECT_EQ(KLEIDICV_ERROR_NOT_IMPLEMENTED,
            gaussian_blur_7x7<TypeParam>()(
                src, sizeof(TypeParam), dst, sizeof(TypeParam), 0, 1, 1,
                KLEIDICV_BORDER_TYPE_REFLECT, context));
  EXPECT_EQ(KLEIDICV_OK, kleidicv_filter_release(context));

  ASSERT_EQ(KLEIDICV_OK,
            kleidicv_filter_create(&context, 1, 2 * sizeof(TypeParam),
                                   kleidicv_rectangle_t{1, 0}));
  EXPECT_EQ(KLEIDICV_ERROR_NOT_IMPLEMENTED,
            gaussian_blur_7x7<TypeParam>()(
                src, sizeof(TypeParam), dst, sizeof(TypeParam), 1, 0, 1,
                KLEIDICV_BORDER_TYPE_REFLECT, context));
  EXPECT_EQ(KLEIDICV_OK, kleidicv_filter_release(context));
}

TYPED_TEST(GaussianBlur, ZeroImageSize15x15) {
  TypeParam src[1] = {}, dst[1];
  kleidicv_filter_context_t *context = nullptr;
  ASSERT_EQ(KLEIDICV_OK,
            kleidicv_filter_create(&context, 1, 4 * sizeof(TypeParam),
                                   kleidicv_rectangle_t{0, 1}));
  EXPECT_EQ(KLEIDICV_ERROR_NOT_IMPLEMENTED,
            gaussian_blur_15x15<TypeParam>()(
                src, sizeof(TypeParam), dst, sizeof(TypeParam), 0, 1, 1,
                KLEIDICV_BORDER_TYPE_REFLECT, context));
  EXPECT_EQ(KLEIDICV_OK, kleidicv_filter_release(context));

  ASSERT_EQ(KLEIDICV_OK,
            kleidicv_filter_create(&context, 1, 4 * sizeof(TypeParam),
                                   kleidicv_rectangle_t{1, 0}));
  EXPECT_EQ(KLEIDICV_ERROR_NOT_IMPLEMENTED,
            gaussian_blur_15x15<TypeParam>()(
                src, sizeof(TypeParam), dst, sizeof(TypeParam), 1, 0, 1,
                KLEIDICV_BORDER_TYPE_REFLECT, context));
  EXPECT_EQ(KLEIDICV_OK, kleidicv_filter_release(context));
}

TYPED_TEST(GaussianBlur, ValidImageSize3x3) {
  using KernelTestParams = GaussianBlurKernelTestParams<TypeParam, 3>;
  kleidicv_filter_context_t *context = nullptr;
  size_t validSize = KernelTestParams::kKernelSize - 1;
  ASSERT_EQ(KLEIDICV_OK,
            kleidicv_filter_create(&context, 1, 2 * sizeof(TypeParam),
                                   kleidicv_rectangle_t{validSize, validSize}));
  test::Array2D<TypeParam> src{validSize, validSize,
                               test::Options::vector_length()};
  src.set(0, 0, {1, 2});
  src.set(1, 0, {1, 2});

  test::Array2D<TypeParam> dst{validSize, validSize,
                               test::Options::vector_length()};
  EXPECT_EQ(KLEIDICV_OK,
            gaussian_blur_3x3<TypeParam>()(
                src.data(), src.stride(), dst.data(), dst.stride(), validSize,
                validSize, 1, KLEIDICV_BORDER_TYPE_REVERSE, context));
  EXPECT_EQ(KLEIDICV_OK, kleidicv_filter_release(context));
}

TYPED_TEST(GaussianBlur, ValidImageSize5x5) {
  using KernelTestParams = GaussianBlurKernelTestParams<TypeParam, 5>;
  kleidicv_filter_context_t *context = nullptr;
  size_t validSize = KernelTestParams::kKernelSize - 1;
  ASSERT_EQ(KLEIDICV_OK,
            kleidicv_filter_create(&context, 1, 2 * sizeof(TypeParam),
                                   kleidicv_rectangle_t{validSize, validSize}));
  test::Array2D<TypeParam> src{validSize, validSize,
                               test::Options::vector_length()};
  src.set(0, 0, {1, 2, 3, 4});
  src.set(1, 0, {1, 2, 3, 4});
  src.set(2, 0, {1, 2, 3, 4});
  src.set(3, 0, {1, 2, 3, 4});

  test::Array2D<TypeParam> dst{validSize, validSize,
                               test::Options::vector_length()};
  EXPECT_EQ(KLEIDICV_OK,
            gaussian_blur_5x5<TypeParam>()(
                src.data(), src.stride(), dst.data(), dst.stride(), validSize,
                validSize, 1, KLEIDICV_BORDER_TYPE_REVERSE, context));
  EXPECT_EQ(KLEIDICV_OK, kleidicv_filter_release(context));
}

TYPED_TEST(GaussianBlur, ValidImageSize7x7) {
  using KernelTestParams = GaussianBlurKernelTestParams<TypeParam, 7>;
  kleidicv_filter_context_t *context = nullptr;
  size_t validSize = KernelTestParams::kKernelSize - 1;
  ASSERT_EQ(KLEIDICV_OK,
            kleidicv_filter_create(&context, 1, 2 * sizeof(TypeParam),
                                   kleidicv_rectangle_t{validSize, validSize}));
  test::Array2D<TypeParam> src{validSize, validSize,
                               test::Options::vector_length()};
  src.set(0, 0, {1, 2, 3, 4, 5, 6});
  src.set(1, 0, {1, 2, 3, 4, 5, 6});
  src.set(2, 0, {1, 2, 3, 4, 5, 6});
  src.set(3, 0, {1, 2, 3, 4, 5, 6});
  src.set(4, 0, {1, 2, 3, 4, 5, 6});
  src.set(5, 0, {1, 2, 3, 4, 5, 6});

  test::Array2D<TypeParam> dst{validSize, validSize,
                               test::Options::vector_length()};
  EXPECT_EQ(KLEIDICV_OK,
            gaussian_blur_7x7<TypeParam>()(
                src.data(), src.stride(), dst.data(), dst.stride(), validSize,
                validSize, 1, KLEIDICV_BORDER_TYPE_REVERSE, context));
  EXPECT_EQ(KLEIDICV_OK, kleidicv_filter_release(context));
}

TYPED_TEST(GaussianBlur, ValidImageSize15x15) {
  using KernelTestParams = GaussianBlurKernelTestParams<TypeParam, 15>;
  kleidicv_filter_context_t *context = nullptr;
  size_t validSize = KernelTestParams::kKernelSize - 1;
  ASSERT_EQ(KLEIDICV_OK,
            kleidicv_filter_create(&context, 1, 4 * sizeof(TypeParam),
                                   kleidicv_rectangle_t{validSize, validSize}));
  test::Array2D<TypeParam> src{validSize, validSize,
                               test::Options::vector_length()};
  src.set(0, 0, {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14});
  src.set(1, 0, {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14});
  src.set(2, 0, {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14});
  src.set(3, 0, {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14});
  src.set(4, 0, {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14});
  src.set(5, 0, {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14});
  src.set(6, 0, {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14});
  src.set(7, 0, {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14});
  src.set(8, 0, {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14});
  src.set(9, 0, {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14});
  src.set(10, 0, {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14});
  src.set(11, 0, {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14});
  src.set(12, 0, {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14});
  src.set(13, 0, {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14});

  test::Array2D<TypeParam> dst{validSize, validSize,
                               test::Options::vector_length()};
  EXPECT_EQ(KLEIDICV_OK,
            gaussian_blur_15x15<TypeParam>()(
                src.data(), src.stride(), dst.data(), dst.stride(), validSize,
                validSize, 1, KLEIDICV_BORDER_TYPE_REVERSE, context));
  EXPECT_EQ(KLEIDICV_OK, kleidicv_filter_release(context));
}

TYPED_TEST(GaussianBlur, UndersizeImage3x3) {
  using KernelTestParams = GaussianBlurKernelTestParams<TypeParam, 3>;
  kleidicv_filter_context_t *context = nullptr;
  size_t underSize = KernelTestParams::kKernelSize - 2;
  size_t validWidth = KernelTestParams::kKernelSize + 10;
  size_t validHeight = KernelTestParams::kKernelSize + 5;
  ASSERT_EQ(KLEIDICV_OK,
            kleidicv_filter_create(&context, 1, 2 * sizeof(TypeParam),
                                   kleidicv_rectangle_t{underSize, underSize}));
  TypeParam src[1] = {}, dst[1];
  EXPECT_EQ(KLEIDICV_ERROR_NOT_IMPLEMENTED,
            gaussian_blur_3x3<TypeParam>()(
                src, sizeof(TypeParam), dst, sizeof(TypeParam), underSize,
                underSize, 1, KLEIDICV_BORDER_TYPE_REPLICATE, context));
  EXPECT_EQ(KLEIDICV_OK, kleidicv_filter_release(context));
  ASSERT_EQ(KLEIDICV_OK, kleidicv_filter_create(
                             &context, 1, 2 * sizeof(TypeParam),
                             kleidicv_rectangle_t{underSize, validHeight}));
  EXPECT_EQ(KLEIDICV_ERROR_NOT_IMPLEMENTED,
            gaussian_blur_3x3<TypeParam>()(
                src, sizeof(TypeParam), dst, sizeof(TypeParam), underSize,
                validHeight, 1, KLEIDICV_BORDER_TYPE_REPLICATE, context));
  EXPECT_EQ(KLEIDICV_OK, kleidicv_filter_release(context));
  ASSERT_EQ(KLEIDICV_OK, kleidicv_filter_create(
                             &context, 1, 2 * sizeof(TypeParam),
                             kleidicv_rectangle_t{validWidth, underSize}));
  EXPECT_EQ(KLEIDICV_ERROR_NOT_IMPLEMENTED,
            gaussian_blur_3x3<TypeParam>()(
                src, sizeof(TypeParam), dst, sizeof(TypeParam), validWidth,
                underSize, 1, KLEIDICV_BORDER_TYPE_REPLICATE, context));
  EXPECT_EQ(KLEIDICV_OK, kleidicv_filter_release(context));
}

TYPED_TEST(GaussianBlur, UndersizeImage5x5) {
  using KernelTestParams = GaussianBlurKernelTestParams<TypeParam, 5>;
  kleidicv_filter_context_t *context = nullptr;
  size_t underSize = KernelTestParams::kKernelSize - 2;
  size_t validWidth = KernelTestParams::kKernelSize + 8;
  size_t validHeight = KernelTestParams::kKernelSize + 3;
  ASSERT_EQ(KLEIDICV_OK,
            kleidicv_filter_create(&context, 1, 2 * sizeof(TypeParam),
                                   kleidicv_rectangle_t{underSize, underSize}));
  TypeParam src[1] = {}, dst[1];
  EXPECT_EQ(KLEIDICV_ERROR_NOT_IMPLEMENTED,
            gaussian_blur_5x5<TypeParam>()(
                src, sizeof(TypeParam), dst, sizeof(TypeParam), underSize,
                underSize, 1, KLEIDICV_BORDER_TYPE_REPLICATE, context));
  EXPECT_EQ(KLEIDICV_OK, kleidicv_filter_release(context));
  ASSERT_EQ(KLEIDICV_OK, kleidicv_filter_create(
                             &context, 1, 2 * sizeof(TypeParam),
                             kleidicv_rectangle_t{underSize, validHeight}));
  EXPECT_EQ(KLEIDICV_ERROR_NOT_IMPLEMENTED,
            gaussian_blur_5x5<TypeParam>()(
                src, sizeof(TypeParam), dst, sizeof(TypeParam), underSize,
                validHeight, 1, KLEIDICV_BORDER_TYPE_REPLICATE, context));
  EXPECT_EQ(KLEIDICV_OK, kleidicv_filter_release(context));
  ASSERT_EQ(KLEIDICV_OK, kleidicv_filter_create(
                             &context, 1, 2 * sizeof(TypeParam),
                             kleidicv_rectangle_t{validWidth, underSize}));
  EXPECT_EQ(KLEIDICV_ERROR_NOT_IMPLEMENTED,
            gaussian_blur_5x5<TypeParam>()(
                src, sizeof(TypeParam), dst, sizeof(TypeParam), validWidth,
                underSize, 1, KLEIDICV_BORDER_TYPE_REPLICATE, context));
  EXPECT_EQ(KLEIDICV_OK, kleidicv_filter_release(context));
}

TYPED_TEST(GaussianBlur, UndersizeImage7x7) {
  using KernelTestParams = GaussianBlurKernelTestParams<TypeParam, 7>;
  kleidicv_filter_context_t *context = nullptr;
  size_t underSize = KernelTestParams::kKernelSize - 2;
  size_t validWidth = KernelTestParams::kKernelSize + 6;
  size_t validHeight = KernelTestParams::kKernelSize + 1;
  ASSERT_EQ(KLEIDICV_OK,
            kleidicv_filter_create(&context, 1, 2 * sizeof(TypeParam),
                                   kleidicv_rectangle_t{underSize, underSize}));
  TypeParam src[1] = {}, dst[1];
  EXPECT_EQ(KLEIDICV_ERROR_NOT_IMPLEMENTED,
            gaussian_blur_7x7<TypeParam>()(
                src, sizeof(TypeParam), dst, sizeof(TypeParam), underSize,
                underSize, 1, KLEIDICV_BORDER_TYPE_REPLICATE, context));
  EXPECT_EQ(KLEIDICV_OK, kleidicv_filter_release(context));
  ASSERT_EQ(KLEIDICV_OK, kleidicv_filter_create(
                             &context, 1, 2 * sizeof(TypeParam),
                             kleidicv_rectangle_t{underSize, validHeight}));
  EXPECT_EQ(KLEIDICV_ERROR_NOT_IMPLEMENTED,
            gaussian_blur_7x7<TypeParam>()(
                src, sizeof(TypeParam), dst, sizeof(TypeParam), underSize,
                validHeight, 1, KLEIDICV_BORDER_TYPE_REPLICATE, context));
  EXPECT_EQ(KLEIDICV_OK, kleidicv_filter_release(context));
  ASSERT_EQ(KLEIDICV_OK, kleidicv_filter_create(
                             &context, 1, 2 * sizeof(TypeParam),
                             kleidicv_rectangle_t{validWidth, underSize}));
  EXPECT_EQ(KLEIDICV_ERROR_NOT_IMPLEMENTED,
            gaussian_blur_7x7<TypeParam>()(
                src, sizeof(TypeParam), dst, sizeof(TypeParam), validWidth,
                underSize, 1, KLEIDICV_BORDER_TYPE_REPLICATE, context));
  EXPECT_EQ(KLEIDICV_OK, kleidicv_filter_release(context));
}

TYPED_TEST(GaussianBlur, UndersizeImage15x15) {
  using KernelTestParams = GaussianBlurKernelTestParams<TypeParam, 15>;
  kleidicv_filter_context_t *context = nullptr;
  size_t underSize = KernelTestParams::kKernelSize - 2;
  size_t validWidth = KernelTestParams::kKernelSize + 10;
  size_t validHeight = KernelTestParams::kKernelSize + 5;
  ASSERT_EQ(KLEIDICV_OK,
            kleidicv_filter_create(&context, 1, 4 * sizeof(TypeParam),
                                   kleidicv_rectangle_t{underSize, underSize}));
  TypeParam src[1] = {}, dst[1];
  EXPECT_EQ(KLEIDICV_ERROR_NOT_IMPLEMENTED,
            gaussian_blur_15x15<TypeParam>()(
                src, sizeof(TypeParam), dst, sizeof(TypeParam), underSize,
                underSize, 1, KLEIDICV_BORDER_TYPE_REPLICATE, context));
  EXPECT_EQ(KLEIDICV_OK, kleidicv_filter_release(context));
  ASSERT_EQ(KLEIDICV_OK, kleidicv_filter_create(
                             &context, 1, 4 * sizeof(TypeParam),
                             kleidicv_rectangle_t{underSize, validHeight}));
  EXPECT_EQ(KLEIDICV_ERROR_NOT_IMPLEMENTED,
            gaussian_blur_15x15<TypeParam>()(
                src, sizeof(TypeParam), dst, sizeof(TypeParam), underSize,
                validHeight, 1, KLEIDICV_BORDER_TYPE_REPLICATE, context));
  EXPECT_EQ(KLEIDICV_OK, kleidicv_filter_release(context));
  ASSERT_EQ(KLEIDICV_OK, kleidicv_filter_create(
                             &context, 1, 4 * sizeof(TypeParam),
                             kleidicv_rectangle_t{validWidth, underSize}));
  EXPECT_EQ(KLEIDICV_ERROR_NOT_IMPLEMENTED,
            gaussian_blur_15x15<TypeParam>()(
                src, sizeof(TypeParam), dst, sizeof(TypeParam), validWidth,
                underSize, 1, KLEIDICV_BORDER_TYPE_REPLICATE, context));
  EXPECT_EQ(KLEIDICV_OK, kleidicv_filter_release(context));
}

TYPED_TEST(GaussianBlur, OversizeImage) {
  kleidicv_filter_context_t *context = nullptr;
  ASSERT_EQ(KLEIDICV_OK,
            kleidicv_filter_create(&context, 1, 2 * sizeof(TypeParam),
                                   kleidicv_rectangle_t{1, 1}));
  TypeParam src[1], dst[1];
  EXPECT_EQ(KLEIDICV_ERROR_RANGE,
            gaussian_blur_3x3<TypeParam>()(
                src, sizeof(TypeParam), dst, sizeof(TypeParam),
                KLEIDICV_MAX_IMAGE_PIXELS + 1, 1, 1,
                KLEIDICV_BORDER_TYPE_REFLECT, context));
  EXPECT_EQ(KLEIDICV_ERROR_RANGE,
            gaussian_blur_3x3<TypeParam>()(
                src, sizeof(TypeParam), dst, sizeof(TypeParam),
                KLEIDICV_MAX_IMAGE_PIXELS, KLEIDICV_MAX_IMAGE_PIXELS, 1,
                KLEIDICV_BORDER_TYPE_REFLECT, context));
  EXPECT_EQ(KLEIDICV_ERROR_RANGE,
            gaussian_blur_5x5<TypeParam>()(
                src, sizeof(TypeParam), dst, sizeof(TypeParam),
                KLEIDICV_MAX_IMAGE_PIXELS + 1, 1, 1,
                KLEIDICV_BORDER_TYPE_REFLECT, context));
  EXPECT_EQ(KLEIDICV_ERROR_RANGE,
            gaussian_blur_5x5<TypeParam>()(
                src, sizeof(TypeParam), dst, sizeof(TypeParam),
                KLEIDICV_MAX_IMAGE_PIXELS, KLEIDICV_MAX_IMAGE_PIXELS, 1,
                KLEIDICV_BORDER_TYPE_REFLECT, context));
  EXPECT_EQ(KLEIDICV_ERROR_RANGE,
            gaussian_blur_7x7<TypeParam>()(
                src, sizeof(TypeParam), dst, sizeof(TypeParam),
                KLEIDICV_MAX_IMAGE_PIXELS + 1, 1, 1,
                KLEIDICV_BORDER_TYPE_REFLECT, context));
  EXPECT_EQ(KLEIDICV_ERROR_RANGE,
            gaussian_blur_7x7<TypeParam>()(
                src, sizeof(TypeParam), dst, sizeof(TypeParam),
                KLEIDICV_MAX_IMAGE_PIXELS, KLEIDICV_MAX_IMAGE_PIXELS, 1,
                KLEIDICV_BORDER_TYPE_REFLECT, context));
  EXPECT_EQ(KLEIDICV_ERROR_RANGE,
            gaussian_blur_15x15<TypeParam>()(
                src, sizeof(TypeParam), dst, sizeof(TypeParam),
                KLEIDICV_MAX_IMAGE_PIXELS + 1, 1, 1,
                KLEIDICV_BORDER_TYPE_REFLECT, context));
  EXPECT_EQ(KLEIDICV_ERROR_RANGE,
            gaussian_blur_15x15<TypeParam>()(
                src, sizeof(TypeParam), dst, sizeof(TypeParam),
                KLEIDICV_MAX_IMAGE_PIXELS, KLEIDICV_MAX_IMAGE_PIXELS, 1,
                KLEIDICV_BORDER_TYPE_REFLECT, context));
  EXPECT_EQ(KLEIDICV_OK, kleidicv_filter_release(context));
}

TYPED_TEST(GaussianBlur, ChannelNumber) {
  using KernelTestParams3x3 = GaussianBlurKernelTestParams<TypeParam, 3>;
  using KernelTestParams5x5 = GaussianBlurKernelTestParams<TypeParam, 5>;
  using KernelTestParams7x7 = GaussianBlurKernelTestParams<TypeParam, 7>;
  using KernelTestParams15x15 = GaussianBlurKernelTestParams<TypeParam, 15>;
  kleidicv_filter_context_t *context = nullptr;
  size_t validSize = KernelTestParams3x3::kKernelSize - 1;

  ASSERT_EQ(KLEIDICV_OK,
            kleidicv_filter_create(&context, 1, 2 * sizeof(TypeParam),
                                   kleidicv_rectangle_t{validSize, validSize}));
  TypeParam src[1], dst[1];
  EXPECT_EQ(KLEIDICV_ERROR_RANGE,
            gaussian_blur_3x3<TypeParam>()(
                src, sizeof(TypeParam), dst, sizeof(TypeParam), validSize,
                validSize, KLEIDICV_MAXIMUM_CHANNEL_COUNT + 1,
                KLEIDICV_BORDER_TYPE_REFLECT, context));

  validSize = KernelTestParams5x5::kKernelSize - 1;
  EXPECT_EQ(KLEIDICV_ERROR_RANGE,
            gaussian_blur_5x5<TypeParam>()(
                src, sizeof(TypeParam), dst, sizeof(TypeParam), validSize,
                validSize, KLEIDICV_MAXIMUM_CHANNEL_COUNT + 1,
                KLEIDICV_BORDER_TYPE_REFLECT, context));

  validSize = KernelTestParams7x7::kKernelSize - 1;
  EXPECT_EQ(KLEIDICV_ERROR_RANGE,
            gaussian_blur_7x7<TypeParam>()(
                src, sizeof(TypeParam), dst, sizeof(TypeParam), validSize,
                validSize, KLEIDICV_MAXIMUM_CHANNEL_COUNT + 1,
                KLEIDICV_BORDER_TYPE_REFLECT, context));

  validSize = KernelTestParams15x15::kKernelSize - 1;
  EXPECT_EQ(KLEIDICV_ERROR_RANGE,
            gaussian_blur_15x15<TypeParam>()(
                src, sizeof(TypeParam), dst, sizeof(TypeParam), validSize,
                validSize, KLEIDICV_MAXIMUM_CHANNEL_COUNT + 1,
                KLEIDICV_BORDER_TYPE_REFLECT, context));
  EXPECT_EQ(KLEIDICV_OK, kleidicv_filter_release(context));
}

TYPED_TEST(GaussianBlur, InvalidContextSizeType) {
  using KernelTestParams3x3 = GaussianBlurKernelTestParams<TypeParam, 3>;
  using KernelTestParams5x5 = GaussianBlurKernelTestParams<TypeParam, 5>;
  using KernelTestParams7x7 = GaussianBlurKernelTestParams<TypeParam, 7>;
  using KernelTestParams15x15 = GaussianBlurKernelTestParams<TypeParam, 15>;
  kleidicv_filter_context_t *context = nullptr;
  size_t validSize = KernelTestParams3x3::kKernelSize - 1;

  ASSERT_EQ(KLEIDICV_OK,
            kleidicv_filter_create(&context, 1, 2 * sizeof(TypeParam) + 1,
                                   kleidicv_rectangle_t{validSize, validSize}));
  TypeParam src[1], dst[1];
  EXPECT_EQ(KLEIDICV_ERROR_CONTEXT_MISMATCH,
            gaussian_blur_3x3<TypeParam>()(
                src, sizeof(TypeParam), dst, sizeof(TypeParam), validSize,
                validSize, 1, KLEIDICV_BORDER_TYPE_REFLECT, context));
  validSize = KernelTestParams5x5::kKernelSize - 1;
  EXPECT_EQ(KLEIDICV_ERROR_CONTEXT_MISMATCH,
            gaussian_blur_5x5<TypeParam>()(
                src, sizeof(TypeParam), dst, sizeof(TypeParam), validSize,
                validSize, 1, KLEIDICV_BORDER_TYPE_REFLECT, context));
  validSize = KernelTestParams7x7::kKernelSize - 1;
  EXPECT_EQ(KLEIDICV_ERROR_CONTEXT_MISMATCH,
            gaussian_blur_7x7<TypeParam>()(
                src, sizeof(TypeParam), dst, sizeof(TypeParam), validSize,
                validSize, 1, KLEIDICV_BORDER_TYPE_REFLECT, context));
  validSize = KernelTestParams15x15::kKernelSize - 1;
  EXPECT_EQ(KLEIDICV_ERROR_CONTEXT_MISMATCH,
            gaussian_blur_15x15<TypeParam>()(
                src, sizeof(TypeParam), dst, sizeof(TypeParam), validSize,
                validSize, 1, KLEIDICV_BORDER_TYPE_REFLECT, context));
  EXPECT_EQ(KLEIDICV_OK, kleidicv_filter_release(context));
}

TYPED_TEST(GaussianBlur, InvalidContextChannelNumber) {
  using KernelTestParams3x3 = GaussianBlurKernelTestParams<TypeParam, 3>;
  using KernelTestParams5x5 = GaussianBlurKernelTestParams<TypeParam, 5>;
  using KernelTestParams7x7 = GaussianBlurKernelTestParams<TypeParam, 7>;
  using KernelTestParams15x15 = GaussianBlurKernelTestParams<TypeParam, 15>;
  kleidicv_filter_context_t *context = nullptr;
  size_t validSize = KernelTestParams3x3::kKernelSize - 1;

  ASSERT_EQ(KLEIDICV_OK,
            kleidicv_filter_create(&context, 2, 2 * sizeof(TypeParam),
                                   kleidicv_rectangle_t{validSize, validSize}));
  TypeParam src[1], dst[1];
  EXPECT_EQ(KLEIDICV_ERROR_CONTEXT_MISMATCH,
            gaussian_blur_3x3<TypeParam>()(
                src, sizeof(TypeParam), dst, sizeof(TypeParam), validSize,
                validSize, 1, KLEIDICV_BORDER_TYPE_REFLECT, context));

  validSize = KernelTestParams5x5::kKernelSize - 1;
  EXPECT_EQ(KLEIDICV_ERROR_CONTEXT_MISMATCH,
            gaussian_blur_5x5<TypeParam>()(
                src, sizeof(TypeParam), dst, sizeof(TypeParam), validSize,
                validSize, 1, KLEIDICV_BORDER_TYPE_REFLECT, context));

  validSize = KernelTestParams7x7::kKernelSize - 1;
  EXPECT_EQ(KLEIDICV_ERROR_CONTEXT_MISMATCH,
            gaussian_blur_7x7<TypeParam>()(
                src, sizeof(TypeParam), dst, sizeof(TypeParam), validSize,
                validSize, 1, KLEIDICV_BORDER_TYPE_REFLECT, context));

  validSize = KernelTestParams15x15::kKernelSize - 1;
  EXPECT_EQ(KLEIDICV_ERROR_CONTEXT_MISMATCH,
            gaussian_blur_15x15<TypeParam>()(
                src, sizeof(TypeParam), dst, sizeof(TypeParam), validSize,
                validSize, 1, KLEIDICV_BORDER_TYPE_REFLECT, context));
  EXPECT_EQ(KLEIDICV_OK, kleidicv_filter_release(context));
}

TYPED_TEST(GaussianBlur, InvalidContextImageSize) {
  using KernelTestParams3x3 = GaussianBlurKernelTestParams<TypeParam, 3>;
  using KernelTestParams5x5 = GaussianBlurKernelTestParams<TypeParam, 5>;
  using KernelTestParams7x7 = GaussianBlurKernelTestParams<TypeParam, 7>;
  using KernelTestParams15x15 = GaussianBlurKernelTestParams<TypeParam, 15>;
  kleidicv_filter_context_t *context = nullptr;
  size_t validSize = KernelTestParams3x3::kKernelSize - 1;

  ASSERT_EQ(KLEIDICV_OK,
            kleidicv_filter_create(&context, 1, 2 * sizeof(TypeParam),
                                   kleidicv_rectangle_t{validSize, validSize}));
  TypeParam src[1], dst[1];
  EXPECT_EQ(KLEIDICV_ERROR_CONTEXT_MISMATCH,
            gaussian_blur_3x3<TypeParam>()(
                src, sizeof(TypeParam), dst, sizeof(TypeParam), validSize + 1,
                validSize + 1, 1, KLEIDICV_BORDER_TYPE_REFLECT, context));

  validSize = KernelTestParams5x5::kKernelSize - 1;
  EXPECT_EQ(KLEIDICV_ERROR_CONTEXT_MISMATCH,
            gaussian_blur_5x5<TypeParam>()(
                src, sizeof(TypeParam), dst, sizeof(TypeParam), validSize + 1,
                validSize + 1, 1, KLEIDICV_BORDER_TYPE_REFLECT, context));

  validSize = KernelTestParams7x7::kKernelSize - 1;
  EXPECT_EQ(KLEIDICV_ERROR_CONTEXT_MISMATCH,
            gaussian_blur_7x7<TypeParam>()(
                src, sizeof(TypeParam), dst, sizeof(TypeParam), validSize + 1,
                validSize + 1, 1, KLEIDICV_BORDER_TYPE_REFLECT, context));

  validSize = KernelTestParams15x15::kKernelSize - 1;
  EXPECT_EQ(KLEIDICV_ERROR_CONTEXT_MISMATCH,
            gaussian_blur_15x15<TypeParam>()(
                src, sizeof(TypeParam), dst, sizeof(TypeParam), validSize + 1,
                validSize + 1, 1, KLEIDICV_BORDER_TYPE_REFLECT, context));
  EXPECT_EQ(KLEIDICV_OK, kleidicv_filter_release(context));
}

#ifdef KLEIDICV_ALLOCATION_TESTS
TEST(FilterCreate, CannotAllocateFilter) {
  MockMallocToFail::enable();
  kleidicv_filter_context_t *context = nullptr;
  kleidicv_rectangle_t rect{KLEIDICV_MAX_IMAGE_PIXELS, 1};
  EXPECT_EQ(KLEIDICV_ERROR_ALLOCATION,
            kleidicv_filter_create(&context, 1, 1, rect));
  MockMallocToFail::disable();
}
#endif

TEST(FilterCreate, OversizeImage) {
  kleidicv_filter_context_t *context = nullptr;

  for (kleidicv_rectangle_t rect : {
           kleidicv_rectangle_t{KLEIDICV_MAX_IMAGE_PIXELS + 1, 1},
           kleidicv_rectangle_t{KLEIDICV_MAX_IMAGE_PIXELS,
                                KLEIDICV_MAX_IMAGE_PIXELS},
       }) {
    EXPECT_EQ(KLEIDICV_ERROR_RANGE,
              kleidicv_filter_create(&context, 1, 1, rect));
    ASSERT_EQ(nullptr, context);
  }
}

TEST(FilterCreate, TypeSize) {
  kleidicv_filter_context_t *context = nullptr;

  EXPECT_EQ(KLEIDICV_ERROR_RANGE,
            kleidicv_filter_create(&context, 1, KLEIDICV_MAXIMUM_TYPE_SIZE + 1,
                                   kleidicv_rectangle_t{1, 1}));
  ASSERT_EQ(nullptr, context);
}

TEST(FilterCreate, ChannelNumber) {
  kleidicv_filter_context_t *context = nullptr;

  EXPECT_EQ(KLEIDICV_ERROR_RANGE,
            kleidicv_filter_create(&context, KLEIDICV_MAXIMUM_CHANNEL_COUNT + 1,
                                   1, kleidicv_rectangle_t{1, 1}));
  ASSERT_EQ(nullptr, context);
}

TEST(FilterCreate, NullPointer) {
  EXPECT_EQ(KLEIDICV_ERROR_NULL_POINTER,
            kleidicv_filter_create(nullptr, 1, 1, kleidicv_rectangle_t{1, 1}));
}
TEST(FilterRelease, NullPointer) {
  EXPECT_EQ(KLEIDICV_ERROR_NULL_POINTER, kleidicv_filter_release(nullptr));
}
