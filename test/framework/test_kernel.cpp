// SPDX-FileCopyrightText: 2023 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#include <gtest/gtest.h>

#include "framework/kernel.h"

/// Tests that the constructor of test::Kernel<T> works for odd width and
/// height.
TEST(Kernel, ConstructOdd) {
  using ElementType = uint8_t;

  test::Array2D<ElementType> mask{3, 3};
  mask.set(0, 0, {1, 2, 3});
  mask.set(2, 0, {4, 5, 6});

  test::Kernel<ElementType> kernel{mask};
  EXPECT_EQ(kernel.width(), 3);
  EXPECT_EQ(kernel.height(), 3);
  EXPECT_EQ(kernel.channels(), 1);
  EXPECT_EQ(kernel.anchor().x, 1);
  EXPECT_EQ(kernel.anchor().y, 1);

  EXPECT_EQ(kernel.top(), 1);
  EXPECT_EQ(kernel.left(), 1);
  EXPECT_EQ(kernel.bottom(), 1);
  EXPECT_EQ(kernel.right(), 1);

  EXPECT_EQ(kernel.at(0, 0)[0], 1);
  EXPECT_EQ(kernel.at(0, 1)[0], 2);
  EXPECT_EQ(kernel.at(0, 2)[0], 3);
  EXPECT_EQ(kernel.at(1, 0)[0], 0);
  EXPECT_EQ(kernel.at(1, 1)[0], 0);
  EXPECT_EQ(kernel.at(1, 2)[0], 0);
  EXPECT_EQ(kernel.at(2, 0)[0], 4);
  EXPECT_EQ(kernel.at(2, 1)[0], 5);
  EXPECT_EQ(kernel.at(2, 2)[0], 6);
}

/// Tests that the constructor of test::Kernel<T> works for even width and
/// height.
TEST(Kernel, ConstructEven) {
  using ElementType = uint8_t;

  test::Array2D<ElementType> mask{4, 4};
  test::Kernel<ElementType> kernel{mask};
  EXPECT_EQ(kernel.width(), 4);
  EXPECT_EQ(kernel.height(), 4);
  EXPECT_EQ(kernel.channels(), 1);
  EXPECT_EQ(kernel.anchor().x, 2);
  EXPECT_EQ(kernel.anchor().y, 2);

  EXPECT_EQ(kernel.top(), 2);
  EXPECT_EQ(kernel.left(), 2);
  EXPECT_EQ(kernel.bottom(), 1);
  EXPECT_EQ(kernel.right(), 1);
}

/// Tests that the default constructor of test::Kernel<T> works with all
/// zero arguments.
TEST(Kernel, ConstructEmpty) {
  using ElementType = uint8_t;

  test::Array2D<ElementType> mask{0, 0};
  test::Kernel<ElementType> kernel{mask};
  EXPECT_EQ(kernel.width(), 0);
  EXPECT_EQ(kernel.height(), 0);
  EXPECT_EQ(kernel.channels(), 1);
  EXPECT_EQ(kernel.anchor().x, 0);
  EXPECT_EQ(kernel.anchor().y, 0);
  EXPECT_EQ(kernel.top(), 0);
  EXPECT_EQ(kernel.left(), 0);
  EXPECT_EQ(kernel.bottom(), 0);
  EXPECT_EQ(kernel.right(), 0);
}
