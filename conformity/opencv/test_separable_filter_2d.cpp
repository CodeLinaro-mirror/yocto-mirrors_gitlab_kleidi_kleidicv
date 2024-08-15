// SPDX-FileCopyrightText: 2024 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#include <vector>

#include "tests.h"

template <typename TypeParam, size_t KernelSize, size_t BorderType>
cv::Mat exec_separable_filter_2d(cv::Mat& input) {
  uint32_t kernel_seed =
      *reinterpret_cast<uint32_t*>(&input.at<TypeParam>(input.rows - 1, 0));
  // clone is required, otherwise the result matrix is treated as part of a
  // bigger image, and it would have impact on what border types are supported
  cv::Mat input_mat = input.rowRange(0, input.rows - 1).clone();

  cv::RNG rng(kernel_seed);
  cv::Mat kernel_x(KernelSize, 1, CV_8UC1);
  rng.fill(kernel_x, cv::RNG::UNIFORM, 0, 5);
  cv::Mat kernel_y(KernelSize, 1, CV_8UC1);
  rng.fill(kernel_y, cv::RNG::UNIFORM, 0, 5);

  cv::Mat result;
  cv::sepFilter2D(input_mat, result, -1, kernel_x, kernel_y, cv::Point(-1, -1),
                  0, BorderType);
  return result;
}

#if MANAGER
template <typename TypeParam, size_t KernelSize, size_t BorderType,
          size_t Channels>
bool test_separable_filter_2d(int index, RecreatedMessageQueue& request_queue,
                              RecreatedMessageQueue& reply_queue) {
  cv::RNG rng(0);

  for (size_t y = 5; y <= 16; ++y) {
    for (size_t x = 5; x <= 16; ++x) {
      // One extra line allocated to be sure the kernel seed can be placed next
      // to the real input
      cv::Mat input(y + 1, x, get_opencv_matrix_type<TypeParam, Channels>());
      rng.fill(input, cv::RNG::UNIFORM, 0, 7);

      uint32_t kernel_seed = rng.next();

      // kernel seed is embedded into the input matrix
      *reinterpret_cast<uint32_t*>(&input.at<TypeParam>(input.rows - 1, 0)) =
          kernel_seed;

      cv::Mat actual =
          exec_separable_filter_2d<TypeParam, KernelSize, BorderType>(input);
      cv::Mat expected = get_expected_from_subordinate(index, request_queue,
                                                       reply_queue, input);

      if (are_matrices_different<TypeParam>(0, actual, expected)) {
        fail_print_matrices(y, x, input, actual, expected);
        return true;
      }
    }
  }

  return false;
}
#endif

std::vector<test>& separable_filter_2d_tests_get() {
  // clang-format off
  static std::vector<test> tests = {
    TEST("Separable Filter 2D 5x5 (u8), BORDER_REFLECT_101, 1 channel", (test_separable_filter_2d<uint8_t, 5, cv::BORDER_REFLECT_101, 1>), (exec_separable_filter_2d<uint8_t, 5, cv::BORDER_REFLECT_101>)),
    TEST("Separable Filter 2D 5x5 (u8), BORDER_REFLECT_101, 2 channel", (test_separable_filter_2d<uint8_t, 5, cv::BORDER_REFLECT_101, 2>), (exec_separable_filter_2d<uint8_t, 5, cv::BORDER_REFLECT_101>)),
    TEST("Separable Filter 2D 5x5 (u8), BORDER_REFLECT_101, 3 channel", (test_separable_filter_2d<uint8_t, 5, cv::BORDER_REFLECT_101, 3>), (exec_separable_filter_2d<uint8_t, 5, cv::BORDER_REFLECT_101>)),
    TEST("Separable Filter 2D 5x5 (u8), BORDER_REFLECT_101, 4 channel", (test_separable_filter_2d<uint8_t, 5, cv::BORDER_REFLECT_101, 4>), (exec_separable_filter_2d<uint8_t, 5, cv::BORDER_REFLECT_101>)),

    TEST("Separable Filter 2D 5x5 (u8), BORDER_REFLECT, 1 channel", (test_separable_filter_2d<uint8_t, 5, cv::BORDER_REFLECT, 1>), (exec_separable_filter_2d<uint8_t, 5, cv::BORDER_REFLECT>)),
    TEST("Separable Filter 2D 5x5 (u8), BORDER_REFLECT, 2 channel", (test_separable_filter_2d<uint8_t, 5, cv::BORDER_REFLECT, 2>), (exec_separable_filter_2d<uint8_t, 5, cv::BORDER_REFLECT>)),
    TEST("Separable Filter 2D 5x5 (u8), BORDER_REFLECT, 3 channel", (test_separable_filter_2d<uint8_t, 5, cv::BORDER_REFLECT, 3>), (exec_separable_filter_2d<uint8_t, 5, cv::BORDER_REFLECT>)),
    TEST("Separable Filter 2D 5x5 (u8), BORDER_REFLECT, 4 channel", (test_separable_filter_2d<uint8_t, 5, cv::BORDER_REFLECT, 4>), (exec_separable_filter_2d<uint8_t, 5, cv::BORDER_REFLECT>)),

    TEST("Separable Filter 2D 5x5 (u8), BORDER_REPLICATE, 1 channel", (test_separable_filter_2d<uint8_t, 5, cv::BORDER_REPLICATE, 1>), (exec_separable_filter_2d<uint8_t, 5, cv::BORDER_REPLICATE>)),
    TEST("Separable Filter 2D 5x5 (u8), BORDER_REPLICATE, 2 channel", (test_separable_filter_2d<uint8_t, 5, cv::BORDER_REPLICATE, 2>), (exec_separable_filter_2d<uint8_t, 5, cv::BORDER_REPLICATE>)),
    TEST("Separable Filter 2D 5x5 (u8), BORDER_REPLICATE, 3 channel", (test_separable_filter_2d<uint8_t, 5, cv::BORDER_REPLICATE, 3>), (exec_separable_filter_2d<uint8_t, 5, cv::BORDER_REPLICATE>)),
    TEST("Separable Filter 2D 5x5 (u8), BORDER_REPLICATE, 4 channel", (test_separable_filter_2d<uint8_t, 5, cv::BORDER_REPLICATE, 4>), (exec_separable_filter_2d<uint8_t, 5, cv::BORDER_REPLICATE>)),

    TEST("Separable Filter 2D 5x5 (u16), BORDER_REFLECT_101, 1 channel", (test_separable_filter_2d<uint16_t, 5, cv::BORDER_REFLECT_101, 1>), (exec_separable_filter_2d<uint16_t, 5, cv::BORDER_REFLECT_101>)),
    TEST("Separable Filter 2D 5x5 (u16), BORDER_REFLECT_101, 2 channel", (test_separable_filter_2d<uint16_t, 5, cv::BORDER_REFLECT_101, 2>), (exec_separable_filter_2d<uint16_t, 5, cv::BORDER_REFLECT_101>)),
    TEST("Separable Filter 2D 5x5 (u16), BORDER_REFLECT_101, 3 channel", (test_separable_filter_2d<uint16_t, 5, cv::BORDER_REFLECT_101, 3>), (exec_separable_filter_2d<uint16_t, 5, cv::BORDER_REFLECT_101>)),
    TEST("Separable Filter 2D 5x5 (u16), BORDER_REFLECT_101, 4 channel", (test_separable_filter_2d<uint16_t, 5, cv::BORDER_REFLECT_101, 4>), (exec_separable_filter_2d<uint16_t, 5, cv::BORDER_REFLECT_101>)),

    TEST("Separable Filter 2D 5x5 (u16), BORDER_REFLECT, 1 channel", (test_separable_filter_2d<uint16_t, 5, cv::BORDER_REFLECT, 1>), (exec_separable_filter_2d<uint16_t, 5, cv::BORDER_REFLECT>)),
    TEST("Separable Filter 2D 5x5 (u16), BORDER_REFLECT, 2 channel", (test_separable_filter_2d<uint16_t, 5, cv::BORDER_REFLECT, 2>), (exec_separable_filter_2d<uint16_t, 5, cv::BORDER_REFLECT>)),
    TEST("Separable Filter 2D 5x5 (u16), BORDER_REFLECT, 3 channel", (test_separable_filter_2d<uint16_t, 5, cv::BORDER_REFLECT, 3>), (exec_separable_filter_2d<uint16_t, 5, cv::BORDER_REFLECT>)),
    TEST("Separable Filter 2D 5x5 (u16), BORDER_REFLECT, 4 channel", (test_separable_filter_2d<uint16_t, 5, cv::BORDER_REFLECT, 4>), (exec_separable_filter_2d<uint16_t, 5, cv::BORDER_REFLECT>)),

    TEST("Separable Filter 2D 5x5 (u16), BORDER_REPLICATE, 1 channel", (test_separable_filter_2d<uint16_t, 5, cv::BORDER_REPLICATE, 1>), (exec_separable_filter_2d<uint16_t, 5, cv::BORDER_REPLICATE>)),
    TEST("Separable Filter 2D 5x5 (u16), BORDER_REPLICATE, 2 channel", (test_separable_filter_2d<uint16_t, 5, cv::BORDER_REPLICATE, 2>), (exec_separable_filter_2d<uint16_t, 5, cv::BORDER_REPLICATE>)),
    TEST("Separable Filter 2D 5x5 (u16), BORDER_REPLICATE, 3 channel", (test_separable_filter_2d<uint16_t, 5, cv::BORDER_REPLICATE, 3>), (exec_separable_filter_2d<uint16_t, 5, cv::BORDER_REPLICATE>)),
    TEST("Separable Filter 2D 5x5 (u16), BORDER_REPLICATE, 4 channel", (test_separable_filter_2d<uint16_t, 5, cv::BORDER_REPLICATE, 4>), (exec_separable_filter_2d<uint16_t, 5, cv::BORDER_REPLICATE>)),
  };
  // clang-format on
  return tests;
}
