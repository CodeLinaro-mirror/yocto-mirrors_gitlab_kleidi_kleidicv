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

KLEIDICV_API(separable_filter_2d, kleidicv_separable_filter_2d_u8, uint8_t)
KLEIDICV_API(separable_filter_2d, kleidicv_separable_filter_2d_u16, uint16_t)

// Implements KernelTestParams for SeparableFilter2D operators.
template <typename ElementType, size_t KernelSize>
struct SeparableFilter2DKernelTestParams;

template <size_t KernelSize>
struct SeparableFilter2DKernelTestParams<uint8_t, KernelSize> {
  using InputType = uint8_t;
  using IntermediateType = uint8_t;
  using OutputType = uint8_t;

  static constexpr size_t kKernelSize = KernelSize;
};  // end of struct SeparableFilter2DKernelTestParams<uint8_t, KernelSize>

template <size_t KernelSize>
struct SeparableFilter2DKernelTestParams<uint16_t, KernelSize> {
  using InputType = uint16_t;
  using IntermediateType = uint16_t;
  using OutputType = uint16_t;

  static constexpr size_t kKernelSize = KernelSize;
};  // end of struct SeparableFilter2DKernelTestParams<uint16_t, KernelSize>

static constexpr std::array<kleidicv_border_type_t, 1> kDefaultBorder = {
    KLEIDICV_BORDER_TYPE_REPLICATE};

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

// Test for SeparableFilter2D operator.
template <class KernelTestParams>
class SeparableFilter2DTest : public test::KernelTest<KernelTestParams> {
  using Base = test::KernelTest<KernelTestParams>;
  using typename test::KernelTest<KernelTestParams>::InputType;
  using typename test::KernelTest<KernelTestParams>::IntermediateType;
  using typename test::KernelTest<KernelTestParams>::OutputType;

 public:
  explicit SeparableFilter2DTest(const InputType *kernel_x,
                                 const InputType *kernel_y)
      : kernel_x_(kernel_x),
        kernel_y_(kernel_y),
        small_array_layouts_{test::small_array_layouts(
            KernelTestParams::kKernelSize, KernelTestParams::kKernelSize)} {
    array_layout_generator_ = make_generator_ptr(small_array_layouts_);
    border_type_generator_ = make_generator_ptr(kDefaultBorder);
  }

  SeparableFilter2DTest &with_array_layouts(
      std::unique_ptr<test::Generator<test::ArrayLayout>> g) {
    array_layout_generator_ = std::move(g);
    return *this;
  }

  SeparableFilter2DTest &with_border_types(
      std::unique_ptr<test::Generator<kleidicv_border_type_t>> g) {
    border_type_generator_ = std::move(g);
    return *this;
  }

  void test(const test::Array2D<IntermediateType> &mask, InputType max_value) {
    test::Kernel kernel{mask};
    // Use the default border values for testing.
    auto kSupportedBorderValues = test::default_border_values();
    // Create generators and execute test.
    test::SequenceGenerator tested_border_values{kSupportedBorderValues};
    test::PseudoRandomNumberGeneratorIntRange<InputType> element_generator{
        0, max_value};
    Base::test(kernel, *array_layout_generator_, *border_type_generator_,
               tested_border_values, element_generator);
  }

 protected:
  const InputType *kernel_x_;
  const InputType *kernel_y_;
  std::array<test::ArrayLayout, 7> small_array_layouts_;
  std::unique_ptr<test::Generator<test::ArrayLayout>> array_layout_generator_;
  std::unique_ptr<test::Generator<kleidicv_border_type_t>>
      border_type_generator_;

  kleidicv_error_t call_api(const test::Array2D<InputType> *input,
                            test::Array2D<OutputType> *output,
                            kleidicv_border_type_t border_type,
                            kleidicv_border_values_t) override {
    kleidicv_filter_context_t *context = nullptr;
    auto ret = kleidicv_filter_context_create(
        &context, input->channels(), KernelTestParams::kKernelSize,
        KernelTestParams::kKernelSize, input->width() / input->channels(),
        input->height());
    if (ret != KLEIDICV_OK) {
      return ret;
    }

    ret = separable_filter_2d<InputType>()(
        input->data(), input->stride(), output->data(), output->stride(),
        input->width() / input->channels(), input->height(), input->channels(),
        kernel_x_, KernelTestParams::kKernelSize, kernel_y_,
        KernelTestParams::kKernelSize, border_type, context);
    auto releaseRet = kleidicv_filter_context_release(context);
    if (releaseRet != KLEIDICV_OK) {
      return releaseRet;
    }

    return ret;
  }
};  // end of class SeparableFilter2DTest<KernelTestParams>

using ElementTypes = ::testing::Types<uint8_t, uint16_t>;

template <typename ElementType>
class SeparableFilter2D : public testing::Test {};

TYPED_TEST_SUITE(SeparableFilter2D, ElementTypes);

// Tests kleidicv_separable_filter_2d_<input_type> API.
TYPED_TEST(SeparableFilter2D, 5x5) {
  using KernelTestParams = SeparableFilter2DKernelTestParams<TypeParam, 5>;

  const TypeParam kernel_x[5] = {5, 0, 1, 2, 2};
  const TypeParam kernel_y[5] = {1, 4, 3, 1, 0};

  // Mask is created by 'kernel_y (outer product) kernel_x'
  test::Array2D<typename KernelTestParams::IntermediateType> mask{5, 5};
  mask.fill([&](size_t row, size_t column) {
    return kernel_y[row] * kernel_x[column];
  });

  SeparableFilter2DTest<KernelTestParams>{kernel_x, kernel_y}
      .with_border_types(make_generator_ptr(kAllBorders))
      .test(mask, 5);
}

TEST(SeparableFilter2D, 5x5_U8OverflowSequence) {
  using TypeParam = uint8_t;

  kleidicv_filter_context_t *context = nullptr;
  ASSERT_EQ(KLEIDICV_OK,
            kleidicv_filter_context_create(&context, 1, 5, 5, 5, 5));
  test::Array2D<TypeParam> src{5, 5, test::Options::vector_length()};
  // clang-format off
  src.set(0, 0, { 1, 2, 3, 4, 5});
  src.set(1, 0, { 2, 3, 4, 5, 6});
  src.set(2, 0, { 3, 4, 5, 6, 7});
  src.set(3, 0, { 4, 5, 6, 7, 8});
  src.set(4, 0, { 5, 6, 7, 8, 9});
  // clang-format on

  test::Array2D<TypeParam> kernel_x{5, 1};
  kernel_x.set(0, 0, {1, 2, 3, 4, 5});
  test::Array2D<TypeParam> kernel_y{5, 1};
  kernel_y.set(0, 0, {5, 6, 7, 8, 9});

  test::Array2D<TypeParam> dst_expected{5, 5, test::Options::vector_length()};
  // clang-format off
  dst_expected.set(0, 0, { 255, 255, 255, 255, 255});
  dst_expected.set(1, 0, { 255, 255, 255, 255, 255});
  dst_expected.set(2, 0, { 255, 255, 255, 255, 255});
  dst_expected.set(3, 0, { 255, 255, 255, 255, 255});
  dst_expected.set(4, 0, { 255, 255, 255, 255, 255});
  // clang-format on

  test::Array2D<TypeParam> dst{5, 5, test::Options::vector_length()};
  EXPECT_EQ(KLEIDICV_OK, separable_filter_2d<TypeParam>()(
                             src.data(), src.stride(), dst.data(), dst.stride(),
                             5, 5, 1, kernel_x.data(), 5, kernel_y.data(), 5,
                             KLEIDICV_BORDER_TYPE_REPLICATE, context));
  EXPECT_EQ(KLEIDICV_OK, kleidicv_filter_context_release(context));
  EXPECT_EQ_ARRAY2D(dst_expected, dst);
}

TEST(SeparableFilter2D, 5x5_U8OverflowMax) {
  using TypeParam = uint8_t;

  kleidicv_filter_context_t *context = nullptr;
  ASSERT_EQ(KLEIDICV_OK,
            kleidicv_filter_context_create(&context, 1, 5, 5, 5, 5));
  test::Array2D<TypeParam> src{5, 5, test::Options::vector_length()};
  // clang-format off
  src.set(0, 0, { 255, 255, 255, 255, 255});
  src.set(1, 0, { 255, 255, 255, 255, 255});
  src.set(2, 0, { 255, 255, 255, 255, 255});
  src.set(3, 0, { 255, 255, 255, 255, 255});
  src.set(4, 0, { 255, 255, 255, 255, 255});
  // clang-format on

  test::Array2D<TypeParam> kernel_x{5, 1};
  kernel_x.set(0, 0, {255, 255, 255, 255, 255});
  test::Array2D<TypeParam> kernel_y{5, 1};
  kernel_y.set(0, 0, {255, 255, 255, 255, 255});

  test::Array2D<TypeParam> dst_expected{5, 5, test::Options::vector_length()};
  // clang-format off
  dst_expected.set(0, 0, { 255, 255, 255, 255, 255});
  dst_expected.set(1, 0, { 255, 255, 255, 255, 255});
  dst_expected.set(2, 0, { 255, 255, 255, 255, 255});
  dst_expected.set(3, 0, { 255, 255, 255, 255, 255});
  dst_expected.set(4, 0, { 255, 255, 255, 255, 255});
  // clang-format on

  test::Array2D<TypeParam> dst{5, 5, test::Options::vector_length()};
  EXPECT_EQ(KLEIDICV_OK, separable_filter_2d<TypeParam>()(
                             src.data(), src.stride(), dst.data(), dst.stride(),
                             5, 5, 1, kernel_x.data(), 5, kernel_y.data(), 5,
                             KLEIDICV_BORDER_TYPE_REPLICATE, context));
  EXPECT_EQ(KLEIDICV_OK, kleidicv_filter_context_release(context));
  EXPECT_EQ_ARRAY2D(dst_expected, dst);
}

TEST(SeparableFilter2D, 5x5_U8OverflowVectorNEON) {
  using TypeParam = uint8_t;

  kleidicv_filter_context_t *context = nullptr;
  ASSERT_EQ(KLEIDICV_OK,
            kleidicv_filter_context_create(&context, 1, 5, 5, 13, 6));
  test::Array2D<TypeParam> src{13, 6, test::Options::vector_length()};
  // clang-format off
  src.set(0, 0, { 232, 175,   8,  66, 167, 249, 190, 176,  89, 230, 120,  71,  14});
  src.set(1, 0, { 222, 254, 230, 253,  64, 127, 144,  43, 172, 110,  22, 232, 233});
  src.set(2, 0, { 106,  40,  40,  59,  18, 204, 247, 252, 179,  69, 163, 190,  58});
  src.set(3, 0, { 213,  22, 107, 111, 233,  10,  51,  17,  35,  14, 197, 157, 237});
  src.set(4, 0, {  96, 180, 160, 185, 146,  15, 103,  62, 227, 180, 249,  82,  83});
  src.set(5, 0, { 167, 150, 176, 149,  65, 246, 237, 234, 138,  51, 159, 218, 245});
  // clang-format on

  test::Array2D<TypeParam> kernel_x{5, 1};
  kernel_x.set(0, 0, {23, 149, 238, 48, 224});
  test::Array2D<TypeParam> kernel_y{5, 1};
  kernel_y.set(0, 0, {96, 254, 32, 81, 7});

  test::Array2D<TypeParam> dst_expected{13, 6, test::Options::vector_length()};
  // clang-format off
  dst_expected.set(0, 0, { 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255});
  dst_expected.set(1, 0, { 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255});
  dst_expected.set(2, 0, { 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255});
  dst_expected.set(3, 0, { 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255});
  dst_expected.set(4, 0, { 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255});
  dst_expected.set(5, 0, { 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255});
  // clang-format on

  test::Array2D<TypeParam> dst{13, 6, test::Options::vector_length()};
  EXPECT_EQ(KLEIDICV_OK, separable_filter_2d<TypeParam>()(
                             src.data(), src.stride(), dst.data(), dst.stride(),
                             13, 6, 1, kernel_x.data(), 5, kernel_y.data(), 5,
                             KLEIDICV_BORDER_TYPE_REPLICATE, context));
  EXPECT_EQ(KLEIDICV_OK, kleidicv_filter_context_release(context));
  EXPECT_EQ_ARRAY2D(dst_expected, dst);
}

TEST(SeparableFilter2D, 5x5_U8OverflowVectorSC) {
  using TypeParam = uint8_t;

  kleidicv_filter_context_t *context = nullptr;
  ASSERT_EQ(KLEIDICV_OK,
            kleidicv_filter_context_create(&context, 1, 5, 5, 13, 6));
  test::Array2D<TypeParam> src{13, 6, test::Options::vector_length()};
  // clang-format off
  src.set(0, 0, { 133, 210, 177,   6,   5, 200,   6, 242, 237,  80, 223, 253, 241});
  src.set(1, 0, { 112, 148, 209, 186, 188, 202,  18, 215, 193, 109, 226, 154, 207});
  src.set(2, 0, {  95, 216,  99, 161, 209, 183,  45, 226, 116, 210, 183,  11, 190});
  src.set(3, 0, { 237, 170,  10,  80, 207,  52,  69, 119,  68,  16, 239, 103,  25});
  src.set(4, 0, { 249, 106, 195, 207,  18, 123, 244,  63, 183,  13,  52, 196, 106});
  src.set(5, 0, {  66,  17, 191, 246, 246, 166, 137, 102,  84, 239, 245, 199, 144});
  // clang-format on

  test::Array2D<TypeParam> kernel_x{5, 1};
  kernel_x.set(0, 0, {99, 31, 197, 141, 71});
  test::Array2D<TypeParam> kernel_y{5, 1};
  kernel_y.set(0, 0, {60, 231, 86, 4, 140});

  test::Array2D<TypeParam> dst_expected{13, 6, test::Options::vector_length()};
  // clang-format off
  dst_expected.set(0, 0, { 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255});
  dst_expected.set(1, 0, { 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255});
  dst_expected.set(2, 0, { 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255});
  dst_expected.set(3, 0, { 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255});
  dst_expected.set(4, 0, { 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255});
  dst_expected.set(5, 0, { 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255});
  // clang-format on

  test::Array2D<TypeParam> dst{13, 6, test::Options::vector_length()};
  EXPECT_EQ(KLEIDICV_OK, separable_filter_2d<TypeParam>()(
                             src.data(), src.stride(), dst.data(), dst.stride(),
                             13, 6, 1, kernel_x.data(), 5, kernel_y.data(), 5,
                             KLEIDICV_BORDER_TYPE_REPLICATE, context));
  EXPECT_EQ(KLEIDICV_OK, kleidicv_filter_context_release(context));
  EXPECT_EQ_ARRAY2D(dst_expected, dst);
}

TEST(SeparableFilter2D, 5x5_U16OverflowSequence) {
  using TypeParam = uint16_t;

  kleidicv_filter_context_t *context = nullptr;
  ASSERT_EQ(KLEIDICV_OK,
            kleidicv_filter_context_create(&context, 1, 5, 5, 7, 8));
  test::Array2D<TypeParam> src{7, 8, test::Options::vector_length()};
  // clang-format off
  src.set(0, 0, { 1, 2, 3, 4, 5, 6, 7});
  src.set(1, 0, { 2, 3, 4, 5, 6, 7, 8});
  src.set(2, 0, { 3, 4, 5, 6, 7, 8, 9});
  src.set(3, 0, { 4, 5, 6, 7, 8, 9, 1});
  src.set(4, 0, { 5, 6, 7, 8, 9, 1, 2});
  src.set(5, 0, { 6, 7, 8, 9, 1, 2, 3});
  src.set(6, 0, { 7, 8, 9, 1, 2, 3, 4});
  src.set(7, 0, { 8, 9, 1, 2, 3, 4, 5});
  // clang-format on

  test::Array2D<TypeParam> kernel_x{5, 1};
  kernel_x.set(0, 0, {38, 0, 38, 0, 38});
  test::Array2D<TypeParam> kernel_y{5, 1};
  kernel_y.set(0, 0, {38, 0, 38, 0, 38});

  test::Array2D<TypeParam> dst_expected{7, 8, test::Options::vector_length()};
  // clang-format off
  dst_expected.set(0, 0, { 30324, 38988, 47652, 60648, 65535, 65535, 65535});
  dst_expected.set(1, 0, { 38988, 47652, 56316, 65535, 65535, 65535, 65535});
  dst_expected.set(2, 0, { 47652, 56316, 64980, 64980, 65535, 65535, 65535});
  dst_expected.set(3, 0, { 60648, 65535, 64980, 65535, 64980, 65535, 56316});
  dst_expected.set(4, 0, { 65535, 65535, 65535, 64980, 65535, 60648, 65535});
  dst_expected.set(5, 0, { 65535, 65535, 64980, 65535, 51984, 60648, 43320});
  dst_expected.set(6, 0, { 65535, 65535, 65535, 60648, 60648, 43320, 51984});
  dst_expected.set(7, 0, { 65535, 65535, 56316, 65535, 43320, 51984, 47652});
  // clang-format on

  test::Array2D<TypeParam> dst{7, 8, test::Options::vector_length()};
  EXPECT_EQ(KLEIDICV_OK, separable_filter_2d<TypeParam>()(
                             src.data(), src.stride(), dst.data(), dst.stride(),
                             7, 8, 1, kernel_x.data(), 5, kernel_y.data(), 5,
                             KLEIDICV_BORDER_TYPE_REPLICATE, context));
  EXPECT_EQ(KLEIDICV_OK, kleidicv_filter_context_release(context));
  EXPECT_EQ_ARRAY2D(dst_expected, dst);
}

TEST(SeparableFilter2D, 5x5_U16OverflowBigKernel) {
  using TypeParam = uint16_t;

  kleidicv_filter_context_t *context = nullptr;
  ASSERT_EQ(KLEIDICV_OK,
            kleidicv_filter_context_create(&context, 1, 5, 5, 7, 8));
  test::Array2D<TypeParam> src{7, 8, test::Options::vector_length()};
  // clang-format off
  src.set(0, 0, { 1, 2, 3, 4, 5, 6, 7});
  src.set(1, 0, { 2, 3, 4, 5, 6, 7, 8});
  src.set(2, 0, { 3, 4, 5, 6, 7, 8, 9});
  src.set(3, 0, { 4, 5, 6, 7, 8, 9, 1});
  src.set(4, 0, { 5, 6, 7, 8, 9, 1, 2});
  src.set(5, 0, { 6, 7, 8, 9, 1, 2, 3});
  src.set(6, 0, { 7, 8, 9, 1, 2, 3, 4});
  src.set(7, 0, { 8, 9, 1, 2, 3, 4, 5});
  // clang-format on

  test::Array2D<TypeParam> kernel_x{5, 1};
  kernel_x.set(0, 0, {83, 94, 83, 94, 83});
  test::Array2D<TypeParam> kernel_y{5, 1};
  kernel_y.set(0, 0, {94, 83, 94, 83, 94});

  test::Array2D<TypeParam> dst_expected{7, 8, test::Options::vector_length()};
  // clang-format off
  dst_expected.set(0, 0, { 65535, 65535, 65535, 65535, 65535, 65535, 65535});
  dst_expected.set(1, 0, { 65535, 65535, 65535, 65535, 65535, 65535, 65535});
  dst_expected.set(2, 0, { 65535, 65535, 65535, 65535, 65535, 65535, 65535});
  dst_expected.set(3, 0, { 65535, 65535, 65535, 65535, 65535, 65535, 65535});
  dst_expected.set(4, 0, { 65535, 65535, 65535, 65535, 65535, 65535, 65535});
  dst_expected.set(5, 0, { 65535, 65535, 65535, 65535, 65535, 65535, 65535});
  dst_expected.set(6, 0, { 65535, 65535, 65535, 65535, 65535, 65535, 65535});
  dst_expected.set(7, 0, { 65535, 65535, 65535, 65535, 65535, 65535, 65535});
  // clang-format on

  test::Array2D<TypeParam> dst{7, 8, test::Options::vector_length()};
  EXPECT_EQ(KLEIDICV_OK, separable_filter_2d<TypeParam>()(
                             src.data(), src.stride(), dst.data(), dst.stride(),
                             7, 8, 1, kernel_x.data(), 5, kernel_y.data(), 5,
                             KLEIDICV_BORDER_TYPE_REPLICATE, context));
  EXPECT_EQ(KLEIDICV_OK, kleidicv_filter_context_release(context));
  EXPECT_EQ_ARRAY2D(dst_expected, dst);
}

TEST(SeparableFilter2D, 5x5_U16OverflowMax) {
  using TypeParam = uint16_t;

  kleidicv_filter_context_t *context = nullptr;
  ASSERT_EQ(KLEIDICV_OK,
            kleidicv_filter_context_create(&context, 1, 5, 5, 7, 8));
  test::Array2D<TypeParam> src{7, 8, test::Options::vector_length()};
  // clang-format off
  src.set(0, 0, { 65535, 65535, 65535, 65535, 65535, 65535, 65535});
  src.set(1, 0, { 65535, 65535, 65535, 65535, 65535, 65535, 65535});
  src.set(2, 0, { 65535, 65535, 65535, 65535, 65535, 65535, 65535});
  src.set(3, 0, { 65535, 65535, 65535, 65535, 65535, 65535, 65535});
  src.set(4, 0, { 65535, 65535, 65535, 65535, 65535, 65535, 65535});
  src.set(5, 0, { 65535, 65535, 65535, 65535, 65535, 65535, 65535});
  src.set(6, 0, { 65535, 65535, 65535, 65535, 65535, 65535, 65535});
  src.set(7, 0, { 65535, 65535, 65535, 65535, 65535, 65535, 65535});
  // clang-format on

  test::Array2D<TypeParam> kernel_x{5, 1};
  kernel_x.set(0, 0, {65535, 65535, 65535, 65535, 65535});
  test::Array2D<TypeParam> kernel_y{5, 1};
  kernel_y.set(0, 0, {65535, 65535, 65535, 65535, 65535});

  test::Array2D<TypeParam> dst_expected{7, 8, test::Options::vector_length()};
  // clang-format off
  dst_expected.set(0, 0, { 65535, 65535, 65535, 65535, 65535, 65535, 65535});
  dst_expected.set(1, 0, { 65535, 65535, 65535, 65535, 65535, 65535, 65535});
  dst_expected.set(2, 0, { 65535, 65535, 65535, 65535, 65535, 65535, 65535});
  dst_expected.set(3, 0, { 65535, 65535, 65535, 65535, 65535, 65535, 65535});
  dst_expected.set(4, 0, { 65535, 65535, 65535, 65535, 65535, 65535, 65535});
  dst_expected.set(5, 0, { 65535, 65535, 65535, 65535, 65535, 65535, 65535});
  dst_expected.set(6, 0, { 65535, 65535, 65535, 65535, 65535, 65535, 65535});
  dst_expected.set(7, 0, { 65535, 65535, 65535, 65535, 65535, 65535, 65535});
  // clang-format on

  test::Array2D<TypeParam> dst{7, 8, test::Options::vector_length()};
  EXPECT_EQ(KLEIDICV_OK, separable_filter_2d<TypeParam>()(
                             src.data(), src.stride(), dst.data(), dst.stride(),
                             7, 8, 1, kernel_x.data(), 5, kernel_y.data(), 5,
                             KLEIDICV_BORDER_TYPE_REPLICATE, context));
  EXPECT_EQ(KLEIDICV_OK, kleidicv_filter_context_release(context));
  EXPECT_EQ_ARRAY2D(dst_expected, dst);
}

TEST(SeparableFilter2D, 5x5_U16OverflowVector) {
  using TypeParam = uint16_t;

  kleidicv_filter_context_t *context = nullptr;
  ASSERT_EQ(KLEIDICV_OK,
            kleidicv_filter_context_create(&context, 1, 5, 5, 18, 7));
  test::Array2D<TypeParam> src{18, 7, test::Options::vector_length()};
  // clang-format off
  src.set(0, 0, {  7069, 15555, 36257, 50924, 19919, 14775,  5812, 63033,
    12337, 31198, 64955, 38064, 52102, 33736, 44794, 28036, 28418, 51544});
  src.set(1, 0, { 56176, 39501, 12937, 60165, 41073, 42249, 26998,  8958,
    17167,   567, 49467, 56007,  9385, 49384, 52038,  3262, 42863, 57617});
  src.set(2, 0, { 53432,  9693, 54092,   741,   835, 61755,  3707,  3429,
    20223, 65475, 42973,  9837, 41947, 41431, 53538,  2774, 50094, 65193});
  src.set(3, 0, {  2673, 45570,  2199, 38120, 55556,  7612, 53485, 44718,
    16967, 60551, 63543, 55699, 45352, 58886, 52300, 36045, 16187,  6794});
  src.set(4, 0, { 50260, 62222, 30989, 44610, 41729, 64829, 48408, 62415,
    20341, 13347, 26792,  9543, 45732,  3551, 43217, 41365,  4666, 41742});
  src.set(5, 0, { 55105, 31681, 64645, 51293, 43515,  8779, 43396, 12372,
    37819, 61444, 10427, 49746, 12989, 58916, 27310, 46273, 60514, 59064});
  src.set(6, 0, { 40983, 23334, 50325, 15939, 50201, 54234,  2318,  5649,
    32631, 44612, 49516, 36557, 20168, 17045, 40077, 60173, 61168,  3247});
  // clang-format on

  test::Array2D<TypeParam> kernel_x{5, 1};
  kernel_x.set(0, 0, {60064, 6000, 11871, 49673, 48017});
  test::Array2D<TypeParam> kernel_y{5, 1};
  kernel_y.set(0, 0, {8956, 29661, 59112, 41299, 41083});

  test::Array2D<TypeParam> dst_expected{18, 7, test::Options::vector_length()};
  // clang-format off
  dst_expected.set(0, 0, { 65535, 65535, 65535, 65535, 65535, 65535, 65535, 65535,
    65535, 65535, 65535, 65535, 65535, 65535, 65535, 65535, 65535, 65535});
  dst_expected.set(1, 0, { 65535, 65535, 65535, 65535, 65535, 65535, 65535, 65535,
    65535, 65535, 65535, 65535, 65535, 65535, 65535, 65535, 65535, 65535});
  dst_expected.set(2, 0, { 65535, 65535, 65535, 65535, 65535, 65535, 65535, 65535,
    65535, 65535, 65535, 65535, 65535, 65535, 65535, 65535, 65535, 65535});
  dst_expected.set(3, 0, { 65535, 65535, 65535, 65535, 65535, 65535, 65535, 65535,
    65535, 65535, 65535, 65535, 65535, 65535, 65535, 65535, 65535, 65535});
  dst_expected.set(4, 0, { 65535, 65535, 65535, 65535, 65535, 65535, 65535, 65535,
    65535, 65535, 65535, 65535, 65535, 65535, 65535, 65535, 65535, 65535});
  dst_expected.set(5, 0, { 65535, 65535, 65535, 65535, 65535, 65535, 65535, 65535,
    65535, 65535, 65535, 65535, 65535, 65535, 65535, 65535, 65535, 65535});
  dst_expected.set(6, 0, { 65535, 65535, 65535, 65535, 65535, 65535, 65535, 65535,
    65535, 65535, 65535, 65535, 65535, 65535, 65535, 65535, 65535, 65535});
  // clang-format on

  test::Array2D<TypeParam> dst{18, 7, test::Options::vector_length()};
  EXPECT_EQ(KLEIDICV_OK, separable_filter_2d<TypeParam>()(
                             src.data(), src.stride(), dst.data(), dst.stride(),
                             18, 7, 1, kernel_x.data(), 5, kernel_y.data(), 5,
                             KLEIDICV_BORDER_TYPE_REPLICATE, context));
  EXPECT_EQ(KLEIDICV_OK, kleidicv_filter_context_release(context));
  EXPECT_EQ_ARRAY2D(dst_expected, dst);
}

TYPED_TEST(SeparableFilter2D, NullPointer) {
  using KernelTestParams = SeparableFilter2DKernelTestParams<TypeParam, 5>;
  kleidicv_filter_context_t *context = nullptr;
  size_t validSize = KernelTestParams::kKernelSize - 1;
  ASSERT_EQ(KLEIDICV_OK, kleidicv_filter_context_create(&context, 1, 5, 5,
                                                        validSize, validSize));
  TypeParam src[1] = {}, dst[1], kernel[5] = {};
  test::test_null_args(separable_filter_2d<TypeParam>(), src, sizeof(TypeParam),
                       dst, sizeof(TypeParam), validSize, validSize, 1, kernel,
                       5, kernel, 5, KLEIDICV_BORDER_TYPE_REPLICATE, context);
  EXPECT_EQ(KLEIDICV_OK, kleidicv_filter_context_release(context));
}

TYPED_TEST(SeparableFilter2D, ZeroImageSize) {
  TypeParam src[1] = {}, dst[1], kernel[5] = {};
  kleidicv_filter_context_t *context = nullptr;
  ASSERT_EQ(KLEIDICV_OK,
            kleidicv_filter_context_create(&context, 1, 5, 5, 1, 1));
  EXPECT_EQ(KLEIDICV_ERROR_NOT_IMPLEMENTED,
            separable_filter_2d<TypeParam>()(
                src, sizeof(TypeParam), dst, sizeof(TypeParam), 0, 5, 1, kernel,
                5, kernel, 5, KLEIDICV_BORDER_TYPE_REPLICATE, context));
  EXPECT_EQ(KLEIDICV_ERROR_NOT_IMPLEMENTED,
            separable_filter_2d<TypeParam>()(
                src, sizeof(TypeParam), dst, sizeof(TypeParam), 5, 0, 1, kernel,
                5, kernel, 5, KLEIDICV_BORDER_TYPE_REPLICATE, context));
  EXPECT_EQ(KLEIDICV_OK, kleidicv_filter_context_release(context));
}

TYPED_TEST(SeparableFilter2D, ValidImageSize) {
  using KernelTestParams = SeparableFilter2DKernelTestParams<TypeParam, 5>;
  kleidicv_filter_context_t *context = nullptr;
  size_t validSize = KernelTestParams::kKernelSize - 1;
  ASSERT_EQ(KLEIDICV_OK, kleidicv_filter_context_create(&context, 1, 5, 5,
                                                        validSize, validSize));
  test::Array2D<TypeParam> src{validSize, validSize,
                               test::Options::vector_length()};
  test::Array2D<TypeParam> dst{validSize, validSize,
                               test::Options::vector_length()};
  TypeParam kernel[5] = {};
  EXPECT_EQ(KLEIDICV_OK, separable_filter_2d<TypeParam>()(
                             src.data(), src.stride(), dst.data(), dst.stride(),
                             validSize, validSize, 1, kernel, 5, kernel, 5,
                             KLEIDICV_BORDER_TYPE_REPLICATE, context));
  EXPECT_EQ(KLEIDICV_OK, kleidicv_filter_context_release(context));
}

TYPED_TEST(SeparableFilter2D, UndersizeImage) {
  using KernelTestParams = SeparableFilter2DKernelTestParams<TypeParam, 5>;
  kleidicv_filter_context_t *context = nullptr;
  size_t underSize = KernelTestParams::kKernelSize - 2;
  size_t validSize = KernelTestParams::kKernelSize - 1;
  ASSERT_EQ(KLEIDICV_OK, kleidicv_filter_context_create(&context, 1, 5, 5,
                                                        validSize, validSize));
  TypeParam src[1] = {}, dst[1], kernel[5] = {};
  EXPECT_EQ(
      KLEIDICV_ERROR_NOT_IMPLEMENTED,
      separable_filter_2d<TypeParam>()(
          src, sizeof(TypeParam), dst, sizeof(TypeParam), underSize, underSize,
          1, kernel, 5, kernel, 5, KLEIDICV_BORDER_TYPE_REPLICATE, context));
  EXPECT_EQ(
      KLEIDICV_ERROR_NOT_IMPLEMENTED,
      separable_filter_2d<TypeParam>()(
          src, sizeof(TypeParam), dst, sizeof(TypeParam), underSize, validSize,
          1, kernel, 5, kernel, 5, KLEIDICV_BORDER_TYPE_REPLICATE, context));
  EXPECT_EQ(
      KLEIDICV_ERROR_NOT_IMPLEMENTED,
      separable_filter_2d<TypeParam>()(
          src, sizeof(TypeParam), dst, sizeof(TypeParam), validSize, underSize,
          1, kernel, 5, kernel, 5, KLEIDICV_BORDER_TYPE_REPLICATE, context));
  EXPECT_EQ(KLEIDICV_OK, kleidicv_filter_context_release(context));
}

TYPED_TEST(SeparableFilter2D, OversizeImage) {
  kleidicv_filter_context_t *context = nullptr;
  ASSERT_EQ(KLEIDICV_OK,
            kleidicv_filter_context_create(&context, 1, 5, 5, 1, 1));
  TypeParam src[1], dst[1], kernel[5] = {};
  EXPECT_EQ(KLEIDICV_ERROR_RANGE,
            separable_filter_2d<TypeParam>()(
                src, sizeof(TypeParam), dst, sizeof(TypeParam),
                KLEIDICV_MAX_IMAGE_PIXELS + 1, 1, 1, kernel, 5, kernel, 5,
                KLEIDICV_BORDER_TYPE_REPLICATE, context));
  EXPECT_EQ(KLEIDICV_ERROR_RANGE,
            separable_filter_2d<TypeParam>()(
                src, sizeof(TypeParam), dst, sizeof(TypeParam),
                KLEIDICV_MAX_IMAGE_PIXELS, KLEIDICV_MAX_IMAGE_PIXELS, 1, kernel,
                5, kernel, 5, KLEIDICV_BORDER_TYPE_REPLICATE, context));
  EXPECT_EQ(KLEIDICV_OK, kleidicv_filter_context_release(context));
}

TYPED_TEST(SeparableFilter2D, ChannelNumber) {
  using KernelTestParams = SeparableFilter2DKernelTestParams<TypeParam, 5>;
  kleidicv_filter_context_t *context = nullptr;
  size_t validSize = KernelTestParams::kKernelSize - 1;

  ASSERT_EQ(KLEIDICV_OK, kleidicv_filter_context_create(&context, 1, 5, 5,
                                                        validSize, validSize));
  TypeParam src[1], dst[1], kernel[5] = {};
  EXPECT_EQ(KLEIDICV_ERROR_RANGE,
            separable_filter_2d<TypeParam>()(
                src, sizeof(TypeParam), dst, sizeof(TypeParam), validSize,
                validSize, KLEIDICV_MAXIMUM_CHANNEL_COUNT + 1, kernel, 5,
                kernel, 5, KLEIDICV_BORDER_TYPE_REPLICATE, context));
  EXPECT_EQ(KLEIDICV_OK, kleidicv_filter_context_release(context));
}

TYPED_TEST(SeparableFilter2D, InvalidContextMaxChannels) {
  using KernelTestParams = SeparableFilter2DKernelTestParams<TypeParam, 5>;
  kleidicv_filter_context_t *context = nullptr;
  size_t validSize = KernelTestParams::kKernelSize - 1;

  ASSERT_EQ(KLEIDICV_OK, kleidicv_filter_context_create(&context, 1, 5, 5,
                                                        validSize, validSize));
  TypeParam src[1], dst[1], kernel[5] = {};
  EXPECT_EQ(
      KLEIDICV_ERROR_CONTEXT_MISMATCH,
      separable_filter_2d<TypeParam>()(
          src, sizeof(TypeParam), dst, sizeof(TypeParam), validSize, validSize,
          2, kernel, 5, kernel, 5, KLEIDICV_BORDER_TYPE_REPLICATE, context));
  EXPECT_EQ(KLEIDICV_OK, kleidicv_filter_context_release(context));
}

TYPED_TEST(SeparableFilter2D, InvalidContextImageSize) {
  using KernelTestParams = SeparableFilter2DKernelTestParams<TypeParam, 5>;
  kleidicv_filter_context_t *context = nullptr;
  size_t validSize = KernelTestParams::kKernelSize - 1;

  ASSERT_EQ(KLEIDICV_OK, kleidicv_filter_context_create(&context, 1, 5, 5,
                                                        validSize, validSize));
  TypeParam src[1], dst[1], kernel[5] = {};
  EXPECT_EQ(KLEIDICV_ERROR_CONTEXT_MISMATCH,
            separable_filter_2d<TypeParam>()(
                src, sizeof(TypeParam), dst, sizeof(TypeParam), validSize + 1,
                validSize, 1, kernel, 5, kernel, 5,
                KLEIDICV_BORDER_TYPE_REPLICATE, context));
  EXPECT_EQ(KLEIDICV_ERROR_CONTEXT_MISMATCH,
            separable_filter_2d<TypeParam>()(
                src, sizeof(TypeParam), dst, sizeof(TypeParam), validSize,
                validSize + 1, 1, kernel, 5, kernel, 5,
                KLEIDICV_BORDER_TYPE_REPLICATE, context));
  EXPECT_EQ(KLEIDICV_ERROR_CONTEXT_MISMATCH,
            separable_filter_2d<TypeParam>()(
                src, sizeof(TypeParam), dst, sizeof(TypeParam), validSize + 1,
                validSize + 1, 1, kernel, 5, kernel, 5,
                KLEIDICV_BORDER_TYPE_REPLICATE, context));

  EXPECT_EQ(KLEIDICV_OK, kleidicv_filter_context_release(context));
}

TYPED_TEST(SeparableFilter2D, InvalidKernelSize) {
  kleidicv_filter_context_t *context = nullptr;
  constexpr size_t kernel_size = 17;

  ASSERT_EQ(KLEIDICV_OK, kleidicv_filter_context_create(
                             &context, 1, kernel_size, kernel_size, kernel_size,
                             kernel_size));
  TypeParam src[kernel_size], dst[kernel_size], kernel[kernel_size] = {};
  EXPECT_EQ(KLEIDICV_ERROR_NOT_IMPLEMENTED,
            separable_filter_2d<TypeParam>()(
                src, sizeof(TypeParam), dst, sizeof(TypeParam), kernel_size,
                kernel_size, 1, kernel, kernel_size, kernel, 5,
                KLEIDICV_BORDER_TYPE_REPLICATE, context));
  EXPECT_EQ(KLEIDICV_ERROR_NOT_IMPLEMENTED,
            separable_filter_2d<TypeParam>()(
                src, sizeof(TypeParam), dst, sizeof(TypeParam), kernel_size,
                kernel_size, 1, kernel, 5, kernel, kernel_size,
                KLEIDICV_BORDER_TYPE_REPLICATE, context));
  EXPECT_EQ(KLEIDICV_OK, kleidicv_filter_context_release(context));
}

TYPED_TEST(SeparableFilter2D, InvalidBorderType) {
  using KernelTestParams = SeparableFilter2DKernelTestParams<TypeParam, 5>;
  kleidicv_filter_context_t *context = nullptr;
  size_t validSize = KernelTestParams::kKernelSize - 1;

  ASSERT_EQ(KLEIDICV_OK, kleidicv_filter_context_create(&context, 1, 5, 5,
                                                        validSize, validSize));
  TypeParam src[1], dst[1], kernel[5] = {};
  EXPECT_EQ(
      KLEIDICV_ERROR_NOT_IMPLEMENTED,
      separable_filter_2d<TypeParam>()(
          src, sizeof(TypeParam), dst, sizeof(TypeParam), validSize, validSize,
          1, kernel, 5, kernel, 5, KLEIDICV_BORDER_TYPE_CONSTANT, context));
  EXPECT_EQ(
      KLEIDICV_ERROR_NOT_IMPLEMENTED,
      separable_filter_2d<TypeParam>()(
          src, sizeof(TypeParam), dst, sizeof(TypeParam), validSize, validSize,
          1, kernel, 5, kernel, 5, KLEIDICV_BORDER_TYPE_TRANSPARENT, context));
  EXPECT_EQ(
      KLEIDICV_ERROR_NOT_IMPLEMENTED,
      separable_filter_2d<TypeParam>()(
          src, sizeof(TypeParam), dst, sizeof(TypeParam), validSize, validSize,
          1, kernel, 5, kernel, 5, KLEIDICV_BORDER_TYPE_NONE, context));
  EXPECT_EQ(KLEIDICV_OK, kleidicv_filter_context_release(context));
}

#ifdef KLEIDICV_ALLOCATION_TESTS
TEST(FilterCreate, CannotAllocateFilter) {
  MockMallocToFail::enable();
  kleidicv_filter_context_t *context = nullptr;
  EXPECT_EQ(KLEIDICV_ERROR_ALLOCATION,
            kleidicv_filter_context_create(&context, 1, 1, 1,
                                           KLEIDICV_MAX_IMAGE_PIXELS, 1));
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
              kleidicv_filter_context_create(&context, 1, 1, 1, rect.width,
                                             rect.height));
    ASSERT_EQ(nullptr, context);
  }
}

TEST(FilterCreate, DifferentKernelSize) {
  kleidicv_filter_context_t *context = nullptr;

  EXPECT_EQ(KLEIDICV_ERROR_NOT_IMPLEMENTED,
            kleidicv_filter_context_create(&context, 1, 7, 15, 1, 1));
  ASSERT_EQ(nullptr, context);
}

TEST(FilterCreate, ChannelNumber) {
  kleidicv_filter_context_t *context = nullptr;

  EXPECT_EQ(KLEIDICV_ERROR_RANGE,
            kleidicv_filter_context_create(
                &context, KLEIDICV_MAXIMUM_CHANNEL_COUNT + 1, 1, 1, 1, 1));
  ASSERT_EQ(nullptr, context);
}

TEST(FilterCreate, NullPointer) {
  EXPECT_EQ(KLEIDICV_ERROR_NULL_POINTER,
            kleidicv_filter_context_create(nullptr, 1, 1, 1, 1, 1));
}

TEST(FilterRelease, NullPointer) {
  EXPECT_EQ(KLEIDICV_ERROR_NULL_POINTER,
            kleidicv_filter_context_release(nullptr));
}
