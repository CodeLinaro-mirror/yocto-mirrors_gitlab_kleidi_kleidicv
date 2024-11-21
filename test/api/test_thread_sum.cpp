// SPDX-FileCopyrightText: 2024 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#include <gtest/gtest.h>

#include <limits>

#include "framework/array.h"
#include "framework/generator.h"
#include "kleidicv/kleidicv.h"
#include "kleidicv_thread/kleidicv_thread.h"
#include "multithreading_fake.h"

TEST(SumThread, CompareSingle) {
  test::PseudoRandomNumberGeneratorFloatRange<float> random_generator{-99999,
                                                                      99999};
  test::Array2D<float> src(32, 32, 0, 1);
  src.fill(random_generator);

  float sum_single = std::numeric_limits<float>::max();
  EXPECT_EQ(KLEIDICV_OK, kleidicv_sum_f32(src.data(), src.stride(), src.width(),
                                          src.height(), &sum_single));
  float sum_threaded = std::numeric_limits<float>::max();
  EXPECT_EQ(KLEIDICV_OK,
            kleidicv_thread_sum_f32(src.data(), src.stride(), src.width(),
                                    src.height(), &sum_threaded,
                                    get_multithreading_fake(3)));
  EXPECT_FLOAT_EQ(sum_single, sum_threaded);
}
