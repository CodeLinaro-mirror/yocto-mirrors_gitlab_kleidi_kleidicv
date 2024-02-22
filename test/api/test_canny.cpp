// SPDX-FileCopyrightText: 2024 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#include <gtest/gtest.h>

#include "framework/utils.h"
#include "intrinsiccv/intrinsiccv.h"

#if INTRINSICCV_EXPERIMENTAL_FEATURE_CANNY

#define INTRINSICCV_CANNY(type, suffix) \
  INTRINSICCV_API(canny, intrinsiccv_canny_##suffix, type)

INTRINSICCV_CANNY(uint8_t, u8);

using ElementTypes = ::testing::Types<uint8_t>;

template <typename ElementType>
class CannyTest : public testing::Test {};

TYPED_TEST_SUITE(CannyTest, ElementTypes);

TYPED_TEST(CannyTest, NullPointer) {
  TypeParam src[1], dst[1];
  test::test_null_args(canny<TypeParam>(), src, sizeof(TypeParam), dst,
                       sizeof(TypeParam), 1, 1, 0.0, 1.0);
}

TYPED_TEST(CannyTest, Misalignment) {
  if (sizeof(TypeParam) == 1) {
    // misalignment impossible
    return;
  }
  TypeParam src[1], dst[1];
  EXPECT_EQ(INTRINSICCV_ERROR_ALIGNMENT,
            canny<TypeParam>()(src, sizeof(TypeParam) + 1, dst,
                               sizeof(TypeParam), 1, 1, 0.0, 1.0));
  EXPECT_EQ(INTRINSICCV_ERROR_ALIGNMENT,
            canny<TypeParam>()(src, sizeof(TypeParam), dst,
                               sizeof(TypeParam) + 1, 1, 1, 0.0, 1.0));
}

TYPED_TEST(CannyTest, ImageSize) {
  TypeParam src[1], dst[1];
  EXPECT_EQ(INTRINSICCV_ERROR_RANGE,
            canny<TypeParam>()(src, sizeof(TypeParam), dst, sizeof(TypeParam),
                               INTRINSICCV_MAX_IMAGE_PIXELS + 1, 1, 0.0, 1.0));
  EXPECT_EQ(INTRINSICCV_ERROR_RANGE,
            canny<TypeParam>()(src, sizeof(TypeParam), dst, sizeof(TypeParam),
                               INTRINSICCV_MAX_IMAGE_PIXELS,
                               INTRINSICCV_MAX_IMAGE_PIXELS, 0.0, 1.0));
}

#endif  // INTRINSICCV_EXPERIMENTAL_FEATURE_CANNY
