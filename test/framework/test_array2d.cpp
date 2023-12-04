// SPDX-FileCopyrightText: 2023 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#include <gtest/gtest-spi.h>
#include <gtest/gtest.h>

#include "framework/array.h"
#include "framework/utils.h"

/// Tests that test::Array2D<T> constructor always creates an object with the
/// same contents.
TEST(Array2D, constructor) {
  size_t width = 5, height = 5;
  test::Array2D<uint32_t> array_1{width, height};
  test::Array2D<uint32_t> array_2{width, height};
  EXPECT_EQ_ARRAY2D(array_1, array_2);
}

/// Tests that test::Array2D<T>.get() works for set/get.
TEST(Array2D, get) {
  size_t width = 1, height = 1;
  test::Array2D<uint32_t> array_1{width, height};
  test::Array2D<uint32_t> array_2{width, height};

  array_1.at(0, 0)[0] = 1;
  EXPECT_EQ(array_1.at(0, 0)[0], 1);

  array_1.at(0, 0)[0] = 2;
  EXPECT_EQ(array_1.at(0, 0)[0], 2);
}

/// Tests that test::Array2D<T>.set() works.
TEST(Array2D, set) {
  size_t width = 5, height = 2;
  test::Array2D<uint32_t> array_1{width, height};

  array_1.set(0, 0, {1, 2, 3, 4, 5});
  EXPECT_EQ(array_1.at(0, 0)[0], 1);
  EXPECT_EQ(array_1.at(0, 1)[0], 2);
  EXPECT_EQ(array_1.at(0, 2)[0], 3);
  EXPECT_EQ(array_1.at(0, 3)[0], 4);
  EXPECT_EQ(array_1.at(0, 4)[0], 5);

  array_1.set(0, 4, {42});
  EXPECT_EQ(array_1.at(0, 4)[0], 42);

  array_1.set(1, 0, {11});
  EXPECT_EQ(array_1.at(1, 0)[0], 11);
}

/// Tests that test::Array2D<T>.fill() works.
TEST(Array2D, fill) {
  size_t width = 5, height = 2;
  test::Array2D<uint32_t> array_1{width, height};
  test::Array2D<uint32_t> array_2{width, height};

  array_1.fill(0);
  for (size_t row = 0; row < height; ++row) {
    for (size_t column = 0; column < width; ++column) {
      EXPECT_EQ(array_1.at(row, column)[0], 0);
    }
  }

  array_1.fill(42);
  for (size_t row = 0; row < height; ++row) {
    for (size_t column = 0; column < width; ++column) {
      EXPECT_EQ(array_1.at(row, column)[0], 42);
    }
  }
}

/// Tests that EXPECT_EQ_ARRAY2D() macro works for fully-equal objects.
TEST(Array2D, ExpectEq_Equal) {
  size_t width = 5, height = 2;
  test::Array2D<uint32_t> array_1{width, height};
  test::Array2D<uint32_t> array_2{width, height};
  EXPECT_EQ_ARRAY2D(array_1, array_2);
}

/// Tests that EXPECT_EQ_ARRAY2D() macro works for equal objects where stride is
/// different.
TEST(Array2D, ExpectEq_Equal_StrideInvariant) {
  size_t width = 5, height = 2;
  test::Array2D<uint32_t> array_1{width, height};
  test::Array2D<uint32_t> array_2{width, height, 1};
  EXPECT_EQ_ARRAY2D(array_1, array_2);
}

/// Tests that EXPECT_EQ_ARRAY2D() macro works for non-equal objects where width
/// is different.
TEST(Array2D, ExpectEq_NotEqual_Width) {
  struct Test {
    static void test() {
      size_t width = 5, height = 2;
      test::Array2D<uint32_t> array_1{width, height};
      test::Array2D<uint32_t> array_2{width + 1, height};
      EXPECT_EQ_ARRAY2D(array_1, array_2);
    }
  };

  EXPECT_FATAL_FAILURE(Test::test(), "Mismatch in width.");
}

/// Tests that EXPECT_EQ_ARRAY2D() macro works for non-equal objects where
/// height is different.
TEST(Array2D, ExpectEq_NotEqual_Height) {
  struct Test {
    static void test() {
      size_t width = 5, height = 2;
      test::Array2D<uint32_t> array_1{width, height};
      test::Array2D<uint32_t> array_2{width, height + 1};
      EXPECT_EQ_ARRAY2D(array_1, array_2);
    }
  };

  EXPECT_FATAL_FAILURE(Test::test(), "Mismatch in height.");
}

/// Tests that EXPECT_EQ_ARRAY2D() macro works for non-equal objects where data
/// is different.
TEST(Array2D, ExpectEq_NotEqual_Data) {
  struct Test {
    static void test() {
      size_t width = 5, height = 2;
      test::Array2D<uint32_t> array_1{width, height};
      test::Array2D<uint32_t> array_2{width, height};
      array_2.at(0, 0)[0] = 42;
      EXPECT_EQ_ARRAY2D(array_1, array_2);
    }
  };

  EXPECT_FATAL_FAILURE(Test::test(), "Mismatch at (row=0, col=0): 0 vs 42.");
}

/// Tests that EXPECT_NE_ARRAY2D() macro works for equal objects.
TEST(Array2D, ExpectNe_Equal) {
  struct Test {
    static void test() {
      size_t width = 5, height = 2;
      test::Array2D<uint32_t> array_1{width, height};
      test::Array2D<uint32_t> array_2{width, height};
      EXPECT_NE_ARRAY2D(array_1, array_2);
    }
  };

  EXPECT_FATAL_FAILURE(Test::test(),
                       "Objects are equal, but expected to differ.");
}

/// Tests that EXPECT_NE_ARRAY2D() macro works for non-equal objects where width
/// is different.
TEST(Array2D, ExpectNe_NotEqual_Width) {
  struct Test {
    static void test() {
      size_t width = 5, height = 2;
      test::Array2D<uint32_t> array_1{width, height};
      test::Array2D<uint32_t> array_2{width + 1, height};
      EXPECT_NE_ARRAY2D(array_1, array_2);
    }
  };

  EXPECT_FATAL_FAILURE(Test::test(), "Mismatch in width.");
}

/// Tests that EXPECT_NE_ARRAY2D() macro works for non-equal objects where
/// height is different.
TEST(Array2D, ExpectNe_NotEqual_Height) {
  struct Test {
    static void test() {
      size_t width = 5, height = 2;
      test::Array2D<uint32_t> array_1{width, height};
      test::Array2D<uint32_t> array_2{width, height + 1};
      EXPECT_NE_ARRAY2D(array_1, array_2);
    }
  };

  EXPECT_FATAL_FAILURE(Test::test(), "Mismatch in height.");
}

/// Tests that EXPECT_NE_ARRAY2D() macro works for non-equal objects where there
/// is a difference in data.
TEST(Array2D, ExpectNe_NotEqual_Data) {
  size_t width = 5, height = 2;
  test::Array2D<uint32_t> array_1{width, height};
  test::Array2D<uint32_t> array_2{width, height};
  array_2.at(1, 0)[0] = 42;
  EXPECT_NE_ARRAY2D(array_1, array_2);
}

static void PaddingClobbered(size_t row, size_t offset) {
  using ElementType = uint32_t;

  size_t width = 5, height = 2, padding = 10;
  size_t stride = width * sizeof(ElementType) + padding;
  test::Array2D<ElementType> array(width, height, padding);

  uint8_t *ptr = reinterpret_cast<uint8_t *>(array.data());
  ptr[row * stride + width * sizeof(ElementType) + offset] = 1;
}

/// Tests that clobbering the first padding byte in the first row is caught.
TEST(Array2D, PaddingClobbered_FirstRow_FirstByte) {
  EXPECT_FATAL_FAILURE(PaddingClobbered(0, 0),
                       "Padding byte was overwritten at (row=0, offset=20)");
}

/// Tests that clobbering the last padding byte in the first row is caught.
TEST(Array2D, PaddingClobbered_FirstRow_LastByte) {
  EXPECT_FATAL_FAILURE(PaddingClobbered(0, 9),
                       "Padding byte was overwritten at (row=0, offset=29)");
}

/// Tests that clobbering the first padding byte in the last row is caught.
TEST(Array2D, PaddingClobbered_LastRow_FirstByte) {
  EXPECT_FATAL_FAILURE(PaddingClobbered(1, 0),
                       "Padding byte was overwritten at (row=1, offset=20)");
}

/// Tests that clobbering the last padding byte in the last row is caught.
TEST(Array2D, PaddingClobbered_LastRow_LastByte) {
  EXPECT_FATAL_FAILURE(PaddingClobbered(1, 9),
                       "Padding byte was overwritten at (row=1, offset=29)");
}

/// Additional tests for coverage purposes.
TEST(Array2D, Coverage) {
  test::Array2D<uint64_t> array{std::numeric_limits<size_t>::max(), 1};
  EXPECT_FALSE(array.valid());

  array.set(0, 0, {});

  uint64_t *ptr_1 = array.at(0, 0);
  EXPECT_EQ(ptr_1, nullptr);
}
