// SPDX-FileCopyrightText: 2024 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#include <gtest/gtest.h>

#include <algorithm>
#include <array>

#include "framework/array.h"
#include "framework/generator.h"
#include "framework/kernel.h"
#include "framework/operation.h"
#include "kleidicv/kleidicv.h"
#include "test_config.h"

#define KLEIDICV_PARAMS(name, impl, type, op)                                 \
  template <typename ElementType,                                             \
            std::enable_if_t<std::is_same_v<ElementType, type>, bool> = true> \
  class name {                                                                \
   public:                                                                    \
    static decltype(auto) api() { return impl; }                              \
    static decltype(auto) operation() {                                       \
      return [](type a, type b) { return op<type>(a, b); };                   \
    }                                                                         \
  };

KLEIDICV_PARAMS(DilateParams, kleidicv_dilate_u8, uint8_t, std::max);
KLEIDICV_PARAMS(ErodeParams, kleidicv_erode_u8, uint8_t, std::min);

template <typename ElementType>
struct MorphologyKernelTestParams {
  using InputType = ElementType;
  using IntermediateType = ElementType;
  using OutputType = ElementType;
};  // end of struct MorphologyKernelTestParams

static constexpr std::array<kleidicv_border_type_t, 1> kDefaultBorder = {
    KLEIDICV_BORDER_TYPE_REPLICATE};

static constexpr std::array<kleidicv_border_type_t, 1> kConstantBorder = {
    KLEIDICV_BORDER_TYPE_CONSTANT};

static constexpr std::array<kleidicv_border_values_t, 1> kDefaultBorderValues =
    {{
        {0, 0, 0, 0},  // default
    }};

template <typename ElementType>
static const std::array<kleidicv_border_values_t, 4> &more_border_values() {
  using limit = std::numeric_limits<ElementType>;
  static const std::array<kleidicv_border_values_t, 4> values = {
      {{0, 0, 0, 0},  // default
       {7, 42, 99, 9},
       {limit::min(), limit::max(), limit::min(), limit::max()},
       {0, limit::min(), limit::max(), 0}}};
  return values;
}

template <typename IterableType>
std::unique_ptr<test::Generator<typename IterableType::value_type>>
make_generator_ptr(IterableType &elements) {
  test::Generator<typename IterableType::value_type> *pg =
      new test::SequenceGenerator(elements);
  return std::unique_ptr<test::Generator<typename IterableType::value_type>>(
      pg);
}

template <class ElementType, template <typename, auto...> class OperationParams,
          size_t kernelWidth, size_t kernelHeight>
class MorphologyTest
    : public test::KernelTest<MorphologyKernelTestParams<ElementType>> {
  using Base = test::KernelTest<MorphologyKernelTestParams<ElementType>>;
  using typename Base::InputType;
  using typename Base::IntermediateType;
  using typename Base::OutputType;

 public:
  MorphologyTest()
      : mask_{kernelWidth, kernelHeight},
        kernel_{mask_},
        iterations_{1},
        small_array_layouts_{
            test::small_array_layouts(kernelWidth, kernelHeight)} {
    array_layout_generator_ = make_generator_ptr(small_array_layouts_);
    border_type_generator_ = make_generator_ptr(kDefaultBorder);
    border_values_generator_ = make_generator_ptr(kDefaultBorderValues);
  }

  MorphologyTest &with_anchor(test::Point anchor) {
    kernel_ = test::Kernel(mask_, anchor);
    return *this;
  }

  MorphologyTest &with_iterations(size_t iter) {
    iterations_ = iter;
    return *this;
  }

  MorphologyTest &with_array_layouts(
      std::unique_ptr<test::Generator<test::ArrayLayout>> g) {
    array_layout_generator_ = std::move(g);
    return *this;
  }

  MorphologyTest &with_border_types(
      std::unique_ptr<test::Generator<kleidicv_border_type_t>> g) {
    border_type_generator_ = std::move(g);
    return *this;
  }

  MorphologyTest &with_border_values(
      std::unique_ptr<test::Generator<kleidicv_border_values_t>> g) {
    border_values_generator_ = std::move(g);
    return *this;
  }

  void test() {
    test::PseudoRandomNumberGenerator<InputType> element_generator;
    Base::test(kernel_, *array_layout_generator_, *border_type_generator_,
               *border_values_generator_, element_generator);
  }

 protected:
  test::Array2D<InputType> mask_;
  test::Kernel<InputType> kernel_;
  size_t iterations_;
  std::array<test::ArrayLayout, 6> small_array_layouts_;
  std::unique_ptr<test::Generator<test::ArrayLayout>> array_layout_generator_;
  std::unique_ptr<test::Generator<kleidicv_border_type_t>>
      border_type_generator_;
  std::unique_ptr<test::Generator<kleidicv_border_values_t>>
      border_values_generator_;

  kleidicv_error_t call_api(const test::Array2D<InputType> *input,
                            test::Array2D<OutputType> *output,
                            kleidicv_border_type_t border_type,
                            kleidicv_border_values_t border_values) override {
    kleidicv_morphology_context_t *context = nullptr;
    auto kernelRect = kleidicv_rectangle_t{kernelWidth, kernelHeight};
    kleidicv_point_t anchor{kernel_.anchor().x, kernel_.anchor().y};
    auto ret = kleidicv_morphology_create(
        &context, kernelRect, anchor, border_type, border_values,
        input->channels(), iterations_, sizeof(InputType),
        kleidicv_rectangle_t{input->width() / input->channels(),
                             input->height()});
    if (ret != KLEIDICV_OK) {
      return ret;
    }

    ret = OperationParams<InputType>::api()(
        input->data(), input->stride(), output->data(), output->stride(),
        input->width() / input->channels(), input->height(), context);
    auto releaseRet = kleidicv_morphology_release(context);
    if (releaseRet != KLEIDICV_OK) {
      return releaseRet;
    }

    return ret;
  }

  void prepare_expected(const test::Kernel<IntermediateType> &kernel,
                        const test::ArrayLayout &array_layout,
                        kleidicv_border_type_t border_type,
                        kleidicv_border_values_t border_values) override {
    Base::prepare_expected(kernel, array_layout, border_type, border_values);
    if (iterations_ > 1) {
      test::Array2D<InputType> saved_input = this->input_;
      for (size_t i = 1; i < iterations_; ++i) {
        this->input_ = this->expected_;
        Base::prepare_expected(kernel, array_layout, border_type,
                               border_values);
      }
      this->input_ = saved_input;
    }
  }

  IntermediateType calculate_expected_at(
      const test::Kernel<IntermediateType> &kernel,
      const test::TwoDimensional<InputType> &source, size_t row,
      size_t column) override {
    IntermediateType result = source.at(row, column)[0];
    for (size_t height = 0; height < kernel.height(); ++height) {
      for (size_t width = 0; width < kernel.width(); ++width) {
        result = OperationParams<InputType>::operation()(
            result,
            source.at(row + height, column + width * source.channels())[0]);
      }
    }
    return result;
  }
};  // end of class class MorphologyTest<OperationParams, kernelWidth,
    // kernelHeight>

template <typename TypeParam>
class Morphology : public testing::Test {};

using ElementTypes = ::testing::Types<uint8_t>;

TYPED_TEST_SUITE(Morphology, ElementTypes);

TYPED_TEST(Morphology, 1xN) {
  std::array<test::ArrayLayout, 14> medium_array_layouts_3x3 =
      test::default_array_layouts(3, 3);

  MorphologyTest<TypeParam, DilateParams, 1, 1>{}.test();
  MorphologyTest<TypeParam, ErodeParams, 1, 1>{}.test();
  MorphologyTest<TypeParam, DilateParams, 1, 2>{}
      .with_array_layouts(make_generator_ptr(medium_array_layouts_3x3))
      .test();
  MorphologyTest<TypeParam, ErodeParams, 1, 2>{}
      .with_array_layouts(make_generator_ptr(medium_array_layouts_3x3))
      .test();
  MorphologyTest<TypeParam, DilateParams, 3, 1>{}.test();
  MorphologyTest<TypeParam, ErodeParams, 3, 1>{}.test();
}

std::array<test::ArrayLayout, 4> get_large_array_layouts(size_t min_width,
                                                         size_t min_height) {
  size_t vl = test::Options::vector_length();
  size_t big_height = std::max(2 * vl + 1, min_height * 4);

  return {{
      // clang-format off
      //         width,         height,  padding, channels
      {  min_width * 8,     min_height,        0,        8},
      {  min_width * 8,     min_height,       vl,        8},
      {  min_width * 2,     big_height,        0,        1},
      {  min_width * 2,     big_height,       vl,        1},
      // clang-format on
  }};
}

TYPED_TEST(Morphology, LargeArrays) {
  std::array<test::ArrayLayout, 4> large_array_layouts =
      get_large_array_layouts(3, 3);

  MorphologyTest<TypeParam, DilateParams, 3, 3>{}
      .with_array_layouts(make_generator_ptr(large_array_layouts))
      .test();
  MorphologyTest<TypeParam, ErodeParams, 3, 3>{}
      .with_array_layouts(make_generator_ptr(large_array_layouts))
      .test();
  MorphologyTest<TypeParam, ErodeParams, 3, 3>{}
      .with_border_types(make_generator_ptr(kConstantBorder))
      .with_array_layouts(make_generator_ptr(large_array_layouts))
      .test();
  MorphologyTest<TypeParam, DilateParams, 3, 3>{}
      .with_border_types(make_generator_ptr(kConstantBorder))
      .with_array_layouts(make_generator_ptr(large_array_layouts))
      .test();
}

TYPED_TEST(Morphology, MediumArrays) {
  std::array<test::ArrayLayout, 14> medium_array_layouts_3x3 =
      test::default_array_layouts(3, 3);
  MorphologyTest<TypeParam, DilateParams, 3, 3>{}
      .with_array_layouts(make_generator_ptr(medium_array_layouts_3x3))
      .test();
  MorphologyTest<TypeParam, ErodeParams, 3, 3>{}
      .with_array_layouts(make_generator_ptr(medium_array_layouts_3x3))
      .test();
  std::array<test::ArrayLayout, 14> medium_array_layouts_5x5 =
      test::default_array_layouts(5, 5);
  MorphologyTest<TypeParam, DilateParams, 5, 5>{}
      .with_array_layouts(make_generator_ptr(medium_array_layouts_5x5))
      .test();
  MorphologyTest<TypeParam, ErodeParams, 5, 5>{}
      .with_array_layouts(make_generator_ptr(medium_array_layouts_5x5))
      .test();
}

TYPED_TEST(Morphology, BorderValues) {
  MorphologyTest<TypeParam, DilateParams, 3, 3>{}
      .with_border_types(make_generator_ptr(kConstantBorder))
      .with_border_values(make_generator_ptr(more_border_values<TypeParam>()))
      .test();
  MorphologyTest<TypeParam, ErodeParams, 3, 3>{}
      .with_border_types(make_generator_ptr(kConstantBorder))
      .with_border_values(make_generator_ptr(more_border_values<TypeParam>()))
      .test();
}

TYPED_TEST(Morphology, UnortodoxSizes) {
  MorphologyTest<TypeParam, DilateParams, 4, 4>{}.test();
  MorphologyTest<TypeParam, ErodeParams, 7, 5>{}.test();
  MorphologyTest<TypeParam, DilateParams, 8, 4>{}.test();
  MorphologyTest<TypeParam, DilateParams, 6, 10>{}.test();
  MorphologyTest<TypeParam, ErodeParams, 12, 4>{}.test();
}

TYPED_TEST(Morphology, Iterations) {
  MorphologyTest<TypeParam, DilateParams, 3, 3>{}.with_iterations(2).test();
  MorphologyTest<TypeParam, ErodeParams, 6, 4>{}.with_iterations(3).test();
  MorphologyTest<TypeParam, DilateParams, 2, 7>{}.with_iterations(4).test();
}

TYPED_TEST(Morphology, Anchors) {
  MorphologyTest<TypeParam, ErodeParams, 3, 5>{}.with_anchor({0, 0}).test();
  MorphologyTest<TypeParam, DilateParams, 3, 5>{}
      .with_border_types(make_generator_ptr(kConstantBorder))
      .with_anchor({2, 0})
      .test();
  MorphologyTest<TypeParam, ErodeParams, 3, 5>{}
      .with_border_types(make_generator_ptr(kConstantBorder))
      .with_anchor({0, 4})
      .test();
  MorphologyTest<TypeParam, DilateParams, 3, 5>{}.with_anchor({2, 4}).test();
}

static kleidicv_error_t make_minimal_context(
    kleidicv_morphology_context_t **context, size_t type_size,
    kleidicv_border_type_t border = KLEIDICV_BORDER_TYPE_REPLICATE) {
  return kleidicv_morphology_create(context, kleidicv_rectangle_t{1, 1},
                                    kleidicv_point_t{0, 0}, border,
                                    kleidicv_border_values_t{0, 0, 1, 1}, 1, 1,
                                    type_size, kleidicv_rectangle_t{1, 1});
}

template <typename ElementType>
static void test_valid_image_size(kleidicv_rectangle_t kernel,
                                  test::Array2D<ElementType> src) {
  size_t validSize = kernel.width - 1;
  kleidicv_rectangle_t image{validSize, validSize};
  kleidicv_border_values_t border_values{0, 0, 1, 1};

  test::Array2D<ElementType> dst{validSize, validSize,
                                 test::Options::vector_length()};

  for (size_t x = 0; x < kernel.width; x += kernel.width - 1) {
    for (size_t y = 0; y < kernel.width; y += kernel.width - 1) {
      kleidicv_point_t anchor{x, y};
      for (kleidicv_border_type_t border : {
               KLEIDICV_BORDER_TYPE_REPLICATE,
               KLEIDICV_BORDER_TYPE_CONSTANT,
           }) {
        kleidicv_morphology_context_t *context = nullptr;
        ASSERT_EQ(KLEIDICV_OK,
                  kleidicv_morphology_create(&context, kernel, anchor, border,
                                             border_values, 1, 1,
                                             sizeof(ElementType), image));
        EXPECT_EQ(KLEIDICV_OK,
                  ErodeParams<ElementType>::api()(
                      src.data(), src.stride(), dst.data(), dst.stride(),
                      validSize, validSize, context));
        EXPECT_EQ(KLEIDICV_OK,
                  DilateParams<ElementType>::api()(
                      src.data(), src.stride(), dst.data(), dst.stride(),
                      validSize, validSize, context));
        EXPECT_EQ(KLEIDICV_OK, kleidicv_morphology_release(context));
      }
    }
  }
}

template <typename ElementType>
static void test_undersize_image(kleidicv_rectangle_t kernel) {
  size_t underSize = kernel.width - 2;
  size_t validWidth = kernel.width + 10;
  size_t validHeight = kernel.height + 5;
  kleidicv_morphology_context_t *context = nullptr;
  kleidicv_rectangle_t image{underSize, underSize};
  kleidicv_rectangle_t imageW{underSize, validHeight};
  kleidicv_rectangle_t imageH{validWidth, underSize};
  kleidicv_border_type_t border = KLEIDICV_BORDER_TYPE_REPLICATE;
  kleidicv_border_values_t border_values{0, 0, 1, 1};
  kleidicv_point_t anchor{1, 1};
  ElementType src[1], dst[1];
  ASSERT_EQ(KLEIDICV_OK, kleidicv_morphology_create(
                             &context, kernel, anchor, border, border_values, 1,
                             1, sizeof(ElementType), image));
  EXPECT_EQ(KLEIDICV_ERROR_NOT_IMPLEMENTED,
            ErodeParams<ElementType>::api()(src, sizeof(ElementType), dst,
                                            sizeof(ElementType), underSize,
                                            underSize, context));
  EXPECT_EQ(KLEIDICV_ERROR_NOT_IMPLEMENTED,
            DilateParams<ElementType>::api()(src, sizeof(ElementType), dst,
                                             sizeof(ElementType), underSize,
                                             underSize, context));
  EXPECT_EQ(KLEIDICV_OK, kleidicv_morphology_release(context));
  ASSERT_EQ(KLEIDICV_OK, kleidicv_morphology_create(
                             &context, kernel, anchor, border, border_values, 1,
                             1, sizeof(ElementType), imageW));
  EXPECT_EQ(KLEIDICV_ERROR_NOT_IMPLEMENTED,
            ErodeParams<ElementType>::api()(src, sizeof(ElementType), dst,
                                            sizeof(ElementType), underSize,
                                            validHeight, context));
  EXPECT_EQ(KLEIDICV_ERROR_NOT_IMPLEMENTED,
            DilateParams<ElementType>::api()(src, sizeof(ElementType), dst,
                                             sizeof(ElementType), underSize,
                                             validHeight, context));
  EXPECT_EQ(KLEIDICV_OK, kleidicv_morphology_release(context));
  ASSERT_EQ(KLEIDICV_OK, kleidicv_morphology_create(
                             &context, kernel, anchor, border, border_values, 1,
                             1, sizeof(ElementType), imageH));
  EXPECT_EQ(KLEIDICV_ERROR_NOT_IMPLEMENTED,
            ErodeParams<ElementType>::api()(src, sizeof(ElementType), dst,
                                            sizeof(ElementType), validWidth,
                                            underSize, context));
  EXPECT_EQ(KLEIDICV_ERROR_NOT_IMPLEMENTED,
            DilateParams<ElementType>::api()(src, sizeof(ElementType), dst,
                                             sizeof(ElementType), validWidth,
                                             underSize, context));
  EXPECT_EQ(KLEIDICV_OK, kleidicv_morphology_release(context));
}

TYPED_TEST(Morphology, UnsupportedBorderType) {
  for (kleidicv_border_type_t border : {
           KLEIDICV_BORDER_TYPE_REFLECT,
           KLEIDICV_BORDER_TYPE_WRAP,
           KLEIDICV_BORDER_TYPE_REVERSE,
           KLEIDICV_BORDER_TYPE_TRANSPARENT,
           KLEIDICV_BORDER_TYPE_NONE,
       }) {
    kleidicv_morphology_context_t *context = nullptr;
    EXPECT_EQ(KLEIDICV_ERROR_NOT_IMPLEMENTED,
              make_minimal_context(&context, sizeof(TypeParam), border));
    ASSERT_EQ(nullptr, context);
  }
}

TYPED_TEST(Morphology, UnsupportedSize) {
  kleidicv_morphology_context_t *context = nullptr;
  kleidicv_rectangle_t small_rect{1, 1};
  kleidicv_point_t anchor{0, 0};
  kleidicv_border_type_t border = KLEIDICV_BORDER_TYPE_REPLICATE;
  kleidicv_border_values_t border_values{0, 0, 1, 1};

  for (kleidicv_rectangle_t bad_rect : {
           kleidicv_rectangle_t{KLEIDICV_MAX_IMAGE_PIXELS + 1, 1},
           kleidicv_rectangle_t{KLEIDICV_MAX_IMAGE_PIXELS,
                                KLEIDICV_MAX_IMAGE_PIXELS},
       }) {
    EXPECT_EQ(KLEIDICV_ERROR_RANGE,
              kleidicv_morphology_create(&context, bad_rect, anchor, border,
                                         border_values, 1, 1, sizeof(TypeParam),
                                         small_rect));
    ASSERT_EQ(nullptr, context);

    EXPECT_EQ(KLEIDICV_ERROR_RANGE,
              kleidicv_morphology_create(&context, small_rect, anchor, border,
                                         border_values, 1, 1, sizeof(TypeParam),
                                         bad_rect));
    ASSERT_EQ(nullptr, context);
  }
}

#ifdef KLEIDICV_ALLOCATION_TESTS
TYPED_TEST(Morphology, CannotAllocateImage) {
  MockMallocToFail::enable();
  kleidicv_morphology_context_t *context = nullptr;
  kleidicv_rectangle_t kernel{3, 3}, image{3072, 2048};
  kleidicv_border_type_t border = KLEIDICV_BORDER_TYPE_REPLICATE;
  kleidicv_border_values_t border_values{0, 0, 1, 1};
  kleidicv_point_t anchor{1, 1};

  EXPECT_EQ(KLEIDICV_ERROR_ALLOCATION,
            kleidicv_morphology_create(&context, kernel, anchor, border,
                                       border_values, 1, 1, sizeof(TypeParam),
                                       image));
  MockMallocToFail::disable();
}
#endif

TYPED_TEST(Morphology, OversizeImage) {
  kleidicv_morphology_context_t *context = nullptr;
  kleidicv_rectangle_t kernel{3, 1UL << 33}, image{1UL << 33, 100};
  kleidicv_border_type_t border = KLEIDICV_BORDER_TYPE_REPLICATE;
  kleidicv_border_values_t border_values{0, 0, 1, 1};
  kleidicv_point_t anchor{1, 1};

  EXPECT_EQ(KLEIDICV_ERROR_RANGE,
            kleidicv_morphology_create(&context, kernel, anchor, border,
                                       border_values, 1, 1, sizeof(TypeParam),
                                       image));
}

TYPED_TEST(Morphology, InvalidAnchors) {
  kleidicv_morphology_context_t *context = nullptr;
  kleidicv_rectangle_t kernel1{1, 1}, kernel2{6, 4}, image{20, 20};
  kleidicv_border_type_t border = KLEIDICV_BORDER_TYPE_REPLICATE;
  kleidicv_border_values_t border_values{0, 0, 1, 1};
  kleidicv_point_t anchor1{1, 0}, anchor2{6, 3}, anchor3{5, 4};

  EXPECT_EQ(KLEIDICV_ERROR_RANGE,
            kleidicv_morphology_create(&context, kernel1, anchor1, border,
                                       border_values, 1, 1, sizeof(TypeParam),
                                       image));
  ASSERT_EQ(nullptr, context);
  EXPECT_EQ(KLEIDICV_ERROR_RANGE,
            kleidicv_morphology_create(&context, kernel2, anchor2, border,
                                       border_values, 1, 1, sizeof(TypeParam),
                                       image));
  ASSERT_EQ(nullptr, context);
  EXPECT_EQ(KLEIDICV_ERROR_RANGE,
            kleidicv_morphology_create(&context, kernel2, anchor3, border,
                                       border_values, 1, 1, sizeof(TypeParam),
                                       image));
  ASSERT_EQ(nullptr, context);
}

TYPED_TEST(Morphology, InvalidTypeSize) {
  kleidicv_morphology_context_t *context = nullptr;

  EXPECT_EQ(
      KLEIDICV_ERROR_RANGE,
      kleidicv_morphology_create(
          &context, kleidicv_rectangle_t{1, 1}, kleidicv_point_t{0, 0},
          KLEIDICV_BORDER_TYPE_REPLICATE, kleidicv_border_values_t{0, 0, 1, 1},
          1, 1, KLEIDICV_MAXIMUM_TYPE_SIZE + 1, kleidicv_rectangle_t{1, 1}));
  ASSERT_EQ(nullptr, context);
}

TYPED_TEST(Morphology, InvalidChannelNumber) {
  kleidicv_morphology_context_t *context = nullptr;

  EXPECT_EQ(KLEIDICV_ERROR_RANGE,
            kleidicv_morphology_create(&context, kleidicv_rectangle_t{1, 1},
                                       kleidicv_point_t{0, 0},
                                       KLEIDICV_BORDER_TYPE_REPLICATE,
                                       kleidicv_border_values_t{0, 0, 1, 1},
                                       KLEIDICV_MAXIMUM_CHANNEL_COUNT + 1, 1, 1,
                                       kleidicv_rectangle_t{1, 1}));
  ASSERT_EQ(nullptr, context);
}

TYPED_TEST(Morphology, ImageBiggerThanContext) {
  kleidicv_morphology_context_t *context = nullptr;
  kleidicv_rectangle_t kernel{3, 3}, image{5, 5};
  kleidicv_border_type_t border = KLEIDICV_BORDER_TYPE_REPLICATE;
  kleidicv_border_values_t border_values{0, 0, 1, 1};
  kleidicv_point_t anchor{1, 1};

  EXPECT_EQ(KLEIDICV_OK, kleidicv_morphology_create(&context, kernel, anchor,
                                                    border, border_values, 1, 1,
                                                    sizeof(TypeParam), image));
  const size_t w = 7, h = 7;
  TypeParam src[w * h], dst[w * h];
  EXPECT_EQ(
      KLEIDICV_ERROR_CONTEXT_MISMATCH,
      ErodeParams<TypeParam>::api()(src, sizeof(TypeParam) * w, dst,
                                    sizeof(TypeParam) * w, w, h, context));
  EXPECT_EQ(
      KLEIDICV_ERROR_CONTEXT_MISMATCH,
      DilateParams<TypeParam>::api()(src, sizeof(TypeParam) * w, dst,
                                     sizeof(TypeParam) * w, w, h, context));
  EXPECT_EQ(KLEIDICV_OK, kleidicv_morphology_release(context));
}

TYPED_TEST(Morphology, DilateNullPointer) {
  kleidicv_morphology_context_t *context = nullptr;
  ASSERT_EQ(KLEIDICV_OK, make_minimal_context(&context, sizeof(TypeParam)));
  TypeParam src[1] = {}, dst[1];
  test::test_null_args(DilateParams<TypeParam>::api(), src, sizeof(TypeParam),
                       dst, sizeof(TypeParam), 1, 1, context);
  EXPECT_EQ(KLEIDICV_OK, kleidicv_morphology_release(context));
}

TYPED_TEST(Morphology, ErodeNullPointer) {
  kleidicv_morphology_context_t *context = nullptr;
  ASSERT_EQ(KLEIDICV_OK, make_minimal_context(&context, sizeof(TypeParam)));
  TypeParam src[1] = {}, dst[1];
  test::test_null_args(ErodeParams<TypeParam>::api(), src, sizeof(TypeParam),
                       dst, sizeof(TypeParam), 1, 1, context);
  EXPECT_EQ(KLEIDICV_OK, kleidicv_morphology_release(context));
}

TYPED_TEST(Morphology, DilateMisalignment) {
  if (sizeof(TypeParam) == 1) {
    // misalignment impossible
    return;
  }
  kleidicv_morphology_context_t *context = nullptr;
  ASSERT_EQ(KLEIDICV_OK, make_minimal_context(&context, sizeof(TypeParam)));
  TypeParam src[1] = {}, dst[1];
  EXPECT_EQ(KLEIDICV_ERROR_ALIGNMENT,
            DilateParams<TypeParam>::api()(src, sizeof(TypeParam) + 1, dst,
                                           sizeof(TypeParam), 1, 1, context));
  EXPECT_EQ(
      KLEIDICV_ERROR_ALIGNMENT,
      DilateParams<TypeParam>::api()(src, sizeof(TypeParam), dst,
                                     sizeof(TypeParam) + 1, 1, 1, context));
  EXPECT_EQ(KLEIDICV_OK, kleidicv_morphology_release(context));
}

TYPED_TEST(Morphology, ErodeMisalignment) {
  if (sizeof(TypeParam) == 1) {
    // misalignment impossible
    return;
  }
  kleidicv_morphology_context_t *context = nullptr;
  ASSERT_EQ(KLEIDICV_OK, make_minimal_context(&context, sizeof(TypeParam)));
  TypeParam src[1] = {}, dst[1];
  EXPECT_EQ(KLEIDICV_ERROR_ALIGNMENT,
            ErodeParams<TypeParam>::api()(src, sizeof(TypeParam) + 1, dst,
                                          sizeof(TypeParam), 1, 1, context));
  EXPECT_EQ(
      KLEIDICV_ERROR_ALIGNMENT,
      ErodeParams<TypeParam>::api()(src, sizeof(TypeParam), dst,
                                    sizeof(TypeParam) + 1, 1, 1, context));
  EXPECT_EQ(KLEIDICV_OK, kleidicv_morphology_release(context));
}

TYPED_TEST(Morphology, DilateZeroImageSize) {
  kleidicv_morphology_context_t *context = nullptr;
  TypeParam src[1], dst[1];
  ASSERT_EQ(
      KLEIDICV_OK,
      kleidicv_morphology_create(
          &context, kleidicv_rectangle_t{1, 1}, kleidicv_point_t{0, 0},
          KLEIDICV_BORDER_TYPE_REPLICATE, kleidicv_border_values_t{0, 0, 1, 1},
          1, 1, sizeof(TypeParam), kleidicv_rectangle_t{0, 1}));
  EXPECT_EQ(KLEIDICV_OK,
            DilateParams<TypeParam>::api()(src, sizeof(TypeParam), dst,
                                           sizeof(TypeParam), 0, 1, context));
  EXPECT_EQ(KLEIDICV_OK, kleidicv_morphology_release(context));

  ASSERT_EQ(
      KLEIDICV_OK,
      kleidicv_morphology_create(
          &context, kleidicv_rectangle_t{1, 1}, kleidicv_point_t{0, 0},
          KLEIDICV_BORDER_TYPE_REPLICATE, kleidicv_border_values_t{0, 0, 1, 1},
          1, 1, sizeof(TypeParam), kleidicv_rectangle_t{1, 0}));
  EXPECT_EQ(KLEIDICV_OK,
            DilateParams<TypeParam>::api()(src, sizeof(TypeParam), dst,
                                           sizeof(TypeParam), 1, 0, context));
  EXPECT_EQ(KLEIDICV_OK, kleidicv_morphology_release(context));
}

TYPED_TEST(Morphology, ErodeZeroImageSize) {
  kleidicv_morphology_context_t *context = nullptr;
  TypeParam src[1], dst[1];
  ASSERT_EQ(
      KLEIDICV_OK,
      kleidicv_morphology_create(
          &context, kleidicv_rectangle_t{1, 1}, kleidicv_point_t{0, 0},
          KLEIDICV_BORDER_TYPE_REPLICATE, kleidicv_border_values_t{0, 0, 1, 1},
          1, 1, sizeof(TypeParam), kleidicv_rectangle_t{0, 1}));
  EXPECT_EQ(KLEIDICV_OK,
            ErodeParams<TypeParam>::api()(src, sizeof(TypeParam), dst,
                                          sizeof(TypeParam), 0, 1, context));
  EXPECT_EQ(KLEIDICV_OK, kleidicv_morphology_release(context));

  ASSERT_EQ(
      KLEIDICV_OK,
      kleidicv_morphology_create(
          &context, kleidicv_rectangle_t{1, 1}, kleidicv_point_t{0, 0},
          KLEIDICV_BORDER_TYPE_REPLICATE, kleidicv_border_values_t{0, 0, 1, 1},
          1, 1, sizeof(TypeParam), kleidicv_rectangle_t{1, 0}));
  EXPECT_EQ(KLEIDICV_OK,
            ErodeParams<TypeParam>::api()(src, sizeof(TypeParam), dst,
                                          sizeof(TypeParam), 1, 0, context));
  EXPECT_EQ(KLEIDICV_OK, kleidicv_morphology_release(context));
}

TYPED_TEST(Morphology, DilateInvalidContextSizeType) {
  kleidicv_morphology_context_t *context = nullptr;
  ASSERT_EQ(KLEIDICV_OK, make_minimal_context(&context, sizeof(TypeParam) + 1));
  TypeParam src[1], dst[1];
  EXPECT_EQ(KLEIDICV_ERROR_CONTEXT_MISMATCH,
            DilateParams<TypeParam>::api()(src, sizeof(TypeParam), dst,
                                           sizeof(TypeParam), 1, 1, context));
  EXPECT_EQ(KLEIDICV_OK, kleidicv_morphology_release(context));
}

TYPED_TEST(Morphology, ErodeInvalidContextSizeType) {
  kleidicv_morphology_context_t *context = nullptr;
  ASSERT_EQ(KLEIDICV_OK, make_minimal_context(&context, sizeof(TypeParam) + 1));
  TypeParam src[1], dst[1];
  EXPECT_EQ(KLEIDICV_ERROR_CONTEXT_MISMATCH,
            ErodeParams<TypeParam>::api()(src, sizeof(TypeParam), dst,
                                          sizeof(TypeParam), 1, 1, context));
  EXPECT_EQ(KLEIDICV_OK, kleidicv_morphology_release(context));
}

TYPED_TEST(Morphology, DilateInvalidContextImageSize) {
  kleidicv_morphology_context_t *context = nullptr;
  ASSERT_EQ(KLEIDICV_OK, make_minimal_context(&context, sizeof(TypeParam)));
  TypeParam src[1], dst[1];
  EXPECT_EQ(KLEIDICV_ERROR_CONTEXT_MISMATCH,
            DilateParams<TypeParam>::api()(src, sizeof(TypeParam), dst,
                                           sizeof(TypeParam), 2, 1, context));
  EXPECT_EQ(KLEIDICV_OK, kleidicv_morphology_release(context));
}

TYPED_TEST(Morphology, ErodeInvalidContextImageSize) {
  kleidicv_morphology_context_t *context = nullptr;
  ASSERT_EQ(KLEIDICV_OK, make_minimal_context(&context, sizeof(TypeParam)));
  TypeParam src[1], dst[1];
  EXPECT_EQ(KLEIDICV_ERROR_CONTEXT_MISMATCH,
            ErodeParams<TypeParam>::api()(src, sizeof(TypeParam), dst,
                                          sizeof(TypeParam), 2, 1, context));
  EXPECT_EQ(KLEIDICV_OK, kleidicv_morphology_release(context));
}

TYPED_TEST(Morphology, ValidImageSize) {
  kleidicv_rectangle_t kernel3x3{3, 3};
  kleidicv_rectangle_t kernel5x5{5, 5};
  test::Array2D<TypeParam> src2x2{kernel3x3.width - 1, kernel3x3.width - 1,
                                  test::Options::vector_length()};
  src2x2.set(0, 0, {1, 2});
  src2x2.set(1, 0, {1, 2});
  test::Array2D<TypeParam> src4x4{kernel5x5.width - 1, kernel5x5.width - 1,
                                  test::Options::vector_length()};
  src4x4.set(0, 0, {1, 2, 3, 4});
  src4x4.set(1, 0, {1, 2, 3, 4});
  src4x4.set(2, 0, {1, 2, 3, 4});
  src4x4.set(3, 0, {1, 2, 3, 4});
  test_valid_image_size<TypeParam>(kernel3x3, src2x2);
  test_valid_image_size<TypeParam>(kernel5x5, src4x4);
}

TYPED_TEST(Morphology, UndersizeImage) {
  kleidicv_rectangle_t kernel3x3{3, 3};
  kleidicv_rectangle_t kernel5x5{5, 5};
  test_undersize_image<TypeParam>(kernel3x3);
  test_undersize_image<TypeParam>(kernel5x5);
}

TYPED_TEST(Morphology, DilateOversizeImage) {
  kleidicv_morphology_context_t *context = nullptr;
  ASSERT_EQ(KLEIDICV_OK, make_minimal_context(&context, sizeof(TypeParam)));
  TypeParam src[1], dst[1];
  EXPECT_EQ(KLEIDICV_ERROR_RANGE,
            DilateParams<TypeParam>::api()(
                src, sizeof(TypeParam), dst, sizeof(TypeParam),
                KLEIDICV_MAX_IMAGE_PIXELS + 1, 1, context));
  EXPECT_EQ(KLEIDICV_ERROR_RANGE,
            DilateParams<TypeParam>::api()(
                src, sizeof(TypeParam), dst, sizeof(TypeParam),
                KLEIDICV_MAX_IMAGE_PIXELS, KLEIDICV_MAX_IMAGE_PIXELS, context));
  EXPECT_EQ(KLEIDICV_OK, kleidicv_morphology_release(context));
}

TYPED_TEST(Morphology, ErodeOversizeImage) {
  kleidicv_morphology_context_t *context = nullptr;
  ASSERT_EQ(KLEIDICV_OK, make_minimal_context(&context, sizeof(TypeParam)));
  TypeParam src[1], dst[1];
  EXPECT_EQ(KLEIDICV_ERROR_RANGE,
            ErodeParams<TypeParam>::api()(
                src, sizeof(TypeParam), dst, sizeof(TypeParam),
                KLEIDICV_MAX_IMAGE_PIXELS + 1, 1, context));
  EXPECT_EQ(KLEIDICV_ERROR_RANGE,
            ErodeParams<TypeParam>::api()(
                src, sizeof(TypeParam), dst, sizeof(TypeParam),
                KLEIDICV_MAX_IMAGE_PIXELS, KLEIDICV_MAX_IMAGE_PIXELS, context));
  EXPECT_EQ(KLEIDICV_OK, kleidicv_morphology_release(context));
}

TEST(MorphologyCreate, NullPointer) {
  EXPECT_EQ(KLEIDICV_ERROR_NULL_POINTER,
            kleidicv_morphology_create(nullptr, kleidicv_rectangle_t{1, 1},
                                       kleidicv_point_t{0, 0},
                                       KLEIDICV_BORDER_TYPE_REPLICATE,
                                       kleidicv_border_values_t{0, 0, 1, 1}, 1,
                                       1, 1, kleidicv_rectangle_t{1, 1}));
}
TEST(MorphologyRelease, NullPointer) {
  EXPECT_EQ(KLEIDICV_ERROR_NULL_POINTER, kleidicv_morphology_release(nullptr));
}
