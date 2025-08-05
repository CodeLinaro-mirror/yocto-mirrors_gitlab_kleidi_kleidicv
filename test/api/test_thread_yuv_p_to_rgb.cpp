// SPDX-FileCopyrightText: 2025 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <thread>

#include "framework/array.h"
#include "framework/generator.h"
#include "kleidicv/kleidicv.h"
#include "kleidicv_thread/kleidicv_thread.h"
#include "multithreading_fake.h"

// Tuple of width, height, thread count.
typedef std::tuple<unsigned, unsigned, unsigned> P;

class YuvpThread : public testing::TestWithParam<P> {
 public:
  template <typename SingleThreadedFunc, typename MultithreadedFunc>
  void check(SingleThreadedFunc single_threaded_func,
             MultithreadedFunc multithreaded_func, size_t channels) {
    unsigned width = 0, height = 0, thread_count = 0;
    std::tie(width, height, thread_count) = GetParam();
    test::Array2D<uint8_t> src(width, (height * 3 + 1) / 2),
        dst_single(size_t{width} * channels, height),
        dst_multi(size_t{width} * channels, height);

    test::PseudoRandomNumberGenerator<uint8_t> generator;
    src.fill(generator);

    kleidicv_error_t single_result =
        single_threaded_func(src.data(), src.stride(), dst_single.data(),
                             dst_single.stride(), width, height, false);

    kleidicv_error_t multi_result = multithreaded_func(
        src.data(), src.stride(), dst_multi.data(), dst_multi.stride(), width,
        height, false, get_multithreading_fake(thread_count));

    EXPECT_EQ(KLEIDICV_OK, single_result);
    EXPECT_EQ(KLEIDICV_OK, multi_result);
    EXPECT_EQ_ARRAY2D(dst_multi, dst_single);
  }

  template <typename Func>
  void run_unsupported(Func impl, size_t channels, bool is_nv21) {
    test::Array2D<uint8_t> src{20, (10 * 3 + 1) / 2};

    test::Array2D<uint8_t> dst{20 * channels, 10, 0, channels};

    test::test_null_args(impl, src.data(), src.stride(), dst.data(),
                         dst.stride(), dst.width(), dst.height(), is_nv21,
                         get_multithreading_fake(2));

    EXPECT_EQ(KLEIDICV_OK,
              impl(src.data(), src.stride(), dst.data(), dst.stride(), 0, 1,
                   is_nv21, get_multithreading_fake(2)));

    EXPECT_EQ(KLEIDICV_OK,
              impl(src.data(), src.stride(), dst.data(), dst.stride(), 1, 0,
                   is_nv21, get_multithreading_fake(2)));

    EXPECT_EQ(KLEIDICV_ERROR_RANGE,
              impl(src.data(), src.stride(), dst.data(), dst.stride(),
                   KLEIDICV_MAX_IMAGE_PIXELS + 1, 1, is_nv21,
                   get_multithreading_fake(2)));
    EXPECT_EQ(KLEIDICV_ERROR_RANGE,
              impl(src.data(), src.stride(), dst.data(), dst.stride(),
                   KLEIDICV_MAX_IMAGE_PIXELS, KLEIDICV_MAX_IMAGE_PIXELS,
                   is_nv21, get_multithreading_fake(2)));
  }
};

TEST_P(YuvpThread, ToBGR) {
  check(kleidicv_yuv_p_to_bgr_u8, kleidicv_thread_yuv_p_to_bgr_u8, 3);
}
TEST_P(YuvpThread, ToBGRA) {
  check(kleidicv_yuv_p_to_bgra_u8, kleidicv_thread_yuv_p_to_bgra_u8, 4);
}
TEST_P(YuvpThread, ToRGB) {
  check(kleidicv_yuv_p_to_rgb_u8, kleidicv_thread_yuv_p_to_rgb_u8, 3);
}
TEST_P(YuvpThread, ToRGBA) {
  check(kleidicv_yuv_p_to_rgba_u8, kleidicv_thread_yuv_p_to_rgba_u8, 4);
}

TEST_F(YuvpThread, ReturnsErrorForUnsupportedCombinations) {
  run_unsupported(kleidicv_thread_yuv_p_to_rgb_u8, 3, true);
  run_unsupported(kleidicv_thread_yuv_p_to_rgba_u8, 4, true);
  run_unsupported(kleidicv_thread_yuv_p_to_bgr_u8, 3, true);
  run_unsupported(kleidicv_thread_yuv_p_to_bgra_u8, 4, true);
  run_unsupported(kleidicv_thread_yuv_p_to_rgb_u8, 3, false);
  run_unsupported(kleidicv_thread_yuv_p_to_rgba_u8, 4, false);
  run_unsupported(kleidicv_thread_yuv_p_to_bgr_u8, 3, false);
  run_unsupported(kleidicv_thread_yuv_p_to_bgra_u8, 4, false);
}

INSTANTIATE_TEST_SUITE_P(, YuvpThread,
                         testing::Values(P{1, 1, 1}, P{1, 2, 1}, P{1, 2, 2},
                                         P{2, 1, 2}, P{2, 2, 1}, P{1, 3, 2},
                                         P{2, 3, 1}, P{6, 4, 1}, P{4, 5, 2},
                                         P{2, 6, 3}, P{1, 7, 4}, P{12, 34, 5},
                                         P{12, 37, 5}, P{16, 37, 5},
                                         P{2, 1000, 2}));
