// SPDX-FileCopyrightText: 2024 - 2025 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#include <vector>

#include "tests.h"

template <size_t KernelSize>
cv::Mat exec_median_blur(cv::Mat& input) {
  cv::Mat result;
  cv::medianBlur(input, result, KernelSize);
  return result;
}

#if MANAGER
template <size_t KernelSize, typename TypeParam, size_t Channels>
bool test_median_blur(int index, RecreatedMessageQueue& request_queue,
                      RecreatedMessageQueue& reply_queue) {
  cv::RNG rng(0);

  for (size_t x = 5; x <= 16; ++x) {
    for (size_t y = 5; y <= 16; ++y) {
      cv::Mat input(x, y, get_opencv_matrix_type<TypeParam, Channels>());
      rng.fill(input, cv::RNG::UNIFORM, std::numeric_limits<TypeParam>::min(),
               std::numeric_limits<TypeParam>::max());

      cv::Mat actual = exec_median_blur<KernelSize>(input);
      cv::Mat expected = get_expected_from_subordinate(index, request_queue,
                                                       reply_queue, input);

      if (are_matrices_different<TypeParam>(0, actual, expected)) {
        fail_print_matrices(x, y, input, actual, expected);
        return true;
      }
    }
  }

  return false;
}
#endif

std::vector<test>& median_blur_tests_get() {
  // clang-format off
  static std::vector<test> tests = {
    TEST("Median 5x5, 1 channel (U8)",  (test_median_blur<5, uint8_t, 1>),  exec_median_blur<5>),
    TEST("Median 5x5, 3 channel (U8)",  (test_median_blur<5, uint8_t, 3>),  exec_median_blur<5>),
    TEST("Median 5x5, 4 channel (U8)",  (test_median_blur<5, uint8_t, 4>),  exec_median_blur<5>),
    TEST("Median 5x5, 1 channel (U16)", (test_median_blur<5, uint16_t, 1>), exec_median_blur<5>),
    TEST("Median 5x5, 3 channel (U16)", (test_median_blur<5, uint16_t, 3>), exec_median_blur<5>),
    TEST("Median 5x5, 4 channel (U16)", (test_median_blur<5, uint16_t, 4>), exec_median_blur<5>),
    TEST("Median 5x5, 1 channel (S16)", (test_median_blur<5, int16_t, 1>),  exec_median_blur<5>),
    TEST("Median 5x5, 3 channel (S16)", (test_median_blur<5, int16_t, 3>),  exec_median_blur<5>),
    TEST("Median 5x5, 4 channel (S16)", (test_median_blur<5, int16_t, 4>),  exec_median_blur<5>),
    TEST("Median 5x5, 1 channel (F32)", (test_median_blur<5, float, 1>),    exec_median_blur<5>),
    TEST("Median 5x5, 3 channel (F32)", (test_median_blur<5, float, 3>),    exec_median_blur<5>),
    TEST("Median 5x5, 4 channel (F32)", (test_median_blur<5, float, 4>),    exec_median_blur<5>),

    TEST("Median 7x7, 1 channel (U8)",  (test_median_blur<7, uint8_t, 1>),  exec_median_blur<7>),
    TEST("Median 7x7, 3 channel (U8)",  (test_median_blur<7, uint8_t, 3>),  exec_median_blur<7>),
    TEST("Median 7x7, 4 channel (U8)",  (test_median_blur<7, uint8_t, 4>),  exec_median_blur<7>),
  };
  // clang-format on
  return tests;
}
