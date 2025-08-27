// SPDX-FileCopyrightText: 2025 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <thread>

#include "framework/array.h"
#include "framework/generator.h"
#include "framework/utils.h"
#include "kleidicv/kleidicv.h"
#include "kleidicv/utils.h"
#include "kleidicv_thread/kleidicv_thread.h"
#include "multithreading_fake.h"
#include "test_config.h"

// Tuple of width, height, thread count.
typedef std::tuple<unsigned, unsigned, unsigned> P;

class RgbToYuv420SpThread : public testing::TestWithParam<P> {
 public:
  template <typename SingleThreadedFunc, typename MultithreadedFunc>
  void check(SingleThreadedFunc single_threaded_func,
             MultithreadedFunc multithreaded_func, size_t channels) {
    unsigned width = 0, height = 0, thread_count = 0;
    std::tie(width, height, thread_count) = GetParam();
    test::Array2D<uint8_t> src(size_t{width} * channels, height, channels),
        y_dst_single(width, height),
        uv_dst_single(KLEIDICV_TARGET_NAMESPACE::align_up(width, 2),
                      (height + 1) / 2),
        y_dst_multi(width, height),
        uv_dst_multi(KLEIDICV_TARGET_NAMESPACE::align_up(width, 2),
                     (height + 1) / 2);

    test::PseudoRandomNumberGenerator<uint8_t> generator;
    src.fill(generator);

    kleidicv_error_t single_result = single_threaded_func(
        src.data(), src.stride(), y_dst_single.data(), y_dst_single.stride(),
        uv_dst_single.data(), uv_dst_single.stride(), width, height, false);

    kleidicv_error_t multi_result = multithreaded_func(
        src.data(), src.stride(), y_dst_multi.data(), y_dst_multi.stride(),
        uv_dst_multi.data(), uv_dst_multi.stride(), width, height, false,
        get_multithreading_fake(thread_count));

    EXPECT_EQ(KLEIDICV_OK, single_result);
    EXPECT_EQ(KLEIDICV_OK, multi_result);
    EXPECT_EQ_ARRAY2D(y_dst_multi, y_dst_single);
    EXPECT_EQ_ARRAY2D(uv_dst_multi, uv_dst_single);
  }
};

TEST_P(RgbToYuv420SpThread, FromBGR) {
  check(kleidicv_bgr_to_yuv420_sp_u8, kleidicv_thread_bgr_to_yuv420_sp_u8, 3);
}
TEST_P(RgbToYuv420SpThread, FromBGRA) {
  check(kleidicv_bgra_to_yuv420_sp_u8, kleidicv_thread_bgra_to_yuv420_sp_u8, 4);
}
TEST_P(RgbToYuv420SpThread, FromRGB) {
  check(kleidicv_rgb_to_yuv420_sp_u8, kleidicv_thread_rgb_to_yuv420_sp_u8, 3);
}
TEST_P(RgbToYuv420SpThread, FromRGBA) {
  check(kleidicv_rgba_to_yuv420_sp_u8, kleidicv_thread_rgba_to_yuv420_sp_u8, 4);
}

INSTANTIATE_TEST_SUITE_P(, RgbToYuv420SpThread,
                         testing::Values(P{1, 1, 1}, P{1, 2, 1}, P{1, 2, 2},
                                         P{2, 1, 2}, P{2, 2, 1}, P{1, 3, 2},
                                         P{2, 3, 1}, P{6, 4, 1}, P{4, 5, 2},
                                         P{2, 6, 3}, P{1, 7, 4}, P{12, 34, 5},
                                         P{12, 37, 5}, P{2, 1000, 2}));
