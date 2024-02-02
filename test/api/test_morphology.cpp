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

static intrinsiccv_morphology_params_t make_minimal_params(size_t type_size) {
  intrinsiccv_morphology_params_t params = {
      .kernel = {1, 1},
      .anchor = {0, 0},
      .border_type = INTRINSICCV_BORDER_TYPE_REPLICATE,
      .border_values = {0, 0, 1, 1},
      .channels = 1,
      .iterations = 1,
      .type_size = type_size,
      .impl = nullptr,
      .data = nullptr,
  };
  return params;
}

TYPED_TEST(DilateTest, NullPointer) {
  intrinsiccv_morphology_params_t params =
      make_minimal_params(sizeof(TypeParam));
  ASSERT_EQ(INTRINSICCV_OK, intrinsiccv_morphology_create(
                                &params, intrinsiccv_rectangle_t{1, 1}));
  TypeParam src[1] = {}, dst[1];
  test::test_null_args(dilate<TypeParam>(), src, sizeof(TypeParam), dst,
                       sizeof(TypeParam), 1, 1, &params);

  {
    intrinsiccv_morphology_params_t invalid_params = params;
    invalid_params.impl = nullptr;
    EXPECT_EQ(INTRINSICCV_ERROR_NULL_POINTER,
              dilate<TypeParam>()(src, sizeof(TypeParam), dst,
                                  sizeof(TypeParam), 1, 1, &invalid_params));
  }

  {
    intrinsiccv_morphology_params_t invalid_params = params;
    invalid_params.data = nullptr;
    EXPECT_EQ(INTRINSICCV_ERROR_NULL_POINTER,
              dilate<TypeParam>()(src, sizeof(TypeParam), dst,
                                  sizeof(TypeParam), 1, 1, &invalid_params));
  }

  EXPECT_EQ(INTRINSICCV_OK, intrinsiccv_morphology_release(&params));
}

TYPED_TEST(ErodeTest, NullPointer) {
  intrinsiccv_morphology_params_t params =
      make_minimal_params(sizeof(TypeParam));
  ASSERT_EQ(INTRINSICCV_OK, intrinsiccv_morphology_create(
                                &params, intrinsiccv_rectangle_t{1, 1}));

  TypeParam src[1] = {}, dst[1];
  test::test_null_args(erode<TypeParam>(), src, sizeof(TypeParam), dst,
                       sizeof(TypeParam), 1, 1, &params);

  {
    intrinsiccv_morphology_params_t invalid_params = params;
    invalid_params.impl = nullptr;
    EXPECT_EQ(INTRINSICCV_ERROR_NULL_POINTER,
              erode<TypeParam>()(src, sizeof(TypeParam), dst, sizeof(TypeParam),
                                 1, 1, &invalid_params));
  }

  {
    intrinsiccv_morphology_params_t invalid_params = params;
    invalid_params.data = nullptr;
    EXPECT_EQ(INTRINSICCV_ERROR_NULL_POINTER,
              erode<TypeParam>()(src, sizeof(TypeParam), dst, sizeof(TypeParam),
                                 1, 1, &invalid_params));
  }

  EXPECT_EQ(INTRINSICCV_OK, intrinsiccv_morphology_release(&params));
}
