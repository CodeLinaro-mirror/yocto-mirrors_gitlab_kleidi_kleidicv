// SPDX-FileCopyrightText: 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#include <algorithm>
#include <vector>

#include "opencv2/video/tracking.hpp"
#include "tests.h"

#if MANAGER
#include <cstddef>
#include <cstdint>

extern "C" {

typedef enum {
  KLEIDICV_OK = 0,
} kleidicv_error_t;

typedef struct kleidicv_optical_flow_pyr_lk_pyramid_t_
    kleidicv_optical_flow_pyr_lk_pyramid_t;
typedef kleidicv_error_t (*kleidicv_thread_callback)(unsigned begin,
                                                     unsigned end, void* data);
typedef kleidicv_error_t (*kleidicv_thread_parallel)(
    kleidicv_thread_callback callback, void* callback_data, void* parallel_data,
    unsigned task_count);
typedef struct {
  kleidicv_thread_parallel parallel;
  void* parallel_data;
} kleidicv_thread_multithreading;

kleidicv_error_t kleidicv_build_optical_flow_pyr_lk_pyramid(
    kleidicv_optical_flow_pyr_lk_pyramid_t** pyramid, const uint8_t* src,
    size_t src_stride, size_t width, size_t height, size_t channels,
    size_t level_count, size_t window_width, size_t window_height);
kleidicv_error_t kleidicv_thread_build_optical_flow_pyr_lk_pyramid(
    kleidicv_optical_flow_pyr_lk_pyramid_t** pyramid, const uint8_t* src,
    size_t src_stride, size_t width, size_t height, size_t channels,
    size_t level_count, size_t window_width, size_t window_height,
    kleidicv_thread_multithreading mt);

kleidicv_error_t kleidicv_optical_flow_pyr_lk_pyramid_release(
    kleidicv_optical_flow_pyr_lk_pyramid_t* pyramid);

kleidicv_error_t kleidicv_optical_flow_pyr_lk_pyramid_get_level_count(
    const kleidicv_optical_flow_pyr_lk_pyramid_t* pyramid, size_t* level_count);

kleidicv_error_t kleidicv_optical_flow_pyr_lk_pyramid_get_image_level(
    const kleidicv_optical_flow_pyr_lk_pyramid_t* pyramid, size_t level,
    const uint8_t** data, size_t* stride, size_t* width, size_t* height);

kleidicv_error_t kleidicv_optical_flow_pyr_lk_pyramid_get_scharr_level(
    const kleidicv_optical_flow_pyr_lk_pyramid_t* pyramid, size_t level,
    const int16_t** data, size_t* stride, size_t* width, size_t* height);

}  // extern "C"
#endif

// Conformity tests in this file validate the build APIs:
// - kleidicv_build_optical_flow_pyr_lk_pyramid
// - kleidicv_thread_build_optical_flow_pyr_lk_pyramid
cv::Mat exec_build_optical_flow_pyr_lk_pyramid(cv::Mat& input) {
  // Subordinate is unused by this test runner. Return deterministic output.
  return input.clone();
}

#if MANAGER
static kleidicv_error_t test_thread_parallel(kleidicv_thread_callback callback,
                                             void* callback_data,
                                             void* parallel_data,
                                             unsigned task_count) {
  const auto thread_count = std::max<unsigned>(
      1U, static_cast<unsigned>(reinterpret_cast<uintptr_t>(parallel_data)));
  const auto task_batch_size =
      std::max<unsigned>(1U, (task_count + thread_count - 1U) / thread_count);

  for (unsigned begin = 0; begin < task_count; begin += task_batch_size) {
    const unsigned end = std::min(task_count, begin + task_batch_size);
    kleidicv_error_t err = callback(begin, end, callback_data);
    if (err != KLEIDICV_OK) {
      return err;
    }
  }
  return KLEIDICV_OK;
}

static bool compare_pyramid_against_expected(
    kleidicv_optical_flow_pyr_lk_pyramid_t* actual_pyramid,
    const std::vector<cv::Mat>& expected_pyramid, const cv::Mat& input,
    size_t width, size_t height, size_t channels, cv::Size win_size,
    size_t max_level) {
  size_t actual_levels = 0;
  kleidicv_error_t err = kleidicv_optical_flow_pyr_lk_pyramid_get_level_count(
      actual_pyramid, &actual_levels);
  if (err != KLEIDICV_OK) {
    std::cout << "kleidicv_optical_flow_pyr_lk_pyramid_get_level_count "
                 "failed: "
              << err << " (window=" << win_size.width << "x" << win_size.height
              << ", image=" << width << "x" << height
              << ", channels=" << channels << ", max_level=" << max_level << ")"
              << std::endl;
    return true;
  }

  const size_t expected_levels = expected_pyramid.size() / 2;
  if (actual_levels != expected_levels) {
    std::cout << "Level count mismatch: actual=" << actual_levels
              << " expected=" << expected_levels
              << " (window=" << win_size.width << "x" << win_size.height
              << ", image=" << width << "x" << height
              << ", channels=" << channels << ", max_level=" << max_level << ")"
              << std::endl;
    return true;
  }

  for (size_t level = 0; level < actual_levels; ++level) {
    const uint8_t* image_data = nullptr;
    size_t image_stride = 0;
    size_t image_width = 0;
    size_t image_height = 0;
    err = kleidicv_optical_flow_pyr_lk_pyramid_get_image_level(
        actual_pyramid, level, &image_data, &image_stride, &image_width,
        &image_height);
    if (err != KLEIDICV_OK) {
      std::cout << "get_image_level failed on level " << level << ": " << err
                << " (window=" << win_size.width << "x" << win_size.height
                << ", image=" << width << "x" << height
                << ", channels=" << channels << ", max_level=" << max_level
                << ")" << std::endl;
      return true;
    }

    const int image_type = CV_MAKETYPE(CV_8U, static_cast<int>(channels));
    cv::Mat actual_image(static_cast<int>(image_height),
                         static_cast<int>(image_width), image_type,
                         const_cast<uint8_t*>(image_data), image_stride);
    cv::Mat expected_image = expected_pyramid[level * 2];
    if (are_matrices_different<uint8_t>(0, actual_image, expected_image)) {
      std::cout << "Image pyramid mismatch at level " << level
                << " (window=" << win_size.width << "x" << win_size.height
                << ", image=" << width << "x" << height
                << ", channels=" << channels << ", max_level=" << max_level
                << ")" << std::endl;
      fail_print_matrices(height, width, const_cast<cv::Mat&>(input),
                          actual_image, expected_image);
      return true;
    }

    const int16_t* scharr_data = nullptr;
    size_t scharr_stride = 0;
    size_t scharr_width = 0;
    size_t scharr_height = 0;
    err = kleidicv_optical_flow_pyr_lk_pyramid_get_scharr_level(
        actual_pyramid, level, &scharr_data, &scharr_stride, &scharr_width,
        &scharr_height);
    if (err != KLEIDICV_OK) {
      std::cout << "get_scharr_level failed on level " << level << ": " << err
                << " (window=" << win_size.width << "x" << win_size.height
                << ", image=" << width << "x" << height
                << ", channels=" << channels << ", max_level=" << max_level
                << ")" << std::endl;
      return true;
    }

    const int scharr_type = CV_MAKETYPE(CV_16S, static_cast<int>(channels * 2));
    cv::Mat actual_scharr(static_cast<int>(scharr_height),
                          static_cast<int>(scharr_width), scharr_type,
                          const_cast<int16_t*>(scharr_data), scharr_stride);
    cv::Mat expected_scharr = expected_pyramid[level * 2 + 1];
    if (are_matrices_different<int16_t>(0, actual_scharr, expected_scharr)) {
      std::cout << "Scharr pyramid mismatch at level " << level
                << " (window=" << win_size.width << "x" << win_size.height
                << ", image=" << width << "x" << height
                << ", channels=" << channels << ", max_level=" << max_level
                << ")" << std::endl;
      fail_print_matrices(height, width, const_cast<cv::Mat&>(input),
                          actual_scharr, expected_scharr);
      return true;
    }
  }

  return false;
}

static bool run_build_optical_flow_case(
    cv::Size win_size, size_t max_level, size_t channels, int /* index */,
    RecreatedMessageQueue& /* request_queue */,
    RecreatedMessageQueue& /* reply_queue */) {
  cv::RNG rng(0);
  const kleidicv_thread_multithreading mt{
      test_thread_parallel, reinterpret_cast<void*>(uintptr_t{4})};

  for (size_t height = 16; height <= 64; height += 8) {
    for (size_t width = 16; width <= 64; width += 8) {
      const int image_type = CV_MAKETYPE(CV_8U, static_cast<int>(channels));
      cv::Mat input(static_cast<int>(height), static_cast<int>(width),
                    image_type);
      rng.fill(input, cv::RNG::UNIFORM, 0, 255);

      std::vector<cv::Mat> expected_pyramid;
      const int expected_last_level = cv::buildOpticalFlowPyramid(
          input, expected_pyramid, win_size, static_cast<int>(max_level), true);

      kleidicv_optical_flow_pyr_lk_pyramid_t* actual_pyramid = nullptr;
      kleidicv_error_t err = kleidicv_build_optical_flow_pyr_lk_pyramid(
          &actual_pyramid, input.ptr<uint8_t>(0), input.step,
          static_cast<size_t>(input.cols), static_cast<size_t>(input.rows),
          channels, max_level + 1, static_cast<size_t>(win_size.width),
          static_cast<size_t>(win_size.height));
      if (err != KLEIDICV_OK) {
        std::cout << "kleidicv_build_optical_flow_pyr_lk_pyramid failed: "
                  << err << " (window=" << win_size.width << "x"
                  << win_size.height << ", image=" << width << "x" << height
                  << ", channels=" << channels << ", max_level=" << max_level
                  << ")" << std::endl;
        return true;
      }

      const size_t expected_levels =
          static_cast<size_t>(expected_last_level + 1);
      expected_pyramid.resize(expected_levels * 2);
      if (compare_pyramid_against_expected(actual_pyramid, expected_pyramid,
                                           input, width, height, channels,
                                           win_size, max_level)) {
        (void)kleidicv_optical_flow_pyr_lk_pyramid_release(actual_pyramid);
        return true;
      }

      err = kleidicv_optical_flow_pyr_lk_pyramid_release(actual_pyramid);
      if (err != KLEIDICV_OK) {
        std::cout << "release failed: " << err << " (window=" << win_size.width
                  << "x" << win_size.height << ", image=" << width << "x"
                  << height << ", channels=" << channels
                  << ", max_level=" << max_level << ")" << std::endl;
        return true;
      }

      kleidicv_optical_flow_pyr_lk_pyramid_t* threaded_pyramid = nullptr;
      err = kleidicv_thread_build_optical_flow_pyr_lk_pyramid(
          &threaded_pyramid, input.ptr<uint8_t>(0), input.step,
          static_cast<size_t>(input.cols), static_cast<size_t>(input.rows),
          channels, max_level + 1, static_cast<size_t>(win_size.width),
          static_cast<size_t>(win_size.height), mt);
      if (err != KLEIDICV_OK) {
        std::cout << "kleidicv_thread_build_optical_flow_pyr_lk_pyramid "
                     "failed: "
                  << err << " (window=" << win_size.width << "x"
                  << win_size.height << ", image=" << width << "x" << height
                  << ", channels=" << channels << ", max_level=" << max_level
                  << ")" << std::endl;
        return true;
      }

      if (compare_pyramid_against_expected(threaded_pyramid, expected_pyramid,
                                           input, width, height, channels,
                                           win_size, max_level)) {
        (void)kleidicv_optical_flow_pyr_lk_pyramid_release(threaded_pyramid);
        return true;
      }

      err = kleidicv_optical_flow_pyr_lk_pyramid_release(threaded_pyramid);
      if (err != KLEIDICV_OK) {
        std::cout << "threaded release failed: " << err
                  << " (window=" << win_size.width << "x" << win_size.height
                  << ", image=" << width << "x" << height
                  << ", channels=" << channels << ", max_level=" << max_level
                  << ")" << std::endl;
        return true;
      }
    }
  }

  return false;
}

#define DEFINE_BUILD_OPTICAL_FLOW_TEST(win_w, win_h, lvl, ch)                  \
  bool test_build_optical_flow_lk_pyramid_w##win_w##x##win_h##_l##lvl##_c##ch( \
      int index, RecreatedMessageQueue& request_queue,                         \
      RecreatedMessageQueue& reply_queue) {                                    \
    return run_build_optical_flow_case(cv::Size{win_w, win_h}, lvl, ch, index, \
                                       request_queue, reply_queue);            \
  }

#define DEFINE_BUILD_OPTICAL_FLOW_TEST_ALL_CHANNELS(win_w, win_h, lvl) \
  DEFINE_BUILD_OPTICAL_FLOW_TEST(win_w, win_h, lvl, 1)                 \
  DEFINE_BUILD_OPTICAL_FLOW_TEST(win_w, win_h, lvl, 2)                 \
  DEFINE_BUILD_OPTICAL_FLOW_TEST(win_w, win_h, lvl, 3)                 \
  DEFINE_BUILD_OPTICAL_FLOW_TEST(win_w, win_h, lvl, 4)

DEFINE_BUILD_OPTICAL_FLOW_TEST_ALL_CHANNELS(3, 3, 0)
DEFINE_BUILD_OPTICAL_FLOW_TEST_ALL_CHANNELS(3, 3, 1)
DEFINE_BUILD_OPTICAL_FLOW_TEST_ALL_CHANNELS(3, 3, 2)
DEFINE_BUILD_OPTICAL_FLOW_TEST_ALL_CHANNELS(3, 3, 3)
DEFINE_BUILD_OPTICAL_FLOW_TEST_ALL_CHANNELS(3, 3, 4)
DEFINE_BUILD_OPTICAL_FLOW_TEST_ALL_CHANNELS(5, 5, 0)
DEFINE_BUILD_OPTICAL_FLOW_TEST_ALL_CHANNELS(5, 5, 1)
DEFINE_BUILD_OPTICAL_FLOW_TEST_ALL_CHANNELS(5, 5, 2)
DEFINE_BUILD_OPTICAL_FLOW_TEST_ALL_CHANNELS(5, 5, 3)
DEFINE_BUILD_OPTICAL_FLOW_TEST_ALL_CHANNELS(5, 5, 4)
DEFINE_BUILD_OPTICAL_FLOW_TEST_ALL_CHANNELS(9, 9, 0)
DEFINE_BUILD_OPTICAL_FLOW_TEST_ALL_CHANNELS(9, 9, 1)
DEFINE_BUILD_OPTICAL_FLOW_TEST_ALL_CHANNELS(9, 9, 2)
DEFINE_BUILD_OPTICAL_FLOW_TEST_ALL_CHANNELS(9, 9, 3)
DEFINE_BUILD_OPTICAL_FLOW_TEST_ALL_CHANNELS(9, 9, 4)
DEFINE_BUILD_OPTICAL_FLOW_TEST_ALL_CHANNELS(15, 15, 0)
DEFINE_BUILD_OPTICAL_FLOW_TEST_ALL_CHANNELS(15, 15, 1)
DEFINE_BUILD_OPTICAL_FLOW_TEST_ALL_CHANNELS(15, 15, 2)
DEFINE_BUILD_OPTICAL_FLOW_TEST_ALL_CHANNELS(15, 15, 3)
DEFINE_BUILD_OPTICAL_FLOW_TEST_ALL_CHANNELS(15, 15, 4)

#undef DEFINE_BUILD_OPTICAL_FLOW_TEST_ALL_CHANNELS
#undef DEFINE_BUILD_OPTICAL_FLOW_TEST
#endif

std::vector<test>& build_optical_flow_pyr_lk_pyramid_tests_get() {
  // clang-format off
  #define BUILD_OPTICAL_FLOW_TEST_ENTRIES_ALL_CHANNELS(win_w, win_h, lvl)                                              \
    TEST("Build Optical Flow PyrLK Pyramid window " #win_w "x" #win_h ", level " #lvl ", 1 channel",            \
         test_build_optical_flow_lk_pyramid_w##win_w##x##win_h##_l##lvl##_c1, exec_build_optical_flow_pyr_lk_pyramid), \
    TEST("Build Optical Flow PyrLK Pyramid window " #win_w "x" #win_h ", level " #lvl ", 2 channel",            \
         test_build_optical_flow_lk_pyramid_w##win_w##x##win_h##_l##lvl##_c2, exec_build_optical_flow_pyr_lk_pyramid), \
    TEST("Build Optical Flow PyrLK Pyramid window " #win_w "x" #win_h ", level " #lvl ", 3 channel",            \
         test_build_optical_flow_lk_pyramid_w##win_w##x##win_h##_l##lvl##_c3, exec_build_optical_flow_pyr_lk_pyramid), \
    TEST("Build Optical Flow PyrLK Pyramid window " #win_w "x" #win_h ", level " #lvl ", 4 channel",            \
         test_build_optical_flow_lk_pyramid_w##win_w##x##win_h##_l##lvl##_c4, exec_build_optical_flow_pyr_lk_pyramid)

  static std::vector<test> tests = {
    BUILD_OPTICAL_FLOW_TEST_ENTRIES_ALL_CHANNELS(3, 3, 0),
    BUILD_OPTICAL_FLOW_TEST_ENTRIES_ALL_CHANNELS(3, 3, 1),
    BUILD_OPTICAL_FLOW_TEST_ENTRIES_ALL_CHANNELS(3, 3, 2),
    BUILD_OPTICAL_FLOW_TEST_ENTRIES_ALL_CHANNELS(3, 3, 3),
    BUILD_OPTICAL_FLOW_TEST_ENTRIES_ALL_CHANNELS(3, 3, 4),
    BUILD_OPTICAL_FLOW_TEST_ENTRIES_ALL_CHANNELS(5, 5, 0),
    BUILD_OPTICAL_FLOW_TEST_ENTRIES_ALL_CHANNELS(5, 5, 1),
    BUILD_OPTICAL_FLOW_TEST_ENTRIES_ALL_CHANNELS(5, 5, 2),
    BUILD_OPTICAL_FLOW_TEST_ENTRIES_ALL_CHANNELS(5, 5, 3),
    BUILD_OPTICAL_FLOW_TEST_ENTRIES_ALL_CHANNELS(5, 5, 4),
    BUILD_OPTICAL_FLOW_TEST_ENTRIES_ALL_CHANNELS(9, 9, 0),
    BUILD_OPTICAL_FLOW_TEST_ENTRIES_ALL_CHANNELS(9, 9, 1),
    BUILD_OPTICAL_FLOW_TEST_ENTRIES_ALL_CHANNELS(9, 9, 2),
    BUILD_OPTICAL_FLOW_TEST_ENTRIES_ALL_CHANNELS(9, 9, 3),
    BUILD_OPTICAL_FLOW_TEST_ENTRIES_ALL_CHANNELS(9, 9, 4),
    BUILD_OPTICAL_FLOW_TEST_ENTRIES_ALL_CHANNELS(15, 15, 0),
    BUILD_OPTICAL_FLOW_TEST_ENTRIES_ALL_CHANNELS(15, 15, 1),
    BUILD_OPTICAL_FLOW_TEST_ENTRIES_ALL_CHANNELS(15, 15, 2),
    BUILD_OPTICAL_FLOW_TEST_ENTRIES_ALL_CHANNELS(15, 15, 3),
    BUILD_OPTICAL_FLOW_TEST_ENTRIES_ALL_CHANNELS(15, 15, 4)
  };
  #undef BUILD_OPTICAL_FLOW_TEST_ENTRIES_ALL_CHANNELS
  // clang-format on
  return tests;
}
