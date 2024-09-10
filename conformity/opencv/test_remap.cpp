// SPDX-FileCopyrightText: 2024 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#include <limits>
#include <type_traits>
#include <vector>

#include "opencv2/core/hal/interface.h"
#include "opencv2/imgproc/hal/interface.h"
#include "tests.h"

const size_t kMaxHeight = 32, kMaxWidth = 32;

template <class ScalarType>
static cv::Mat get_source_mat(int Format) {
  auto generate_source = [&]() {
    cv::Mat m{kMaxHeight, kMaxWidth, Format};
    for (size_t row = 0; row < kMaxHeight; ++row) {
      for (size_t column = 0; column < kMaxWidth; ++column) {
        m.at<ScalarType>(row, column) =
            (row * kMaxWidth + column) % std::numeric_limits<ScalarType>::max();
      }
    }
    return m;
  };
  static cv::Mat source = generate_source();
  return source;
}

// BorderValue is interpreted as 1/1000, i.e. 500 for 0.5
template <class ScalarType, int Format, int Interpolation, int BorderMode,
          int BorderValue>
cv::Mat exec_remap16(cv::Mat& mapxy_mat) {
  cv::Mat empty;
  cv::Mat result(mapxy_mat.size().height, mapxy_mat.size().width, Format);
  cv::Mat source_mat = get_source_mat<ScalarType>(Format);
  remap(source_mat, result, mapxy_mat, empty, Interpolation, BorderMode,
        BorderValue / 1000.0);
  return result;
}

#if MANAGER
template <class ScalarType, int Format, int Interpolation, int BorderMode,
          int BorderValue>
bool test_remap16(int index, RecreatedMessageQueue& request_queue,
                  RecreatedMessageQueue& reply_queue) {
  cv::RNG rng(0);

  for (size_t x = 5; x <= kMaxWidth; x += 3) {
    for (size_t y = 5; y <= kMaxHeight; y += 2) {
      cv::Mat source_mat = get_source_mat<ScalarType>(Format);
      cv::Mat mapxy_mat(x, y, CV_16SC2);
      rng.fill(mapxy_mat, cv::RNG::UNIFORM, -3, kMaxWidth + 3);

      cv::Mat actual_mat = exec_remap16<ScalarType, Format, Interpolation,
                                        BorderMode, BorderValue>(mapxy_mat);
      cv::Mat expected_mat = get_expected_from_subordinate(
          index, request_queue, reply_queue, mapxy_mat);

      bool success =
          (CV_MAT_DEPTH(Format) == CV_8U &&
           !are_matrices_different<uint8_t>(1, actual_mat, expected_mat)) ||
          (CV_MAT_DEPTH(Format) == CV_16U &&
           !are_matrices_different<uint16_t>(1, actual_mat, expected_mat));
      if (!success) {
        fail_print_matrices(x, y, source_mat, actual_mat, expected_mat);
        return true;
      }
    }
  }
  return false;
}
#endif

std::vector<test>& remap_tests_get() {
  // clang-format off
  static std::vector<test> tests = {
    TEST("Remap16s uint8", (test_remap16<uint8_t, CV_8UC1, cv::INTER_NEAREST, cv::BORDER_REPLICATE, 0>), (exec_remap16<uint8_t, CV_8UC1, cv::INTER_NEAREST, cv::BORDER_REPLICATE, 0>)),
  };
  // clang-format on
  return tests;
}
