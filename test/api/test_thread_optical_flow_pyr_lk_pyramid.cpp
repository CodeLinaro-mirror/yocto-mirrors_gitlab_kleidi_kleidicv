// SPDX-FileCopyrightText: 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <tuple>

#include "framework/array.h"
#include "framework/generator.h"
#include "framework/utils.h"
#include "kleidicv/kleidicv.h"
#include "kleidicv_thread/kleidicv_thread.h"
#include "multithreading_fake.h"

// Tuple of width, height, channels, requested_level_count, window_size,
// thread_count.
typedef std::tuple<unsigned, unsigned, unsigned, unsigned, unsigned, unsigned>
    P;

class OpticalFlowPyrLkThread : public testing::TestWithParam<P> {
 public:
  void check_builders_match() {
    unsigned width = 0;
    unsigned height = 0;
    unsigned channels = 0;
    unsigned requested_level_count = 0;
    unsigned window = 0;
    unsigned thread_count = 0;
    std::tie(width, height, channels, requested_level_count, window,
             thread_count) = GetParam();

    const size_t width_bytes = static_cast<size_t>(width) * channels;
    test::Array2D<uint8_t> src(width_bytes, height);
    test::PseudoRandomNumberGenerator<uint8_t> generator;
    src.fill(generator);

    kleidicv_optical_flow_pyr_lk_pyramid_t* single = nullptr;
    kleidicv_optical_flow_pyr_lk_pyramid_t* multi = nullptr;

    const kleidicv_error_t single_result =
        kleidicv_build_optical_flow_pyr_lk_pyramid(
            &single, src.data(), src.stride(), width, height, channels,
            requested_level_count, window, window);

    const kleidicv_error_t multi_result =
        kleidicv_thread_build_optical_flow_pyr_lk_pyramid(
            &multi, src.data(), src.stride(), width, height, channels,
            requested_level_count, window, window,
            get_multithreading_fake(thread_count));

    ASSERT_EQ(KLEIDICV_OK, single_result);
    ASSERT_EQ(KLEIDICV_OK, multi_result);
    ASSERT_NE(nullptr, single);
    ASSERT_NE(nullptr, multi);

    size_t single_levels = 0;
    size_t multi_levels = 0;
    ASSERT_EQ(KLEIDICV_OK, kleidicv_optical_flow_pyr_lk_pyramid_get_level_count(
                               single, &single_levels));
    ASSERT_EQ(KLEIDICV_OK, kleidicv_optical_flow_pyr_lk_pyramid_get_level_count(
                               multi, &multi_levels));

    ASSERT_EQ(single_levels, multi_levels);

    for (size_t level = 0; level < single_levels; ++level) {
      const uint8_t* single_image_data = nullptr;
      const uint8_t* multi_image_data = nullptr;
      size_t single_image_stride = 0;
      size_t multi_image_stride = 0;
      size_t single_image_width = 0;
      size_t multi_image_width = 0;
      size_t single_image_height = 0;
      size_t multi_image_height = 0;

      ASSERT_EQ(KLEIDICV_OK,
                kleidicv_optical_flow_pyr_lk_pyramid_get_image_level(
                    single, level, &single_image_data, &single_image_stride,
                    &single_image_width, &single_image_height));
      ASSERT_EQ(KLEIDICV_OK,
                kleidicv_optical_flow_pyr_lk_pyramid_get_image_level(
                    multi, level, &multi_image_data, &multi_image_stride,
                    &multi_image_width, &multi_image_height));

      ASSERT_EQ(single_image_width, multi_image_width);
      ASSERT_EQ(single_image_height, multi_image_height);
      for (size_t y = 0; y < single_image_height; ++y) {
        for (size_t x = 0; x < single_image_width * channels; ++x) {
          ASSERT_EQ(single_image_data[y * single_image_stride + x],
                    multi_image_data[y * multi_image_stride + x]);
        }
      }

      const int16_t* single_scharr_data = nullptr;
      const int16_t* multi_scharr_data = nullptr;
      size_t single_scharr_stride = 0;
      size_t multi_scharr_stride = 0;
      size_t single_scharr_width = 0;
      size_t multi_scharr_width = 0;
      size_t single_scharr_height = 0;
      size_t multi_scharr_height = 0;

      ASSERT_EQ(KLEIDICV_OK,
                kleidicv_optical_flow_pyr_lk_pyramid_get_scharr_level(
                    single, level, &single_scharr_data, &single_scharr_stride,
                    &single_scharr_width, &single_scharr_height));
      ASSERT_EQ(KLEIDICV_OK,
                kleidicv_optical_flow_pyr_lk_pyramid_get_scharr_level(
                    multi, level, &multi_scharr_data, &multi_scharr_stride,
                    &multi_scharr_width, &multi_scharr_height));

      ASSERT_EQ(single_scharr_width, multi_scharr_width);
      ASSERT_EQ(single_scharr_height, multi_scharr_height);

      for (size_t y = 0; y < single_scharr_height; ++y) {
        const auto* single_row = reinterpret_cast<const int16_t*>(
            reinterpret_cast<const uint8_t*>(single_scharr_data) +
            y * single_scharr_stride);
        const auto* multi_row = reinterpret_cast<const int16_t*>(
            reinterpret_cast<const uint8_t*>(multi_scharr_data) +
            y * multi_scharr_stride);
        for (size_t x = 0; x < single_scharr_width * channels * 2; ++x) {
          ASSERT_EQ(single_row[x], multi_row[x]);
        }
      }
    }

    ASSERT_EQ(KLEIDICV_OK,
              kleidicv_optical_flow_pyr_lk_pyramid_release(single));
    ASSERT_EQ(KLEIDICV_OK, kleidicv_optical_flow_pyr_lk_pyramid_release(multi));
  }

  void run_unsupported() {
    test::Array2D<uint8_t> src(8, 8);
    ASSERT_TRUE(src.valid());
    kleidicv_optical_flow_pyr_lk_pyramid_t* pyramid = nullptr;

    EXPECT_EQ(KLEIDICV_ERROR_RANGE,
              kleidicv_thread_build_optical_flow_pyr_lk_pyramid(
                  &pyramid, src.data(), src.stride(), 0, 8, 1, 1, 5, 5,
                  get_multithreading_fake(2)));

    EXPECT_EQ(KLEIDICV_ERROR_RANGE,
              kleidicv_thread_build_optical_flow_pyr_lk_pyramid(
                  &pyramid, src.data(), src.stride(), 8, 0, 1, 1, 5, 5,
                  get_multithreading_fake(2)));

    EXPECT_EQ(KLEIDICV_ERROR_RANGE,
              kleidicv_thread_build_optical_flow_pyr_lk_pyramid(
                  &pyramid, src.data(), src.stride(), 8, 8, 1, 0, 5, 5,
                  get_multithreading_fake(2)));

    EXPECT_EQ(KLEIDICV_ERROR_RANGE,
              kleidicv_thread_build_optical_flow_pyr_lk_pyramid(
                  &pyramid, src.data(), src.stride(), 8, 8, 1, 1, 2, 5,
                  get_multithreading_fake(2)));

    EXPECT_EQ(KLEIDICV_ERROR_RANGE,
              kleidicv_thread_build_optical_flow_pyr_lk_pyramid(
                  &pyramid, src.data(), src.stride(), 8, 8, 1, 1, 5, 2,
                  get_multithreading_fake(2)));

    EXPECT_EQ(KLEIDICV_ERROR_RANGE,
              kleidicv_thread_build_optical_flow_pyr_lk_pyramid(
                  &pyramid, src.data(), 7, 8, 8, 1, 1, 5, 5,
                  get_multithreading_fake(2)));

    EXPECT_EQ(
        KLEIDICV_ERROR_RANGE,
        kleidicv_thread_build_optical_flow_pyr_lk_pyramid(
            &pyramid, src.data(), 16, 8, 8, KLEIDICV_MAXIMUM_CHANNEL_COUNT + 1,
            1, 5, 5, get_multithreading_fake(2)));
  }
};

TEST_P(OpticalFlowPyrLkThread, MatchesSingleThreadedBuilder) {
  check_builders_match();
}

TEST_F(OpticalFlowPyrLkThread, UnsupportedCombinations) { run_unsupported(); }

TEST_F(OpticalFlowPyrLkThread, NullPointer) {
  test::Array2D<uint8_t> src(8, 8);
  ASSERT_TRUE(src.valid());
  kleidicv_optical_flow_pyr_lk_pyramid_t* pyramid = nullptr;
  test::test_null_args(kleidicv_thread_build_optical_flow_pyr_lk_pyramid,
                       &pyramid, src.data(), src.stride(), 8, 8, 1, 1, 5, 5,
                       get_multithreading_fake(2));
}

INSTANTIATE_TEST_SUITE_P(
    , OpticalFlowPyrLkThread,
    testing::Values(P{16, 16, 1, 4, 5, 1}, P{16, 16, 1, 4, 5, 2},
                    P{31, 27, 1, 6, 5, 3}, P{64, 48, 1, 6, 9, 4},
                    P{48, 64, 1, 6, 15, 6}, P{33, 65, 1, 8, 5, 8},
                    P{32, 24, 2, 5, 5, 3}, P{32, 24, 3, 5, 5, 4},
                    P{32, 24, 4, 5, 5, 5}));
