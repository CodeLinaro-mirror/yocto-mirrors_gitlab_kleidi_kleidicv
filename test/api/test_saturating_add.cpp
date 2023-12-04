// SPDX-FileCopyrightText: 2023 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#include <gtest/gtest.h>
#include <intrinsiccv.h>

#include "framework/array.h"
#include "framework/utils.h"

static void test_intrinsiccv_saturating_add_u8(size_t src_a_padding,
                                               size_t src_b_padding,
                                               size_t dst_padding) {
  // '3': The operation is unrolled twice. We need a full vector path and some
  // scalar processing.
  const size_t width = 3 * test::Options::vector_length() - 1;
  const size_t height = 1;

  test::Array2D<uint8_t> source_a{width, height, src_a_padding};
  ASSERT_TRUE(source_a.valid());
  test::Array2D<uint8_t> source_b{width, height, src_b_padding};
  ASSERT_TRUE(source_b.valid());
  test::Array2D<uint8_t> actual{width, height, dst_padding};
  ASSERT_TRUE(actual.valid());
  test::Array2D<uint8_t> expected{width, height};
  ASSERT_TRUE(expected.valid());

  // This will test the vector path.
  // clang-format off
  source_a.set(0, 0, {0, 1, 127, 127, 254, 254, 255});
  source_b.set(0, 0, {0, 1, 127, 128,   1,   2, 255});
  expected.set(0, 0, {0, 2, 254, 255, 255, 255, 255});
  // clang-format on

  // This will test the scalar path, if present.
  size_t col = 2 * test::Options::vector_length();
  // clang-format off
  source_a.set(0, col, {0, 1, 127, 127, 254, 254, 255});
  source_b.set(0, col, {0, 1, 127, 128,   1,   2, 255});
  expected.set(0, col, {0, 2, 254, 255, 255, 255, 255});
  // clang-format on

  intrinsiccv_saturating_add_u8(source_a.data(), source_a.stride(),
                                source_b.data(), source_b.stride(),
                                actual.data(), actual.stride(), width, height);
  EXPECT_EQ_ARRAY2D(expected, actual);
}

/// Tests that \ref intrinsiccv_saturating_add_u8 works when there are no
/// padding bytes at the end of rows.
TEST(Add, U8_NoPadding) { test_intrinsiccv_saturating_add_u8(0, 0, 0); }

/// Tests that \ref intrinsiccv_saturating_add_u8 works when there are padding
/// bytes at the end of rows.
TEST(Add, U8_WithPadding) {
  size_t padding = test::Options::vector_length();
  test_intrinsiccv_saturating_add_u8(padding, 0, 0);
}
