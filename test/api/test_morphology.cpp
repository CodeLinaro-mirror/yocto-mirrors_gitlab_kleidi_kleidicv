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
#include "intrinsiccv/intrinsiccv.h"

#define INTRINSICCV_PARAMS(name, impl, type, op)                              \
  template <typename ElementType,                                             \
            std::enable_if_t<std::is_same_v<ElementType, type>, bool> = true> \
  class name {                                                                \
   public:                                                                    \
    static decltype(auto) api() { return &impl; }                             \
    static decltype(auto) operation() {                                       \
      return [](type a, type b) { return op<type>(a, b); };                   \
    }                                                                         \
  };

INTRINSICCV_PARAMS(DilateParams, intrinsiccv_dilate_u8, uint8_t, std::max);
INTRINSICCV_PARAMS(ErodeParams, intrinsiccv_erode_u8, uint8_t, std::min);

template <typename ElementType>
struct MorphologyKernelTestParams {
  using InputType = ElementType;
  using IntermediateType = ElementType;
  using OutputType = ElementType;
};  // end of struct MorphologyKernelTestParams

static constexpr std::array<intrinsiccv_border_type_t, 2> kSupportedBorders = {
    INTRINSICCV_BORDER_TYPE_REPLICATE,
    INTRINSICCV_BORDER_TYPE_CONSTANT,
};

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
      : mask_{kernelWidth, kernelHeight}, kernel_{mask_}, iterations_{1} {}

  MorphologyTest &with_anchor(test::Point anchor) {
    kernel_ = test::Kernel(mask_, anchor);
    return *this;
  }

  MorphologyTest &with_iterations(size_t iter) {
    iterations_ = iter;
    return *this;
  }

  void test() {
    auto array_layouts = test::default_array_layouts(kernelWidth, kernelHeight);
    test::SequenceGenerator tested_array_layouts{array_layouts};
    test::SequenceGenerator tested_borders{kSupportedBorders};
    test::SequenceGenerator tested_border_values{test_border_values()};
    test::PseudoRandomNumberGenerator<InputType> element_generator;
    Base::test(kernel_, tested_array_layouts, tested_borders,
               tested_border_values, element_generator);
  }

 protected:
  test::Array2D<InputType> mask_;
  test::Kernel<InputType> kernel_;
  size_t iterations_;

  intrinsiccv_error_t call_api(
      const test::Array2D<InputType> *input, test::Array2D<OutputType> *output,
      intrinsiccv_border_type_t border_type,
      intrinsiccv_border_values_t border_values) override {
    intrinsiccv_morphology_context_t *context = nullptr;
    auto kernelRect = intrinsiccv_rectangle_t{kernelWidth, kernelHeight};
    intrinsiccv_point_t anchor{kernel_.anchor().x, kernel_.anchor().y};
    auto ret = intrinsiccv_morphology_create(
        &context, kernelRect, anchor, border_type, border_values,
        input->channels(), iterations_, sizeof(InputType),
        intrinsiccv_rectangle_t{input->width() / input->channels(),
                                input->height()});
    if (ret != INTRINSICCV_OK) {
      return ret;
    }

    ret = OperationParams<InputType>::api()(
        input->data(), input->stride(), output->data(), output->stride(),
        input->width() / input->channels(), input->height(), context);
    auto releaseRet = intrinsiccv_morphology_release(context);
    if (releaseRet != INTRINSICCV_OK) {
      return releaseRet;
    }

    return ret;
  }

  void prepare_expected(const test::Kernel<IntermediateType> &kernel,
                        const test::ArrayLayout &array_layout,
                        intrinsiccv_border_type_t border_type,
                        intrinsiccv_border_values_t border_values) override {
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

  static constexpr double min_border() {
    return static_cast<double>(std::numeric_limits<InputType>::min());
  }

  static constexpr double max_border() {
    return static_cast<double>(std::numeric_limits<InputType>::max());
  }

  const std::array<intrinsiccv_border_values_t, 4> &test_border_values() const {
    static const std::array<intrinsiccv_border_values_t, 4> values = {
        {{0, 0, 0, 0},  // default
         {7, 42, 99, 9},
         {min_border(), max_border(), min_border(), max_border()},
         {0, min_border(), max_border(), 0}}};
    return values;
  }
};  // end of class class MorphologyTest<OperationParams, kernelWidth,
    // kernelHeight>

template <typename TypeParam>
class Morphology : public testing::Test {};

using ElementTypes = ::testing::Types<uint8_t>;

TYPED_TEST_SUITE(Morphology, ElementTypes);

TYPED_TEST(Morphology, Dilate1x1) {
  MorphologyTest<TypeParam, DilateParams, 1, 1>{}.test();
}

TYPED_TEST(Morphology, Erode1x1) {
  MorphologyTest<TypeParam, ErodeParams, 1, 1>{}.test();
}

TYPED_TEST(Morphology, Dilate1x2) {
  MorphologyTest<TypeParam, DilateParams, 1, 2>{}.test();
}

TYPED_TEST(Morphology, Erode1x2) {
  MorphologyTest<TypeParam, ErodeParams, 1, 2>{}.test();
}

TYPED_TEST(Morphology, Dilate3x1) {
  MorphologyTest<TypeParam, DilateParams, 3, 1>{}.test();
}

TYPED_TEST(Morphology, Erode3x1) {
  MorphologyTest<TypeParam, ErodeParams, 3, 1>{}.test();
}

TYPED_TEST(Morphology, Dilate3x3) {
  MorphologyTest<TypeParam, DilateParams, 3, 3>{}.test();
}

TYPED_TEST(Morphology, Erode3x3) {
  MorphologyTest<TypeParam, ErodeParams, 3, 3>{}.test();
}

TYPED_TEST(Morphology, Dilate5x5) {
  MorphologyTest<TypeParam, DilateParams, 5, 5>{}.test();
}

TYPED_TEST(Morphology, Erode5x5) {
  MorphologyTest<TypeParam, ErodeParams, 5, 5>{}.test();
}

TYPED_TEST(Morphology, Dilate11x11) {
  MorphologyTest<TypeParam, DilateParams, 17, 17>{}.test();
}

TYPED_TEST(Morphology, Erode11x11) {
  MorphologyTest<TypeParam, ErodeParams, 17, 17>{}.test();
}

TYPED_TEST(Morphology, Dilate4x4) {
  MorphologyTest<TypeParam, DilateParams, 4, 4>{}.test();
}

TYPED_TEST(Morphology, Erode7x5) {
  MorphologyTest<TypeParam, ErodeParams, 7, 5>{}.test();
}

TYPED_TEST(Morphology, Dilate8x4) {
  MorphologyTest<TypeParam, DilateParams, 8, 4>{}.test();
}

TYPED_TEST(Morphology, Dilate6x10) {
  MorphologyTest<TypeParam, DilateParams, 8, 14>{}.test();
}

TYPED_TEST(Morphology, Erode12x4) {
  MorphologyTest<TypeParam, ErodeParams, 18, 4>{}.test();
}

TYPED_TEST(Morphology, Iterations) {
  MorphologyTest<TypeParam, DilateParams, 3, 3>{}.with_iterations(2).test();
  MorphologyTest<TypeParam, ErodeParams, 6, 4>{}.with_iterations(3).test();
  MorphologyTest<TypeParam, DilateParams, 2, 7>{}.with_iterations(4).test();
}

TYPED_TEST(Morphology, Anchors) {
  MorphologyTest<TypeParam, ErodeParams, 3, 5>{}.with_anchor({0, 0}).test();
  MorphologyTest<TypeParam, DilateParams, 3, 5>{}.with_anchor({2, 0}).test();
  MorphologyTest<TypeParam, ErodeParams, 3, 5>{}.with_anchor({0, 4}).test();
  MorphologyTest<TypeParam, DilateParams, 3, 5>{}.with_anchor({2, 4}).test();
}

static intrinsiccv_error_t make_minimal_context(
    intrinsiccv_morphology_context_t **context, size_t type_size,
    intrinsiccv_border_type_t border = INTRINSICCV_BORDER_TYPE_REPLICATE) {
  return intrinsiccv_morphology_create(
      context, intrinsiccv_rectangle_t{1, 1}, intrinsiccv_point_t{0, 0}, border,
      intrinsiccv_border_values_t{0, 0, 1, 1}, 1, 1, type_size,
      intrinsiccv_rectangle_t{1, 1});
}

TYPED_TEST(Morphology, UnsupportedBorderType) {
  for (intrinsiccv_border_type_t border : {
           INTRINSICCV_BORDER_TYPE_REFLECT,
           INTRINSICCV_BORDER_TYPE_WRAP,
           INTRINSICCV_BORDER_TYPE_REVERSE,
           INTRINSICCV_BORDER_TYPE_TRANSPARENT,
           INTRINSICCV_BORDER_TYPE_NONE,
       }) {
    intrinsiccv_morphology_context_t *context = nullptr;
    EXPECT_EQ(INTRINSICCV_ERROR_NOT_IMPLEMENTED,
              make_minimal_context(&context, sizeof(TypeParam), border));
    ASSERT_EQ(nullptr, context);
  }
}

TYPED_TEST(Morphology, UnsupportedSize) {
  intrinsiccv_morphology_context_t *context = nullptr;
  intrinsiccv_rectangle_t small_rect{1, 1};
  intrinsiccv_point_t anchor{0, 0};
  intrinsiccv_border_type_t border = INTRINSICCV_BORDER_TYPE_REPLICATE;
  intrinsiccv_border_values_t border_values{0, 0, 1, 1};

  for (intrinsiccv_rectangle_t bad_rect : {
           intrinsiccv_rectangle_t{INTRINSICCV_MAX_IMAGE_PIXELS + 1, 1},
           intrinsiccv_rectangle_t{INTRINSICCV_MAX_IMAGE_PIXELS,
                                   INTRINSICCV_MAX_IMAGE_PIXELS},
       }) {
    EXPECT_EQ(INTRINSICCV_ERROR_RANGE,
              intrinsiccv_morphology_create(&context, bad_rect, anchor, border,
                                            border_values, 1, 1,
                                            sizeof(TypeParam), small_rect));
    ASSERT_EQ(nullptr, context);

    EXPECT_EQ(INTRINSICCV_ERROR_RANGE,
              intrinsiccv_morphology_create(&context, small_rect, anchor,
                                            border, border_values, 1, 1,
                                            sizeof(TypeParam), bad_rect));
    ASSERT_EQ(nullptr, context);
  }
}

TYPED_TEST(Morphology, TooBigImage) {
  intrinsiccv_morphology_context_t *context = nullptr;
  intrinsiccv_rectangle_t kernel{3, 10000}, image{1UL << 34, 10000};
  intrinsiccv_border_type_t border = INTRINSICCV_BORDER_TYPE_REPLICATE;
  intrinsiccv_border_values_t border_values{0, 0, 1, 1};
  intrinsiccv_point_t anchor{1, 1};

  EXPECT_EQ(INTRINSICCV_ERROR_ALLOCATION,
            intrinsiccv_morphology_create(&context, kernel, anchor, border,
                                          border_values, 1, 1,
                                          sizeof(TypeParam), image));

  intrinsiccv_rectangle_t kernel2{3, 1UL << 33}, image2{1UL << 33, 100};
  EXPECT_EQ(INTRINSICCV_ERROR_RANGE,
            intrinsiccv_morphology_create(&context, kernel2, anchor, border,
                                          border_values, 1, 1,
                                          sizeof(TypeParam), image2));
}

TYPED_TEST(Morphology, InvalidAnchors) {
  intrinsiccv_morphology_context_t *context = nullptr;
  intrinsiccv_rectangle_t kernel1{1, 1}, kernel2{6, 4}, image{20, 20};
  intrinsiccv_border_type_t border = INTRINSICCV_BORDER_TYPE_REPLICATE;
  intrinsiccv_border_values_t border_values{0, 0, 1, 1};
  intrinsiccv_point_t anchor1{1, 0}, anchor2{6, 3}, anchor3{5, 4};

  EXPECT_EQ(INTRINSICCV_ERROR_RANGE,
            intrinsiccv_morphology_create(&context, kernel1, anchor1, border,
                                          border_values, 1, 1,
                                          sizeof(TypeParam), image));
  ASSERT_EQ(nullptr, context);
  EXPECT_EQ(INTRINSICCV_ERROR_RANGE,
            intrinsiccv_morphology_create(&context, kernel2, anchor2, border,
                                          border_values, 1, 1,
                                          sizeof(TypeParam), image));
  ASSERT_EQ(nullptr, context);
  EXPECT_EQ(INTRINSICCV_ERROR_RANGE,
            intrinsiccv_morphology_create(&context, kernel2, anchor3, border,
                                          border_values, 1, 1,
                                          sizeof(TypeParam), image));
  ASSERT_EQ(nullptr, context);
}

TYPED_TEST(Morphology, InvalidTypeSize) {
  intrinsiccv_morphology_context_t *context = nullptr;

  EXPECT_EQ(
      INTRINSICCV_ERROR_RANGE,
      intrinsiccv_morphology_create(
          &context, intrinsiccv_rectangle_t{1, 1}, intrinsiccv_point_t{0, 0},
          INTRINSICCV_BORDER_TYPE_REPLICATE,
          intrinsiccv_border_values_t{0, 0, 1, 1}, 1, 1,
          INTRINSICCV_MAXIMUM_TYPE_SIZE + 1, intrinsiccv_rectangle_t{1, 1}));
  ASSERT_EQ(nullptr, context);
}

TYPED_TEST(Morphology, InvalidChannelNumber) {
  intrinsiccv_morphology_context_t *context = nullptr;

  EXPECT_EQ(INTRINSICCV_ERROR_RANGE,
            intrinsiccv_morphology_create(
                &context, intrinsiccv_rectangle_t{1, 1},
                intrinsiccv_point_t{0, 0}, INTRINSICCV_BORDER_TYPE_REPLICATE,
                intrinsiccv_border_values_t{0, 0, 1, 1},
                INTRINSICCV_MAXIMUM_CHANNEL_COUNT + 1, 1, 1,
                intrinsiccv_rectangle_t{1, 1}));
  ASSERT_EQ(nullptr, context);
}

TYPED_TEST(Morphology, ImageBiggerThanContext) {
  intrinsiccv_morphology_context_t *context = nullptr;
  intrinsiccv_rectangle_t kernel{3, 3}, image{5, 5};
  intrinsiccv_border_type_t border = INTRINSICCV_BORDER_TYPE_REPLICATE;
  intrinsiccv_border_values_t border_values{0, 0, 1, 1};
  intrinsiccv_point_t anchor{1, 1};

  EXPECT_EQ(INTRINSICCV_OK, intrinsiccv_morphology_create(
                                &context, kernel, anchor, border, border_values,
                                1, 1, sizeof(TypeParam), image));
  const size_t w = 7, h = 7;
  TypeParam src[w * h], dst[w * h];
  EXPECT_EQ(
      INTRINSICCV_ERROR_CONTEXT_MISMATCH,
      ErodeParams<TypeParam>::api()(src, sizeof(TypeParam) * w, dst,
                                    sizeof(TypeParam) * w, w, h, context));
  EXPECT_EQ(
      INTRINSICCV_ERROR_CONTEXT_MISMATCH,
      DilateParams<TypeParam>::api()(src, sizeof(TypeParam) * w, dst,
                                     sizeof(TypeParam) * w, w, h, context));
  EXPECT_EQ(INTRINSICCV_OK, intrinsiccv_morphology_release(context));
}

TYPED_TEST(Morphology, DilateNullPointer) {
  intrinsiccv_morphology_context_t *context = nullptr;
  ASSERT_EQ(INTRINSICCV_OK, make_minimal_context(&context, sizeof(TypeParam)));
  TypeParam src[1] = {}, dst[1];
  test::test_null_args(DilateParams<TypeParam>::api(), src, sizeof(TypeParam),
                       dst, sizeof(TypeParam), 1, 1, context);
  EXPECT_EQ(INTRINSICCV_OK, intrinsiccv_morphology_release(context));
}

TYPED_TEST(Morphology, ErodeNullPointer) {
  intrinsiccv_morphology_context_t *context = nullptr;
  ASSERT_EQ(INTRINSICCV_OK, make_minimal_context(&context, sizeof(TypeParam)));

  TypeParam src[1] = {}, dst[1];
  test::test_null_args(ErodeParams<TypeParam>::api(), src, sizeof(TypeParam),
                       dst, sizeof(TypeParam), 1, 1, context);

  EXPECT_EQ(INTRINSICCV_OK, intrinsiccv_morphology_release(context));
}

TYPED_TEST(Morphology, DilateMisalignment) {
  if (sizeof(TypeParam) == 1) {
    // misalignment impossible
    return;
  }
  intrinsiccv_morphology_context_t *context = nullptr;
  ASSERT_EQ(INTRINSICCV_OK, make_minimal_context(&context, sizeof(TypeParam)));
  TypeParam src[1] = {}, dst[1];
  EXPECT_EQ(INTRINSICCV_ERROR_ALIGNMENT,
            DilateParams<TypeParam>::api()(src, sizeof(TypeParam) + 1, dst,
                                           sizeof(TypeParam), 1, 1, context));
  EXPECT_EQ(
      INTRINSICCV_ERROR_ALIGNMENT,
      DilateParams<TypeParam>::api()(src, sizeof(TypeParam), dst,
                                     sizeof(TypeParam) + 1, 1, 1, context));
  EXPECT_EQ(INTRINSICCV_OK, intrinsiccv_morphology_release(context));
}

TYPED_TEST(Morphology, ErodeMisalignment) {
  if (sizeof(TypeParam) == 1) {
    // misalignment impossible
    return;
  }
  intrinsiccv_morphology_context_t *context = nullptr;
  ASSERT_EQ(INTRINSICCV_OK, make_minimal_context(&context, sizeof(TypeParam)));
  TypeParam src[1] = {}, dst[1];
  EXPECT_EQ(INTRINSICCV_ERROR_ALIGNMENT,
            ErodeParams<TypeParam>::api()(src, sizeof(TypeParam) + 1, dst,
                                          sizeof(TypeParam), 1, 1, context));
  EXPECT_EQ(
      INTRINSICCV_ERROR_ALIGNMENT,
      ErodeParams<TypeParam>::api()(src, sizeof(TypeParam), dst,
                                    sizeof(TypeParam) + 1, 1, 1, context));
  EXPECT_EQ(INTRINSICCV_OK, intrinsiccv_morphology_release(context));
}

TYPED_TEST(Morphology, DilateImageSize) {
  intrinsiccv_morphology_context_t *context = nullptr;
  ASSERT_EQ(INTRINSICCV_OK, make_minimal_context(&context, sizeof(TypeParam)));
  TypeParam src[1], dst[1];
  EXPECT_EQ(INTRINSICCV_ERROR_RANGE,
            DilateParams<TypeParam>::api()(
                src, sizeof(TypeParam), dst, sizeof(TypeParam),
                INTRINSICCV_MAX_IMAGE_PIXELS + 1, 1, context));
  EXPECT_EQ(
      INTRINSICCV_ERROR_RANGE,
      DilateParams<TypeParam>::api()(
          src, sizeof(TypeParam), dst, sizeof(TypeParam),
          INTRINSICCV_MAX_IMAGE_PIXELS, INTRINSICCV_MAX_IMAGE_PIXELS, context));
  EXPECT_EQ(INTRINSICCV_OK, intrinsiccv_morphology_release(context));
}

TYPED_TEST(Morphology, ErodeImageSize) {
  intrinsiccv_morphology_context_t *context = nullptr;
  ASSERT_EQ(INTRINSICCV_OK, make_minimal_context(&context, sizeof(TypeParam)));
  TypeParam src[1], dst[1];
  EXPECT_EQ(INTRINSICCV_ERROR_RANGE,
            ErodeParams<TypeParam>::api()(
                src, sizeof(TypeParam), dst, sizeof(TypeParam),
                INTRINSICCV_MAX_IMAGE_PIXELS + 1, 1, context));
  EXPECT_EQ(
      INTRINSICCV_ERROR_RANGE,
      ErodeParams<TypeParam>::api()(
          src, sizeof(TypeParam), dst, sizeof(TypeParam),
          INTRINSICCV_MAX_IMAGE_PIXELS, INTRINSICCV_MAX_IMAGE_PIXELS, context));
  EXPECT_EQ(INTRINSICCV_OK, intrinsiccv_morphology_release(context));
}

TYPED_TEST(Morphology, DilateInvalidContextSizeType) {
  intrinsiccv_morphology_context_t *context = nullptr;
  ASSERT_EQ(INTRINSICCV_OK,
            make_minimal_context(&context, sizeof(TypeParam) + 1));
  TypeParam src[1], dst[1];
  EXPECT_EQ(INTRINSICCV_ERROR_CONTEXT_MISMATCH,
            DilateParams<TypeParam>::api()(src, sizeof(TypeParam), dst,
                                           sizeof(TypeParam), 1, 1, context));
  EXPECT_EQ(INTRINSICCV_OK, intrinsiccv_morphology_release(context));
}

TYPED_TEST(Morphology, ErodeInvalidContextSizeType) {
  intrinsiccv_morphology_context_t *context = nullptr;
  ASSERT_EQ(INTRINSICCV_OK,
            make_minimal_context(&context, sizeof(TypeParam) + 1));
  TypeParam src[1], dst[1];
  EXPECT_EQ(INTRINSICCV_ERROR_CONTEXT_MISMATCH,
            ErodeParams<TypeParam>::api()(src, sizeof(TypeParam), dst,
                                          sizeof(TypeParam), 1, 1, context));
  EXPECT_EQ(INTRINSICCV_OK, intrinsiccv_morphology_release(context));
}

TYPED_TEST(Morphology, DilateInvalidContextImageSize) {
  intrinsiccv_morphology_context_t *context = nullptr;
  ASSERT_EQ(INTRINSICCV_OK, make_minimal_context(&context, sizeof(TypeParam)));
  TypeParam src[1], dst[1];
  EXPECT_EQ(INTRINSICCV_ERROR_CONTEXT_MISMATCH,
            DilateParams<TypeParam>::api()(src, sizeof(TypeParam), dst,
                                           sizeof(TypeParam), 2, 1, context));
  EXPECT_EQ(INTRINSICCV_OK, intrinsiccv_morphology_release(context));
}

TYPED_TEST(Morphology, ErodeInvalidContextImageSize) {
  intrinsiccv_morphology_context_t *context = nullptr;
  ASSERT_EQ(INTRINSICCV_OK, make_minimal_context(&context, sizeof(TypeParam)));
  TypeParam src[1], dst[1];
  EXPECT_EQ(INTRINSICCV_ERROR_CONTEXT_MISMATCH,
            ErodeParams<TypeParam>::api()(src, sizeof(TypeParam), dst,
                                          sizeof(TypeParam), 2, 1, context));
  EXPECT_EQ(INTRINSICCV_OK, intrinsiccv_morphology_release(context));
}
