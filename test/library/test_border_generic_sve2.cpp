// SPDX-FileCopyrightText: 2025 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#include <gtest/gtest.h>

#include "../../kleidicv/include/kleidicv/types.h"
#include "../../kleidicv/src/filters/border_generic_sc.h"
#include "framework/array.h"
#include "framework/utils.h"
#include "kleidicv/workspace/border_types.h"

template <typename ElementType>
void test_sve_border(size_t width, size_t margin, size_t channels,
                     kleidicv::FixedBorderType border_type,
                     std::initializer_list<ElementType> expected_values) {
  size_t total_width = channels * (width + 2 * margin);
  test::Array2D<ElementType> expected(total_width, 1);
  expected.set(0, 0, expected_values);

  test::Array2D<ElementType> actual(total_width, 1);
  for (size_t x = 0; x < width * channels; ++x) {
    actual.set(0, margin * channels + x,
               {*expected.at(0, margin * channels + x)});
  }

  kleidicv::Rows<ElementType> rows(actual.at(0, margin * channels), width,
                                   channels);
  if (border_type == kleidicv::FixedBorderType::REPLICATE) {
    if (channels == 3) {
      svuint8_t sv0, sv1, sv2;
      KLEIDICV_TARGET_NAMESPACE::BorderMaker3ch<
          ElementType, kleidicv::FixedBorderType::REPLICATE>
          border(sv0, sv1, sv2);
      border.make(rows, margin, width);
    } else {
      svuint8_t sv;
      KLEIDICV_TARGET_NAMESPACE::BorderMaker124ch<
          ElementType, kleidicv::FixedBorderType::REPLICATE>
          border(static_cast<ptrdiff_t>(channels), sv);
      border.make(rows, margin, width);
    }
  } else if (border_type == kleidicv::FixedBorderType::REVERSE) {
    svuint8_t sv;
    svbool_t pg;
    KLEIDICV_TARGET_NAMESPACE::BorderMaker<ElementType,
                                           kleidicv::FixedBorderType::REVERSE>
        border(static_cast<ptrdiff_t>(channels), sv, pg);
    border.make(rows, margin, width);
  } else if (border_type == kleidicv::FixedBorderType::REFLECT) {
    svuint8_t sv;
    svbool_t pg;
    KLEIDICV_TARGET_NAMESPACE::BorderMaker<ElementType,
                                           kleidicv::FixedBorderType::REFLECT>
        border(static_cast<ptrdiff_t>(channels), sv, pg);
    border.make(rows, margin, width);
  } else if (border_type == kleidicv::FixedBorderType::WRAP) {
    svbool_t pg;
    KLEIDICV_TARGET_NAMESPACE::BorderMaker<ElementType,
                                           kleidicv::FixedBorderType::WRAP>
        border(static_cast<ptrdiff_t>(channels), pg);
    border.make(rows, margin, width);
  }

  EXPECT_EQ_ARRAY2D(expected, actual);
}

TEST(BorderMaker, Replicate_1Ch_1Element) {
  test_sve_border<uint8_t>(6, 1, 1, kleidicv::FixedBorderType::REPLICATE,
                           {1, 1, 2, 3, 4, 5, 6, 6});
}

TEST(BorderMaker, Replicate_1Ch_2Elements) {
  test_sve_border<uint8_t>(6, 2, 1, kleidicv::FixedBorderType::REPLICATE,
                           {1, 1, 1, 2, 3, 4, 5, 6, 6, 6});
}

TEST(BorderMaker, Replicate_1Ch_3Elements) {
  test_sve_border<uint8_t>(6, 3, 1, kleidicv::FixedBorderType::REPLICATE,
                           {1, 1, 1, 1, 2, 3, 4, 5, 6, 6, 6, 6});
}

TEST(BorderMaker, Replicate_1Ch_9Elements) {
  test_sve_border<uint8_t>(
      6, 9, 1, kleidicv::FixedBorderType::REPLICATE,
      {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 3, 4, 5, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6});
}

TEST(BorderMaker, Replicate_2Ch_1Element) {
  test_sve_border<uint8_t>(3, 1, 2, kleidicv::FixedBorderType::REPLICATE,
                           {1, 2, 1, 2, 3, 4, 5, 6, 5, 6});
}

TEST(BorderMaker, Replicate_2Ch_2Elements) {
  test_sve_border<uint8_t>(3, 2, 2, kleidicv::FixedBorderType::REPLICATE,
                           {1, 2, 1, 2, 1, 2, 3, 4, 5, 6, 5, 6, 5, 6});
}

TEST(BorderMaker, Replicate_2Ch_3Elements) {
  test_sve_border<uint8_t>(
      3, 3, 2, kleidicv::FixedBorderType::REPLICATE,
      {1, 2, 1, 2, 1, 2, 1, 2, 3, 4, 5, 6, 5, 6, 5, 6, 5, 6});
}

TEST(BorderMaker, Replicate_2Ch_5Elements) {
  test_sve_border<uint8_t>(3, 5, 2, kleidicv::FixedBorderType::REPLICATE,
                           {1, 2, 1, 2, 1, 2, 1, 2, 1, 2, 1, 2, 3,
                            4, 5, 6, 5, 6, 5, 6, 5, 6, 5, 6, 5, 6});
}

TEST(BorderMaker, Replicate_3Ch_1Element) {
  test_sve_border<uint8_t>(3, 1, 3, kleidicv::FixedBorderType::REPLICATE,
                           {1, 2, 3, 1, 2, 3, 4, 5, 6, 7, 8, 9, 7, 8, 9});
}

TEST(BorderMaker, Replicate_3Ch_2Elements) {
  test_sve_border<uint8_t>(
      3, 2, 3, kleidicv::FixedBorderType::REPLICATE,
      {1, 2, 3, 1, 2, 3, 1, 2, 3, 4, 5, 6, 7, 8, 9, 7, 8, 9, 7, 8, 9});
}

TEST(BorderMaker, Replicate_3Ch_3Elements) {
  test_sve_border<uint8_t>(3, 3, 3, kleidicv::FixedBorderType::REPLICATE,
                           {1, 2, 3, 1, 2, 3, 1, 2, 3, 1, 2, 3, 4, 5,
                            6, 7, 8, 9, 7, 8, 9, 7, 8, 9, 7, 8, 9});
}

TEST(BorderMaker, Replicate_3Ch_5Elements) {
  test_sve_border<uint8_t>(
      3, 5, 3, kleidicv::FixedBorderType::REPLICATE,
      {1, 2, 3, 1, 2, 3, 1, 2, 3, 1, 2, 3, 1, 2, 3, 1, 2, 3, 4, 5,
       6, 7, 8, 9, 7, 8, 9, 7, 8, 9, 7, 8, 9, 7, 8, 9, 7, 8, 9});
}

TEST(BorderMaker, Replicate_3Ch_17Elements) {
  test_sve_border<uint8_t>(
      3, 17, 3, kleidicv::FixedBorderType::REPLICATE,
      {1, 2, 3, 1, 2, 3, 1, 2, 3, 1, 2, 3, 1, 2, 3, 1, 2, 3, 1, 2, 3, 1, 2,
       3, 1, 2, 3, 1, 2, 3, 1, 2, 3, 1, 2, 3, 1, 2, 3, 1, 2, 3, 1, 2, 3, 1,
       2, 3, 1, 2, 3, 1, 2, 3, 4, 5, 6, 7, 8, 9, 7, 8, 9, 7, 8, 9, 7, 8, 9,
       7, 8, 9, 7, 8, 9, 7, 8, 9, 7, 8, 9, 7, 8, 9, 7, 8, 9, 7, 8, 9, 7, 8,
       9, 7, 8, 9, 7, 8, 9, 7, 8, 9, 7, 8, 9, 7, 8, 9, 7, 8, 9});
}

TEST(BorderMaker, Replicate_4Ch_1Element) {
  test_sve_border<uint8_t>(
      3, 1, 4, kleidicv::FixedBorderType::REPLICATE,
      {1, 2, 3, 4, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 9, 10, 11, 12});
}

TEST(BorderMaker, Replicate_4Ch_2Elements) {
  test_sve_border<uint8_t>(3, 2, 4, kleidicv::FixedBorderType::REPLICATE,
                           {1, 2, 3, 4,  1,  2,  3, 4,  1,  2,  3, 4,  5,  6,
                            7, 8, 9, 10, 11, 12, 9, 10, 11, 12, 9, 10, 11, 12});
}

TEST(BorderMaker, Replicate_4Ch_3Elements) {
  test_sve_border<uint8_t>(
      3, 3, 4, kleidicv::FixedBorderType::REPLICATE,
      {1, 2, 3, 4,  1,  2,  3, 4,  1,  2,  3, 4,  1,  2,  3, 4,  5,  6,
       7, 8, 9, 10, 11, 12, 9, 10, 11, 12, 9, 10, 11, 12, 9, 10, 11, 12});
}

TEST(BorderMaker, Replicate_4Ch_5Elements) {
  test_sve_border<uint8_t>(
      3, 5, 4, kleidicv::FixedBorderType::REPLICATE,
      {1, 2,  3,  4,  1, 2,  3,  4,  1, 2,  3,  4,  1,  2,  3,  4,  1,  2,
       3, 4,  1,  2,  3, 4,  5,  6,  7, 8,  9,  10, 11, 12, 9,  10, 11, 12,
       9, 10, 11, 12, 9, 10, 11, 12, 9, 10, 11, 12, 9,  10, 11, 12});
}

////////////////////// REVERSE //////////////////////

TEST(BorderMaker, Reverse_1Ch_1Element) {
  test_sve_border<uint8_t>(6, 1, 1, kleidicv::FixedBorderType::REVERSE,
                           {2, 1, 2, 3, 4, 5, 6, 5});
}

TEST(BorderMaker, Reverse_1Ch_2Elements) {
  test_sve_border<uint8_t>(6, 2, 1, kleidicv::FixedBorderType::REVERSE,
                           {3, 2, 1, 2, 3, 4, 5, 6, 5, 4});
}

TEST(BorderMaker, Reverse_1Ch_3Elements) {
  test_sve_border<uint8_t>(6, 3, 1, kleidicv::FixedBorderType::REVERSE,
                           {4, 3, 2, 1, 2, 3, 4, 5, 6, 5, 4, 3});
}

TEST(BorderMaker, Reverse_1Ch_9Elements) {
  test_sve_border<uint8_t>(10, 9, 1, kleidicv::FixedBorderType::REVERSE,
                           {10, 9, 8, 7, 6,  5, 4, 3, 2, 1, 2, 3, 4, 5,
                            6,  7, 8, 9, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1});
}

TEST(BorderMaker, Reverse_2Ch_1Element) {
  test_sve_border<uint8_t>(3, 1, 2, kleidicv::FixedBorderType::REVERSE,
                           {3, 4, 1, 2, 3, 4, 5, 6, 3, 4});
}

TEST(BorderMaker, Reverse_2Ch_2Elements) {
  test_sve_border<uint8_t>(3, 2, 2, kleidicv::FixedBorderType::REVERSE,
                           {5, 6, 3, 4, 1, 2, 3, 4, 5, 6, 3, 4, 1, 2});
}

TEST(BorderMaker, Reverse_2Ch_3Elements) {
  test_sve_border<uint8_t>(
      4, 3, 2, kleidicv::FixedBorderType::REVERSE,
      {7, 8, 5, 6, 3, 4, 1, 2, 3, 4, 5, 6, 7, 8, 5, 6, 3, 4, 1, 2});
}

TEST(BorderMaker, Reverse_2Ch_5Elements) {
  test_sve_border<uint8_t>(
      6, 5, 2, kleidicv::FixedBorderType::REVERSE,
      {11, 12, 9, 10, 7,  8,  5, 6,  3, 4, 1, 2, 3, 4, 5, 6,
       7,  8,  9, 10, 11, 12, 9, 10, 7, 8, 5, 6, 3, 4, 1, 2});
}

TEST(BorderMaker, Reverse_3Ch_1Element) {
  test_sve_border<uint8_t>(3, 1, 3, kleidicv::FixedBorderType::REVERSE,
                           {4, 5, 6, 1, 2, 3, 4, 5, 6, 7, 8, 9, 4, 5, 6});
}

TEST(BorderMaker, Reverse_3Ch_2Elements) {
  test_sve_border<uint8_t>(
      3, 2, 3, kleidicv::FixedBorderType::REVERSE,
      {7, 8, 9, 4, 5, 6, 1, 2, 3, 4, 5, 6, 7, 8, 9, 4, 5, 6, 1, 2, 3});
}

TEST(BorderMaker, Reverse_3Ch_3Elements) {
  test_sve_border<uint8_t>(4, 3, 3, kleidicv::FixedBorderType::REVERSE,
                           {10, 11, 12, 7,  8,  9,  4, 5, 6, 1, 2, 3, 4, 5, 6,
                            7,  8,  9,  10, 11, 12, 7, 8, 9, 4, 5, 6, 1, 2, 3});
}

TEST(BorderMaker, Reverse_3Ch_5Elements) {
  test_sve_border<uint8_t>(
      6, 5, 3, kleidicv::FixedBorderType::REVERSE,
      {16, 17, 18, 13, 14, 15, 10, 11, 12, 7,  8,  9,  4,  5,  6,  1,
       2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12, 13, 14, 15, 16, 17,
       18, 13, 14, 15, 10, 11, 12, 7,  8,  9,  4,  5,  6,  1,  2,  3});
}

TEST(BorderMaker, Reverse_4Ch_1Element) {
  test_sve_border<uint8_t>(
      3, 1, 4, kleidicv::FixedBorderType::REVERSE,
      {5, 6, 7, 8, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 5, 6, 7, 8});
}

TEST(BorderMaker, Reverse_4Ch_2Elements) {
  test_sve_border<uint8_t>(3, 2, 4, kleidicv::FixedBorderType::REVERSE,
                           {9, 10, 11, 12, 5,  6,  7, 8, 1, 2, 3, 4, 5, 6,
                            7, 8,  9,  10, 11, 12, 5, 6, 7, 8, 1, 2, 3, 4});
}

TEST(BorderMaker, Reverse_4Ch_3Elements) {
  test_sve_border<uint8_t>(
      4, 3, 4, kleidicv::FixedBorderType::REVERSE,
      {13, 14, 15, 16, 9,  10, 11, 12, 5, 6,  7,  8,  1, 2, 3, 4, 5, 6, 7, 8,
       9,  10, 11, 12, 13, 14, 15, 16, 9, 10, 11, 12, 5, 6, 7, 8, 1, 2, 3, 4});
}

TEST(BorderMaker, Reverse_4Ch_5Elements) {
  test_sve_border<uint8_t>(
      6, 5, 4, kleidicv::FixedBorderType::REVERSE,
      {21, 22, 23, 24, 17, 18, 19, 20, 13, 14, 15, 16, 9,  10, 11, 12,
       5,  6,  7,  8,  1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12,
       13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 17, 18, 19, 20,
       13, 14, 15, 16, 9,  10, 11, 12, 5,  6,  7,  8,  1,  2,  3,  4});
}

////////////////////// REFLECT //////////////////////

/*      if (column_index == 0) {

      case FixedBorderType::REFLECT:
          return get(6, 5, 4, 3, 2, 1, 0, 0, 1, 2, 3, 4, 5, 6, 7);

      case FixedBorderType::WRAP:
          return get(height_ - 7, height_ - 6, height_ - 5, height_ - 4,
                     height_ - 3, height_ - 2, height_ - 1, 0, 1, 2, 3, 4, 5, 6,
                     7);

      case FixedBorderType::REVERSE:
          return get(7, 6, 5, 4, 3, 2, 1, 0, 1, 2, 3, 4, 5, 6, 7);
*/

TEST(BorderMaker, Reflect_1Ch_1Element) {
  test_sve_border<uint8_t>(6, 1, 1, kleidicv::FixedBorderType::REFLECT,
                           {1, 1, 2, 3, 4, 5, 6, 6});
}

TEST(BorderMaker, Reflect_1Ch_2Elements) {
  test_sve_border<uint8_t>(6, 2, 1, kleidicv::FixedBorderType::REFLECT,
                           {2, 1, 1, 2, 3, 4, 5, 6, 6, 5});
}

TEST(BorderMaker, Reflect_1Ch_3Elements) {
  test_sve_border<uint8_t>(6, 3, 1, kleidicv::FixedBorderType::REFLECT,
                           {3, 2, 1, 1, 2, 3, 4, 5, 6, 6, 5, 4});
}

TEST(BorderMaker, Reflect_1Ch_9Elements) {
  test_sve_border<uint8_t>(10, 9, 1, kleidicv::FixedBorderType::REFLECT,
                           {9, 8, 7, 6, 5,  4,  3, 2, 1, 1, 2, 3, 4, 5,
                            6, 7, 8, 9, 10, 10, 9, 8, 7, 6, 5, 4, 3, 2});
}

TEST(BorderMaker, Reflect_2Ch_1Element) {
  test_sve_border<uint8_t>(3, 1, 2, kleidicv::FixedBorderType::REFLECT,
                           {1, 2, 1, 2, 3, 4, 5, 6, 5, 6});
}

TEST(BorderMaker, Reflect_2Ch_2Elements) {
  test_sve_border<uint8_t>(3, 2, 2, kleidicv::FixedBorderType::REFLECT,
                           {3, 4, 1, 2, 1, 2, 3, 4, 5, 6, 5, 6, 3, 4});
}

TEST(BorderMaker, Reflect_2Ch_3Elements) {
  test_sve_border<uint8_t>(
      4, 3, 2, kleidicv::FixedBorderType::REFLECT,
      {5, 6, 3, 4, 1, 2, 1, 2, 3, 4, 5, 6, 7, 8, 7, 8, 5, 6, 3, 4});
}

TEST(BorderMaker, Reflect_2Ch_5Elements) {
  test_sve_border<uint8_t>(
      6, 5, 2, kleidicv::FixedBorderType::REFLECT,
      {9, 10, 7, 8,  5,  6,  3,  4,  1, 2,  1, 2, 3, 4, 5, 6,
       7, 8,  9, 10, 11, 12, 11, 12, 9, 10, 7, 8, 5, 6, 3, 4});
}

TEST(BorderMaker, Reflect_3Ch_1Element) {
  test_sve_border<uint8_t>(3, 1, 3, kleidicv::FixedBorderType::REFLECT,
                           {1, 2, 3, 1, 2, 3, 4, 5, 6, 7, 8, 9, 7, 8, 9});
}

TEST(BorderMaker, Reflect_3Ch_2Elements) {
  test_sve_border<uint8_t>(
      3, 2, 3, kleidicv::FixedBorderType::REFLECT,
      {4, 5, 6, 1, 2, 3, 1, 2, 3, 4, 5, 6, 7, 8, 9, 7, 8, 9, 4, 5, 6});
}

TEST(BorderMaker, Reflect_3Ch_3Elements) {
  test_sve_border<uint8_t>(4, 3, 3, kleidicv::FixedBorderType::REFLECT,
                           {7, 8, 9, 4,  5,  6,  1,  2,  3,  1, 2, 3, 4, 5, 6,
                            7, 8, 9, 10, 11, 12, 10, 11, 12, 7, 8, 9, 4, 5, 6});
}

TEST(BorderMaker, Reflect_3Ch_5Elements) {
  test_sve_border<uint8_t>(
      6, 5, 3, kleidicv::FixedBorderType::REFLECT,
      {13, 14, 15, 10, 11, 12, 7,  8,  9,  4,  5,  6,  1,  2,  3,  1,
       2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12, 13, 14, 15, 16, 17,
       18, 16, 17, 18, 13, 14, 15, 10, 11, 12, 7,  8,  9,  4,  5,  6});
}

TEST(BorderMaker, Reflect_4Ch_1Element) {
  test_sve_border<uint8_t>(
      3, 1, 4, kleidicv::FixedBorderType::REFLECT,
      {1, 2, 3, 4, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 9, 10, 11, 12});
}

TEST(BorderMaker, Reflect_4Ch_2Elements) {
  test_sve_border<uint8_t>(3, 2, 4, kleidicv::FixedBorderType::REFLECT,
                           {5, 6, 7, 8,  1,  2,  3, 4,  1,  2,  3, 4, 5, 6,
                            7, 8, 9, 10, 11, 12, 9, 10, 11, 12, 5, 6, 7, 8});
}

TEST(BorderMaker, Reflect_4Ch_3Elements) {
  test_sve_border<uint8_t>(
      4, 3, 4, kleidicv::FixedBorderType::REFLECT,
      {9,  10, 11, 12, 5, 6,  7,  8,  1,  2,  3,  4,  1,  2,
       3,  4,  5,  6,  7, 8,  9,  10, 11, 12, 13, 14, 15, 16,
       13, 14, 15, 16, 9, 10, 11, 12, 5,  6,  7,  8});
}

TEST(BorderMaker, Reflect_4Ch_5Elements) {
  test_sve_border<uint8_t>(
      6, 5, 4, kleidicv::FixedBorderType::REFLECT,
      {17, 18, 19, 20, 13, 14, 15, 16, 9,  10, 11, 12, 5,  6,  7,  8,
       1,  2,  3,  4,  1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12,
       13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 21, 22, 23, 24,
       17, 18, 19, 20, 13, 14, 15, 16, 9,  10, 11, 12, 5,  6,  7,  8});
}

////////////////////// WRAP //////////////////////

/*      if (column_index == 0) {

      case FixedBorderType::REFLECT:
          return get(6, 5, 4, 3, 2, 1, 0, 0, 1, 2, 3, 4, 5, 6, 7);

      case FixedBorderType::WRAP:
          return get(height_ - 7, height_ - 6, height_ - 5, height_ - 4,
                     height_ - 3, height_ - 2, height_ - 1, 0, 1, 2, 3, 4, 5, 6,
                     7);

      case FixedBorderType::REVERSE:
          return get(7, 6, 5, 4, 3, 2, 1, 0, 1, 2, 3, 4, 5, 6, 7);
*/

TEST(BorderMaker, Wrap_1Ch_1Element) {
  test_sve_border<uint8_t>(6, 1, 1, kleidicv::FixedBorderType::WRAP,
                           {6, 1, 2, 3, 4, 5, 6, 1});
}

TEST(BorderMaker, Wrap_1Ch_2Elements) {
  test_sve_border<uint8_t>(6, 2, 1, kleidicv::FixedBorderType::WRAP,
                           {5, 6, 1, 2, 3, 4, 5, 6, 1, 2});
}

TEST(BorderMaker, Wrap_1Ch_3Elements) {
  test_sve_border<uint8_t>(6, 3, 1, kleidicv::FixedBorderType::WRAP,
                           {4, 5, 6, 1, 2, 3, 4, 5, 6, 1, 2, 3});
}

TEST(BorderMaker, Wrap_1Ch_9Elements) {
  test_sve_border<uint8_t>(10, 9, 1, kleidicv::FixedBorderType::WRAP,
                           {2, 3, 4, 5, 6,  7, 8, 9, 10, 1, 2, 3, 4, 5,
                            6, 7, 8, 9, 10, 1, 2, 3, 4,  5, 6, 7, 8, 9});
}

TEST(BorderMaker, Wrap_2Ch_1Element) {
  test_sve_border<uint8_t>(3, 1, 2, kleidicv::FixedBorderType::WRAP,
                           {5, 6, 1, 2, 3, 4, 5, 6, 1, 2});
}

TEST(BorderMaker, Wrap_2Ch_2Elements) {
  test_sve_border<uint8_t>(3, 2, 2, kleidicv::FixedBorderType::WRAP,
                           {3, 4, 5, 6, 1, 2, 3, 4, 5, 6, 1, 2, 3, 4});
}

TEST(BorderMaker, Wrap_2Ch_3Elements) {
  test_sve_border<uint8_t>(
      4, 3, 2, kleidicv::FixedBorderType::WRAP,
      {3, 4, 5, 6, 7, 8, 1, 2, 3, 4, 5, 6, 7, 8, 1, 2, 3, 4, 5, 6});
}

TEST(BorderMaker, Wrap_2Ch_5Elements) {
  test_sve_border<uint8_t>(
      6, 5, 2, kleidicv::FixedBorderType::WRAP,
      {3, 4, 5, 6,  7,  8,  9, 10, 11, 12, 1, 2, 3, 4, 5, 6,
       7, 8, 9, 10, 11, 12, 1, 2,  3,  4,  5, 6, 7, 8, 9, 10});
}

TEST(BorderMaker, Wrap_3Ch_1Element) {
  test_sve_border<uint8_t>(3, 1, 3, kleidicv::FixedBorderType::WRAP,
                           {7, 8, 9, 1, 2, 3, 4, 5, 6, 7, 8, 9, 1, 2, 3});
}

TEST(BorderMaker, Wrap_3Ch_2Elements) {
  test_sve_border<uint8_t>(
      3, 2, 3, kleidicv::FixedBorderType::WRAP,
      {4, 5, 6, 7, 8, 9, 1, 2, 3, 4, 5, 6, 7, 8, 9, 1, 2, 3, 4, 5, 6});
}

TEST(BorderMaker, Wrap_3Ch_3Elements) {
  test_sve_border<uint8_t>(4, 3, 3, kleidicv::FixedBorderType::WRAP,
                           {4, 5, 6, 7,  8,  9,  10, 11, 12, 1, 2, 3, 4, 5, 6,
                            7, 8, 9, 10, 11, 12, 1,  2,  3,  4, 5, 6, 7, 8, 9});
}

TEST(BorderMaker, Wrap_3Ch_5Elements) {
  test_sve_border<uint8_t>(
      6, 5, 3, kleidicv::FixedBorderType::WRAP,
      {4,  5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 1,
       2,  3, 4, 5, 6, 7, 8,  9,  10, 11, 12, 13, 14, 15, 16, 17,
       18, 1, 2, 3, 4, 5, 6,  7,  8,  9,  10, 11, 12, 13, 14, 15});
}

TEST(BorderMaker, Wrap_4Ch_1Element) {
  test_sve_border<uint8_t>(
      3, 1, 4, kleidicv::FixedBorderType::WRAP,
      {9, 10, 11, 12, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 1, 2, 3, 4});
}

TEST(BorderMaker, Wrap_4Ch_2Elements) {
  test_sve_border<uint8_t>(3, 2, 4, kleidicv::FixedBorderType::WRAP,
                           {5, 6, 7, 8,  9,  10, 11, 12, 1, 2, 3, 4, 5, 6,
                            7, 8, 9, 10, 11, 12, 1,  2,  3, 4, 5, 6, 7, 8});
}

TEST(BorderMaker, Wrap_4Ch_3Elements) {
  test_sve_border<uint8_t>(4, 3, 4, kleidicv::FixedBorderType::WRAP,
                           {5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 1,  2,
                            3, 4, 5, 6, 7, 8,  9,  10, 11, 12, 13, 14, 15, 16,
                            1, 2, 3, 4, 5, 6,  7,  8,  9,  10, 11, 12});
}

TEST(BorderMaker, Wrap_4Ch_5Elements) {
  test_sve_border<uint8_t>(
      6, 5, 4, kleidicv::FixedBorderType::WRAP,
      {5,  6,  7,  8,  9,  10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20,
       21, 22, 23, 24, 1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12,
       13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 1,  2,  3,  4,
       5,  6,  7,  8,  9,  10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20});
}
