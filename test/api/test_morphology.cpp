// SPDX-FileCopyrightText: 2024 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#include <gtest/gtest.h>
#include <intrinsiccv.h>

#include "framework/operation.h"

#define INTRINSICCV_DILATE(type, suffix) \
  INTRINSICCV_API(dilate, intrinsiccv_dilate_##suffix, type)

#define INTRINSICCV_ERODE(type, suffix) \
  INTRINSICCV_API(erode, intrinsiccv_erode_##suffix, type)

INTRINSICCV_DILATE(uint8_t, u8);
INTRINSICCV_ERODE(uint8_t, u8);

template <typename ElementType>
class DilateTest : public testing::Test {};

template <typename ElementType>
class ErodeTest : public testing::Test {};

using ElementTypes = ::testing::Types<uint8_t>;
TYPED_TEST_SUITE(DilateTest, ElementTypes);
TYPED_TEST_SUITE(ErodeTest, ElementTypes);

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
