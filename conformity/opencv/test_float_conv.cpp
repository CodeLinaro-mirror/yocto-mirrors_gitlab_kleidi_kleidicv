// SPDX-FileCopyrightText: 2024 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#include "test_float_conv.h"

#include <vector>

float floatval(uint32_t v) {
  static_assert(sizeof(float) == 4);
  return *reinterpret_cast<float*>(&v);
}

float quietNaN = floatval(0x7FC00000);
float signalingNaN = floatval(0x7FA00000);
float posInfinity = floatval(0x7F800000);
float negInfinity = floatval(0xFF800000);

float minusNaN = floatval(0xFF800001);
float plusNaN = floatval(0x7F800001);
float plusZero = floatval(0x00000000);
float minusZero = floatval(0x80000000);

float oneNaN = floatval(0x7FC00001);
float zeroDivZero = floatval(0xFFC00000);
float floatMin = floatval(0x00800000);
float floatMax = floatval(0x7F7FFFFF);

float posSubnormalMin = floatval(0x00000001);
float posSubnormalMax = floatval(0x007FFFFF);
float negSubnormalMin = floatval(0x80000001);
float negSubnormalMax = floatval(0x807FFFFF);

template <bool Signed>
cv::Mat exec_float32_to_int8(cv::Mat& input) {
  cv::Mat result;
  input.convertTo(result, Signed ? CV_8SC1 : CV_8UC1);
  return result;
}

cv::Mat exec_int8_to_float32(cv::Mat& input) {
  cv::Mat result;
  input.convertTo(result, CV_32FC1);
  return result;
}

#if MANAGER
template <bool Signed, size_t Channels>
bool test_float32_to_int8_random(int index,
                                 RecreatedMessageQueue& request_queue,
                                 RecreatedMessageQueue& reply_queue) {
  cv::RNG rng(0);

  for (size_t x = 5; x <= 16; ++x) {
    for (size_t y = 5; y <= 16; ++y) {
      cv::Mat input(x, y, CV_32FC(Channels));
      rng.fill(input, cv::RNG::UNIFORM, Signed ? -1000 : 0, 1000);

      cv::Mat actual = exec_float32_to_int8<Signed>(input);
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

template <bool Signed, size_t Channels>
bool test_int8_to_float32_random(int index,
                                 RecreatedMessageQueue& request_queue,
                                 RecreatedMessageQueue& reply_queue) {
  cv::RNG rng(0);

  for (size_t x = 5; x <= 16; ++x) {
    for (size_t y = 5; y <= 16; ++y) {
      cv::Mat input(x, y, Signed ? CV_8SC(Channels) : CV_8UC(Channels));
      rng.fill(input, cv::RNG::UNIFORM, Signed ? -1000 : 0, 1000);

      cv::Mat actual = exec_int8_to_float32(input);
      cv::Mat expected = get_expected_from_subordinate(index, request_queue,
                                                       reply_queue, input);

      if (are_matrices_different<float>(0, actual, expected)) {
        fail_print_matrices(x, y, input, actual, expected);
        return true;
      }
    }
  }

  return false;
}

static constexpr int custom_data_height = 8;
static constexpr int custom_data_width = 4;

static float custom_data_float[custom_data_height * custom_data_width] = {
    // clang-format off
  quietNaN, signalingNaN, posInfinity, negInfinity,
  minusNaN, plusNaN, plusZero, minusZero,
  oneNaN, zeroDivZero, floatMin, floatMax,
  posSubnormalMin, posSubnormalMax, negSubnormalMin, negSubnormalMax,
  1111.11, -1112.22, 113.33, 114.44,
  111.51, 112.62, 113.73, 114.84,
  126.66, 127.11, 128.66, 129.11,
  11.5, 12.5, -11.5, -12.5,
    // clang-format on
};

static int8_t custom_data_int8[custom_data_height * custom_data_width] = {
    // clang-format off
  -128, -128, 126, 127,
  -128, -128, -128, -128,
  -128, -128, -128, 126,
  -127, -127, -127, 125,
  126, 127, 113, 114,
  112, 113, 114, 115,
  12, 12, 12, 12,
  11, 11, 11, 11,
    // clang-format on
};

static uint8_t custom_data_uint8[custom_data_height * custom_data_width] = {
    // clang-format off
  0, 0, 254, 255,
  0, 0, 0, 0,
  0, 0, 0, 254,
  1, 1, 1, 253,
  254, 255, 113, 114,
  112, 113, 114, 115,
  12, 12, 12, 12,
  11, 11, 11, 11,
    // clang-format on
};

template <bool Signed>
bool test_float32_to_int8_custom(int index,
                                 RecreatedMessageQueue& request_queue,
                                 RecreatedMessageQueue& reply_queue) {
  cv::Mat input(custom_data_height, custom_data_width, CV_32FC1,
                custom_data_float);

  cv::Mat actual = exec_float32_to_int8<Signed>(input);
  cv::Mat expected =
      get_expected_from_subordinate(index, request_queue, reply_queue, input);

  if (are_matrices_different<uint8_t>(0, actual, expected)) {
    fail_print_matrices(custom_data_height, custom_data_width, input, actual,
                        expected);
    return true;
  }

  return false;
}

template <bool Signed>
bool test_int8_to_float32_custom(int index,
                                 RecreatedMessageQueue& request_queue,
                                 RecreatedMessageQueue& reply_queue) {
  cv::Mat input(custom_data_height, custom_data_width,
                Signed ? CV_8SC1 : CV_8UC1,
                Signed ? static_cast<void*>(custom_data_int8)
                       : static_cast<void*>(custom_data_uint8));

  cv::Mat actual = exec_int8_to_float32(input);
  cv::Mat expected =
      get_expected_from_subordinate(index, request_queue, reply_queue, input);

  if (are_matrices_different<float>(0, actual, expected)) {
    fail_print_matrices(custom_data_height, custom_data_width, input, actual,
                        expected);
    return true;
  }

  return false;
}
#endif

std::vector<test>& float_conversion_tests_get() {
  // clang-format off
  static std::vector<test> tests = {
    TEST("Float32 to Signed Int8, fill, 1 channel", (test_float32_to_int8_random<true, 1>), exec_float32_to_int8<true>),
    TEST("Float32 to Signed Int8, fill, 2 channel", (test_float32_to_int8_random<true, 2>), exec_float32_to_int8<true>),
    TEST("Float32 to Signed Int8, fill, 3 channel", (test_float32_to_int8_random<true, 3>), exec_float32_to_int8<true>),
    TEST("Float32 to Signed Int8, fill, 4 channel", (test_float32_to_int8_random<true, 4>), exec_float32_to_int8<true>),

    TEST("Float32 to Unsigned Int8, fill, 1 channel", (test_float32_to_int8_random<false, 1>), exec_float32_to_int8<false>),
    TEST("Float32 to Unsigned Int8, fill, 2 channel", (test_float32_to_int8_random<false, 2>), exec_float32_to_int8<false>),
    TEST("Float32 to Unsigned Int8, fill, 3 channel", (test_float32_to_int8_random<false, 3>), exec_float32_to_int8<false>),
    TEST("Float32 to Unsigned Int8, fill, 4 channel", (test_float32_to_int8_random<false, 4>), exec_float32_to_int8<false>),

    TEST("Float32 to Signed Int8, custom (special)", test_float32_to_int8_custom<true>, exec_float32_to_int8<true>),
    TEST("Float32 to Unsigned Int8, custom (special)", test_float32_to_int8_custom<false>, exec_float32_to_int8<false>),

    TEST("Signed Int8 to Float32, fill, 1 channel", (test_int8_to_float32_random<true, 1>), exec_int8_to_float32),
    TEST("Signed Int8 to Float32, fill, 2 channel", (test_int8_to_float32_random<true, 2>), exec_int8_to_float32),
    TEST("Signed Int8 to Float32, fill, 3 channel", (test_int8_to_float32_random<true, 3>), exec_int8_to_float32),
    TEST("Signed Int8 to Float32, fill, 4 channel", (test_int8_to_float32_random<true, 4>), exec_int8_to_float32),

    TEST("Unsigned Int8 to Float32, fill, 1 channel", (test_int8_to_float32_random<false, 1>), exec_int8_to_float32),
    TEST("Unsigned Int8 to Float32, fill, 2 channel", (test_int8_to_float32_random<false, 2>), exec_int8_to_float32),
    TEST("Unsigned Int8 to Float32, fill, 3 channel", (test_int8_to_float32_random<false, 3>), exec_int8_to_float32),
    TEST("Unsigned Int8 to Float32, fill, 4 channel", (test_int8_to_float32_random<false, 4>), exec_int8_to_float32),

    TEST("Signed Int8 Float32, custom (special)", test_int8_to_float32_custom<true>, exec_int8_to_float32),
    TEST("Unigned Int8 Float32, custom (special)", test_int8_to_float32_custom<false>, exec_int8_to_float32),
  };
  // clang-format on
  return tests;
}
