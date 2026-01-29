// SPDX-FileCopyrightText: 2024 - 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#include <limits>
#include <type_traits>
#include <vector>

#include "opencv2/core/hal/interface.h"
#include "opencv2/imgproc/hal/interface.h"
#include "utils.h"

// Factor is interpreted as 1/1000, i.e. 500 for 0.5
template <int Factor, int Type>
cv::Mat exec_resize(cv::Mat& input_mat) {
  cv::Mat result(input_mat.size().height * Factor / 1000,
                 input_mat.size().width * Factor / 1000, input_mat.type());
  resize(input_mat, result, result.size(), 0, 0, Type);
  return result;
}

#if MANAGER
template <typename T>
static T get_threshold(int);

template <>
float get_threshold(int) {
  return 0.01F;
}

template <>
uint8_t get_threshold(int Factor) {
  if (Factor == 500 || Factor == 2000 || Factor == 4000) {
    return 1;
  }

  // OpenCV uses 7 bit fractionl part for interpolation, that results in a
  // bigger error
  return 4;
}

template <int Factor, int Type, int MinSize, int MaxSize, size_t Format>
bool test_resize(int index, RecreatedMessageQueue& request_queue,
                 RecreatedMessageQueue& reply_queue) {
  cv::RNG rng(0);

  for (size_t h = MinSize; h <= MaxSize; h += 2) {
    for (size_t w = MinSize; w <= MaxSize; w += 3) {
      cv::Mat input_mat;
      if constexpr (Factor < 1000) {
        input_mat.create(h * 1000 / Factor, w * 1000 / Factor, Format);
      } else {
        input_mat.create(h, w, Format);
      }
      if constexpr (CV_MAT_DEPTH(Format) == CV_32F) {
        rng.fill(input_mat, cv::RNG::UNIFORM, 1, 1000000);
      } else if constexpr (CV_MAT_DEPTH(Format) == CV_8U) {
        rng.fill(input_mat, cv::RNG::UNIFORM, 0, 255);
      } else {
        return true;
      }
      cv::Mat actual_mat = exec_resize<Factor, Type>(input_mat);
      cv::Mat expected_mat = get_expected_from_subordinate(
          index, request_queue, reply_queue, input_mat);

      bool success =
          (CV_MAT_DEPTH(Format) == CV_32F &&
           !are_float_matrices_different<float>(get_threshold<float>(Factor),
                                                actual_mat, expected_mat)) ||
          (CV_MAT_DEPTH(Format) == CV_8U &&
           !are_matrices_different<uint8_t>(get_threshold<uint8_t>(Factor),
                                            actual_mat, expected_mat));
      if (!success) {
        fail_print_matrices(h, w, input_mat, actual_mat, expected_mat);
        return true;
      }
    }
  }
  return false;
}
#endif

std::vector<test>& resize_tests_get() {
  // clang-format off
  static std::vector<test> tests = {
    TEST("Resize2x2 float32, INTER_LINEAR", (test_resize<2000, CV_HAL_INTER_LINEAR, 5, 16, CV_32FC1>), (exec_resize<2000, CV_HAL_INTER_LINEAR>)),
    TEST("Resize4x4 float32, INTER_LINEAR", (test_resize<4000, CV_HAL_INTER_LINEAR, 5, 16, CV_32FC1>), (exec_resize<4000, CV_HAL_INTER_LINEAR>)),
    TEST("Resize8x8 float32, INTER_LINEAR", (test_resize<8000, CV_HAL_INTER_LINEAR, 5, 16, CV_32FC1>), (exec_resize<8000, CV_HAL_INTER_LINEAR>)),

    TEST("Resize0.5x0.5 uint8, INTER_AREA", (test_resize<500, CV_HAL_INTER_AREA, 5, 32, CV_8UC1>), (exec_resize<500, CV_HAL_INTER_AREA>)),
    TEST("Resize0.5x0.5 uint8, INTER_LINEAR", (test_resize<500, CV_HAL_INTER_LINEAR, 5, 32, CV_8UC1>), (exec_resize<500, CV_HAL_INTER_LINEAR>)),
    TEST("Resize2x2 uint8, INTER_LINEAR", (test_resize<2000, CV_HAL_INTER_LINEAR, 5, 16, CV_8UC1>), (exec_resize<2000, CV_HAL_INTER_LINEAR>)),
    TEST("Resize4x4 uint8, INTER_LINEAR", (test_resize<4000, CV_HAL_INTER_LINEAR, 5, 16, CV_8UC1>), (exec_resize<4000, CV_HAL_INTER_LINEAR>)),

    TEST("Resize0.777x0.777 uint8, INTER_LINEAR", (test_resize<777, CV_HAL_INTER_LINEAR, 21, 64, CV_8UC1>), (exec_resize<777, CV_HAL_INTER_LINEAR>)),
    TEST("Resize0.444x0.444 uint8, INTER_LINEAR", (test_resize<444, CV_HAL_INTER_LINEAR, 21, 64, CV_8UC1>), (exec_resize<444, CV_HAL_INTER_LINEAR>)),
  };
  // clang-format on
  return tests;
}
