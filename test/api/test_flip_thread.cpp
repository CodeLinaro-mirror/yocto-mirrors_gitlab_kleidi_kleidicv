// SPDX-FileCopyrightText: 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "framework/array.h"
#include "framework/generator.h"
#include "framework/utils.h"
#include "kleidicv/ctypes.h"
#include "kleidicv/kleidicv.h"
#include "kleidicv_thread/kleidicv_thread.h"
#include "multithreading_fake.h"

namespace {

constexpr std::array<kleidicv_flip_mode_t, 3> kFlipModes{
    KLEIDICV_FLIP_HORIZONTAL, KLEIDICV_FLIP_VERTICAL, KLEIDICV_FLIP_BOTH};

// Map each destination pixel to its source, preserving channel order.
template <typename ScalarType, size_t kScalarsPerPixel>
void calculate_expected(const test::Array2D<ScalarType> &source,
                        test::Array2D<ScalarType> &expected,
                        kleidicv_flip_mode_t flip_mode) {
  const size_t width = source.width() / kScalarsPerPixel;
  const size_t height = source.height();
  for (size_t row = 0; row < height; ++row) {
    const size_t src_row =
        flip_mode == KLEIDICV_FLIP_HORIZONTAL ? row : height - 1 - row;
    for (size_t column = 0; column < width; ++column) {
      const size_t src_column =
          flip_mode == KLEIDICV_FLIP_VERTICAL ? column : width - 1 - column;
      for (size_t channel = 0; channel < kScalarsPerPixel; ++channel) {
        *expected.at(row, column * kScalarsPerPixel + channel) =
            *source.at(src_row, src_column * kScalarsPerPixel + channel);
      }
    }
  }
}

template <typename ScalarType, size_t kScalarsPerPixel>
void run_flip_case(size_t width, size_t height, size_t src_padding,
                   size_t dst_padding, kleidicv_flip_mode_t flip_mode,
                   bool in_place, kleidicv_thread_multithreading mt) {
  const size_t width_scalars = width * kScalarsPerPixel;
  const size_t pixel_size = sizeof(ScalarType) * kScalarsPerPixel;

  // Array2D checks row padding on destruction; EXPECT_EQ_ARRAY2D compares
  // active elements only, so callers supply padding to expose stray writes.
  test::Array2D<ScalarType> source(
      width_scalars, height, src_padding * kScalarsPerPixel, kScalarsPerPixel);
  test::PseudoRandomNumberGenerator<ScalarType> generator;
  source.fill(generator);

  // Keep an untouched reference for the scalar result and source preservation.
  const test::Array2D<ScalarType> original = source;

  const size_t expected_padding = in_place ? src_padding : dst_padding;
  test::Array2D<ScalarType> expected(width_scalars, height,
                                     expected_padding * kScalarsPerPixel,
                                     kScalarsPerPixel);
  calculate_expected<ScalarType, kScalarsPerPixel>(original, expected,
                                                   flip_mode);

  test::Array2D<ScalarType> actual;
  if (in_place) {
    actual = source;
    ASSERT_EQ(KLEIDICV_OK,
              kleidicv_thread_flip(actual.data(), actual.stride(), width,
                                   height, actual.data(), actual.stride(),
                                   flip_mode, pixel_size, mt));
  } else {
    actual = test::Array2D<ScalarType>(width_scalars, height,
                                       dst_padding * kScalarsPerPixel,
                                       kScalarsPerPixel);
    ASSERT_EQ(KLEIDICV_OK,
              kleidicv_thread_flip(source.data(), source.stride(), width,
                                   height, actual.data(), actual.stride(),
                                   flip_mode, pixel_size, mt));
    EXPECT_EQ_ARRAY2D(original, source);
  }
  EXPECT_EQ_ARRAY2D(expected, actual);
}

class FlipThread : public testing::TestWithParam<size_t> {
 protected:
  void run_case(size_t width, size_t height, kleidicv_flip_mode_t flip_mode,
                bool in_place, unsigned thread_count) const {
    SCOPED_TRACE(testing::Message()
                 << "pixel_size=" << GetParam() << ", width=" << width
                 << ", height=" << height << ", mode=" << flip_mode
                 << ", in_place=" << in_place
                 << ", thread_count=" << thread_count);
    const auto mt = get_multithreading_fake(thread_count);
    switch (GetParam()) {
      case sizeof(uint8_t):
        run_flip_case<uint8_t, 1>(width, height, 1, 3, flip_mode, in_place, mt);
        break;
      case sizeof(uint16_t):
        run_flip_case<uint16_t, 1>(width, height, 1, 3, flip_mode, in_place,
                                   mt);
        break;
      case sizeof(uint32_t):
        run_flip_case<uint32_t, 1>(width, height, 1, 3, flip_mode, in_place,
                                   mt);
        break;
      case sizeof(uint64_t):
        run_flip_case<uint64_t, 1>(width, height, 1, 3, flip_mode, in_place,
                                   mt);
        break;
      case sizeof(uint8_t) * 3:
        run_flip_case<uint8_t, 3>(width, height, 1, 3, flip_mode, in_place, mt);
        break;
      case sizeof(uint16_t) * 3:
        run_flip_case<uint16_t, 3>(width, height, 1, 3, flip_mode, in_place,
                                   mt);
        break;
      default:
        FAIL() << "Unsupported pixel size for flip test.";
    }
  }
};

TEST_P(FlipThread, MatchesScalarForVariedTaskSplits) {
  const size_t vector_length = test::Options::vector_lanes<uint8_t>();

  // Combine tiny images with wider images of odd and even heights.
  const std::array<std::array<size_t, 2>, 4> dimensions{
      {{1, 1}, {2, 2}, {vector_length + 1, 5}, {2 * vector_length - 1, 8}}};

  // Exercise uneven task splits and requests for more threads than work items.
  constexpr std::array<unsigned, 4> kThreadCounts{1, 2, 3, 7};

  for (const auto &dimension : dimensions) {
    for (kleidicv_flip_mode_t flip_mode : kFlipModes) {
      for (bool in_place : {false, true}) {
        for (unsigned thread_count : kThreadCounts) {
          run_case(dimension[0], dimension[1], flip_mode, in_place,
                   thread_count);
        }
      }
    }
  }
}

INSTANTIATE_TEST_SUITE_P(AllPixelSizes, FlipThread,
                         testing::Values(1, 2, 3, 4, 6, 8),
                         testing::PrintToStringParamName());

struct RecordingParallelState {
  size_t invocation_count = 0;
  size_t task_count = 0;
};

kleidicv_error_t recording_parallel(kleidicv_thread_callback callback,
                                    void *callback_data, void *parallel_data,
                                    size_t task_count) {
  auto *state = static_cast<RecordingParallelState *>(parallel_data);
  ++state->invocation_count;
  state->task_count = task_count;

  // Run one work item at a time, in reverse order, to exercise independence.
  for (size_t work_item_end = task_count; work_item_end > 0; --work_item_end) {
    kleidicv_error_t error =
        callback(work_item_end - 1, work_item_end, callback_data);
    if (error != KLEIDICV_OK) {
      return error;
    }
  }
  return KLEIDICV_OK;
}

TEST(FlipThreadScheduling, UsesRowsOrIndependentMirroredPairs) {
  constexpr size_t kWidth = 13;
  constexpr size_t kPadding = 4;

  for (size_t height : {size_t{6}, size_t{7}}) {
    for (kleidicv_flip_mode_t flip_mode : kFlipModes) {
      for (bool in_place : {false, true}) {
        SCOPED_TRACE(testing::Message()
                     << "height=" << height << ", mode=" << flip_mode
                     << ", in_place=" << in_place);

        RecordingParallelState state;
        const kleidicv_thread_multithreading mt{recording_parallel, &state};
        run_flip_case<uint8_t, 1>(kWidth, height, kPadding, kPadding, flip_mode,
                                  in_place, mt);

        size_t expected_task_count = height;
        if (in_place && flip_mode == KLEIDICV_FLIP_VERTICAL) {
          // Each task owns a mirrored row pair; an odd middle row is unchanged.
          expected_task_count = height / 2;
        } else if (in_place && flip_mode == KLEIDICV_FLIP_BOTH) {
          // The odd middle row also needs a task to flip it horizontally.
          expected_task_count = (height + 1) / 2;
        }

        EXPECT_EQ(size_t{1}, state.invocation_count);
        EXPECT_EQ(expected_task_count, state.task_count);
      }
    }
  }
}

kleidicv_error_t reject_parallel(kleidicv_thread_callback, void *,
                                 void *parallel_data, size_t) {
  auto *invocation_count = static_cast<size_t *>(parallel_data);
  ++*invocation_count;
  return KLEIDICV_ERROR_CONTEXT_MISMATCH;
}

TEST(FlipThreadValidation, HandlesValidationAndNoWorkBeforeScheduling) {
  std::vector<uint8_t> src(64);
  std::vector<uint8_t> dst(64);
  size_t scheduler_invocation_count = 0;
  const kleidicv_thread_multithreading mt{reject_parallel,
                                          &scheduler_invocation_count};

  auto invoke = [&](const void *call_src, size_t src_stride, size_t width,
                    size_t height, void *call_dst, size_t dst_stride,
                    kleidicv_flip_mode_t flip_mode, size_t pixel_size) {
    scheduler_invocation_count = 0;
    kleidicv_error_t error =
        kleidicv_thread_flip(call_src, src_stride, width, height, call_dst,
                             dst_stride, flip_mode, pixel_size, mt);
    EXPECT_EQ(size_t{0}, scheduler_invocation_count);
    return error;
  };

  EXPECT_EQ(KLEIDICV_ERROR_NULL_POINTER, invoke(nullptr, 1, 1, 1, dst.data(), 1,
                                                KLEIDICV_FLIP_HORIZONTAL, 1));
  EXPECT_EQ(KLEIDICV_ERROR_NULL_POINTER, invoke(src.data(), 1, 1, 1, nullptr, 1,
                                                KLEIDICV_FLIP_HORIZONTAL, 1));

  EXPECT_EQ(KLEIDICV_ERROR_ALIGNMENT,
            invoke(src.data() + 1, 2, 1, 1, dst.data(), 2,
                   KLEIDICV_FLIP_HORIZONTAL, 2));
  EXPECT_EQ(KLEIDICV_ERROR_ALIGNMENT,
            invoke(src.data(), 2, 1, 1, dst.data() + 1, 2,
                   KLEIDICV_FLIP_HORIZONTAL, 2));
  EXPECT_EQ(KLEIDICV_ERROR_ALIGNMENT, invoke(src.data(), 3, 1, 2, dst.data(), 2,
                                             KLEIDICV_FLIP_HORIZONTAL, 2));
  EXPECT_EQ(KLEIDICV_ERROR_ALIGNMENT, invoke(src.data(), 2, 1, 2, dst.data(), 3,
                                             KLEIDICV_FLIP_HORIZONTAL, 2));

  EXPECT_EQ(
      KLEIDICV_ERROR_NOT_IMPLEMENTED,
      invoke(src.data(), 1, 1, 1, dst.data(), 1, kleidicv_flip_mode_t{}, 1));

  EXPECT_EQ(KLEIDICV_ERROR_RANGE,
            invoke(src.data(), 1, KLEIDICV_MAX_IMAGE_PIXELS + 1, 1, dst.data(),
                   1, KLEIDICV_FLIP_HORIZONTAL, 1));
  EXPECT_EQ(KLEIDICV_ERROR_RANGE,
            invoke(src.data(), 1, KLEIDICV_MAX_IMAGE_PIXELS,
                   KLEIDICV_MAX_IMAGE_PIXELS, dst.data(), 1,
                   KLEIDICV_FLIP_HORIZONTAL, 1));
  EXPECT_EQ(KLEIDICV_ERROR_RANGE, invoke(src.data(), 1, 1, 1, src.data(), 2,
                                         KLEIDICV_FLIP_HORIZONTAL, 1));

  EXPECT_EQ(KLEIDICV_OK, invoke(src.data(), 1, 0, 7, dst.data(), 1,
                                KLEIDICV_FLIP_HORIZONTAL, 1));
  EXPECT_EQ(KLEIDICV_OK, invoke(src.data(), 1, 7, 0, dst.data(), 1,
                                KLEIDICV_FLIP_HORIZONTAL, 1));
  EXPECT_EQ(KLEIDICV_OK, invoke(src.data(), 1, 1, 1, src.data(), 1,
                                KLEIDICV_FLIP_VERTICAL, 1));
}

}  // namespace
