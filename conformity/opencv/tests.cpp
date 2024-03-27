// SPDX-FileCopyrightText: 2024 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#include "tests.h"

#include <iostream>
#include <string>
#include <utility>
#include <vector>

#include "opencv2/core.hpp"
#include "opencv2/imgproc.hpp"

#if MANAGER

template <typename T>
static auto abs_diff(T a, T b) {
  return a > b ? a - b : b - a;
}

template <typename T>
static bool are_matrices_different(T threshold, cv::Mat& A, cv::Mat& B) {
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

template <bool Vertical, size_t Channels>
bool test_sobel(int index, RecreatedMessageQueue& request_queue,
                RecreatedMessageQueue& reply_queue) {
  cv::RNG rng(0);

  for (size_t x = 5; x <= 16; ++x) {
    for (size_t y = 5; y <= 16; ++y) {
      cv::Mat src(x, y, CV_8UC(Channels));
      rng.fill(src, cv::RNG::UNIFORM, 0, 255);

      cv::Mat manager_result;
      if constexpr (Vertical) {
        cv::Sobel(src, manager_result, CV_16S, 0, 1, 3, 1.0, 0.0,
                  cv::BORDER_REPLICATE);
      } else {
        cv::Sobel(src, manager_result, CV_16S, 1, 0, 3, 1.0, 0.0,
                  cv::BORDER_REPLICATE);
      }

      request_queue.request_operation(index, src);
      reply_queue.wait();
      if (reply_queue.last_cmd() != index) {
        throw std::runtime_error("Invalid reply from subordinate");
      }

      cv::Mat subord_result = reply_queue.cv_mat_from_last_msg();

      if (are_matrices_different<uint8_t>(0, manager_result, subord_result)) {
        std::cout << "[FAIL]" << std::endl;
        std::cout << "height=" << x << std::endl;
        std::cout << "width=" << y << std::endl;
        std::cout << "=== Src Matrix:" << std::endl;
        std::cout << src << std::endl << std::endl;
        std::cout << "=== Manager result:" << std::endl;
        std::cout << manager_result << std::endl << std::endl;
        std::cout << "=== Subordinate result:" << std::endl;
        std::cout << subord_result << std::endl << std::endl;

        return true;
      }
    }
  }

  return false;
}

template <size_t KernelSize, size_t BorderType, size_t Channels>
bool test_gaussian_blur(int index, RecreatedMessageQueue& request_queue,
                        RecreatedMessageQueue& reply_queue) {
  cv::RNG rng(0);
  cv::Size kernel(KernelSize, KernelSize);

  for (size_t x = 5; x <= 16; ++x) {
    for (size_t y = 5; y <= 16; ++y) {
      cv::Mat src(x, y, CV_8UC(Channels));
      rng.fill(src, cv::RNG::UNIFORM, 0, 255);

      cv::Mat manager_result;
      cv::GaussianBlur(src, manager_result, kernel, 0, 0, BorderType);

      request_queue.request_operation(index, src);
      reply_queue.wait();
      if (reply_queue.last_cmd() != index) {
        throw std::runtime_error("Invalid reply from subordinate");
      }

      cv::Mat subord_result = reply_queue.cv_mat_from_last_msg();

      if (are_matrices_different<uint8_t>(0, manager_result, subord_result)) {
        std::cout << "[FAIL]" << std::endl;
        std::cout << "height=" << x << std::endl;
        std::cout << "width=" << y << std::endl;
        std::cout << "=== Src Matrix:" << std::endl;
        std::cout << src << std::endl << std::endl;
        std::cout << "=== Manager result:" << std::endl;
        std::cout << manager_result << std::endl << std::endl;
        std::cout << "=== Subordinate result:" << std::endl;
        std::cout << subord_result << std::endl << std::endl;

        return true;
      }
    }
  }

  return false;
}

using test = std::pair<std::string, decltype(test_sobel<true, 1>)*>;
#define TEST(name, manager_func, subordinate_func) \
  { name, manager_func }

#else  // MANAGER

template <bool Vertical>
cv::Mat exec_sobel(cv::Mat& input) {
  cv::Mat result;
  if constexpr (Vertical) {
    cv::Sobel(input, result, CV_16S, 0, 1, 3, 1.0, 0.0, cv::BORDER_REPLICATE);
  } else {
    cv::Sobel(input, result, CV_16S, 1, 0, 3, 1.0, 0.0, cv::BORDER_REPLICATE);
  }
  return result;
}

template <size_t KernelSize, size_t BorderType>
cv::Mat exec_gaussian_blur(cv::Mat& inp) {
  cv::Size kernel(KernelSize, KernelSize);
  cv::Mat out;
  cv::GaussianBlur(inp, out, kernel, 0, 0, BorderType);
  return out;
}

using test = std::pair<std::string, decltype(exec_sobel<true>)*>;
#define TEST(name, manager_func, subordinate_func) \
  { name, subordinate_func }

#endif  // MANAGER

// clang-format off
std::vector<test> tests = {
  TEST("Sobel Vertical, 1 channel", (test_sobel<true, 1>), exec_sobel<true>),
  TEST("Sobel Vertical, 2 channel", (test_sobel<true, 2>), exec_sobel<true>),
  TEST("Sobel Vertical, 3 channel", (test_sobel<true, 3>), exec_sobel<true>),
  TEST("Sobel Vertical, 4 channel", (test_sobel<true, 4>), exec_sobel<true>),

  TEST("Sobel Horizontal, 1 channel", (test_sobel<false, 1>), exec_sobel<false>),
  TEST("Sobel Horizontal, 2 channel", (test_sobel<false, 2>), exec_sobel<false>),
  TEST("Sobel Horizontal, 3 channel", (test_sobel<false, 3>), exec_sobel<false>),
  TEST("Sobel Horizontal, 4 channel", (test_sobel<false, 4>), exec_sobel<false>),

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
};
// clang-format on

#if MANAGER
int run_tests(RecreatedMessageQueue& request_queue,
              RecreatedMessageQueue& reply_queue) {
  for (int i = 0; i < static_cast<int>(tests.size()); ++i) {
    std::cout << "Testing " + tests[i].first << std::endl;
    if (tests[i].second(i, request_queue, reply_queue)) {
      return 1;
    }
  }
  request_queue.request_exit();

  return 0;
}
#else   // MANAGER
void wait_for_requests(OpenedMessageQueue& request_queue,
                       OpenedMessageQueue& reply_queue) {
  while (true) {
    request_queue.wait();
    int cmd = request_queue.last_cmd();

    if (cmd < 0) {
      // Exit requested
      break;
    }

    if (cmd > static_cast<int>(tests.size())) {
      throw std::runtime_error("Invalid operation requestd in subordinate");
    }

    cv::Mat input = request_queue.cv_mat_from_last_msg();
    cv::Mat result = tests[cmd].second(input);
    reply_queue.reply_operation(cmd, result);
  }
}
#endif  // MANAGER
