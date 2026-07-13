// SPDX-FileCopyrightText: 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#include <limits>
#include <vector>

#include "opencv2/core.hpp"
#include "tests.h"

template <int FlipCode>
static cv::Mat exec_flip(cv::Mat& input_mat) {
  cv::Mat result;
  cv::flip(input_mat, result, FlipCode);
  return result;
}

template <int FlipCode>
static cv::Mat exec_flip_inplace(cv::Mat& input_mat) {
  cv::Mat result = input_mat.clone();
  cv::flip(result, result, FlipCode);
  return result;
}

#if MANAGER
template <typename TypeParam, size_t Channels, int FlipCode>
bool test_flip(int index, RecreatedMessageQueue& request_queue,
               RecreatedMessageQueue& reply_queue) {
  cv::RNG rng(0);

  for (size_t height = 5; height <= 16; ++height) {
    for (size_t width = 5; width <= 16; ++width) {
      cv::Mat input(height, width,
                    get_opencv_matrix_type<TypeParam, Channels>());
      rng.fill(input, cv::RNG::UNIFORM, std::numeric_limits<TypeParam>::min(),
               std::numeric_limits<TypeParam>::max());

      cv::Mat actual = exec_flip<FlipCode>(input);
      cv::Mat expected = get_expected_from_subordinate(index, request_queue,
                                                       reply_queue, input);

      if (are_matrices_different<TypeParam>(0, actual, expected)) {
        fail_print_matrices(input.rows, input.cols, input, actual, expected);
        return true;  // true on fail
      }
    }
  }

  return false;
}

template <typename TypeParam, size_t Channels, int FlipCode>
bool test_flip_inplace(int index, RecreatedMessageQueue& request_queue,
                       RecreatedMessageQueue& reply_queue) {
  cv::RNG rng(0);

  for (size_t height = 5; height <= 16; ++height) {
    for (size_t width = 5; width <= 16; ++width) {
      cv::Mat input(height, width,
                    get_opencv_matrix_type<TypeParam, Channels>());
      rng.fill(input, cv::RNG::UNIFORM, std::numeric_limits<TypeParam>::min(),
               std::numeric_limits<TypeParam>::max());

      cv::Mat actual = exec_flip_inplace<FlipCode>(input);
      cv::Mat expected = get_expected_from_subordinate(index, request_queue,
                                                       reply_queue, input);

      if (are_matrices_different<TypeParam>(0, actual, expected)) {
        fail_print_matrices(input.rows, input.cols, input, actual, expected);
        return true;
      }
    }
  }

  return false;
}
#endif

#define FLIP_TEST(type, channels, direction_name, direction_code, label) \
  TEST("Flip " direction_name ", " #channels " channel (" label ")",     \
       (test_flip<type, channels, direction_code>),                      \
       (exec_flip<direction_code>))

#define FLIP_INPLACE_TEST(type, channels, direction_name, direction_code,     \
                          label)                                              \
  TEST("Flip " direction_name " In-place, " #channels " channel (" label ")", \
       (test_flip_inplace<type, channels, direction_code>),                   \
       (exec_flip_inplace<direction_code>))

std::vector<test>& flip_tests_get() {
  static std::vector<test> tests = {
      FLIP_TEST(uint8_t, 1, "Horizontal", 1, "U8"),
      FLIP_TEST(uint8_t, 2, "Horizontal", 1, "U8"),
      FLIP_TEST(uint8_t, 3, "Horizontal", 1, "U8"),
      FLIP_TEST(uint8_t, 4, "Horizontal", 1, "U8"),
      FLIP_TEST(uint16_t, 1, "Horizontal", 1, "U16"),
      FLIP_TEST(uint16_t, 2, "Horizontal", 1, "U16"),
      FLIP_TEST(uint16_t, 3, "Horizontal", 1, "U16"),
      FLIP_TEST(uint16_t, 4, "Horizontal", 1, "U16"),

      FLIP_INPLACE_TEST(uint8_t, 1, "Horizontal", 1, "U8"),
      FLIP_INPLACE_TEST(uint8_t, 2, "Horizontal", 1, "U8"),
      FLIP_INPLACE_TEST(uint8_t, 3, "Horizontal", 1, "U8"),
      FLIP_INPLACE_TEST(uint8_t, 4, "Horizontal", 1, "U8"),
      FLIP_INPLACE_TEST(uint16_t, 1, "Horizontal", 1, "U16"),
      FLIP_INPLACE_TEST(uint16_t, 2, "Horizontal", 1, "U16"),
      FLIP_INPLACE_TEST(uint16_t, 3, "Horizontal", 1, "U16"),
      FLIP_INPLACE_TEST(uint16_t, 4, "Horizontal", 1, "U16"),
  };

  return tests;
}
