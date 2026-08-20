// SPDX-FileCopyrightText: 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <limits>

#include "kleidicv/kleidicv.h"
#include "kleidicv_thread/kleidicv_thread.h"

namespace {

struct TaskCountCapture {
  size_t call_count = 0;
  size_t task_count = 0;
};

kleidicv_error_t capture_task_count_and_abort(
    kleidicv_thread_callback /*callback*/, void * /*callback_data*/,
    void *parallel_data, size_t task_count) {
  auto *capture = static_cast<TaskCountCapture *>(parallel_data);
  ++capture->call_count;
  capture->task_count = task_count;
  return KLEIDICV_ERROR_CONTEXT_MISMATCH;
}

size_t task_count_above_unsigned() {
  size_t task_count = static_cast<size_t>(std::numeric_limits<unsigned>::max());
  return ++task_count;
}

TEST(ThreadScheduler, ParallelBatchesPreservesLargeHeight) {
  if (std::numeric_limits<size_t>::max() <=
      std::numeric_limits<unsigned>::max()) {
    GTEST_SKIP() << "size_t does not have a wider range than unsigned";
  }

  const size_t height = task_count_above_unsigned();
  uint8_t src = 0;
  uint8_t dst = 0;
  TaskCountCapture capture;
  const kleidicv_thread_multithreading mt{capture_task_count_and_abort,
                                          &capture};

  // A zero-width image needs no backing storage. The scheduler aborts without
  // invoking the work callback after capturing the requested task count.
  EXPECT_EQ(
      KLEIDICV_ERROR_CONTEXT_MISMATCH,
      kleidicv_thread_scale_u8(&src, 0, &dst, 0, 0, height, 1.0, 0.0, mt));
  EXPECT_EQ(size_t{1}, capture.call_count);
  EXPECT_EQ(height, capture.task_count);
}

}  // namespace
