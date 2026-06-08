// SPDX-FileCopyrightText: 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <random>
#include <tuple>
#include <vector>

#include "framework/array.h"
#include "kleidicv/ctypes.h"
#include "kleidicv/kleidicv.h"
#include "kleidicv_thread/kleidicv_thread.h"
#include "multithreading_fake.h"
#include "test_config.h"

namespace {

using AddPaddingByCopyThreadParams = std::tuple<unsigned, unsigned, unsigned>;

constexpr size_t kMaximumPixelSize =
    static_cast<size_t>(KLEIDICV_MAXIMUM_TYPE_SIZE) *
    KLEIDICV_MAXIMUM_CHANNEL_COUNT;

std::array<uint8_t, 16> add_padding_by_copy_border_value() {
  return {11,  29,  47, 83, 101, 149, 173, 211,
          227, 241, 13, 31, 59,  71,  97,  131};
}
void expect_eq_vector2D(const uint8_t* lhs, const uint8_t* rhs, size_t width,
                        size_t height, size_t stride, size_t pixel_size) {
  for (size_t row = 0; row < height; ++row) {
    EXPECT_EQ(
        std::memcmp(lhs + row * stride, rhs + row * stride, width * pixel_size),
        0);
  }
}

void check_add_padding_by_copy(size_t src_width, size_t src_height,
                               size_t top_padding, size_t bottom_padding,
                               size_t left_padding, size_t right_padding,
                               size_t pixel_size,
                               kleidicv_border_type_t border_type,
                               uintptr_t thread_count) {
  const size_t src_row_width = std::max<size_t>(1, src_width * pixel_size);
  const size_t src_rows = std::max<size_t>(1, src_height);
  const size_t dst_row_width = std::max<size_t>(
      1, (src_width + left_padding + right_padding) * pixel_size);
  const size_t dst_rows =
      std::max<size_t>(1, src_height + top_padding + bottom_padding);
  const size_t src_stride = src_row_width;
  const size_t dst_stride = dst_row_width;

  std::vector<uint8_t> src(src_stride * src_rows, 0);
  std::vector<uint8_t> dst_single(dst_stride * dst_rows, 0xA5);
  std::vector<uint8_t> dst_multi(dst_stride * dst_rows, 0xA5);

  std::mt19937 generator{
      static_cast<std::mt19937::result_type>(test::Options::seed())};
  std::generate(src.begin(), src.end(), generator);

  const auto border_value = add_padding_by_copy_border_value();
  const void* border_ptr = border_type == KLEIDICV_BORDER_TYPE_CONSTANT
                               ? border_value.data()
                               : nullptr;

  kleidicv_error_t single_result = kleidicv_add_padding_by_copy(
      src.data(), src_stride, dst_single.data(), dst_stride, src_width,
      src_height, top_padding, bottom_padding, left_padding, right_padding,
      pixel_size, border_type, border_ptr);

  kleidicv_error_t multi_result = kleidicv_thread_add_padding_by_copy(
      src.data(), src_stride, dst_multi.data(), dst_stride, src_width,
      src_height, top_padding, bottom_padding, left_padding, right_padding,
      pixel_size, border_type, border_ptr,
      get_multithreading_fake(thread_count));

  EXPECT_EQ(single_result, multi_result);
  if (single_result == KLEIDICV_OK) {
    expect_eq_vector2D(dst_multi.data(), dst_single.data(), dst_row_width,
                       dst_rows, dst_stride, 1);
  }
}

class AddPaddingByCopyThread
    : public testing::TestWithParam<AddPaddingByCopyThreadParams> {};

TEST_P(AddPaddingByCopyThread, Constant) {
  const auto [width, height, thread_count] = GetParam();
  check_add_padding_by_copy(width + 5, height + 4, 3, 4, 5, 6, 1,
                            KLEIDICV_BORDER_TYPE_CONSTANT, thread_count);
}

TEST_P(AddPaddingByCopyThread, Replicate) {
  const auto [width, height, thread_count] = GetParam();
  check_add_padding_by_copy(width + 6, height + 3, 4, 3, 2, 7, 2,
                            KLEIDICV_BORDER_TYPE_REPLICATE, thread_count);
}

TEST_P(AddPaddingByCopyThread, Reflect) {
  const auto [width, height, thread_count] = GetParam();
  check_add_padding_by_copy(width + 7, height + 5, 3, 4, 5, 6, 1,
                            KLEIDICV_BORDER_TYPE_REFLECT, thread_count);
}

TEST_P(AddPaddingByCopyThread, WrapFast) {
  const auto [width, height, thread_count] = GetParam();
  check_add_padding_by_copy(width + 8, height + 3, 8, 7, 3, 4, 2,
                            KLEIDICV_BORDER_TYPE_WRAP, thread_count);
}

TEST_P(AddPaddingByCopyThread, Reverse3Channel) {
  const auto [width, height, thread_count] = GetParam();
  check_add_padding_by_copy(width + 9, height + 5, 3, 4, 4, 5, 3,
                            KLEIDICV_BORDER_TYPE_REVERSE, thread_count);
}

TEST_P(AddPaddingByCopyThread, IndexedWrapFallback) {
  const auto [width, height, thread_count] = GetParam();
  const size_t src_width = width + 4;
  check_add_padding_by_copy(src_width, height + 6, 3, 4, src_width + 2,
                            src_width + 1, 6, KLEIDICV_BORDER_TYPE_WRAP,
                            thread_count);
}

TEST_P(AddPaddingByCopyThread, IndexedReverseFallback) {
  const auto [width, height, thread_count] = GetParam();
  const size_t src_width = width + 3;
  check_add_padding_by_copy(src_width, height + 4, 3, 4, src_width,
                            src_width + 1, 3, KLEIDICV_BORDER_TYPE_REVERSE,
                            thread_count);
}

TEST_P(AddPaddingByCopyThread, LargeVerticalPadding) {
  const auto [width, height, thread_count] = GetParam();
  check_add_padding_by_copy(width + 3, height + 2, 9, 10, 6, 5, 2,
                            KLEIDICV_BORDER_TYPE_REFLECT, thread_count);
}

TEST(AddPaddingByCopyThreadValidation, ZeroSizedConstantImages) {
  check_add_padding_by_copy(0, 3, 2, 1, 4, 3, 3, KLEIDICV_BORDER_TYPE_CONSTANT,
                            4);
  check_add_padding_by_copy(4, 0, 2, 3, 1, 2, 4, KLEIDICV_BORDER_TYPE_CONSTANT,
                            4);
}

TEST(AddPaddingByCopyThreadValidation, NotImplemented) {
  uint8_t src[1] = {};
  uint8_t dst[1] = {};
  const auto border_value = add_padding_by_copy_border_value();

  EXPECT_EQ(KLEIDICV_ERROR_NOT_IMPLEMENTED,
            kleidicv_thread_add_padding_by_copy(
                src, sizeof(src), dst, sizeof(dst), 1, 1, 0, 0, 0, 0, 0,
                KLEIDICV_BORDER_TYPE_CONSTANT, border_value.data(),
                get_multithreading_fake(2)));

  EXPECT_EQ(KLEIDICV_ERROR_NOT_IMPLEMENTED,
            kleidicv_thread_add_padding_by_copy(
                src, sizeof(src), dst, sizeof(dst), 1, 1, 0, 0, 0, 0,
                kMaximumPixelSize + 1, KLEIDICV_BORDER_TYPE_CONSTANT,
                border_value.data(), get_multithreading_fake(2)));

  EXPECT_EQ(KLEIDICV_ERROR_NOT_IMPLEMENTED,
            kleidicv_thread_add_padding_by_copy(
                src, sizeof(src), dst, sizeof(dst), 1, 1, 0, 0, 0, 0, 1,
                KLEIDICV_BORDER_TYPE_TRANSPARENT, border_value.data(),
                get_multithreading_fake(2)));

  EXPECT_EQ(
      KLEIDICV_ERROR_NOT_IMPLEMENTED,
      kleidicv_thread_add_padding_by_copy(
          src, sizeof(src), dst, sizeof(dst), 0, 1, 0, 0, 1, 1, 1,
          KLEIDICV_BORDER_TYPE_REPLICATE, nullptr, get_multithreading_fake(2)));

  EXPECT_EQ(
      KLEIDICV_ERROR_NOT_IMPLEMENTED,
      kleidicv_thread_add_padding_by_copy(
          src, sizeof(src), dst, sizeof(dst), 1, 0, 0, 0, 1, 1, 1,
          KLEIDICV_BORDER_TYPE_WRAP, nullptr, get_multithreading_fake(2)));
}

TEST(AddPaddingByCopyThreadValidation, NullPointers) {
  uint8_t src[8] = {};
  uint8_t dst[16] = {};
  const auto border_value = add_padding_by_copy_border_value();
  const auto mt = get_multithreading_fake(2);

  EXPECT_EQ(KLEIDICV_ERROR_NULL_POINTER,
            kleidicv_thread_add_padding_by_copy(
                nullptr, sizeof(src), dst, sizeof(dst), 4, 4, 1, 2, 1, 2, 2,
                KLEIDICV_BORDER_TYPE_CONSTANT, border_value.data(), mt));

  EXPECT_EQ(KLEIDICV_ERROR_NULL_POINTER,
            kleidicv_thread_add_padding_by_copy(
                src, sizeof(src), nullptr, sizeof(dst), 4, 4, 1, 2, 1, 2, 2,
                KLEIDICV_BORDER_TYPE_CONSTANT, border_value.data(), mt));

  EXPECT_EQ(KLEIDICV_ERROR_NULL_POINTER,
            kleidicv_thread_add_padding_by_copy(
                src, sizeof(src), dst, sizeof(dst), 4, 4, 1, 2, 1, 2, 2,
                KLEIDICV_BORDER_TYPE_CONSTANT, nullptr, mt));

  EXPECT_EQ(KLEIDICV_OK,
            kleidicv_thread_add_padding_by_copy(
                src, sizeof(src), dst, sizeof(dst), 4, 1, 0, 0, 1, 2, 2,
                KLEIDICV_BORDER_TYPE_REPLICATE, nullptr, mt));
}

TEST(AddPaddingByCopyThreadValidation, Range) {
  uint8_t src[1] = {};
  uint8_t dst[1] = {};
  const auto border_value = add_padding_by_copy_border_value();
  const auto mt = get_multithreading_fake(2);

  EXPECT_EQ(KLEIDICV_ERROR_RANGE,
            kleidicv_thread_add_padding_by_copy(
                src, sizeof(src), dst, sizeof(dst),
                KLEIDICV_MAX_IMAGE_PIXELS + 1, 0, 0, 0, 0, 0, 1,
                KLEIDICV_BORDER_TYPE_CONSTANT, border_value.data(), mt));

  EXPECT_EQ(KLEIDICV_ERROR_RANGE,
            kleidicv_thread_add_padding_by_copy(
                src, sizeof(src), dst, sizeof(dst), 0,
                KLEIDICV_MAX_IMAGE_PIXELS + 1, 0, 0, 0, 0, 1,
                KLEIDICV_BORDER_TYPE_CONSTANT, border_value.data(), mt));

  EXPECT_EQ(KLEIDICV_ERROR_RANGE,
            kleidicv_thread_add_padding_by_copy(
                src, sizeof(src), dst, sizeof(dst), 1, 1, 0, 0,
                KLEIDICV_MAX_IMAGE_PIXELS, 1, 1, KLEIDICV_BORDER_TYPE_CONSTANT,
                border_value.data(), mt));

  EXPECT_EQ(
      KLEIDICV_ERROR_RANGE,
      kleidicv_thread_add_padding_by_copy(
          src, sizeof(src), dst, sizeof(dst), 1, 1, KLEIDICV_MAX_IMAGE_PIXELS,
          1, 0, 0, 1, KLEIDICV_BORDER_TYPE_CONSTANT, border_value.data(), mt));

  EXPECT_EQ(KLEIDICV_ERROR_RANGE,
            kleidicv_thread_add_padding_by_copy(
                src, sizeof(src), dst, sizeof(dst), 1, 1, 0,
                KLEIDICV_MAX_IMAGE_PIXELS + 1, 0, 0, 1,
                KLEIDICV_BORDER_TYPE_CONSTANT, border_value.data(), mt));

  EXPECT_EQ(KLEIDICV_ERROR_RANGE,
            kleidicv_thread_add_padding_by_copy(
                src, sizeof(src), dst, sizeof(dst), 1, 1, 0, 0, 0,
                KLEIDICV_MAX_IMAGE_PIXELS + 1, 1, KLEIDICV_BORDER_TYPE_CONSTANT,
                border_value.data(), mt));

  EXPECT_EQ(KLEIDICV_ERROR_RANGE,
            kleidicv_thread_add_padding_by_copy(
                src, sizeof(src), dst, sizeof(dst),
                KLEIDICV_MAX_IMAGE_PIXELS - 29, 1, 0, 0, 10, 20, 1,
                KLEIDICV_BORDER_TYPE_CONSTANT, border_value.data(), mt));

  EXPECT_EQ(KLEIDICV_ERROR_RANGE,
            kleidicv_thread_add_padding_by_copy(
                src, sizeof(src), dst, sizeof(dst), 1,
                KLEIDICV_MAX_IMAGE_PIXELS - 29, 10, 20, 0, 0, 1,
                KLEIDICV_BORDER_TYPE_CONSTANT, border_value.data(), mt));

  EXPECT_EQ(KLEIDICV_ERROR_RANGE,
            kleidicv_thread_add_padding_by_copy(
                src, sizeof(src), dst, sizeof(dst), 1, 1,
                KLEIDICV_MAX_IMAGE_PIXELS - 1, 0, KLEIDICV_MAX_IMAGE_PIXELS - 1,
                0, 1, KLEIDICV_BORDER_TYPE_CONSTANT, border_value.data(), mt));

  EXPECT_EQ(KLEIDICV_ERROR_RANGE,
            kleidicv_thread_add_padding_by_copy(
                src, sizeof(src), dst, sizeof(dst), 1, 1, 0,
                KLEIDICV_MAX_IMAGE_PIXELS - 1, 0, KLEIDICV_MAX_IMAGE_PIXELS - 1,
                1, KLEIDICV_BORDER_TYPE_CONSTANT, border_value.data(), mt));

  EXPECT_EQ(KLEIDICV_ERROR_RANGE,
            kleidicv_thread_add_padding_by_copy(
                src, sizeof(src), dst, sizeof(dst),
                KLEIDICV_MAX_IMAGE_PIXELS + 1, 1, 0, 0, 0, 0, 1,
                KLEIDICV_BORDER_TYPE_CONSTANT, border_value.data(), mt));

  EXPECT_EQ(
      KLEIDICV_ERROR_RANGE,
      kleidicv_thread_add_padding_by_copy(
          src, sizeof(src), dst, sizeof(dst), KLEIDICV_MAX_IMAGE_PIXELS, 1, 1,
          0, 0, 0, 1, KLEIDICV_BORDER_TYPE_CONSTANT, border_value.data(), mt));

  EXPECT_EQ(KLEIDICV_ERROR_RANGE,
            kleidicv_thread_add_padding_by_copy(
                src, sizeof(src), dst, sizeof(dst),
                KLEIDICV_MAX_IMAGE_PIXELS / 2 + 1, 1, 0, 1, 0, 0, 1,
                KLEIDICV_BORDER_TYPE_CONSTANT, border_value.data(), mt));

  EXPECT_EQ(
      KLEIDICV_ERROR_RANGE,
      kleidicv_thread_add_padding_by_copy(
          src, sizeof(src), dst, sizeof(dst), 1, KLEIDICV_MAX_IMAGE_PIXELS, 0,
          0, 1, 0, 1, KLEIDICV_BORDER_TYPE_CONSTANT, border_value.data(), mt));

  EXPECT_EQ(KLEIDICV_ERROR_RANGE,
            kleidicv_thread_add_padding_by_copy(
                src, sizeof(src), dst, sizeof(dst), KLEIDICV_MAX_IMAGE_PIXELS,
                KLEIDICV_MAX_IMAGE_PIXELS, 0, 0, 0, 0, 1,
                KLEIDICV_BORDER_TYPE_CONSTANT, border_value.data(), mt));

  EXPECT_EQ(KLEIDICV_ERROR_RANGE,
            kleidicv_thread_add_padding_by_copy(
                src, sizeof(src), dst, sizeof(dst),
                std::numeric_limits<size_t>::max(), 1, 0, 0, 1, 0, 1,
                KLEIDICV_BORDER_TYPE_CONSTANT, border_value.data(), mt));

  EXPECT_EQ(KLEIDICV_ERROR_RANGE,
            kleidicv_thread_add_padding_by_copy(
                src, sizeof(src), dst, sizeof(dst), 1, 1, 0, 0,
                std::numeric_limits<size_t>::max(), 0, 1,
                KLEIDICV_BORDER_TYPE_CONSTANT, border_value.data(), mt));

  EXPECT_EQ(KLEIDICV_ERROR_RANGE,
            kleidicv_thread_add_padding_by_copy(
                src, sizeof(src), dst, sizeof(dst),
                std::numeric_limits<size_t>::max() - 1, 1, 0, 0, 1, 1, 1,
                KLEIDICV_BORDER_TYPE_CONSTANT, border_value.data(), mt));

  EXPECT_EQ(KLEIDICV_ERROR_RANGE,
            kleidicv_thread_add_padding_by_copy(
                src, sizeof(src), dst, sizeof(dst), 1, 1,
                std::numeric_limits<size_t>::max(), 0, 0, 0, 1,
                KLEIDICV_BORDER_TYPE_CONSTANT, border_value.data(), mt));

  EXPECT_EQ(KLEIDICV_ERROR_RANGE,
            kleidicv_thread_add_padding_by_copy(
                src, sizeof(src), dst, sizeof(dst), 1,
                std::numeric_limits<size_t>::max() - 1, 1, 1, 0, 0, 1,
                KLEIDICV_BORDER_TYPE_CONSTANT, border_value.data(), mt));
}

#ifdef KLEIDICV_ALLOCATION_TESTS
TEST(AddPaddingByCopyThreadValidation, ReturnsRangeWhenHeapAllocationFails) {
  constexpr size_t kIndexedLeftPadding = 4;
  constexpr size_t kIndexedRightPadding = 0;

  test::Array2D<uint8_t> src{3, 2};
  test::Array2D<uint8_t> dst{3 + kIndexedLeftPadding + kIndexedRightPadding, 2};
  const auto mt = get_multithreading_fake(2);

  MockMallocToFail::enable();
  const kleidicv_error_t result = kleidicv_thread_add_padding_by_copy(
      src.data(), src.stride(), dst.data(), dst.stride(), 3, 2, 0, 0,
      kIndexedLeftPadding, kIndexedRightPadding, 1, KLEIDICV_BORDER_TYPE_WRAP,
      nullptr, mt);
  MockMallocToFail::disable();
  EXPECT_EQ(KLEIDICV_ERROR_RANGE, result);
}
#endif

INSTANTIATE_TEST_SUITE_P(
    , AddPaddingByCopyThread,
    testing::Values(AddPaddingByCopyThreadParams{1, 1, 1},
                    AddPaddingByCopyThreadParams{1, 2, 1},
                    AddPaddingByCopyThreadParams{1, 2, 2},
                    AddPaddingByCopyThreadParams{2, 1, 2},
                    AddPaddingByCopyThreadParams{2, 2, 1},
                    AddPaddingByCopyThreadParams{1, 3, 2},
                    AddPaddingByCopyThreadParams{2, 3, 1},
                    AddPaddingByCopyThreadParams{6, 4, 1},
                    AddPaddingByCopyThreadParams{4, 5, 2},
                    AddPaddingByCopyThreadParams{2, 6, 3},
                    AddPaddingByCopyThreadParams{1, 7, 4},
                    AddPaddingByCopyThreadParams{12, 34, 5},
                    AddPaddingByCopyThreadParams{1, 16, 1},
                    AddPaddingByCopyThreadParams{1, 32, 1},
                    AddPaddingByCopyThreadParams{1, 32, 2},
                    AddPaddingByCopyThreadParams{2, 16, 2},
                    AddPaddingByCopyThreadParams{2, 32, 1},
                    AddPaddingByCopyThreadParams{1, 48, 2},
                    AddPaddingByCopyThreadParams{2, 48, 1},
                    AddPaddingByCopyThreadParams{6, 64, 1},
                    AddPaddingByCopyThreadParams{4, 80, 2},
                    AddPaddingByCopyThreadParams{2, 96, 3},
                    AddPaddingByCopyThreadParams{1, 112, 4},
                    AddPaddingByCopyThreadParams{12, 34, 5},
                    AddPaddingByCopyThreadParams{40, 34, 5}));

}  // namespace
