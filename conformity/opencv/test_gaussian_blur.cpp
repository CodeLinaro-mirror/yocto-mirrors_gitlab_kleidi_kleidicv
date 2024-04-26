// SPDX-FileCopyrightText: 2024 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#include "test_gaussian_blur.h"

#include <vector>

template <size_t KernelSize, size_t BorderType>
cv::Mat exec_gaussian_blur(cv::Mat& input) {
  cv::Size kernel(KernelSize, KernelSize);
  cv::Mat result;
  cv::GaussianBlur(input, result, kernel, 0, 0, BorderType);
  return result;
}

#if MANAGER
template <size_t KernelSize, size_t BorderType, size_t Channels>
bool test_gaussian_blur(int index, RecreatedMessageQueue& request_queue,
                        RecreatedMessageQueue& reply_queue) {
  cv::RNG rng(0);

  for (size_t x = 5; x <= 16; ++x) {
    for (size_t y = 5; y <= 16; ++y) {
      cv::Mat input(x, y, CV_8UC(Channels));
      rng.fill(input, cv::RNG::UNIFORM, 0, 255);

      cv::Mat actual = exec_gaussian_blur<KernelSize, BorderType>(input);
      cv::Mat expected = get_expected_from_subordinate(index, request_queue,
                                                       reply_queue, input);

      if (are_matrices_different<uint8_t>(0, actual, expected)) {
        fail_print_matrices(x, y, input, actual, expected);
        return true;
      }
    }
  }

  return false;
}
#endif

std::vector<test>& gaussian_blur_tests_get() {
  // clang-format off
  static std::vector<test> tests = {
    TEST("Gaussian blur 3x3, BORDER_REFLECT_101, 1 channel", (test_gaussian_blur<3, cv::BORDER_REFLECT_101, 1>), (exec_gaussian_blur<3, cv::BORDER_REFLECT_101>)),
    TEST("Gaussian blur 3x3, BORDER_REFLECT_101, 2 channel", (test_gaussian_blur<3, cv::BORDER_REFLECT_101, 2>), (exec_gaussian_blur<3, cv::BORDER_REFLECT_101>)),
    TEST("Gaussian blur 3x3, BORDER_REFLECT_101, 3 channel", (test_gaussian_blur<3, cv::BORDER_REFLECT_101, 3>), (exec_gaussian_blur<3, cv::BORDER_REFLECT_101>)),
    TEST("Gaussian blur 3x3, BORDER_REFLECT_101, 4 channel", (test_gaussian_blur<3, cv::BORDER_REFLECT_101, 4>), (exec_gaussian_blur<3, cv::BORDER_REFLECT_101>)),

    TEST("Gaussian blur 3x3, BORDER_REFLECT, 1 channel", (test_gaussian_blur<3, cv::BORDER_REFLECT, 1>), (exec_gaussian_blur<3, cv::BORDER_REFLECT>)),
    TEST("Gaussian blur 3x3, BORDER_REFLECT, 2 channel", (test_gaussian_blur<3, cv::BORDER_REFLECT, 2>), (exec_gaussian_blur<3, cv::BORDER_REFLECT>)),
    TEST("Gaussian blur 3x3, BORDER_REFLECT, 3 channel", (test_gaussian_blur<3, cv::BORDER_REFLECT, 3>), (exec_gaussian_blur<3, cv::BORDER_REFLECT>)),
    TEST("Gaussian blur 3x3, BORDER_REFLECT, 4 channel", (test_gaussian_blur<3, cv::BORDER_REFLECT, 4>), (exec_gaussian_blur<3, cv::BORDER_REFLECT>)),

    TEST("Gaussian blur 3x3, BORDER_WRAP, 1 channel", (test_gaussian_blur<3, cv::BORDER_WRAP, 1>), (exec_gaussian_blur<3, cv::BORDER_WRAP>)),
    TEST("Gaussian blur 3x3, BORDER_WRAP, 2 channel", (test_gaussian_blur<3, cv::BORDER_WRAP, 2>), (exec_gaussian_blur<3, cv::BORDER_WRAP>)),
    TEST("Gaussian blur 3x3, BORDER_WRAP, 3 channel", (test_gaussian_blur<3, cv::BORDER_WRAP, 3>), (exec_gaussian_blur<3, cv::BORDER_WRAP>)),
    TEST("Gaussian blur 3x3, BORDER_WRAP, 4 channel", (test_gaussian_blur<3, cv::BORDER_WRAP, 4>), (exec_gaussian_blur<3, cv::BORDER_WRAP>)),

    TEST("Gaussian blur 3x3, BORDER_REPLICATE, 1 channel", (test_gaussian_blur<3, cv::BORDER_REPLICATE, 1>), (exec_gaussian_blur<3, cv::BORDER_REPLICATE>)),
    TEST("Gaussian blur 3x3, BORDER_REPLICATE, 2 channel", (test_gaussian_blur<3, cv::BORDER_REPLICATE, 2>), (exec_gaussian_blur<3, cv::BORDER_REPLICATE>)),
    TEST("Gaussian blur 3x3, BORDER_REPLICATE, 3 channel", (test_gaussian_blur<3, cv::BORDER_REPLICATE, 3>), (exec_gaussian_blur<3, cv::BORDER_REPLICATE>)),
    TEST("Gaussian blur 3x3, BORDER_REPLICATE, 4 channel", (test_gaussian_blur<3, cv::BORDER_REPLICATE, 4>), (exec_gaussian_blur<3, cv::BORDER_REPLICATE>)),

    TEST("Gaussian blur 5x5, BORDER_REFLECT_101, 1 channel", (test_gaussian_blur<5, cv::BORDER_REFLECT_101, 1>), (exec_gaussian_blur<5, cv::BORDER_REFLECT_101>)),
    TEST("Gaussian blur 5x5, BORDER_REFLECT_101, 2 channel", (test_gaussian_blur<5, cv::BORDER_REFLECT_101, 2>), (exec_gaussian_blur<5, cv::BORDER_REFLECT_101>)),
    TEST("Gaussian blur 5x5, BORDER_REFLECT_101, 3 channel", (test_gaussian_blur<5, cv::BORDER_REFLECT_101, 3>), (exec_gaussian_blur<5, cv::BORDER_REFLECT_101>)),
    TEST("Gaussian blur 5x5, BORDER_REFLECT_101, 4 channel", (test_gaussian_blur<5, cv::BORDER_REFLECT_101, 4>), (exec_gaussian_blur<5, cv::BORDER_REFLECT_101>)),

    TEST("Gaussian blur 5x5, BORDER_REFLECT, 1 channel", (test_gaussian_blur<5, cv::BORDER_REFLECT, 1>), (exec_gaussian_blur<5, cv::BORDER_REFLECT>)),
    TEST("Gaussian blur 5x5, BORDER_REFLECT, 2 channel", (test_gaussian_blur<5, cv::BORDER_REFLECT, 2>), (exec_gaussian_blur<5, cv::BORDER_REFLECT>)),
    TEST("Gaussian blur 5x5, BORDER_REFLECT, 3 channel", (test_gaussian_blur<5, cv::BORDER_REFLECT, 3>), (exec_gaussian_blur<5, cv::BORDER_REFLECT>)),
    TEST("Gaussian blur 5x5, BORDER_REFLECT, 4 channel", (test_gaussian_blur<5, cv::BORDER_REFLECT, 4>), (exec_gaussian_blur<5, cv::BORDER_REFLECT>)),

    TEST("Gaussian blur 5x5, BORDER_WRAP, 1 channel", (test_gaussian_blur<5, cv::BORDER_WRAP, 1>), (exec_gaussian_blur<5, cv::BORDER_WRAP>)),
    TEST("Gaussian blur 5x5, BORDER_WRAP, 2 channel", (test_gaussian_blur<5, cv::BORDER_WRAP, 2>), (exec_gaussian_blur<5, cv::BORDER_WRAP>)),
    TEST("Gaussian blur 5x5, BORDER_WRAP, 3 channel", (test_gaussian_blur<5, cv::BORDER_WRAP, 3>), (exec_gaussian_blur<5, cv::BORDER_WRAP>)),
    TEST("Gaussian blur 5x5, BORDER_WRAP, 4 channel", (test_gaussian_blur<5, cv::BORDER_WRAP, 4>), (exec_gaussian_blur<5, cv::BORDER_WRAP>)),

    TEST("Gaussian blur 5x5, BORDER_REPLICATE, 1 channel", (test_gaussian_blur<5, cv::BORDER_REPLICATE, 1>), (exec_gaussian_blur<5, cv::BORDER_REPLICATE>)),
    TEST("Gaussian blur 5x5, BORDER_REPLICATE, 2 channel", (test_gaussian_blur<5, cv::BORDER_REPLICATE, 2>), (exec_gaussian_blur<5, cv::BORDER_REPLICATE>)),
    TEST("Gaussian blur 5x5, BORDER_REPLICATE, 3 channel", (test_gaussian_blur<5, cv::BORDER_REPLICATE, 3>), (exec_gaussian_blur<5, cv::BORDER_REPLICATE>)),
    TEST("Gaussian blur 5x5, BORDER_REPLICATE, 4 channel", (test_gaussian_blur<5, cv::BORDER_REPLICATE, 4>), (exec_gaussian_blur<5, cv::BORDER_REPLICATE>)),

    TEST("Gaussian blur 7x7, BORDER_REFLECT_101, 1 channel", (test_gaussian_blur<7, cv::BORDER_REFLECT_101, 1>), (exec_gaussian_blur<7, cv::BORDER_REFLECT_101>)),
    TEST("Gaussian blur 7x7, BORDER_REFLECT_101, 2 channel", (test_gaussian_blur<7, cv::BORDER_REFLECT_101, 2>), (exec_gaussian_blur<7, cv::BORDER_REFLECT_101>)),
    TEST("Gaussian blur 7x7, BORDER_REFLECT_101, 3 channel", (test_gaussian_blur<7, cv::BORDER_REFLECT_101, 3>), (exec_gaussian_blur<7, cv::BORDER_REFLECT_101>)),
    TEST("Gaussian blur 7x7, BORDER_REFLECT_101, 4 channel", (test_gaussian_blur<7, cv::BORDER_REFLECT_101, 4>), (exec_gaussian_blur<7, cv::BORDER_REFLECT_101>)),

    TEST("Gaussian blur 7x7, BORDER_REFLECT, 1 channel", (test_gaussian_blur<7, cv::BORDER_REFLECT, 1>), (exec_gaussian_blur<7, cv::BORDER_REFLECT>)),
    TEST("Gaussian blur 7x7, BORDER_REFLECT, 2 channel", (test_gaussian_blur<7, cv::BORDER_REFLECT, 2>), (exec_gaussian_blur<7, cv::BORDER_REFLECT>)),
    TEST("Gaussian blur 7x7, BORDER_REFLECT, 3 channel", (test_gaussian_blur<7, cv::BORDER_REFLECT, 3>), (exec_gaussian_blur<7, cv::BORDER_REFLECT>)),
    TEST("Gaussian blur 7x7, BORDER_REFLECT, 4 channel", (test_gaussian_blur<7, cv::BORDER_REFLECT, 4>), (exec_gaussian_blur<7, cv::BORDER_REFLECT>)),

    TEST("Gaussian blur 7x7, BORDER_WRAP, 1 channel", (test_gaussian_blur<7, cv::BORDER_WRAP, 1>), (exec_gaussian_blur<7, cv::BORDER_WRAP>)),
    TEST("Gaussian blur 7x7, BORDER_WRAP, 2 channel", (test_gaussian_blur<7, cv::BORDER_WRAP, 2>), (exec_gaussian_blur<7, cv::BORDER_WRAP>)),
    TEST("Gaussian blur 7x7, BORDER_WRAP, 3 channel", (test_gaussian_blur<7, cv::BORDER_WRAP, 3>), (exec_gaussian_blur<7, cv::BORDER_WRAP>)),
    TEST("Gaussian blur 7x7, BORDER_WRAP, 4 channel", (test_gaussian_blur<7, cv::BORDER_WRAP, 4>), (exec_gaussian_blur<7, cv::BORDER_WRAP>)),

    TEST("Gaussian blur 7x7, BORDER_REPLICATE, 1 channel", (test_gaussian_blur<7, cv::BORDER_REPLICATE, 1>), (exec_gaussian_blur<7, cv::BORDER_REPLICATE>)),
    TEST("Gaussian blur 7x7, BORDER_REPLICATE, 2 channel", (test_gaussian_blur<7, cv::BORDER_REPLICATE, 2>), (exec_gaussian_blur<7, cv::BORDER_REPLICATE>)),
    TEST("Gaussian blur 7x7, BORDER_REPLICATE, 3 channel", (test_gaussian_blur<7, cv::BORDER_REPLICATE, 3>), (exec_gaussian_blur<7, cv::BORDER_REPLICATE>)),
    TEST("Gaussian blur 7x7, BORDER_REPLICATE, 4 channel", (test_gaussian_blur<7, cv::BORDER_REPLICATE, 4>), (exec_gaussian_blur<7, cv::BORDER_REPLICATE>)),
  };
  // clang-format on
  return tests;
}
