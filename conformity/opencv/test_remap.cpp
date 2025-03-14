// SPDX-FileCopyrightText: 2024 - 2025 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#include <limits>
#include <type_traits>
#include <vector>

#include "opencv2/core/hal/interface.h"
#include "opencv2/imgproc/hal/interface.h"
#include "tests.h"

const int kMaxHeight = 36, kMaxWidth = 32;

template <class ScalarType, int Channels>
cv::Mat get_source_mat(int format) {
  auto generate_source = [&]() {
    cv::Mat m(kMaxHeight, kMaxWidth, format);
    const int64_t kMaxValue = std::numeric_limits<ScalarType>::max();
    for (size_t row = 0; row < kMaxHeight; ++row) {
      for (size_t column = 0; column < kMaxWidth; ++column) {
        // Create as many different differences between neighbouring pixels as
        // possible
        cv::Vec<ScalarType, Channels> pixel_value;
        for (size_t ch = 0; ch < Channels; ++ch) {
          size_t counter = row + column + ch;
          pixel_value[ch] =
              (counter % 2) ? kMaxValue : (counter % (kMaxValue + 1));
        }
        m.at<cv::Vec<ScalarType, Channels>>(row, column) = pixel_value;
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
cv::Mat exec_remap_s16(cv::Mat& mapxy_mat) {
  cv::Mat source_mat = get_source_mat<ScalarType, CV_MAT_CN(Format)>(Format);
  cv::Mat result(mapxy_mat.rows, mapxy_mat.cols, Format);
  cv::Mat empty;
  remap(source_mat, result, mapxy_mat, empty, Interpolation, BorderMode,
        BorderValue / 1000.0);
  return result;
}

#if MANAGER
template <class ScalarType, int Format, int Interpolation, int BorderMode,
          int BorderValue>
bool test_remap_s16(int index, RecreatedMessageQueue& request_queue,
                    RecreatedMessageQueue& reply_queue) {
  cv::Mat source_mat = get_source_mat<ScalarType, CV_MAT_CN(Format)>(Format);
  cv::RNG rng(0);

  for (size_t w = 5; w <= kMaxWidth; w += 3) {
    for (size_t h = 5; h <= kMaxHeight; h += 2) {
      cv::Mat source_mat =
          get_source_mat<ScalarType, CV_MAT_CN(Format)>(Format);
      cv::Mat mapxy_mat(w, h, CV_16SC2);
      rng.fill(mapxy_mat, cv::RNG::UNIFORM, -3, kMaxWidth + 3);

      cv::Mat actual_mat = exec_remap_s16<ScalarType, Format, Interpolation,
                                          BorderMode, BorderValue>(mapxy_mat);
      cv::Mat expected_mat = get_expected_from_subordinate(
          index, request_queue, reply_queue, mapxy_mat);

      bool success =
          (CV_MAT_DEPTH(Format) == CV_8U &&
           !are_matrices_different<uint8_t>(0, actual_mat, expected_mat)) ||
          (CV_MAT_DEPTH(Format) == CV_16U &&
           !are_matrices_different<uint16_t>(0, actual_mat, expected_mat));
      if (!success) {
        fail_print_matrices(w, h, source_mat, actual_mat, expected_mat);
        return true;
      }
    }
  }
  return false;
}
#endif

// BorderValue is interpreted as 1/1000, i.e. 500 for 0.5
template <class ScalarType, int Format, int Interpolation, int BorderMode,
          int BorderValue>
cv::Mat exec_remap_s16point5(cv::Mat& map_mat) {
  cv::Mat empty;
  // integer part is 16SC2, twice as much data as the fractional part, 16UC1
  int height = map_mat.rows * 2 / 3;
  cv::Mat mapxy_mat = map_mat.rowRange(0, height);
  ushort* p_frac = map_mat.rowRange(height, map_mat.rows).ptr<ushort>();
  cv::Mat mapfrac_mat{height, map_mat.cols, CV_16UC1, p_frac};
  cv::Mat result(mapxy_mat.rows, mapxy_mat.cols, Format);
  cv::Mat source_mat = get_source_mat<ScalarType, CV_MAT_CN(Format)>(Format);
  remap(source_mat, result, mapxy_mat, mapfrac_mat, Interpolation, BorderMode,
        BorderValue / 1000.0);
  return result;
}

#if MANAGER
template <class ScalarType, int Format, int Interpolation, int BorderMode,
          int BorderValue>
bool test_remap_s16point5(int index, RecreatedMessageQueue& request_queue,
                          RecreatedMessageQueue& reply_queue) {
  cv::Mat source_mat = get_source_mat<ScalarType, CV_MAT_CN(Format)>(Format);
  cv::RNG rng(0);

  for (int w = 5; w <= kMaxWidth; ++w) {
    for (int h = 6; h <= kMaxHeight; h += 2) {  // h must be even
      cv::Mat map_mat(h * 3 / 2, w, CV_16SC2);
      cv::Mat mapxy_mat = map_mat.rowRange(0, h);
      ushort* p_frac = map_mat.rowRange(h, map_mat.rows).ptr<ushort>();
      cv::Mat mapfrac_mat{h, map_mat.cols, CV_16UC1, p_frac};
      rng.fill(mapxy_mat, cv::RNG::UNIFORM, -3, kMaxWidth + 3);
      // Test out of range fractional part too
      rng.fill(mapfrac_mat, cv::RNG::UNIFORM, 0, cv::INTER_TAB_SIZE2 * 3 / 2);

      cv::Mat actual_mat =
          exec_remap_s16point5<ScalarType, Format, Interpolation, BorderMode,
                               BorderValue>(map_mat);
      cv::Mat expected_mat = get_expected_from_subordinate(
          index, request_queue, reply_queue, map_mat);

      bool success =
          (CV_MAT_DEPTH(Format) == CV_8U &&
           !are_matrices_different<uint8_t>(1, actual_mat, expected_mat)) ||
          (CV_MAT_DEPTH(Format) == CV_16U &&
           !are_matrices_different<uint16_t>(1, actual_mat, expected_mat));
      if (!success) {
        fail_print_matrices(w, h, source_mat, actual_mat, expected_mat);
        return true;
      }
    }
  }
  return false;
}
#endif

// BorderValue is interpreted as 1/1000, i.e. 500 for 0.5
template <class ScalarType, int Format, int Interpolation, int BorderMode,
          int BorderValue>
cv::Mat exec_remap_f32(cv::Mat& mapxy_mat) {
  cv::Mat source_mat = get_source_mat<ScalarType, CV_MAT_CN(Format)>(Format);
  cv::Mat result(mapxy_mat.rows, mapxy_mat.cols, Format);

  cv::Mat mapx_mat = mapxy_mat.rowRange(0, mapxy_mat.rows / 2);
  cv::Mat mapy_mat = mapxy_mat.rowRange(mapxy_mat.rows / 2, mapxy_mat.rows);

  remap(source_mat, result, mapx_mat, mapy_mat, Interpolation, BorderMode,
        BorderValue / 1000.0);
  return result;
}

#if MANAGER
template <class ScalarType, int Format, int Interpolation, int BorderMode,
          int BorderValue>
bool test_remap_f32(int index, RecreatedMessageQueue& request_queue,
                    RecreatedMessageQueue& reply_queue) {
  cv::Mat source_mat = get_source_mat<ScalarType, CV_MAT_CN(Format)>(Format);
  cv::RNG rng(0);

  for (size_t w = 5; w <= kMaxWidth * 2; w += 3) {
    for (size_t h = 5; h <= kMaxHeight * 2; h += 2) {
      cv::Mat map_mat(h * 2, w, CV_32FC1);
      cv::Mat mapx_mat = map_mat.rowRange(0, h);
      cv::Mat mapy_mat = map_mat.rowRange(h, map_mat.rows);
      for (size_t y = 0; y < h; ++y) {
        for (size_t x = 0; x < w; ++x) {
          // Values from -0.49 to 0.49, so exactly 0.5 is excluded

          // Reason: When rounding floating point values to integer, OpenCV does
          // scalar rounding that works differently based on the rounding
          // environment. E.g. it can use "Rounding to nearest, ties to even",
          // while KleidiCV always uses "Rounding to nearest, towards plus
          // infinity". To prevent these differences, values with exactly 0.5
          // fractional part are excluded.
          float divisor = (1.01 * 0x1p32);
          float epsilon = 0x1p-16;
          float fractionX = rng.next() / divisor - 0.5F + epsilon;
          float fractionY = rng.next() / divisor - 0.5F + epsilon;
          mapx_mat.at<float>(y, x) =
              (static_cast<int32_t>(rng.next() % (kMaxWidth + 6)) - 3) +
              fractionX;
          mapy_mat.at<float>(y, x) =
              (static_cast<int32_t>(rng.next() % (kMaxHeight + 6)) - 3) +
              fractionY;
        }
      }

      cv::Mat actual_mat = exec_remap_f32<ScalarType, Format, Interpolation,
                                          BorderMode, BorderValue>(map_mat);

      cv::Mat expected_mat = get_expected_from_subordinate(
          index, request_queue, reply_queue, map_mat);

      // clang-format off
      // Reference algorithm in OpenCV uses integer interpolation with 5 bits
      // fractional part. That means that the maximum error between that and the
      // exact result in one dimension can be as big as 2^data_bits / 2^5 / 2
      // (we cannot have more than 2^32 different results), for two dimensions
      // it is the double, which is 8 for u8 and 2048 for u16.
      //
      // Example in 16 bits:
      // Source height = 36, width = 32;
      // mapx = 31.17005, mapy = 35.326836
      // so this is a corner case (bottom right corner):
      //    65469  123
      //      123  123    (constant border)
      //
      // Interpolation:
      // xfrac = 0.17005, yfrac = 0.326836
      // line0 = 65469 * (1 - 0.17005) + 123 * 0.17005 = 54356.9127
      // line1 = 123
      // EXACT RESULT calculated in float:
      //       54356.9127 * (1 - 0.326836) + 123 * 0.326836 = 36631.3176087828
      //
      // WITH 5-bit fractional part:
      // xfrac is rounded to 0.15625  (5/32)
      // line0 = 65469 * (1 - 0.15625) + 123 * 0.15625 = 55258.6875
      // (diff is less than 1024, as provisioned)
      // 2nd dimension: line1 = 123
      // yfrac is rounded to 0.3125 (10/32)
      // 5BIT RESULT:
      // 55258.6875 * (1 - 0.3125) + 123 * 0.3125 = 38028.78515625
      // clang-format on
      bool success =
          (CV_MAT_DEPTH(Format) == CV_8U &&
           !are_matrices_different<uint8_t>(8, actual_mat, expected_mat)) ||
          (CV_MAT_DEPTH(Format) == CV_16U &&
           !are_matrices_different<uint16_t>(2048, actual_mat, expected_mat));
      if (!success) {
        fail_print_matrices(w, h, source_mat, actual_mat, expected_mat);
        std::cout << "=== mapx_mat:" << std::endl;
        std::cout << mapx_mat << std::endl << std::endl;
        std::cout << "=== mapy_mat:" << std::endl;
        std::cout << mapy_mat << std::endl << std::endl;
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
    TEST("RemapS16 uint8 Replicate", (test_remap_s16<uint8_t, CV_8UC1, cv::INTER_NEAREST, cv::BORDER_REPLICATE, 0>), (exec_remap_s16<uint8_t, CV_8UC1, cv::INTER_NEAREST, cv::BORDER_REPLICATE, 0>)),
    TEST("RemapS16 uint16 Replicate", (test_remap_s16<uint16_t, CV_16UC1, cv::INTER_NEAREST, cv::BORDER_REPLICATE, 0>), (exec_remap_s16<uint16_t, CV_16UC1, cv::INTER_NEAREST, cv::BORDER_REPLICATE, 0>)),
    TEST("RemapS16 uint8 Constant", (test_remap_s16<uint8_t, CV_8UC1, cv::INTER_NEAREST, cv::BORDER_CONSTANT, 12321>), (exec_remap_s16<uint8_t, CV_8UC1, cv::INTER_NEAREST, cv::BORDER_CONSTANT, 12321>)),
    TEST("RemapS16 uint16 Constant", (test_remap_s16<uint16_t, CV_16UC1, cv::INTER_NEAREST, cv::BORDER_CONSTANT, 12321>), (exec_remap_s16<uint16_t, CV_16UC1, cv::INTER_NEAREST, cv::BORDER_CONSTANT, 12321>)),

    TEST("RemapS16Point5 uint8 Replicate", (test_remap_s16point5<uint8_t, CV_8UC1, cv::INTER_LINEAR, cv::BORDER_REPLICATE, 0>), (exec_remap_s16point5<uint8_t, CV_8UC1, cv::INTER_LINEAR, cv::BORDER_REPLICATE, 0>)),
    TEST("RemapS16Point5 uint8 Replicate 4ch", (test_remap_s16point5<uint8_t, CV_8UC4, cv::INTER_LINEAR, cv::BORDER_REPLICATE, 0>), (exec_remap_s16point5<uint8_t, CV_8UC4, cv::INTER_LINEAR, cv::BORDER_REPLICATE, 0>)),
    TEST("RemapS16Point5 uint16 Replicate", (test_remap_s16point5<uint16_t, CV_16UC1, cv::INTER_LINEAR, cv::BORDER_REPLICATE, 0>), (exec_remap_s16point5<uint16_t, CV_16UC1, cv::INTER_LINEAR, cv::BORDER_REPLICATE, 0>)),
    TEST("RemapS16Point5 uint16 Replicate 4ch", (test_remap_s16point5<uint16_t, CV_16UC4, cv::INTER_LINEAR, cv::BORDER_REPLICATE, 0>), (exec_remap_s16point5<uint16_t, CV_16UC4, cv::INTER_LINEAR, cv::BORDER_REPLICATE, 0>)),
    TEST("RemapS16Point5 uint8 Constant", (test_remap_s16point5<uint8_t, CV_8UC1, cv::INTER_LINEAR, cv::BORDER_CONSTANT, 12321>), (exec_remap_s16point5<uint8_t, CV_8UC1, cv::INTER_LINEAR, cv::BORDER_CONSTANT, 12321>)),
    TEST("RemapS16Point5 uint8 Constant 4ch", (test_remap_s16point5<uint8_t, CV_8UC4, cv::INTER_LINEAR, cv::BORDER_CONSTANT, 12321>), (exec_remap_s16point5<uint8_t, CV_8UC4, cv::INTER_LINEAR, cv::BORDER_CONSTANT, 12321>)),
    TEST("RemapS16Point5 uint16 Constant", (test_remap_s16point5<uint16_t, CV_16UC1, cv::INTER_LINEAR, cv::BORDER_CONSTANT, 12321>), (exec_remap_s16point5<uint16_t, CV_16UC1, cv::INTER_LINEAR, cv::BORDER_CONSTANT, 12321>)),
    TEST("RemapS16Point5 uint16 Constant 4ch", (test_remap_s16point5<uint16_t, CV_16UC4, cv::INTER_LINEAR, cv::BORDER_CONSTANT, 12321>), (exec_remap_s16point5<uint16_t, CV_16UC4, cv::INTER_LINEAR, cv::BORDER_CONSTANT, 12321>)),

    TEST("RemapF32 uint8 Replicate Linear 1ch", (test_remap_f32<uint8_t, CV_8UC1, cv::INTER_LINEAR, cv::BORDER_REPLICATE, 0>), (exec_remap_f32<uint8_t, CV_8UC1, cv::INTER_LINEAR, cv::BORDER_REPLICATE, 0>)),
    TEST("RemapF32 uint16 Replicate Linear 1ch", (test_remap_f32<uint16_t, CV_16UC1, cv::INTER_LINEAR, cv::BORDER_REPLICATE, 0>), (exec_remap_f32<uint16_t, CV_16UC1, cv::INTER_LINEAR, cv::BORDER_REPLICATE, 0>)),
    TEST("RemapF32 uint8 Constant Linear 1ch", (test_remap_f32<uint8_t, CV_8UC1, cv::INTER_LINEAR, cv::BORDER_CONSTANT, 12321>), (exec_remap_f32<uint8_t, CV_8UC1, cv::INTER_LINEAR, cv::BORDER_CONSTANT, 12321>)),
    TEST("RemapF32 uint16 Constant Linear 1ch", (test_remap_f32<uint16_t, CV_16UC1, cv::INTER_LINEAR, cv::BORDER_CONSTANT, 123210>), (exec_remap_f32<uint16_t, CV_16UC1, cv::INTER_LINEAR, cv::BORDER_CONSTANT, 123210>)),
    TEST("RemapF32 uint8 Replicate Nearest 1ch", (test_remap_f32<uint8_t, CV_8UC1, cv::INTER_NEAREST, cv::BORDER_REPLICATE, 0>), (exec_remap_f32<uint8_t, CV_8UC1, cv::INTER_NEAREST, cv::BORDER_REPLICATE, 0>)),
    TEST("RemapF32 uint16 Replicate Nearest 1ch", (test_remap_f32<uint16_t, CV_16UC1, cv::INTER_NEAREST, cv::BORDER_REPLICATE, 0>), (exec_remap_f32<uint16_t, CV_16UC1, cv::INTER_NEAREST, cv::BORDER_REPLICATE, 0>)),
    TEST("RemapF32 uint8 Constant Nearest 1ch", (test_remap_f32<uint8_t, CV_8UC1, cv::INTER_NEAREST, cv::BORDER_CONSTANT, 12321>), (exec_remap_f32<uint8_t, CV_8UC1, cv::INTER_NEAREST, cv::BORDER_CONSTANT, 12321>)),
    TEST("RemapF32 uint16 Constant Nearest 1ch", (test_remap_f32<uint16_t, CV_16UC1, cv::INTER_NEAREST, cv::BORDER_CONSTANT, 123210>), (exec_remap_f32<uint16_t, CV_16UC1, cv::INTER_NEAREST, cv::BORDER_CONSTANT, 123210>)),

    TEST("RemapF32 uint8 Replicate Linear 2ch", (test_remap_f32<uint8_t, CV_8UC2, cv::INTER_LINEAR, cv::BORDER_REPLICATE, 0>), (exec_remap_f32<uint8_t, CV_8UC2, cv::INTER_LINEAR, cv::BORDER_REPLICATE, 0>)),
    TEST("RemapF32 uint16 Replicate Linear 2ch", (test_remap_f32<uint16_t, CV_16UC2, cv::INTER_LINEAR, cv::BORDER_REPLICATE, 0>), (exec_remap_f32<uint16_t, CV_16UC2, cv::INTER_LINEAR, cv::BORDER_REPLICATE, 0>)),
    TEST("RemapF32 uint8 Constant Linear 2ch", (test_remap_f32<uint8_t, CV_8UC2, cv::INTER_LINEAR, cv::BORDER_CONSTANT, 12321>), (exec_remap_f32<uint8_t, CV_8UC2, cv::INTER_LINEAR, cv::BORDER_CONSTANT, 12321>)),
    TEST("RemapF32 uint16 Constant Linear 2ch", (test_remap_f32<uint16_t, CV_16UC2, cv::INTER_LINEAR, cv::BORDER_CONSTANT, 123210>), (exec_remap_f32<uint16_t, CV_16UC2, cv::INTER_LINEAR, cv::BORDER_CONSTANT, 123210>)),
    TEST("RemapF32 uint8 Replicate Nearest 2ch", (test_remap_f32<uint8_t, CV_8UC2, cv::INTER_NEAREST, cv::BORDER_REPLICATE, 0>), (exec_remap_f32<uint8_t, CV_8UC2, cv::INTER_NEAREST, cv::BORDER_REPLICATE, 0>)),
    TEST("RemapF32 uint16 Replicate Nearest 2ch", (test_remap_f32<uint16_t, CV_16UC2, cv::INTER_NEAREST, cv::BORDER_REPLICATE, 0>), (exec_remap_f32<uint16_t, CV_16UC2, cv::INTER_NEAREST, cv::BORDER_REPLICATE, 0>)),
    TEST("RemapF32 uint8 Constant Nearest 2ch", (test_remap_f32<uint8_t, CV_8UC2, cv::INTER_NEAREST, cv::BORDER_CONSTANT, 12321>), (exec_remap_f32<uint8_t, CV_8UC2, cv::INTER_NEAREST, cv::BORDER_CONSTANT, 12321>)),
    TEST("RemapF32 uint16 Constant Nearest 2ch", (test_remap_f32<uint16_t, CV_16UC2, cv::INTER_NEAREST, cv::BORDER_CONSTANT, 123210>), (exec_remap_f32<uint16_t, CV_16UC2, cv::INTER_NEAREST, cv::BORDER_CONSTANT, 123210>)),
  };
  // clang-format on
  return tests;
}
