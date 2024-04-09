// SPDX-FileCopyrightText: 2024 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#ifndef INTRINSICCV_OPENCV_CONFORMITY_TESTS_H_
#define INTRINSICCV_OPENCV_CONFORMITY_TESTS_H_

#include <iostream>
#include <string>
#include <utility>

#include "common.h"

#if MANAGER
template <typename T>
static auto abs_diff(T a, T b) {
  return a > b ? a - b : b - a;
}

template <typename T>
bool are_matrices_different(T threshold, cv::Mat& A, cv::Mat& B) {
  if (A.rows != B.rows || A.cols != B.cols || A.type() != B.type()) {
    std::cout << "Matrix size/type mismatch" << std::endl;
    return true;
  }

  for (int i = 0; i < A.rows; ++i) {
    for (int j = 0; j < (A.cols * CV_MAT_CN(A.type())); ++j) {
      if (abs_diff<T>(A.at<T>(i, j), B.at<T>(i, j)) > threshold) {
        std::cout << "=== Mismatch at: " << i << " " << j << std::endl
                  << std::endl;
        return true;
      }
    }
  }

  return false;
}

void fail_print_matrices(size_t height, size_t width, cv::Mat& input,
                         cv::Mat& manager_result, cv::Mat& subord_result);

cv::Mat get_expected_from_subordinate(int index,
                                      RecreatedMessageQueue& request_queue,
                                      RecreatedMessageQueue& reply_queue,
                                      cv::Mat& input);

int run_tests(RecreatedMessageQueue& request_queue,
              RecreatedMessageQueue& reply_queue);

typedef bool (*test_function)(int index, RecreatedMessageQueue& request_queue,
                              RecreatedMessageQueue& reply_queue);
using test = std::pair<std::string, test_function>;
#define TEST(name, test_func, x) \
  { name, test_func }
#else
void wait_for_requests(OpenedMessageQueue& request_queue,
                       OpenedMessageQueue& reply_queue);

typedef cv::Mat (*exec_function)(cv::Mat& input);
using test = std::pair<std::string, exec_function>;
#define TEST(name, x, exec_func) \
  { name, exec_func }
#endif

#endif  // INTRINSICCV_OPENCV_CONFORMITY_TESTS_H_
