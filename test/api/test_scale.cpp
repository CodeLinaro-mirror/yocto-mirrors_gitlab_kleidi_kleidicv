// SPDX-FileCopyrightText: 2024 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#include <gtest/gtest.h>

#include <type_traits>

#include "framework/operation.h"
#include "intrinsiccv/intrinsiccv.h"

#define INTRINSICCV_SCALE(type, suffix) \
  INTRINSICCV_API(scale, intrinsiccv_scale_##suffix, type)

INTRINSICCV_SCALE(uint8_t, u8);

template <typename ElementType>
class ScaleTest : public testing::Test {};

using ElementTypes = ::testing::Types<uint8_t>;
TYPED_TEST_SUITE(ScaleTest, ElementTypes);

TYPED_TEST(ScaleTest, NullPointer) {
  TypeParam src[1] = {}, dst[1];
  test::test_null_args(scale<TypeParam>(), src, sizeof(TypeParam), dst,
                       sizeof(TypeParam), 1, 1, 2, 0);
}

TYPED_TEST(ScaleTest, Misalignment) {
  if (sizeof(TypeParam) == 1) {
    // misalignment impossible
    return;
  }
  TypeParam src[1] = {}, dst[1];
  EXPECT_EQ(INTRINSICCV_ERROR_ALIGNMENT,
            scale<TypeParam>()(src, sizeof(TypeParam) + 1, dst,
                               sizeof(TypeParam), 1, 1, 2, 0));
  EXPECT_EQ(INTRINSICCV_ERROR_ALIGNMENT,
            scale<TypeParam>()(src, sizeof(TypeParam), dst,
                               sizeof(TypeParam) + 1, 1, 1, 2, 0));
}
