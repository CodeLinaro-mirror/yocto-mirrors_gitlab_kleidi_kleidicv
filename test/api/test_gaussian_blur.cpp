// SPDX-FileCopyrightText: 2024 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#include <gtest/gtest.h>

#include "framework/utils.h"
#include "intrinsiccv/intrinsiccv.h"

#define INTRINSICCV_GAUSSIAN_BLUR(type, kernel_suffix, type_suffix)          \
  INTRINSICCV_API(gaussian_blur_##kernel_suffix,                             \
                  intrinsiccv_gaussian_blur_##kernel_suffix##_##type_suffix, \
                  type)

INTRINSICCV_GAUSSIAN_BLUR(uint8_t, 3x3, u8);
INTRINSICCV_GAUSSIAN_BLUR(uint8_t, 5x5, u8);

using ElementTypes = ::testing::Types<uint8_t>;

template <typename ElementType>
class GaussianBlurTest : public testing::Test {};

TYPED_TEST_SUITE(GaussianBlurTest, ElementTypes);

TYPED_TEST(GaussianBlurTest, NullPointer) {
  intrinsiccv_filter_context_t *context = nullptr;
  ASSERT_EQ(INTRINSICCV_OK,
            intrinsiccv_filter_create(&context, 1, sizeof(TypeParam),
                                      intrinsiccv_rectangle_t{1, 1}));
  TypeParam src[1] = {}, dst[1];
  test::test_null_args(gaussian_blur_3x3<TypeParam>(), src, sizeof(TypeParam),
                       dst, sizeof(TypeParam), 1, 1, 1,
                       INTRINSICCV_BORDER_TYPE_REFLECT, context);
  test::test_null_args(gaussian_blur_5x5<TypeParam>(), src, sizeof(TypeParam),
                       dst, sizeof(TypeParam), 1, 1, 1,
                       INTRINSICCV_BORDER_TYPE_REFLECT, context);
  EXPECT_EQ(INTRINSICCV_OK, intrinsiccv_filter_release(context));
}

TYPED_TEST(GaussianBlurTest, Misalignment) {
  if (sizeof(TypeParam) == 1) {
    // misalignment impossible
    return;
  }
  intrinsiccv_filter_context_t *context = nullptr;
  ASSERT_EQ(INTRINSICCV_OK,
            intrinsiccv_filter_create(&context, 1, sizeof(TypeParam),
                                      intrinsiccv_rectangle_t{1, 1}));
  TypeParam src[1] = {}, dst[1];
  EXPECT_EQ(INTRINSICCV_ERROR_ALIGNMENT,
            gaussian_blur_3x3<TypeParam>()(
                src, sizeof(TypeParam) + 1, dst, sizeof(TypeParam), 1, 1, 1,
                INTRINSICCV_BORDER_TYPE_REFLECT, context));
  EXPECT_EQ(INTRINSICCV_ERROR_ALIGNMENT,
            gaussian_blur_3x3<TypeParam>()(
                src, sizeof(TypeParam), dst, sizeof(TypeParam) + 1, 1, 1, 1,
                INTRINSICCV_BORDER_TYPE_REFLECT, context));
  EXPECT_EQ(INTRINSICCV_ERROR_ALIGNMENT,
            gaussian_blur_5x5<TypeParam>()(
                src, sizeof(TypeParam) + 1, dst, sizeof(TypeParam), 1, 1, 1,
                INTRINSICCV_BORDER_TYPE_REFLECT, context));
  EXPECT_EQ(INTRINSICCV_ERROR_ALIGNMENT,
            gaussian_blur_5x5<TypeParam>()(
                src, sizeof(TypeParam), dst, sizeof(TypeParam) + 1, 1, 1, 1,
                INTRINSICCV_BORDER_TYPE_REFLECT, context));
  EXPECT_EQ(INTRINSICCV_OK, intrinsiccv_filter_release(context));
}
