// SPDX-FileCopyrightText: 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <cstdint>

#include "kleidicv/filters/blur_and_downsample.h"
#include "kleidicv/filters/gaussian_blur.h"
#include "kleidicv/filters/scharr.h"
#include "kleidicv/filters/separable_filter_2d.h"
#include "kleidicv/filters/sobel.h"
#include "kleidicv/kleidicv.h"
#include "kleidicv_thread/kleidicv_thread.h"

namespace {

template <typename T, size_t N>
void fill_pattern(std::array<T, N> &values) {
  for (size_t index = 0; index < N; ++index) {
    values[index] = static_cast<T>((index * index + 17 * index + 3) % 251);
  }
}

template <typename T, size_t N>
void copy_rows(const std::array<T, N> &full, std::array<T, N> &partial,
               size_t row_elements, size_t y_begin, size_t y_end) {
  for (size_t row = y_begin; row < y_end; ++row) {
    for (size_t column = 0; column < row_elements; ++column) {
      size_t index = row * row_elements + column;
      partial[index] = full[index];
    }
  }
}

class ThreadFilterValidation : public testing::Test {
 protected:
  kleidicv_thread_multithreading scheduler() {
    parallel_call_count_ = 0;
    return {unexpected_parallel, this};
  }

  void expect_rejected(kleidicv_error_t expected,
                       kleidicv_error_t actual) const {
    EXPECT_EQ(expected, actual);
    EXPECT_EQ(size_t{0}, parallel_call_count_);
  }

  static constexpr size_t kLargeWidth =
      static_cast<size_t>(KLEIDICV_MAX_IMAGE_PIXELS);

 private:
  static kleidicv_error_t unexpected_parallel(
      kleidicv_thread_callback /*callback*/, void * /*callback_data*/,
      void *parallel_data, size_t /*task_count*/) {
    auto *test = static_cast<ThreadFilterValidation *>(parallel_data);
    ++test->parallel_call_count_;
    return KLEIDICV_ERROR_CONTEXT_MISMATCH;
  }

  size_t parallel_call_count_ = 0;
};

TEST_F(ThreadFilterValidation, GaussianBlur) {
  uint8_t src = 0;
  uint8_t dst = 0;

  expect_rejected(KLEIDICV_ERROR_NOT_IMPLEMENTED,
                  kleidicv_thread_gaussian_blur_u8(
                      nullptr, 1, &dst, 1, 1, 1, 1, 3, 3, 0.0F, 0.0F,
                      KLEIDICV_BORDER_TYPE_REPLICATE, scheduler()));
  expect_rejected(KLEIDICV_ERROR_NOT_IMPLEMENTED,
                  kleidicv_thread_gaussian_blur_u8(
                      nullptr, 1, &dst, 1, 3, 3, 1, 3, 3, 0.0F, 0.0F,
                      KLEIDICV_BORDER_TYPE_TRANSPARENT, scheduler()));
  expect_rejected(KLEIDICV_ERROR_NULL_POINTER,
                  kleidicv_thread_gaussian_blur_u8(
                      nullptr, 1, &dst, 1, 3, 3, 1, 3, 3, 0.0F, 0.0F,
                      KLEIDICV_BORDER_TYPE_REPLICATE, scheduler()));
  expect_rejected(KLEIDICV_ERROR_NULL_POINTER,
                  kleidicv_thread_gaussian_blur_u8(
                      nullptr, 1, &dst, 1, kLargeWidth, 3, 1, 3, 3, 0.0F, 0.0F,
                      KLEIDICV_BORDER_TYPE_REPLICATE, scheduler()));
  expect_rejected(KLEIDICV_ERROR_RANGE,
                  kleidicv_thread_gaussian_blur_u8(
                      &src, 1, &dst, 1, kLargeWidth, 3, 1, 3, 3, 0.0F, 0.0F,
                      KLEIDICV_BORDER_TYPE_REPLICATE, scheduler()));
}

TEST_F(ThreadFilterValidation, SeparableFilterU8) {
  uint8_t src = 0;
  uint8_t dst = 0;
  uint8_t kernel[5] = {};

  expect_rejected(KLEIDICV_ERROR_NOT_IMPLEMENTED,
                  kleidicv_thread_separable_filter_2d_u8(
                      nullptr, 1, &dst, 1, 5, 5, 1, kernel, 3, kernel, 5,
                      KLEIDICV_BORDER_TYPE_REPLICATE, scheduler()));
  expect_rejected(KLEIDICV_ERROR_NOT_IMPLEMENTED,
                  kleidicv_thread_separable_filter_2d_u8(
                      nullptr, 1, &dst, 1, 5, 5, 1, kernel, 5, kernel, 5,
                      KLEIDICV_BORDER_TYPE_TRANSPARENT, scheduler()));
  expect_rejected(
      KLEIDICV_ERROR_NULL_POINTER,
      kleidicv_thread_separable_filter_2d_u8(
          &src, 1, &dst, 1, 5, 5, KLEIDICV_MAXIMUM_CHANNEL_COUNT + 1, nullptr,
          5, kernel, 5, KLEIDICV_BORDER_TYPE_REPLICATE, scheduler()));
  expect_rejected(KLEIDICV_ERROR_NULL_POINTER,
                  kleidicv_thread_separable_filter_2d_u8(
                      nullptr, 1, &dst, 1, kLargeWidth, 5, 1, kernel, 5, kernel,
                      5, KLEIDICV_BORDER_TYPE_REPLICATE, scheduler()));
  expect_rejected(KLEIDICV_ERROR_RANGE,
                  kleidicv_thread_separable_filter_2d_u8(
                      &src, 1, &dst, 1, kLargeWidth, 5, 1, nullptr, 5, kernel,
                      5, KLEIDICV_BORDER_TYPE_REPLICATE, scheduler()));
  expect_rejected(KLEIDICV_ERROR_RANGE,
                  kleidicv_thread_separable_filter_2d_u8(
                      &src, 1, &dst, 1, kLargeWidth, 5, 1, kernel, 5, kernel, 5,
                      KLEIDICV_BORDER_TYPE_REPLICATE, scheduler()));
}

TEST_F(ThreadFilterValidation, SeparableFilterU16) {
  uint16_t src = 0;
  uint16_t dst = 0;
  uint16_t kernel[5] = {};

  expect_rejected(KLEIDICV_ERROR_ALIGNMENT,
                  kleidicv_thread_separable_filter_2d_u16(
                      &src, sizeof(src), &dst, 1, 5, 5, 1, kernel, 5, kernel, 5,
                      KLEIDICV_BORDER_TYPE_REPLICATE, scheduler()));
  expect_rejected(
      KLEIDICV_ERROR_RANGE,
      kleidicv_thread_separable_filter_2d_u16(
          &src, sizeof(src), &dst, sizeof(dst), kLargeWidth, 5, 1, kernel, 5,
          kernel, 5, KLEIDICV_BORDER_TYPE_REPLICATE, scheduler()));
}

TEST_F(ThreadFilterValidation, BlurAndDownsample) {
  uint8_t src = 0;
  uint8_t dst = 0;

  expect_rejected(KLEIDICV_ERROR_NOT_IMPLEMENTED,
                  kleidicv_thread_blur_and_downsample_u8(
                      nullptr, 1, 3, 4, &dst, 1, 1,
                      KLEIDICV_BORDER_TYPE_REPLICATE, scheduler()));
  expect_rejected(KLEIDICV_ERROR_NOT_IMPLEMENTED,
                  kleidicv_thread_blur_and_downsample_u8(
                      nullptr, 1, 4, 4, &dst, 1, 1,
                      KLEIDICV_BORDER_TYPE_TRANSPARENT, scheduler()));
  expect_rejected(KLEIDICV_ERROR_NULL_POINTER,
                  kleidicv_thread_blur_and_downsample_u8(
                      &src, 1, 4, 4, nullptr, 1, 1,
                      KLEIDICV_BORDER_TYPE_REPLICATE, scheduler()));
  expect_rejected(KLEIDICV_ERROR_NULL_POINTER,
                  kleidicv_thread_blur_and_downsample_u8(
                      nullptr, 1, kLargeWidth, 4, &dst, 1, 1,
                      KLEIDICV_BORDER_TYPE_REPLICATE, scheduler()));
  expect_rejected(KLEIDICV_ERROR_RANGE,
                  kleidicv_thread_blur_and_downsample_u8(
                      &src, 1, kLargeWidth, 4, &dst, 1, 1,
                      KLEIDICV_BORDER_TYPE_REPLICATE, scheduler()));
}

TEST_F(ThreadFilterValidation, SobelHorizontal) {
  uint8_t src = 0;
  int16_t dst = 0;

  expect_rejected(KLEIDICV_ERROR_NOT_IMPLEMENTED,
                  kleidicv_thread_sobel_3x3_horizontal_s16_u8(
                      nullptr, 1, &dst, sizeof(dst), 1, 3, 1, scheduler()));
  expect_rejected(
      KLEIDICV_ERROR_NULL_POINTER,
      kleidicv_thread_sobel_3x3_horizontal_s16_u8(
          nullptr, 1, &dst, sizeof(dst), kLargeWidth, 3, 1, scheduler()));
  expect_rejected(KLEIDICV_ERROR_NOT_IMPLEMENTED,
                  kleidicv_thread_sobel_3x3_horizontal_s16_u8(
                      &src, 1, &dst, sizeof(dst), 3, 3,
                      KLEIDICV_MAXIMUM_CHANNEL_COUNT + 1, scheduler()));
  expect_rejected(KLEIDICV_ERROR_RANGE,
                  kleidicv_thread_sobel_3x3_horizontal_s16_u8(
                      &src, 1, &dst, sizeof(dst), kLargeWidth, 3,
                      KLEIDICV_MAXIMUM_CHANNEL_COUNT + 1, scheduler()));
}

TEST_F(ThreadFilterValidation, SobelVertical) {
  uint8_t src = 0;
  int16_t dst = 0;

  expect_rejected(KLEIDICV_ERROR_ALIGNMENT,
                  kleidicv_thread_sobel_3x3_vertical_s16_u8(&src, 1, &dst, 1, 3,
                                                            3, 1, scheduler()));
  expect_rejected(
      KLEIDICV_ERROR_RANGE,
      kleidicv_thread_sobel_3x3_vertical_s16_u8(
          &src, 1, &dst, sizeof(dst), kLargeWidth, 3, 1, scheduler()));
}

TEST_F(ThreadFilterValidation, Scharr) {
  uint8_t src = 0;
  int16_t dst = 0;

  expect_rejected(KLEIDICV_ERROR_NOT_IMPLEMENTED,
                  kleidicv_thread_scharr_interleaved_s16_u8(
                      nullptr, 1, 2, 3, 1, &dst, sizeof(dst), scheduler()));
  expect_rejected(KLEIDICV_ERROR_NOT_IMPLEMENTED,
                  kleidicv_thread_scharr_interleaved_s16_u8(
                      nullptr, 1, 3, 3, KLEIDICV_MAXIMUM_CHANNEL_COUNT + 1,
                      &dst, sizeof(dst), scheduler()));
  expect_rejected(
      KLEIDICV_ERROR_NULL_POINTER,
      kleidicv_thread_scharr_interleaved_s16_u8(
          nullptr, 1, kLargeWidth, 3, 1, &dst, sizeof(dst), scheduler()));
  expect_rejected(KLEIDICV_ERROR_NULL_POINTER,
                  kleidicv_thread_scharr_interleaved_s16_u8(
                      &src, 1, 3, 3, 1, nullptr, sizeof(dst), scheduler()));
  expect_rejected(KLEIDICV_ERROR_ALIGNMENT,
                  kleidicv_thread_scharr_interleaved_s16_u8(
                      &src, 1, 3, 3, 1, &dst, 1, scheduler()));
  expect_rejected(
      KLEIDICV_ERROR_RANGE,
      kleidicv_thread_scharr_interleaved_s16_u8(
          &src, 1, kLargeWidth, 3, 1, &dst, sizeof(dst), scheduler()));
}

TEST(FilterStripeWorkers, GaussianBlurFixedHonorsRowRange) {
  constexpr size_t kWidth = 7;
  constexpr size_t kHeight = 6;
  constexpr size_t kChannels = 1;
  constexpr size_t kKernelSize = 3;
  constexpr size_t kBegin = 2;
  constexpr size_t kEnd = 4;
  constexpr uint8_t kUntouched = 0xA5;
  std::array<uint8_t, kWidth * kHeight> src = {};
  std::array<uint8_t, kWidth * kHeight> expected = {};
  std::array<uint8_t, kWidth * kHeight> actual = {};
  fill_pattern(src);
  actual.fill(kUntouched);
  auto expected_partial = actual;

  const auto validation = kleidicv::gaussian_blur_validate(
      src.data(), kWidth, actual.data(), kWidth, kWidth, kHeight, kChannels,
      kKernelSize, kKernelSize, 0.0F, 0.0F, KLEIDICV_BORDER_TYPE_REPLICATE);
  ASSERT_EQ(KLEIDICV_OK, validation.error);
  ASSERT_EQ(KLEIDICV_OK,
            kleidicv_gaussian_blur_u8(src.data(), kWidth, expected.data(),
                                      kWidth, kWidth, kHeight, kChannels,
                                      kKernelSize, kKernelSize, 0.0F, 0.0F,
                                      KLEIDICV_BORDER_TYPE_REPLICATE));
  ASSERT_EQ(KLEIDICV_OK,
            kleidicv_gaussian_blur_fixed_stripe_u8(
                src.data(), kWidth, actual.data(), kWidth, kWidth, kHeight,
                kBegin, kEnd, kChannels, kKernelSize, kKernelSize, 0.0F, 0.0F,
                validation.fixed_border_type));

  copy_rows(expected, expected_partial, kWidth, kBegin, kEnd);
  EXPECT_EQ(expected_partial, actual);
}

TEST(FilterStripeWorkers, SeparableFilterU8HonorsRowRange) {
  constexpr size_t kWidth = 7;
  constexpr size_t kHeight = 6;
  constexpr size_t kChannels = 1;
  constexpr size_t kKernelSize = 5;
  constexpr size_t kBegin = 2;
  constexpr size_t kEnd = 4;
  constexpr uint8_t kUntouched = 0xA5;
  const std::array<uint8_t, kKernelSize> kernel = {0, 0, 1, 0, 0};
  std::array<uint8_t, kWidth * kHeight> src = {};
  std::array<uint8_t, kWidth * kHeight> expected = {};
  std::array<uint8_t, kWidth * kHeight> actual = {};
  fill_pattern(src);
  actual.fill(kUntouched);
  auto expected_partial = actual;

  const auto validation = kleidicv::separable_filter_2d_validate(
      src.data(), kWidth, actual.data(), kWidth, kWidth, kHeight, kChannels,
      kernel.data(), kKernelSize, kernel.data(), kKernelSize,
      KLEIDICV_BORDER_TYPE_REPLICATE);
  ASSERT_EQ(KLEIDICV_OK, validation.error);
  ASSERT_EQ(KLEIDICV_OK,
            kleidicv_separable_filter_2d_u8(
                src.data(), kWidth, expected.data(), kWidth, kWidth, kHeight,
                kChannels, kernel.data(), kKernelSize, kernel.data(),
                kKernelSize, KLEIDICV_BORDER_TYPE_REPLICATE));
  ASSERT_EQ(KLEIDICV_OK,
            kleidicv_separable_filter_2d_stripe_u8(
                src.data(), kWidth, actual.data(), kWidth, kWidth, kHeight,
                kBegin, kEnd, kChannels, kernel.data(), kKernelSize,
                kernel.data(), kKernelSize, validation.fixed_border_type));

  copy_rows(expected, expected_partial, kWidth, kBegin, kEnd);
  EXPECT_EQ(expected_partial, actual);
}

TEST(FilterStripeWorkers, SeparableFilterU16HonorsRowRange) {
  constexpr size_t kWidth = 7;
  constexpr size_t kHeight = 6;
  constexpr size_t kChannels = 1;
  constexpr size_t kKernelSize = 5;
  constexpr size_t kBegin = 2;
  constexpr size_t kEnd = 4;
  constexpr uint16_t kUntouched = 0xA5A5;
  constexpr size_t kStride = kWidth * sizeof(uint16_t);
  const std::array<uint16_t, kKernelSize> kernel = {0, 0, 1, 0, 0};
  std::array<uint16_t, kWidth * kHeight> src = {};
  std::array<uint16_t, kWidth * kHeight> expected = {};
  std::array<uint16_t, kWidth * kHeight> actual = {};
  fill_pattern(src);
  actual.fill(kUntouched);
  auto expected_partial = actual;

  const auto validation = kleidicv::separable_filter_2d_validate(
      src.data(), kStride, actual.data(), kStride, kWidth, kHeight, kChannels,
      kernel.data(), kKernelSize, kernel.data(), kKernelSize,
      KLEIDICV_BORDER_TYPE_REPLICATE);
  ASSERT_EQ(KLEIDICV_OK, validation.error);
  ASSERT_EQ(KLEIDICV_OK,
            kleidicv_separable_filter_2d_u16(
                src.data(), kStride, expected.data(), kStride, kWidth, kHeight,
                kChannels, kernel.data(), kKernelSize, kernel.data(),
                kKernelSize, KLEIDICV_BORDER_TYPE_REPLICATE));
  ASSERT_EQ(KLEIDICV_OK,
            kleidicv_separable_filter_2d_stripe_u16(
                src.data(), kStride, actual.data(), kStride, kWidth, kHeight,
                kBegin, kEnd, kChannels, kernel.data(), kKernelSize,
                kernel.data(), kKernelSize, validation.fixed_border_type));

  copy_rows(expected, expected_partial, kWidth, kBegin, kEnd);
  EXPECT_EQ(expected_partial, actual);
}

TEST(FilterStripeWorkers, BlurAndDownsampleHonorsSourceRowRange) {
  constexpr size_t kSrcWidth = 7;
  constexpr size_t kSrcHeight = 7;
  constexpr size_t kDstWidth = (kSrcWidth + 1) / 2;
  constexpr size_t kDstHeight = (kSrcHeight + 1) / 2;
  constexpr size_t kChannels = 1;
  constexpr size_t kBegin = 1;
  constexpr size_t kEnd = 6;
  constexpr size_t kDstBegin = 1;
  constexpr size_t kDstEnd = 3;
  constexpr uint8_t kUntouched = 0xA5;
  std::array<uint8_t, kSrcWidth * kSrcHeight> src = {};
  std::array<uint8_t, kDstWidth * kDstHeight> expected = {};
  std::array<uint8_t, kDstWidth * kDstHeight> actual = {};
  fill_pattern(src);
  actual.fill(kUntouched);
  auto expected_partial = actual;

  const auto validation = kleidicv::blur_and_downsample_validate(
      src.data(), kSrcWidth, kSrcWidth, kSrcHeight, actual.data(), kDstWidth,
      kChannels, KLEIDICV_BORDER_TYPE_REPLICATE);
  ASSERT_EQ(KLEIDICV_OK, validation.error);
  ASSERT_EQ(KLEIDICV_OK,
            kleidicv_blur_and_downsample_u8(
                src.data(), kSrcWidth, kSrcWidth, kSrcHeight, expected.data(),
                kDstWidth, kChannels, KLEIDICV_BORDER_TYPE_REPLICATE));
  ASSERT_EQ(KLEIDICV_OK, kleidicv_blur_and_downsample_stripe_u8(
                             src.data(), kSrcWidth, kSrcWidth, kSrcHeight,
                             actual.data(), kDstWidth, kBegin, kEnd, kChannels,
                             validation.fixed_border_type));

  copy_rows(expected, expected_partial, kDstWidth, kDstBegin, kDstEnd);
  EXPECT_EQ(expected_partial, actual);
}

TEST(FilterStripeWorkers, SobelHonorsRowRange) {
  constexpr size_t kWidth = 7;
  constexpr size_t kHeight = 6;
  constexpr size_t kChannels = 1;
  constexpr size_t kBegin = 2;
  constexpr size_t kEnd = 4;
  constexpr size_t kDstStride = kWidth * sizeof(int16_t);
  constexpr int16_t kUntouched = -12345;
  std::array<uint8_t, kWidth * kHeight> src = {};
  fill_pattern(src);

  auto test_direction = [&](auto public_api, auto stripe_api) {
    std::array<int16_t, kWidth * kHeight> expected = {};
    std::array<int16_t, kWidth * kHeight> actual = {};
    actual.fill(kUntouched);
    auto expected_partial = actual;

    ASSERT_EQ(KLEIDICV_OK,
              kleidicv::sobel_validate(src.data(), kWidth, actual.data(),
                                       kDstStride, kWidth, kHeight, kChannels));
    ASSERT_EQ(KLEIDICV_OK, public_api(src.data(), kWidth, expected.data(),
                                      kDstStride, kWidth, kHeight, kChannels));
    ASSERT_EQ(KLEIDICV_OK,
              stripe_api(src.data(), kWidth, actual.data(), kDstStride, kWidth,
                         kHeight, kBegin, kEnd, kChannels));

    copy_rows(expected, expected_partial, kWidth, kBegin, kEnd);
    EXPECT_EQ(expected_partial, actual);
  };

  test_direction(kleidicv_sobel_3x3_horizontal_s16_u8,
                 kleidicv_sobel_3x3_horizontal_stripe_s16_u8);
  test_direction(kleidicv_sobel_3x3_vertical_s16_u8,
                 kleidicv_sobel_3x3_vertical_stripe_s16_u8);
}

TEST(FilterStripeWorkers, ScharrHonorsRowRange) {
  constexpr size_t kSrcWidth = 7;
  constexpr size_t kSrcHeight = 6;
  constexpr size_t kSrcChannels = 1;
  constexpr size_t kDstWidth = (kSrcWidth - 2) * kSrcChannels * 2;
  constexpr size_t kDstHeight = kSrcHeight - 2;
  constexpr size_t kDstStride = kDstWidth * sizeof(int16_t);
  constexpr size_t kBegin = 1;
  constexpr size_t kEnd = 3;
  constexpr int16_t kUntouched = -12345;
  std::array<uint8_t, kSrcWidth * kSrcHeight> src = {};
  std::array<int16_t, kDstWidth * kDstHeight> expected = {};
  std::array<int16_t, kDstWidth * kDstHeight> actual = {};
  fill_pattern(src);
  actual.fill(kUntouched);
  auto expected_partial = actual;

  ASSERT_EQ(KLEIDICV_OK, kleidicv::scharr_interleaved_validate(
                             src.data(), kSrcWidth, kSrcWidth, kSrcHeight,
                             kSrcChannels, actual.data(), kDstStride));
  ASSERT_EQ(KLEIDICV_OK, kleidicv_scharr_interleaved_s16_u8(
                             src.data(), kSrcWidth, kSrcWidth, kSrcHeight,
                             kSrcChannels, expected.data(), kDstStride));
  ASSERT_EQ(KLEIDICV_OK,
            kleidicv_scharr_interleaved_stripe_s16_u8(
                src.data(), kSrcWidth, kSrcWidth, kSrcHeight, kSrcChannels,
                actual.data(), kDstStride, kBegin, kEnd));

  copy_rows(expected, expected_partial, kDstWidth, kBegin, kEnd);
  EXPECT_EQ(expected_partial, actual);
}

}  // namespace
