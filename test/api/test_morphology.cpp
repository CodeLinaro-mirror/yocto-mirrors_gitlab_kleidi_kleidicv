// SPDX-FileCopyrightText: 2024 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#include <gtest/gtest.h>

#include "framework/operation.h"
#include "intrinsiccv/intrinsiccv.h"

#define INTRINSICCV_DILATE(type, suffix) \
  INTRINSICCV_API(dilate, intrinsiccv_dilate_##suffix, type)

#define INTRINSICCV_ERODE(type, suffix) \
  INTRINSICCV_API(erode, intrinsiccv_erode_##suffix, type)

INTRINSICCV_DILATE(uint8_t, u8);
INTRINSICCV_ERODE(uint8_t, u8);

template <typename ElementType>
class MorphologyTest : public testing::Test {};

template <typename ElementType>
class DilateTest : public testing::Test {};

template <typename ElementType>
class ErodeTest : public testing::Test {};

using ElementTypes = ::testing::Types<uint8_t>;
TYPED_TEST_SUITE(MorphologyTest, ElementTypes);
TYPED_TEST_SUITE(DilateTest, ElementTypes);
TYPED_TEST_SUITE(ErodeTest, ElementTypes);

TYPED_TEST(MorphologyTest, UnsupportedBorderType) {
  for (intrinsiccv_border_type_t border : {
           INTRINSICCV_BORDER_TYPE_REFLECT,
           INTRINSICCV_BORDER_TYPE_WRAP,
           INTRINSICCV_BORDER_TYPE_REVERSE,
           INTRINSICCV_BORDER_TYPE_TRANSPARENT,
           INTRINSICCV_BORDER_TYPE_NONE,
       }) {
    intrinsiccv_morphology_context_t *context = nullptr;
    EXPECT_EQ(
        INTRINSICCV_ERROR_NOT_IMPLEMENTED,
        intrinsiccv_morphology_create(
            &context, intrinsiccv_rectangle_t{1, 1}, intrinsiccv_point_t{0, 0},
            border, intrinsiccv_border_values_t{0, 0, 1, 1}, 1, 1,
            sizeof(TypeParam), intrinsiccv_rectangle_t{1, 1}));
    ASSERT_EQ(nullptr, context);
  }
}

TYPED_TEST(MorphologyTest, UnsupportedSize) {
  intrinsiccv_morphology_context_t *context = nullptr;

  for (intrinsiccv_rectangle_t rect : {
           intrinsiccv_rectangle_t{INTRINSICCV_MAX_IMAGE_PIXELS + 1, 1},
           intrinsiccv_rectangle_t{INTRINSICCV_MAX_IMAGE_PIXELS,
                                   INTRINSICCV_MAX_IMAGE_PIXELS},
       }) {
    EXPECT_EQ(INTRINSICCV_ERROR_RANGE,
              intrinsiccv_morphology_create(
                  &context, rect, intrinsiccv_point_t{0, 0},
                  INTRINSICCV_BORDER_TYPE_REPLICATE,
                  intrinsiccv_border_values_t{0, 0, 1, 1}, 1, 1,
                  sizeof(TypeParam), intrinsiccv_rectangle_t{1, 1}));
    ASSERT_EQ(nullptr, context);

    EXPECT_EQ(INTRINSICCV_ERROR_RANGE,
              intrinsiccv_morphology_create(
                  &context, intrinsiccv_rectangle_t{1, 1},
                  intrinsiccv_point_t{0, 0}, INTRINSICCV_BORDER_TYPE_REPLICATE,
                  intrinsiccv_border_values_t{0, 0, 1, 1}, 1, 1,
                  sizeof(TypeParam), rect));
    ASSERT_EQ(nullptr, context);
  }
}

static intrinsiccv_error_t make_minimal_context(
    intrinsiccv_morphology_context_t **context, size_t type_size) {
  return intrinsiccv_morphology_create(
      context, intrinsiccv_rectangle_t{1, 1}, intrinsiccv_point_t{0, 0},
      INTRINSICCV_BORDER_TYPE_REPLICATE,
      intrinsiccv_border_values_t{0, 0, 1, 1}, 1, 1, type_size,
      intrinsiccv_rectangle_t{1, 1});
}

TYPED_TEST(DilateTest, NullPointer) {
  intrinsiccv_morphology_context_t *context = nullptr;
  ASSERT_EQ(INTRINSICCV_OK, make_minimal_context(&context, sizeof(TypeParam)));
  TypeParam src[1] = {}, dst[1];
  test::test_null_args(dilate<TypeParam>(), src, sizeof(TypeParam), dst,
                       sizeof(TypeParam), 1, 1, context);
  EXPECT_EQ(INTRINSICCV_OK, intrinsiccv_morphology_release(context));
}

TYPED_TEST(ErodeTest, NullPointer) {
  intrinsiccv_morphology_context_t *context = nullptr;
  ASSERT_EQ(INTRINSICCV_OK, make_minimal_context(&context, sizeof(TypeParam)));

  TypeParam src[1] = {}, dst[1];
  test::test_null_args(erode<TypeParam>(), src, sizeof(TypeParam), dst,
                       sizeof(TypeParam), 1, 1, context);

  EXPECT_EQ(INTRINSICCV_OK, intrinsiccv_morphology_release(context));
}

TYPED_TEST(DilateTest, Misalignment) {
  if (sizeof(TypeParam) == 1) {
    // misalignment impossible
    return;
  }
  intrinsiccv_morphology_context_t *context = nullptr;
  ASSERT_EQ(INTRINSICCV_OK, make_minimal_context(&context, sizeof(TypeParam)));
  TypeParam src[1] = {}, dst[1];
  EXPECT_EQ(INTRINSICCV_ERROR_ALIGNMENT,
            dilate<TypeParam>()(src, sizeof(TypeParam) + 1, dst,
                                sizeof(TypeParam), 1, 1, context));
  EXPECT_EQ(INTRINSICCV_ERROR_ALIGNMENT,
            dilate<TypeParam>()(src, sizeof(TypeParam), dst,
                                sizeof(TypeParam) + 1, 1, 1, context));
  EXPECT_EQ(INTRINSICCV_OK, intrinsiccv_morphology_release(context));
}

TYPED_TEST(ErodeTest, Misalignment) {
  if (sizeof(TypeParam) == 1) {
    // misalignment impossible
    return;
  }
  intrinsiccv_morphology_context_t *context = nullptr;
  ASSERT_EQ(INTRINSICCV_OK, make_minimal_context(&context, sizeof(TypeParam)));
  TypeParam src[1] = {}, dst[1];
  EXPECT_EQ(INTRINSICCV_ERROR_ALIGNMENT,
            erode<TypeParam>()(src, sizeof(TypeParam) + 1, dst,
                               sizeof(TypeParam), 1, 1, context));
  EXPECT_EQ(INTRINSICCV_ERROR_ALIGNMENT,
            erode<TypeParam>()(src, sizeof(TypeParam), dst,
                               sizeof(TypeParam) + 1, 1, 1, context));
  EXPECT_EQ(INTRINSICCV_OK, intrinsiccv_morphology_release(context));
}

TYPED_TEST(DilateTest, ImageSize) {
  intrinsiccv_morphology_context_t *context = nullptr;
  ASSERT_EQ(INTRINSICCV_OK, make_minimal_context(&context, sizeof(TypeParam)));
  TypeParam src[1], dst[1];
  EXPECT_EQ(INTRINSICCV_ERROR_RANGE,
            dilate<TypeParam>()(src, sizeof(TypeParam), dst, sizeof(TypeParam),
                                INTRINSICCV_MAX_IMAGE_PIXELS + 1, 1, context));
  EXPECT_EQ(INTRINSICCV_ERROR_RANGE,
            dilate<TypeParam>()(src, sizeof(TypeParam), dst, sizeof(TypeParam),
                                INTRINSICCV_MAX_IMAGE_PIXELS,
                                INTRINSICCV_MAX_IMAGE_PIXELS, context));
  EXPECT_EQ(INTRINSICCV_OK, intrinsiccv_morphology_release(context));
}

TYPED_TEST(ErodeTest, ImageSize) {
  intrinsiccv_morphology_context_t *context = nullptr;
  ASSERT_EQ(INTRINSICCV_OK, make_minimal_context(&context, sizeof(TypeParam)));
  TypeParam src[1], dst[1];
  EXPECT_EQ(INTRINSICCV_ERROR_RANGE,
            erode<TypeParam>()(src, sizeof(TypeParam), dst, sizeof(TypeParam),
                               INTRINSICCV_MAX_IMAGE_PIXELS + 1, 1, context));
  EXPECT_EQ(INTRINSICCV_ERROR_RANGE,
            erode<TypeParam>()(src, sizeof(TypeParam), dst, sizeof(TypeParam),
                               INTRINSICCV_MAX_IMAGE_PIXELS,
                               INTRINSICCV_MAX_IMAGE_PIXELS, context));
  EXPECT_EQ(INTRINSICCV_OK, intrinsiccv_morphology_release(context));
}

TYPED_TEST(DilateTest, InvalidContextSizeType) {
  intrinsiccv_morphology_context_t *context = nullptr;
  ASSERT_EQ(INTRINSICCV_OK,
            make_minimal_context(&context, sizeof(TypeParam) + 1));
  TypeParam src[1], dst[1];
  EXPECT_EQ(INTRINSICCV_ERROR_CONTEXT_MISMATCH,
            dilate<TypeParam>()(src, sizeof(TypeParam), dst, sizeof(TypeParam),
                                1, 1, context));
  EXPECT_EQ(INTRINSICCV_OK, intrinsiccv_morphology_release(context));
}

TYPED_TEST(ErodeTest, InvalidContextSizeType) {
  intrinsiccv_morphology_context_t *context = nullptr;
  ASSERT_EQ(INTRINSICCV_OK,
            make_minimal_context(&context, sizeof(TypeParam) + 1));
  TypeParam src[1], dst[1];
  EXPECT_EQ(INTRINSICCV_ERROR_CONTEXT_MISMATCH,
            erode<TypeParam>()(src, sizeof(TypeParam), dst, sizeof(TypeParam),
                               1, 1, context));
  EXPECT_EQ(INTRINSICCV_OK, intrinsiccv_morphology_release(context));
}

TYPED_TEST(DilateTest, InvalidContextImageSize) {
  intrinsiccv_morphology_context_t *context = nullptr;
  ASSERT_EQ(INTRINSICCV_OK, make_minimal_context(&context, sizeof(TypeParam)));
  TypeParam src[1], dst[1];
  EXPECT_EQ(INTRINSICCV_ERROR_CONTEXT_MISMATCH,
            dilate<TypeParam>()(src, sizeof(TypeParam), dst, sizeof(TypeParam),
                                2, 1, context));
  EXPECT_EQ(INTRINSICCV_OK, intrinsiccv_morphology_release(context));
}

TYPED_TEST(ErodeTest, InvalidContextImageSize) {
  intrinsiccv_morphology_context_t *context = nullptr;
  ASSERT_EQ(INTRINSICCV_OK, make_minimal_context(&context, sizeof(TypeParam)));
  TypeParam src[1], dst[1];
  EXPECT_EQ(INTRINSICCV_ERROR_CONTEXT_MISMATCH,
            erode<TypeParam>()(src, sizeof(TypeParam), dst, sizeof(TypeParam),
                               2, 1, context));
  EXPECT_EQ(INTRINSICCV_OK, intrinsiccv_morphology_release(context));
}
