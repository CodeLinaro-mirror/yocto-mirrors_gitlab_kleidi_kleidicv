// SPDX-FileCopyrightText: 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <vector>

#include "framework/array.h"
#include "framework/utils.h"
#include "kleidicv/ctypes.h"
#include "kleidicv/kleidicv.h"
#include "kleidicv/transform/add_padding_by_copy.h"
#include "test_config.h"

namespace {

constexpr size_t kMaximumPixelSize =
    static_cast<size_t>(KLEIDICV_MAXIMUM_TYPE_SIZE) *
    KLEIDICV_MAXIMUM_CHANNEL_COUNT;

struct AddPaddingByCopyParams {
  size_t width;
  size_t height;
  size_t top;
  size_t bottom;
  size_t left;
  size_t right;
  size_t pixel_size;
  size_t src_padding;
  size_t dst_padding;
  kleidicv_border_type_t border_type;
};

size_t add_padding_by_copy_required_alignment(size_t pixel_size) {
  switch (pixel_size) {
    case sizeof(uint16_t):
    case 3 * sizeof(uint16_t):
      return alignof(uint16_t);
    case sizeof(uint32_t):
    case 3 * sizeof(uint32_t):
      return alignof(uint32_t);
    case sizeof(uint64_t):
      return alignof(uint64_t);
    default:
      return alignof(uint8_t);
  }
}

size_t add_padding_by_copy_aligned_padding(size_t row_width, size_t padding,
                                           size_t alignment) {
  const size_t remainder = (row_width + padding) % alignment;
  return remainder == 0 ? padding : padding + alignment - remainder;
}

std::vector<uint64_t> add_padding_by_copy_aligned_storage(size_t byte_size) {
  const size_t element_count =
      (byte_size + sizeof(uint64_t) - 1) / sizeof(uint64_t);
  return std::vector<uint64_t>(std::max<size_t>(1, element_count));
}

constexpr std::array<kleidicv_border_type_t, 5> kBorderTypes = {
    KLEIDICV_BORDER_TYPE_CONSTANT, KLEIDICV_BORDER_TYPE_REPLICATE,
    KLEIDICV_BORDER_TYPE_REFLECT,  KLEIDICV_BORDER_TYPE_WRAP,
    KLEIDICV_BORDER_TYPE_REVERSE,
};

constexpr std::array<size_t, 5> kPixelSizes = {1, 2, 3, 4, 8};

int border_interpolate(int p, int len, kleidicv_border_type_t border_type) {
  if (0 <= p && p < len) {
    return p;
  }

  if (border_type == KLEIDICV_BORDER_TYPE_REPLICATE) {
    return p < 0 ? 0 : len - 1;
  }

  if (border_type == KLEIDICV_BORDER_TYPE_REFLECT ||
      border_type == KLEIDICV_BORDER_TYPE_REVERSE) {
    const int delta = border_type == KLEIDICV_BORDER_TYPE_REVERSE ? 1 : 0;
    if (len == 1) {
      return 0;
    }
    do {
      if (p < 0) {
        p = -p - 1 + delta;
      } else {
        p = len - 1 - (p - len) - delta;
      }
    } while (p < 0 || p >= len);
    return p;
  }

  if (border_type == KLEIDICV_BORDER_TYPE_WRAP) {
    if (p < 0) {
      p -= ((p - len + 1) / len) * len;
    }
    if (p >= len) {
      p %= len;
    }
    return p;
  }

  if (border_type == KLEIDICV_BORDER_TYPE_CONSTANT) {
    return -1;
  }

  return -2;
}

class AddPaddingByCopyTest : public testing::Test {
 protected:
  static uint8_t border_value_byte(size_t byte) {
    return static_cast<uint8_t>((byte * 37 + 11) & 0xff);
  }

  static std::array<uint8_t, kMaximumPixelSize> border_value() {
    std::array<uint8_t, kMaximumPixelSize> border{};
    for (size_t byte = 0; byte < border.size(); ++byte) {
      border[byte] = border_value_byte(byte);
    }
    return border;
  }

  static std::vector<uint8_t> border_value(size_t pixel_size) {
    std::vector<uint8_t> border(pixel_size);
    for (size_t byte = 0; byte < border.size(); ++byte) {
      border[byte] = border_value_byte(byte);
    }
    return border;
  }

  static void fill_source(test::Array2D<uint8_t>& src, size_t pixel_size) {
    src.fill([pixel_size](size_t row, size_t column) -> std::optional<uint8_t> {
      const size_t pixel = column / pixel_size;
      const size_t channel = column % pixel_size;
      return static_cast<uint8_t>((row * 37 + pixel * 19 + channel * 53 + 17) &
                                  0xff);
    });
  }

  static void calculate_expected(const test::Array2D<uint8_t>& src,
                                 test::Array2D<uint8_t>& expected,
                                 const AddPaddingByCopyParams& params,
                                 const uint8_t* border_value) {
    const size_t dst_width = params.width + params.left + params.right;
    const size_t dst_height = params.height + params.top + params.bottom;

    for (size_t dst_y = 0; dst_y < dst_height; ++dst_y) {
      const int src_y = border_interpolate(
          static_cast<int>(dst_y) - static_cast<int>(params.top),
          static_cast<int>(params.height), params.border_type);
      for (size_t dst_x = 0; dst_x < dst_width; ++dst_x) {
        const int src_x = border_interpolate(
            static_cast<int>(dst_x) - static_cast<int>(params.left),
            static_cast<int>(params.width), params.border_type);
        for (size_t byte = 0; byte < params.pixel_size; ++byte) {
          uint8_t value = border_value[byte];
          if (src_x >= 0 && src_y >= 0) {
            value =
                *src.at(static_cast<size_t>(src_y),
                        static_cast<size_t>(src_x) * params.pixel_size + byte);
          }
          *expected.at(dst_y, dst_x * params.pixel_size + byte) = value;
        }
      }
    }
  }

  static void run_reference_test(const AddPaddingByCopyParams& params) {
    SCOPED_TRACE(::testing::Message()
                 << "w=" << params.width << ", h=" << params.height
                 << ", top=" << params.top << ", bottom=" << params.bottom
                 << ", left=" << params.left << ", right=" << params.right
                 << ", pixel_size=" << params.pixel_size << ", src_padding="
                 << params.src_padding << ", dst_padding=" << params.dst_padding
                 << ", border_type=" << static_cast<int>(params.border_type));
    const size_t alignment =
        add_padding_by_copy_required_alignment(params.pixel_size);
    const size_t src_row_width = params.width * params.pixel_size;
    const size_t dst_width = params.width + params.left + params.right;
    const size_t dst_height = params.height + params.top + params.bottom;
    const size_t dst_row_width = dst_width * params.pixel_size;
    const size_t src_padding = add_padding_by_copy_aligned_padding(
        src_row_width, params.src_padding, alignment);
    const size_t dst_padding = add_padding_by_copy_aligned_padding(
        dst_row_width, params.dst_padding, alignment);

    test::Array2D<uint8_t> src{src_row_width, params.height, src_padding,
                               params.pixel_size};
    test::Array2D<uint8_t> actual{dst_row_width, dst_height, dst_padding,
                                  params.pixel_size};
    test::Array2D<uint8_t> expected{dst_row_width, dst_height, dst_padding,
                                    params.pixel_size};

    const auto border = border_value();
    fill_source(src, params.pixel_size);
    actual.fill(0xa5);
    calculate_expected(src, expected, params, border.data());

    std::vector<uint64_t> src_storage = add_padding_by_copy_aligned_storage(
        src.stride() * std::max<size_t>(params.height, 1));
    std::vector<uint64_t> dst_storage = add_padding_by_copy_aligned_storage(
        actual.stride() * std::max<size_t>(dst_height, 1));
    uint8_t* src_data = reinterpret_cast<uint8_t*>(src_storage.data());
    uint8_t* dst_data = reinterpret_cast<uint8_t*>(dst_storage.data());
    copy_active_rows_to_raw(src, src_data, src.stride());

    ASSERT_EQ(
        KLEIDICV_OK,
        kleidicv_add_padding_by_copy(
            src_data, src.stride(), dst_data, actual.stride(), params.width,
            params.height, params.top, params.bottom, params.left, params.right,
            params.pixel_size, params.border_type,
            params.border_type == KLEIDICV_BORDER_TYPE_CONSTANT ? border.data()
                                                                : nullptr));

    copy_active_rows_from_raw(dst_data, actual.stride(), actual);
    EXPECT_EQ_ARRAY2D(expected, actual);
  }

  static void run_operation_reference_test(
      const AddPaddingByCopyParams& params) {
    SCOPED_TRACE(::testing::Message()
                 << "internal operation, w=" << params.width << ", h="
                 << params.height << ", pixel_size=" << params.pixel_size
                 << ", border_type=" << static_cast<int>(params.border_type));
    const size_t alignment =
        add_padding_by_copy_required_alignment(params.pixel_size);
    const size_t src_row_width = params.width * params.pixel_size;
    const size_t dst_width = params.width + params.left + params.right;
    const size_t dst_height = params.height + params.top + params.bottom;
    const size_t dst_row_width = dst_width * params.pixel_size;
    const size_t src_padding = add_padding_by_copy_aligned_padding(
        src_row_width, params.src_padding, alignment);
    const size_t dst_padding = add_padding_by_copy_aligned_padding(
        dst_row_width, params.dst_padding, alignment);

    test::Array2D<uint8_t> src{src_row_width, params.height, src_padding,
                               params.pixel_size};
    test::Array2D<uint8_t> actual{dst_row_width, dst_height, dst_padding,
                                  params.pixel_size};
    test::Array2D<uint8_t> expected{dst_row_width, dst_height, dst_padding,
                                    params.pixel_size};

    const auto border = border_value(params.pixel_size);
    fill_source(src, params.pixel_size);
    actual.fill(0xa5);
    calculate_expected(src, expected, params, border.data());

    std::vector<uint64_t> src_storage = add_padding_by_copy_aligned_storage(
        src.stride() * std::max<size_t>(params.height, 1));
    std::vector<uint64_t> dst_storage = add_padding_by_copy_aligned_storage(
        actual.stride() * std::max<size_t>(dst_height, 1));
    uint8_t* src_data = reinterpret_cast<uint8_t*>(src_storage.data());
    uint8_t* dst_data = reinterpret_cast<uint8_t*>(dst_storage.data());
    copy_active_rows_to_raw(src, src_data, src.stride());

    const kleidicv::AddPaddingByCopyBorderStrategy strategy =
        kleidicv::resolve_border_strategy(params.width, params.left,
                                          params.right, params.border_type);
    const kleidicv::AddPaddingByCopyParameters operation_params{
        src_data,
        dst_data,
        src.stride(),
        actual.stride(),
        params.width,
        params.height,
        params.top,
        params.bottom,
        params.left,
        params.right,
        params.pixel_size,
        params.border_type,
        params.border_type == KLEIDICV_BORDER_TYPE_CONSTANT ? border.data()
                                                            : nullptr};

    kleidicv::AddPaddingByCopyOpPointer operation =
        kleidicv_create_add_padding_by_copy_operation(operation_params,
                                                      strategy);
    ASSERT_NE(nullptr, operation);
    ASSERT_EQ(KLEIDICV_OK, operation->process_stripe(0, dst_height));

    copy_active_rows_from_raw(dst_data, actual.stride(), actual);
    EXPECT_EQ_ARRAY2D(expected, actual);
  }

  static void copy_active_rows_to_raw(const test::Array2D<uint8_t>& src,
                                      uint8_t* dst, size_t dst_stride) {
    for (size_t row = 0; row < src.height(); ++row) {
      for (size_t column = 0; column < src.width(); ++column) {
        dst[row * dst_stride + column] = *src.at(row, column);
      }
    }
  }

  static void copy_active_rows_from_raw(const uint8_t* src, size_t src_stride,
                                        test::Array2D<uint8_t>& dst) {
    for (size_t row = 0; row < dst.height(); ++row) {
      for (size_t column = 0; column < dst.width(); ++column) {
        *dst.at(row, column) = src[row * src_stride + column];
      }
    }
  }
};

TEST_F(AddPaddingByCopyTest, MatchesReferenceAcrossGeneratedCases) {
  const std::array<size_t, 3> widths = {1, 2,
                                        test::Options::vector_length() + 1};
  const std::array<size_t, 3> heights = {1, 3, 5};

  for (kleidicv_border_type_t border_type : kBorderTypes) {
    for (size_t pixel_size : kPixelSizes) {
      for (size_t width : widths) {
        for (size_t height : heights) {
          run_reference_test({
              width,
              height,
              1 + (height % 2),
              1 + (width % 3),
              1 + ((width + pixel_size) % 4),
              1 + ((height + pixel_size) % 5),
              pixel_size,
              (width + pixel_size) % 7,
              (height + pixel_size) % 11,
              border_type,
          });
        }
      }
    }
  }
}

TEST_F(AddPaddingByCopyTest, MatchesReferenceAtVectorLengthBoundaries) {
  const size_t vector_length = test::Options::vector_length();
  const size_t below_vector_length = vector_length > 1 ? vector_length - 1 : 1;

  const std::vector<AddPaddingByCopyParams> cases = {
      {below_vector_length, 2, 1, 1, 1, 2, 1, 0, 0,
       KLEIDICV_BORDER_TYPE_CONSTANT},
      {vector_length, 3, 2, 1, 2, 1, 1, 3, 5, KLEIDICV_BORDER_TYPE_REPLICATE},
      {vector_length + 1, 4, 1, 2, 3, 4, 2, 1, 0, KLEIDICV_BORDER_TYPE_WRAP},
      {2 * vector_length, 2, 1, 1, 5, 4, 3, 0, 3, KLEIDICV_BORDER_TYPE_REFLECT},
      {2 * vector_length + 3, 5, 2, 1, 4, 5, 4, 5, 1,
       KLEIDICV_BORDER_TYPE_REVERSE},
  };

  for (const auto& params : cases) {
    run_reference_test(params);
  }
}

TEST_F(AddPaddingByCopyTest, MatchesReferenceForContextPredicateBranches) {
  const std::vector<AddPaddingByCopyParams> typed_context_cases = {
      {5, 3, 1, 1, 2, 2, 2, 1, 3, KLEIDICV_BORDER_TYPE_REPLICATE},
      {5, 3, 1, 1, 2, 2, 4, 2, 1, KLEIDICV_BORDER_TYPE_REFLECT},
      {4, 3, 1, 1, 2, 2, 6, 3, 5, KLEIDICV_BORDER_TYPE_REFLECT},
      {4, 3, 1, 1, 2, 2, 8, 1, 3, KLEIDICV_BORDER_TYPE_REVERSE},
      {3, 3, 1, 1, 2, 2, 12, 3, 1, KLEIDICV_BORDER_TYPE_REVERSE},
  };

  for (const auto& params : typed_context_cases) {
    run_reference_test(params);
  }

  run_operation_reference_test({
      3,
      2,
      1,
      1,
      2,
      1,
      test::Options::vector_length() + 1,
      3,
      5,
      KLEIDICV_BORDER_TYPE_REFLECT,
  });
}

TEST_F(AddPaddingByCopyTest, Misalignment) {
  constexpr size_t kWidth = 2;
  constexpr size_t kHeight = 2;
  constexpr size_t kTopPadding = 1;
  constexpr size_t kBottomPadding = 1;
  constexpr size_t kLeftPadding = 1;
  constexpr size_t kRightPadding = 1;

  for (size_t pixel_size : {2, 4, 6, 8, 12}) {
    const size_t alignment = add_padding_by_copy_required_alignment(pixel_size);
    const size_t src_stride = kWidth * pixel_size;
    const size_t dst_width = kWidth + kLeftPadding + kRightPadding;
    const size_t dst_height = kHeight + kTopPadding + kBottomPadding;
    const size_t dst_stride = dst_width * pixel_size;

    SCOPED_TRACE(::testing::Message() << "pixel_size=" << pixel_size);

    std::vector<uint64_t> src_storage =
        add_padding_by_copy_aligned_storage(src_stride * kHeight + alignment);
    std::vector<uint64_t> dst_storage = add_padding_by_copy_aligned_storage(
        dst_stride * dst_height + alignment);
    const uint8_t* src = reinterpret_cast<const uint8_t*>(src_storage.data());
    uint8_t* dst = reinterpret_cast<uint8_t*>(dst_storage.data());
    const auto border = border_value();

    EXPECT_EQ(KLEIDICV_ERROR_ALIGNMENT,
              kleidicv_add_padding_by_copy(
                  src + 1, src_stride, dst, dst_stride, kWidth, kHeight,
                  kTopPadding, kBottomPadding, kLeftPadding, kRightPadding,
                  pixel_size, KLEIDICV_BORDER_TYPE_CONSTANT, border.data()));
    EXPECT_EQ(KLEIDICV_ERROR_ALIGNMENT,
              kleidicv_add_padding_by_copy(
                  src, src_stride + 1, dst, dst_stride, kWidth, kHeight,
                  kTopPadding, kBottomPadding, kLeftPadding, kRightPadding,
                  pixel_size, KLEIDICV_BORDER_TYPE_CONSTANT, border.data()));
    EXPECT_EQ(KLEIDICV_ERROR_ALIGNMENT,
              kleidicv_add_padding_by_copy(
                  src, src_stride, dst + 1, dst_stride, kWidth, kHeight,
                  kTopPadding, kBottomPadding, kLeftPadding, kRightPadding,
                  pixel_size, KLEIDICV_BORDER_TYPE_CONSTANT, border.data()));
    EXPECT_EQ(KLEIDICV_ERROR_ALIGNMENT,
              kleidicv_add_padding_by_copy(
                  src, src_stride, dst, dst_stride + 1, kWidth, kHeight,
                  kTopPadding, kBottomPadding, kLeftPadding, kRightPadding,
                  pixel_size, KLEIDICV_BORDER_TYPE_CONSTANT, border.data()));

    // Ignore stride alignment if there is only one row.
    EXPECT_EQ(KLEIDICV_OK,
              kleidicv_add_padding_by_copy(
                  src, src_stride + 1, dst, dst_stride + 1, kWidth, 1, 0, 0, 0,
                  0, pixel_size, KLEIDICV_BORDER_TYPE_REPLICATE, nullptr));
  }
}

TEST_F(AddPaddingByCopyTest, MatchesReferenceForBorderPresenceCombinations) {
  struct BorderCombinationCase {
    size_t top;
    size_t bottom;
    size_t left;
    size_t right;
  };

  const std::array<BorderCombinationCase, 8> combinations = {{
      {0, 0, 3, 0},
      {0, 0, 0, 4},
      {2, 0, 0, 0},
      {0, 3, 0, 0},
      {2, 3, 4, 5},
      {2, 3, 0, 0},
      {0, 0, 4, 5},
      {0, 0, 0, 0},
  }};

  const std::array<size_t, 5> pixel_sizes = {1, 2, 3, 4, 8};
  const size_t width = 6;
  const size_t height = 5;

  for (kleidicv_border_type_t border_type : kBorderTypes) {
    for (size_t combo_index = 0; combo_index < combinations.size();
         ++combo_index) {
      const auto& combination = combinations[combo_index];
      run_reference_test({
          width,
          height,
          combination.top,
          combination.bottom,
          combination.left,
          combination.right,
          pixel_sizes[combo_index % pixel_sizes.size()],
          (combo_index * 3) % 7,
          (combo_index * 5) % 11,
          border_type,
      });
    }
  }
}

TEST_F(AddPaddingByCopyTest, MatchesReferenceForSpecializedAndFallbackPaths) {
  const std::vector<AddPaddingByCopyParams> cases = {
      {3, 2, 1, 2, 2, 1, 1, 5, 7, KLEIDICV_BORDER_TYPE_CONSTANT},
      {5, 4, 2, 1, 3, 2, 3, 5, 1, KLEIDICV_BORDER_TYPE_CONSTANT},
      {4, 3, 2, 1, 3, 2, 6, 4, 5, KLEIDICV_BORDER_TYPE_CONSTANT},
      {3, 2, 1, 2, 2, 3, 12, 6, 8, KLEIDICV_BORDER_TYPE_CONSTANT},
      {2, 3, 1, 2, 2, 1, 16, 5, 7, KLEIDICV_BORDER_TYPE_CONSTANT},
      {385, 2, 1, 1, 1, 1, 8, 13, 17, KLEIDICV_BORDER_TYPE_CONSTANT},
      {1, 4, 3, 2, 5, 4, 2, 6, 9, KLEIDICV_BORDER_TYPE_REPLICATE},
      {7, 5, 1, 2, 3, 4, 1, 4, 6, KLEIDICV_BORDER_TYPE_WRAP},
      {7, 4, 2, 1, 10, 9, 3, 5, 0, KLEIDICV_BORDER_TYPE_WRAP},
      {6, 5, 1, 1, 5, 6, 4, 3, 2, KLEIDICV_BORDER_TYPE_REFLECT},
      {16, 3, 1, 1, 16, 16, 4, 2, 3, KLEIDICV_BORDER_TYPE_REFLECT},
      {16, 2, 1, 1, 16, 16, 6, 1, 4, KLEIDICV_BORDER_TYPE_REFLECT},
      {8, 2, 1, 1, 8, 8, 12, 3, 2, KLEIDICV_BORDER_TYPE_REFLECT},
      {4, 2, 1, 1, 4, 4, 16, 2, 1, KLEIDICV_BORDER_TYPE_REFLECT},
      {4, 3, 2, 1, 9, 8, 2, 0, 5, KLEIDICV_BORDER_TYPE_REFLECT},
      {6, 5, 2, 1, 4, 5, 1, 8, 4, KLEIDICV_BORDER_TYPE_REVERSE},
      {4, 3, 1, 2, 4, 6, 5, 2, 4, KLEIDICV_BORDER_TYPE_REVERSE},
      {3, 4, 1, 2, 17, 19, 1, 3, 5, KLEIDICV_BORDER_TYPE_WRAP},
      {5, 3, 2, 1, 18, 16, 4, 7, 2, KLEIDICV_BORDER_TYPE_REFLECT},
      {5, 4, 1, 3, 17, 18, 3, 1, 6, KLEIDICV_BORDER_TYPE_REVERSE},
      {1, 2, 3, 1, 12, 11, 2, 4, 3, KLEIDICV_BORDER_TYPE_REFLECT},
      {1, 3, 2, 2, 10, 9, 1, 0, 5, KLEIDICV_BORDER_TYPE_REVERSE},
  };

  for (const auto& params : cases) {
    run_reference_test(params);
  }
}

TEST_F(AddPaddingByCopyTest, MatchesReferenceForWrapFastPathLayouts) {
  const std::vector<AddPaddingByCopyParams> cases = {
      {17, 5, 1, 2, 3, 0, 1, 2, 4, KLEIDICV_BORDER_TYPE_WRAP},
      {17, 5, 1, 2, 0, 3, 1, 4, 2, KLEIDICV_BORDER_TYPE_WRAP},
      {21, 4, 2, 1, 5, 4, 1, 1, 3, KLEIDICV_BORDER_TYPE_WRAP},
      {19, 3, 1, 1, 7, 6, 2, 5, 0, KLEIDICV_BORDER_TYPE_WRAP},
      {13, 4, 1, 2, 3, 4, 3, 2, 1, KLEIDICV_BORDER_TYPE_WRAP},
      {11, 3, 2, 1, 4, 5, 6, 3, 2, KLEIDICV_BORDER_TYPE_WRAP},
      {9, 3, 1, 2, 2, 3, 12, 4, 1, KLEIDICV_BORDER_TYPE_WRAP},
      {7, 4, 1, 1, 1, 2, 8, 0, 5, KLEIDICV_BORDER_TYPE_WRAP},
  };

  for (const auto& params : cases) {
    run_reference_test(params);
  }
}

TEST_F(AddPaddingByCopyTest, MatchesReferenceForFunctionCoverageLoopShapes) {
  const std::vector<AddPaddingByCopyParams> cases = {
      {32, 2, 1, 0, 0, 0, 1, 3, 5, KLEIDICV_BORDER_TYPE_CONSTANT},
      {16, 2, 1, 0, 0, 0, 6, 1, 2, KLEIDICV_BORDER_TYPE_CONSTANT},
      {5, 2, 1, 0, 0, 0, 12, 2, 3, KLEIDICV_BORDER_TYPE_CONSTANT},
      {96, 2, 1, 0, 96, 0, 1, 3, 5, KLEIDICV_BORDER_TYPE_REFLECT},
      {48, 2, 1, 0, 48, 0, 2, 3, 5, KLEIDICV_BORDER_TYPE_REFLECT},
      {24, 2, 1, 0, 24, 0, 4, 3, 5, KLEIDICV_BORDER_TYPE_REFLECT},
      {8, 2, 1, 0, 8, 0, 8, 3, 5, KLEIDICV_BORDER_TYPE_REFLECT},
      {32, 2, 1, 0, 32, 0, 3, 3, 5, KLEIDICV_BORDER_TYPE_REFLECT},
      {9, 2, 1, 0, 9, 0, 6, 3, 5, KLEIDICV_BORDER_TYPE_REFLECT},
      {5, 2, 1, 0, 5, 0, 12, 3, 5, KLEIDICV_BORDER_TYPE_REFLECT},
  };

  for (const auto& params : cases) {
    run_reference_test(params);
  }
}

TEST_F(AddPaddingByCopyTest, MatchesReferenceForLargeVerticalPaddingModes) {
  const std::vector<AddPaddingByCopyParams> cases = {
      {7, 3, 19, 17, 2, 3, 1, 4, 5, KLEIDICV_BORDER_TYPE_WRAP},
      {8, 2, 21, 18, 6, 5, 4, 2, 3, KLEIDICV_BORDER_TYPE_REFLECT},
      {9, 2, 22, 19, 6, 5, 2, 1, 4, KLEIDICV_BORDER_TYPE_REVERSE},
  };

  for (const auto& params : cases) {
    run_reference_test(params);
  }
}

TEST_F(AddPaddingByCopyTest, MatchesReferenceForSinglePixelDimensions) {
  const std::vector<AddPaddingByCopyParams> cases = {
      {1, 1, 11, 10, 9, 8, 2, 4, 3, KLEIDICV_BORDER_TYPE_WRAP},
      {1, 4, 9, 8, 13, 12, 4, 1, 2, KLEIDICV_BORDER_TYPE_REFLECT},
      {5, 1, 7, 9, 4, 5, 3, 2, 1, KLEIDICV_BORDER_TYPE_REVERSE},
      {1, 1, 12, 11, 10, 9, 1, 0, 5, KLEIDICV_BORDER_TYPE_REVERSE},
  };

  for (const auto& params : cases) {
    run_reference_test(params);
  }
}

TEST_F(AddPaddingByCopyTest, MatchesReferenceAtMaximumPixelSize) {
  run_reference_test({3, 2, 1, 2, 4, 3, kMaximumPixelSize, 5, 7,
                      KLEIDICV_BORDER_TYPE_REFLECT});
}

TEST_F(AddPaddingByCopyTest, AllowsZeroSizedConstantImages) {
  run_reference_test(
      {0, 3, 2, 1, 4, 3, 3, 0, 5, KLEIDICV_BORDER_TYPE_CONSTANT});
  run_reference_test(
      {4, 0, 2, 3, 1, 2, 4, 0, 3, KLEIDICV_BORDER_TYPE_CONSTANT});
}

TEST_F(AddPaddingByCopyTest, ReturnsErrorForInvalidArguments) {
  test::Array2D<uint8_t> src{8, 4, 3, 2};
  test::Array2D<uint8_t> dst{16, 7, 5, 2};
  const auto border = border_value();

  test::test_null_args(kleidicv_add_padding_by_copy, src.data(), src.stride(),
                       dst.data(), dst.stride(), 4U, 4U, 1U, 2U, 1U, 2U, 1U,
                       KLEIDICV_BORDER_TYPE_CONSTANT, border.data());

  EXPECT_EQ(KLEIDICV_OK,
            kleidicv_add_padding_by_copy(
                src.data(), src.stride(), dst.data(), dst.stride(), 4, 4, 1, 2,
                1, 2, 1, KLEIDICV_BORDER_TYPE_REPLICATE, nullptr));

  EXPECT_EQ(KLEIDICV_ERROR_NOT_IMPLEMENTED,
            kleidicv_add_padding_by_copy(
                src.data(), src.stride(), dst.data(), dst.stride(), 4, 4, 1, 2,
                1, 2, 0, KLEIDICV_BORDER_TYPE_CONSTANT, border.data()));

  EXPECT_EQ(
      KLEIDICV_ERROR_NOT_IMPLEMENTED,
      kleidicv_add_padding_by_copy(
          src.data(), src.stride(), dst.data(), dst.stride(), 4, 4, 1, 2, 1, 2,
          kMaximumPixelSize + 1, KLEIDICV_BORDER_TYPE_CONSTANT, border.data()));

  EXPECT_EQ(KLEIDICV_ERROR_NOT_IMPLEMENTED,
            kleidicv_add_padding_by_copy(
                src.data(), src.stride(), dst.data(), dst.stride(), 4, 4, 1, 2,
                1, 2, 1, KLEIDICV_BORDER_TYPE_TRANSPARENT, border.data()));

  EXPECT_EQ(KLEIDICV_ERROR_NOT_IMPLEMENTED,
            kleidicv_add_padding_by_copy(
                src.data(), src.stride(), dst.data(), dst.stride(), 0, 4, 1, 2,
                1, 2, 1, KLEIDICV_BORDER_TYPE_REPLICATE, nullptr));

  EXPECT_EQ(KLEIDICV_ERROR_NOT_IMPLEMENTED,
            kleidicv_add_padding_by_copy(src.data(), src.stride(), dst.data(),
                                         dst.stride(), 4, 0, 1, 2, 1, 2, 1,
                                         KLEIDICV_BORDER_TYPE_WRAP, nullptr));

  EXPECT_EQ(KLEIDICV_ERROR_RANGE,
            kleidicv_add_padding_by_copy(
                src.data(), src.stride(), dst.data(), dst.stride(), 1, 1, 0, 0,
                KLEIDICV_MAX_IMAGE_PIXELS, 1, 1, KLEIDICV_BORDER_TYPE_CONSTANT,
                border.data()));

  EXPECT_EQ(KLEIDICV_ERROR_RANGE,
            kleidicv_add_padding_by_copy(
                src.data(), src.stride(), dst.data(), dst.stride(), 1, 1,
                KLEIDICV_MAX_IMAGE_PIXELS, 1, 0, 0, 1,
                KLEIDICV_BORDER_TYPE_CONSTANT, border.data()));

  EXPECT_EQ(KLEIDICV_ERROR_RANGE,
            kleidicv_add_padding_by_copy(
                src.data(), src.stride(), dst.data(), dst.stride(),
                KLEIDICV_MAX_IMAGE_PIXELS - 29, 1, 0, 0, 10, 20, 1,
                KLEIDICV_BORDER_TYPE_CONSTANT, border.data()));

  EXPECT_EQ(KLEIDICV_ERROR_RANGE,
            kleidicv_add_padding_by_copy(
                src.data(), src.stride(), dst.data(), dst.stride(), 1,
                KLEIDICV_MAX_IMAGE_PIXELS - 29, 10, 20, 0, 0, 1,
                KLEIDICV_BORDER_TYPE_CONSTANT, border.data()));

  EXPECT_EQ(KLEIDICV_ERROR_RANGE,
            kleidicv_add_padding_by_copy(
                src.data(), src.stride(), dst.data(), dst.stride(), 1, 1,
                KLEIDICV_MAX_IMAGE_PIXELS - 1, 0, KLEIDICV_MAX_IMAGE_PIXELS - 1,
                0, 1, KLEIDICV_BORDER_TYPE_CONSTANT, border.data()));

  EXPECT_EQ(KLEIDICV_ERROR_RANGE,
            kleidicv_add_padding_by_copy(
                src.data(), src.stride(), dst.data(), dst.stride(), 1, 1, 0,
                KLEIDICV_MAX_IMAGE_PIXELS - 1, 0, KLEIDICV_MAX_IMAGE_PIXELS - 1,
                1, KLEIDICV_BORDER_TYPE_CONSTANT, border.data()));

  EXPECT_EQ(KLEIDICV_ERROR_RANGE,
            kleidicv_add_padding_by_copy(
                src.data(), src.stride(), dst.data(), dst.stride(),
                KLEIDICV_MAX_IMAGE_PIXELS + 1, 1, 0, 0, 0, 0, 1,
                KLEIDICV_BORDER_TYPE_CONSTANT, border.data()));

  EXPECT_EQ(KLEIDICV_ERROR_RANGE,
            kleidicv_add_padding_by_copy(
                src.data(), src.stride(), dst.data(), dst.stride(),
                KLEIDICV_MAX_IMAGE_PIXELS, 1, 1, 0, 0, 0, 1,
                KLEIDICV_BORDER_TYPE_CONSTANT, border.data()));

  EXPECT_EQ(KLEIDICV_ERROR_RANGE,
            kleidicv_add_padding_by_copy(
                src.data(), src.stride(), dst.data(), dst.stride(),
                KLEIDICV_MAX_IMAGE_PIXELS / 2 + 1, 1, 0, 1, 0, 0, 1,
                KLEIDICV_BORDER_TYPE_CONSTANT, border.data()));

  EXPECT_EQ(KLEIDICV_ERROR_RANGE,
            kleidicv_add_padding_by_copy(
                src.data(), src.stride(), dst.data(), dst.stride(), 1,
                KLEIDICV_MAX_IMAGE_PIXELS, 0, 0, 1, 0, 1,
                KLEIDICV_BORDER_TYPE_CONSTANT, border.data()));

  EXPECT_EQ(KLEIDICV_ERROR_RANGE,
            kleidicv_add_padding_by_copy(
                src.data(), src.stride(), dst.data(), dst.stride(),
                KLEIDICV_MAX_IMAGE_PIXELS, KLEIDICV_MAX_IMAGE_PIXELS, 0, 0, 0,
                0, 1, KLEIDICV_BORDER_TYPE_CONSTANT, border.data()));

  EXPECT_EQ(KLEIDICV_ERROR_RANGE,
            kleidicv_add_padding_by_copy(
                src.data(), src.stride(), dst.data(), dst.stride(),
                std::numeric_limits<size_t>::max(), 1, 0, 0, 1, 0, 1,
                KLEIDICV_BORDER_TYPE_CONSTANT, border.data()));

  EXPECT_EQ(KLEIDICV_ERROR_RANGE,
            kleidicv_add_padding_by_copy(
                src.data(), src.stride(), dst.data(), dst.stride(), 1, 1, 0, 0,
                std::numeric_limits<size_t>::max(), 0, 1,
                KLEIDICV_BORDER_TYPE_CONSTANT, border.data()));

  EXPECT_EQ(KLEIDICV_ERROR_RANGE,
            kleidicv_add_padding_by_copy(
                src.data(), src.stride(), dst.data(), dst.stride(),
                std::numeric_limits<size_t>::max() - 1, 1, 0, 0, 1, 1, 1,
                KLEIDICV_BORDER_TYPE_CONSTANT, border.data()));

  EXPECT_EQ(KLEIDICV_ERROR_RANGE,
            kleidicv_add_padding_by_copy(
                src.data(), src.stride(), dst.data(), dst.stride(), 1, 1,
                std::numeric_limits<size_t>::max(), 0, 0, 0, 1,
                KLEIDICV_BORDER_TYPE_CONSTANT, border.data()));

  EXPECT_EQ(KLEIDICV_ERROR_RANGE,
            kleidicv_add_padding_by_copy(
                src.data(), src.stride(), dst.data(), dst.stride(), 1,
                std::numeric_limits<size_t>::max() - 1, 1, 1, 0, 0, 1,
                KLEIDICV_BORDER_TYPE_CONSTANT, border.data()));
}

#ifdef KLEIDICV_ALLOCATION_TESTS
TEST_F(AddPaddingByCopyTest, ReturnsAllocationWhenHeapAllocationFails) {
  constexpr size_t kIndexedLeftPadding = 4;
  constexpr size_t kIndexedRightPadding = 0;

  {
    test::Array2D<uint8_t> src{3, 2};
    test::Array2D<uint8_t> dst{3 + kIndexedLeftPadding + kIndexedRightPadding,
                               2};
    MockMallocToFail::enable();
    const kleidicv_error_t result = kleidicv_add_padding_by_copy(
        src.data(), src.stride(), dst.data(), dst.stride(), 3, 2, 0, 0,
        kIndexedLeftPadding, kIndexedRightPadding, 1, KLEIDICV_BORDER_TYPE_WRAP,
        nullptr);
    MockMallocToFail::disable();
    EXPECT_EQ(KLEIDICV_ERROR_ALLOCATION, result);
  }
}
#endif

}  // namespace
