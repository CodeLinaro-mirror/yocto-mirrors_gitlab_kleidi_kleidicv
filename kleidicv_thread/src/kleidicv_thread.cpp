// SPDX-FileCopyrightText: 2024 - 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#include "kleidicv_thread/kleidicv_thread.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <limits>

#include "heap_array.h"
#include "kleidicv/arithmetics/scale.h"
#include "kleidicv/conversions/rgb_to_yuv.h"
#include "kleidicv/conversions/yuv_to_rgb.h"
#include "kleidicv/ctypes.h"
#include "kleidicv/filters/blur_and_downsample.h"
#include "kleidicv/filters/gaussian_blur.h"
#include "kleidicv/filters/median_blur.h"
#include "kleidicv/filters/scharr.h"
#include "kleidicv/filters/separable_filter_2d.h"
#include "kleidicv/filters/sobel.h"
#include "kleidicv/kleidicv.h"
#include "kleidicv/resize/resize_linear.h"
#include "kleidicv/transform/add_padding_by_copy.h"
#include "kleidicv/transform/flip.h"
#include "kleidicv/transform/remap.h"
#include "kleidicv/transform/rotate.h"
#include "kleidicv/transform/warp_perspective.h"
#if KLEIDICV_ENABLE_SME
#include "kleidicv/dispatch.h"
#endif  // KLEIDICV_ENABLE_SME

template <typename Callback>
struct BatchContext {
  Callback *callback;
  size_t count;
  size_t min_batch_size;
  size_t task_count;
};

template <typename Callback>
static kleidicv_error_t kleidicv_thread_batch_callback(size_t task_begin,
                                                       size_t task_end,
                                                       void *data) {
  auto *context = static_cast<BatchContext<Callback> *>(data);
  size_t begin = task_begin * context->min_batch_size;
  size_t end = task_end * context->min_batch_size;
  if (task_end == context->task_count) {
    end = context->count;
  }

  return (*context->callback)(begin, end);
}

// Operations in the Neon backend have both a vector path and a scalar path.
// The vector path is used to process most data and the scalar path is used to
// process the parts of the data that don't fit into the vector width.
// For floating point operations in particular, the results may be very slightly
// different between vector and scalar paths.
//
// When using multithreading, images are divided into parts to be processed by
// each thread, and this could change which parts of the data end up being
// processed by the vector and scalar paths.
//
// If an implementation is sensitive to these very slight differences, set
// min_batch_size to the Neon vector length (16 bytes). That makes every batch
// handed to a thread a multiple of the vector width; only the final batch may
// be longer to reach the end of the data. No batch can be shorter than vector
// length because that could change behaviour for operations that try to avoid
// the tail loop (see the TryToAvoidTailLoop class).
// This technique only works if the data is longer than vector length.
//
// On the other hand, measurements showed that increasing the batch size can
// cause degradation of the multithreaded performance.
template <typename Callback>
inline kleidicv_error_t parallel_batches(Callback callback,
                                         kleidicv_thread_multithreading mt,
                                         size_t count,
                                         size_t min_batch_size = 1) {
  const size_t task_count = std::max(size_t{1}, (count) / min_batch_size);
  BatchContext<Callback> context{&callback, count, min_batch_size, task_count};
  return mt.parallel(kleidicv_thread_batch_callback<Callback>, &context,
                     mt.parallel_data, task_count);
}

template <typename SrcT, typename DstT, typename F, typename... Args>
inline kleidicv_error_t kleidicv_thread_unary_op_impl(
    F f, kleidicv_thread_multithreading mt, const SrcT *src, size_t src_stride,
    DstT *dst, size_t dst_stride, size_t width, size_t height, Args... args) {
  auto callback = [=](size_t begin, size_t end) {
    return f(src + static_cast<ptrdiff_t>(begin * src_stride / sizeof(SrcT)),
             src_stride,
             dst + static_cast<ptrdiff_t>(begin * dst_stride / sizeof(DstT)),
             dst_stride, width, end - begin, args...);
  };
  return parallel_batches(callback, mt, height);
}

#if KLEIDICV_ENABLE_SME
const bool kHwCapsHasSme = kleidicv::is_sme_supported();

static std::atomic<int> &available_sme_thread_num() {
  static std::atomic<int> value{KLEIDICV_MAX_SME_THREADS};
  return value;
}

struct SmeThreadRunResult {
  bool did_run;
  kleidicv_error_t error;
};

template <typename Callback>
SmeThreadRunResult try_to_run_sme_thread(Callback callback) {
  auto &available_threads = available_sme_thread_num();
  int thread_num_local = available_threads.load();
  // Attempt to atomically decrement available_sme_thread_num if we think we
  // have more than zero threads, else update the local value.
  while (thread_num_local > 0 && !available_threads.compare_exchange_strong(
                                     thread_num_local, thread_num_local - 1)) {
  }

  if (thread_num_local > 0) {
    // As exceptions are turned off at build time it is fine to run the callback
    // and plainly increase the thread counter afterwards.
    kleidicv_error_t r = callback();
    available_threads++;
    return SmeThreadRunResult{true, r};
  }

  return SmeThreadRunResult{false, kleidicv_error_t{}};
}

#endif  // KLEIDICV_ENABLE_SME

template <typename SrcT, typename DstT, typename F, typename... Args>
inline kleidicv_error_t kleidicv_thread_binary_op_impl(
    F f, kleidicv_thread_multithreading mt, const SrcT *src_a,
    size_t src_a_stride, const SrcT *src_b, size_t src_b_stride, DstT *dst,
    size_t dst_stride, size_t width, size_t height, Args... args) {
  auto callback = [=](size_t begin, size_t end) {
    return f(
        src_a + static_cast<ptrdiff_t>(begin * src_a_stride / sizeof(SrcT)),
        src_a_stride,
        src_b + static_cast<ptrdiff_t>(begin * src_b_stride / sizeof(SrcT)),
        src_b_stride,
        dst + static_cast<ptrdiff_t>(begin * dst_stride / sizeof(DstT)),
        dst_stride, width, end - begin, args...);
  };
  return parallel_batches(callback, mt, height);
}

#define KLEIDICV_THREAD_UNARY_OP_IMPL(suffix, src_type, dst_type)            \
  kleidicv_error_t kleidicv_thread_##suffix(                                 \
      const src_type *src, size_t src_stride, dst_type *dst,                 \
      size_t dst_stride, size_t width, size_t height,                        \
      kleidicv_thread_multithreading mt) {                                   \
    return kleidicv_thread_unary_op_impl(kleidicv_##suffix, mt, src,         \
                                         src_stride, dst, dst_stride, width, \
                                         height);                            \
  }

KLEIDICV_THREAD_UNARY_OP_IMPL(gray_to_rgb_u8, uint8_t, uint8_t);
KLEIDICV_THREAD_UNARY_OP_IMPL(gray_to_rgba_u8, uint8_t, uint8_t);
KLEIDICV_THREAD_UNARY_OP_IMPL(rgb_to_bgr_u8, uint8_t, uint8_t);
KLEIDICV_THREAD_UNARY_OP_IMPL(rgb_to_rgb_u8, uint8_t, uint8_t);
KLEIDICV_THREAD_UNARY_OP_IMPL(rgba_to_bgra_u8, uint8_t, uint8_t);
KLEIDICV_THREAD_UNARY_OP_IMPL(rgba_to_rgba_u8, uint8_t, uint8_t);
KLEIDICV_THREAD_UNARY_OP_IMPL(rgb_to_bgra_u8, uint8_t, uint8_t);
KLEIDICV_THREAD_UNARY_OP_IMPL(rgb_to_rgba_u8, uint8_t, uint8_t);
KLEIDICV_THREAD_UNARY_OP_IMPL(rgba_to_bgr_u8, uint8_t, uint8_t);
KLEIDICV_THREAD_UNARY_OP_IMPL(rgba_to_rgb_u8, uint8_t, uint8_t);
KLEIDICV_THREAD_UNARY_OP_IMPL(exp_f32, float, float);
KLEIDICV_THREAD_UNARY_OP_IMPL(f32_to_s8, float, int8_t);
KLEIDICV_THREAD_UNARY_OP_IMPL(f32_to_u8, float, uint8_t);
KLEIDICV_THREAD_UNARY_OP_IMPL(s8_to_f32, int8_t, float);
KLEIDICV_THREAD_UNARY_OP_IMPL(u8_to_f32, uint8_t, float);

#define KLEIDICV_THREAD_INRANGE_OP_IMPL(suffix, src_type, dst_type)          \
  kleidicv_error_t kleidicv_thread_##suffix(                                 \
      const src_type *src, size_t src_stride, dst_type *dst,                 \
      size_t dst_stride, size_t width, size_t height, src_type lower_bound,  \
      src_type upper_bound, kleidicv_thread_multithreading mt) {             \
    return kleidicv_thread_unary_op_impl(kleidicv_##suffix, mt, src,         \
                                         src_stride, dst, dst_stride, width, \
                                         height, lower_bound, upper_bound);  \
  }

KLEIDICV_THREAD_INRANGE_OP_IMPL(in_range_u8, uint8_t, uint8_t);
KLEIDICV_THREAD_INRANGE_OP_IMPL(in_range_f32, float, uint8_t);

kleidicv_error_t kleidicv_thread_threshold_binary_u8(
    const uint8_t *src, size_t src_stride, uint8_t *dst, size_t dst_stride,
    size_t width, size_t height, uint8_t threshold, uint8_t value,
    kleidicv_thread_multithreading mt) {
  return kleidicv_thread_unary_op_impl(kleidicv_threshold_binary_u8, mt, src,
                                       src_stride, dst, dst_stride, width,
                                       height, threshold, value);
}

kleidicv_error_t kleidicv_thread_scale_u8(const uint8_t *src, size_t src_stride,
                                          uint8_t *dst, size_t dst_stride,
                                          size_t width, size_t height,
                                          double scale, double shift,
                                          kleidicv_thread_multithreading mt) {
  CHECK_POINTER_AND_STRIDE(src, src_stride, height);
  CHECK_POINTER_AND_STRIDE(dst, dst_stride, height);
  CHECK_IMAGE_SIZE(width, height);

  const std::array<uint8_t, 256> precalculated_table =
      kleidicv::neon::precalculate_scale_table_u8(scale, shift);
  return kleidicv_thread_unary_op_impl(
      kleidicv::neon::scale_with_precalculated_table_u8, mt, src, src_stride,
      dst, dst_stride, width, height, scale, shift, precalculated_table);
}

kleidicv_error_t kleidicv_thread_scale_f32(const float *src, size_t src_stride,
                                           float *dst, size_t dst_stride,
                                           size_t width, size_t height,
                                           double scale, double shift,
                                           kleidicv_thread_multithreading mt) {
  return kleidicv_thread_unary_op_impl(kleidicv_scale_f32, mt, src, src_stride,
                                       dst, dst_stride, width, height, scale,
                                       shift);
}

kleidicv_error_t kleidicv_thread_scale_u8_f16(
    const uint8_t *src, size_t src_stride, float16_t *dst, size_t dst_stride,
    size_t width, size_t height, double scale, double shift,
    kleidicv_thread_multithreading mt) {
  return kleidicv_thread_unary_op_impl(kleidicv_scale_u8_f16, mt, src,
                                       src_stride, dst, dst_stride, width,
                                       height, scale, shift);
}

#define KLEIDICV_THREAD_BINARY_OP_IMPL(suffix, type)                         \
  kleidicv_error_t kleidicv_thread_##suffix(                                 \
      const type *src_a, size_t src_a_stride, const type *src_b,             \
      size_t src_b_stride, type *dst, size_t dst_stride, size_t width,       \
      size_t height, kleidicv_thread_multithreading mt) {                    \
    return kleidicv_thread_binary_op_impl(kleidicv_##suffix, mt, src_a,      \
                                          src_a_stride, src_b, src_b_stride, \
                                          dst, dst_stride, width, height);   \
  }

#define KLEIDICV_THREAD_BINARY_OP_SCALE_IMPL(suffix, type, scaletype)         \
  kleidicv_error_t kleidicv_thread_##suffix(                                  \
      const type *src_a, size_t src_a_stride, const type *src_b,              \
      size_t src_b_stride, type *dst, size_t dst_stride, size_t width,        \
      size_t height, scaletype scale, kleidicv_thread_multithreading mt) {    \
    return kleidicv_thread_binary_op_impl(                                    \
        kleidicv_##suffix, mt, src_a, src_a_stride, src_b, src_b_stride, dst, \
        dst_stride, width, height, scale);                                    \
  }

KLEIDICV_THREAD_BINARY_OP_IMPL(saturating_add_s8, int8_t);
KLEIDICV_THREAD_BINARY_OP_IMPL(saturating_add_u8, uint8_t);
KLEIDICV_THREAD_BINARY_OP_IMPL(saturating_add_s16, int16_t);
KLEIDICV_THREAD_BINARY_OP_IMPL(saturating_add_u16, uint16_t);
KLEIDICV_THREAD_BINARY_OP_IMPL(saturating_add_s32, int32_t);
KLEIDICV_THREAD_BINARY_OP_IMPL(saturating_add_u32, uint32_t);
KLEIDICV_THREAD_BINARY_OP_IMPL(saturating_add_s64, int64_t);
KLEIDICV_THREAD_BINARY_OP_IMPL(saturating_add_u64, uint64_t);
KLEIDICV_THREAD_BINARY_OP_IMPL(saturating_sub_s8, int8_t);
KLEIDICV_THREAD_BINARY_OP_IMPL(saturating_sub_u8, uint8_t);
KLEIDICV_THREAD_BINARY_OP_IMPL(saturating_sub_s16, int16_t);
KLEIDICV_THREAD_BINARY_OP_IMPL(saturating_sub_u16, uint16_t);
KLEIDICV_THREAD_BINARY_OP_IMPL(saturating_sub_s32, int32_t);
KLEIDICV_THREAD_BINARY_OP_IMPL(saturating_sub_u32, uint32_t);
KLEIDICV_THREAD_BINARY_OP_IMPL(saturating_sub_s64, int64_t);
KLEIDICV_THREAD_BINARY_OP_IMPL(saturating_sub_u64, uint64_t);
KLEIDICV_THREAD_BINARY_OP_IMPL(saturating_absdiff_u8, uint8_t);
KLEIDICV_THREAD_BINARY_OP_IMPL(saturating_absdiff_s8, int8_t);
KLEIDICV_THREAD_BINARY_OP_IMPL(saturating_absdiff_u16, uint16_t);
KLEIDICV_THREAD_BINARY_OP_IMPL(saturating_absdiff_s16, int16_t);
KLEIDICV_THREAD_BINARY_OP_IMPL(saturating_absdiff_s32, int32_t);
KLEIDICV_THREAD_BINARY_OP_SCALE_IMPL(saturating_multiply_u8, uint8_t, double);
KLEIDICV_THREAD_BINARY_OP_SCALE_IMPL(saturating_multiply_s8, int8_t, double);
KLEIDICV_THREAD_BINARY_OP_SCALE_IMPL(saturating_multiply_u16, uint16_t, double);
KLEIDICV_THREAD_BINARY_OP_SCALE_IMPL(saturating_multiply_s16, int16_t, double);
KLEIDICV_THREAD_BINARY_OP_SCALE_IMPL(saturating_multiply_s32, int32_t, double);
KLEIDICV_THREAD_BINARY_OP_IMPL(bitwise_and, uint8_t);
KLEIDICV_THREAD_BINARY_OP_IMPL(compare_equal_u8, uint8_t);
KLEIDICV_THREAD_BINARY_OP_IMPL(compare_greater_u8, uint8_t);

kleidicv_error_t kleidicv_thread_saturating_add_abs_with_threshold_s16(
    const int16_t *src_a, size_t src_a_stride, const int16_t *src_b,
    size_t src_b_stride, int16_t *dst, size_t dst_stride, size_t width,
    size_t height, int16_t threshold, kleidicv_thread_multithreading mt) {
  return kleidicv_thread_binary_op_impl(
      kleidicv_saturating_add_abs_with_threshold_s16, mt, src_a, src_a_stride,
      src_b, src_b_stride, dst, dst_stride, width, height, threshold);
}

kleidicv_error_t kleidicv_thread_transpose(const void *src, size_t src_stride,
                                           void *dst, size_t dst_stride,
                                           size_t src_width, size_t src_height,
                                           size_t pixel_size,
                                           kleidicv_thread_multithreading mt) {
  if (src == dst) {
    if (src_width != src_height) {
      // In-place transpose only implemented if width and height are the same.
      return KLEIDICV_ERROR_NOT_IMPLEMENTED;
    }
    // In-place transpose swaps elements across the diagonal of the same
    // matrix, so parallel tiles can touch overlapping rows/cols and race.
    // Call the single-threaded implementation to avoid data corruption.
    return kleidicv_transpose(src, src_stride, dst, dst_stride, src_width,
                              src_height, pixel_size);
  }
  // Validate supported element sizes before spinning up worker threads for a
  // call that the underlying transpose implementation would reject anyway.
  switch (pixel_size) {
    case sizeof(uint8_t):
    case sizeof(uint16_t):
    case sizeof(uint8_t) * 3:
    case sizeof(uint16_t) * 3:
    case sizeof(uint32_t):
    case sizeof(uint64_t):
      break;
    default:
      return KLEIDICV_ERROR_NOT_IMPLEMENTED;
  }
  auto callback = [=](size_t begin, size_t end) {
    return kleidicv_transpose(
        static_cast<const uint8_t *>(src) + begin * pixel_size, src_stride,
        static_cast<uint8_t *>(dst) + begin * dst_stride, dst_stride,
        end - begin, src_height, pixel_size);
  };
  return parallel_batches(callback, mt, src_width, 64);
}

kleidicv_error_t kleidicv_thread_flip(const void *src, size_t src_stride,
                                      size_t width, size_t height, void *dst,
                                      size_t dst_stride,
                                      kleidicv_flip_mode_t flip_mode,
                                      size_t pixel_size,
                                      kleidicv_thread_multithreading mt) {
  kleidicv_error_t error = kleidicv::flip_validate(
      src, src_stride, width, height, dst, dst_stride, flip_mode, pixel_size);
  if (error != KLEIDICV_OK) {
    return error;
  }
  if (width == 0 || height == 0) {
    return KLEIDICV_OK;
  }

  const size_t work_item_count =
      kleidicv::flip_work_item_count(height, src == dst, flip_mode);
  if (work_item_count == 0) {
    return KLEIDICV_OK;
  }

  auto callback = [=](size_t work_items_begin, size_t work_items_end) {
    return kleidicv_flip_work_items(src, src_stride, width, height, dst,
                                    dst_stride, flip_mode, pixel_size,
                                    work_items_begin, work_items_end);
  };
  return parallel_batches(callback, mt, work_item_count);
}

kleidicv_error_t kleidicv_thread_rotate(const void *src, size_t src_stride,
                                        size_t width, size_t height, void *dst,
                                        size_t dst_stride, int angle,
                                        size_t pixel_size,
                                        kleidicv_thread_multithreading mt) {
  if (!kleidicv::rotate_is_implemented(src, dst, angle, pixel_size)) {
    return KLEIDICV_ERROR_NOT_IMPLEMENTED;
  }
  // reading in columns and writing out rows tends to perform better
  auto callback = [=](size_t begin, size_t end) {
    const size_t dst_column_offset = (angle == 90) ? begin : (width - end);
    return kleidicv_rotate(
        static_cast<const uint8_t *>(src) + begin * pixel_size, src_stride,
        end - begin, height,
        static_cast<uint8_t *>(dst) + dst_column_offset * dst_stride,
        dst_stride, angle, pixel_size);
  };
  return parallel_batches(callback, mt, width, 64);
}

kleidicv_error_t kleidicv_thread_add_padding_by_copy(
    const void *src, size_t src_stride, void *dst, size_t dst_stride,
    size_t src_width, size_t src_height, size_t top_padding,
    size_t bottom_padding, size_t left_padding, size_t right_padding,
    size_t pixel_size, kleidicv_border_type_t border_type,
    const void *border_value, kleidicv_thread_multithreading mt) {
  const auto *src_bytes = reinterpret_cast<const uint8_t *>(src);
  auto *dst_bytes = reinterpret_cast<uint8_t *>(dst);

  auto result = kleidicv::add_padding_by_copy_checks(
      src_bytes, src_stride, dst_bytes, dst_stride, src_width, src_height,
      top_padding, bottom_padding, left_padding, right_padding, pixel_size,
      border_type, border_value);
  if (std::holds_alternative<kleidicv_error_t>(result)) {
    return std::get<kleidicv_error_t>(result);
  }
  const size_t dst_height = std::get<size_t>(result);

  const kleidicv::AddPaddingByCopyBorderStrategy strategy =
      kleidicv::resolve_border_strategy(src_width, left_padding, right_padding,
                                        border_type);

  const kleidicv::AddPaddingByCopyParameters parameters{
      src_bytes,  dst_bytes,   src_stride,     dst_stride,   src_width,
      src_height, top_padding, bottom_padding, left_padding, right_padding,
      pixel_size, border_type, border_value};

  auto operation =
      kleidicv_create_add_padding_by_copy_operation(parameters, strategy);

  if (!operation) {
    return KLEIDICV_ERROR_ALLOCATION;
  }

  auto callback = [&operation](size_t begin, size_t end) {
    return operation->process_stripe(begin, end);
  };

  return parallel_batches(callback, mt, dst_height, 4);
}

kleidicv_error_t kleidicv_thread_yuv_to_rgb_u8(
    const uint8_t *src, size_t src_stride, uint8_t *dst, size_t dst_stride,
    size_t width, size_t height, kleidicv_color_conversion_t color_format,
    kleidicv_thread_multithreading mt) {
  // Extract the base format
  const size_t base_format = static_cast<size_t>(
      color_format & KLEIDICV_COLOR_CONVERSION_YUV_FMT_MASK);
  if (base_format == KLEIDICV_COLOR_CONVERSION_FMT_YUV444) {
    return kleidicv_thread_unary_op_impl(kleidicv_yuv444_to_rgb_u8, mt, src,
                                         src_stride, dst, dst_stride, width,
                                         height, color_format);
  }

  if (base_format == KLEIDICV_COLOR_CONVERSION_FMT_YUV422) {
    return kleidicv_thread_unary_op_impl(kleidicv_yuv422_to_rgb_u8, mt, src,
                                         src_stride, dst, dst_stride, width,
                                         height, color_format);
  }

  if (base_format == KLEIDICV_COLOR_CONVERSION_FMT_YUV420SP) {
    if (src == nullptr) {
      return KLEIDICV_ERROR_NULL_POINTER;
    }
    const uint8_t *src_uv = src + src_stride * height;
    return kleidicv_thread_yuv_semiplanar_to_rgb_u8(
        src, src_stride, src_uv, src_stride, dst, dst_stride, width, height,
        color_format, mt);
  }

  auto callback = [=](size_t begin, size_t end) {
    return kleidicv_yuv420p_to_rgb_stripe_u8(src, src_stride, dst, dst_stride,
                                             width, height, color_format, begin,
                                             end);
  };
  return parallel_batches(callback, mt, (height + 1) / 2);
}

kleidicv_error_t kleidicv_thread_rgb_to_yuv_semiplanar_u8(
    const uint8_t *src, size_t src_stride, uint8_t *y_dst, size_t y_stride,
    uint8_t *uv_dst, size_t uv_stride, size_t width, size_t height,
    kleidicv_color_conversion_t color_format,
    kleidicv_thread_multithreading mt) {
  auto callback = [=](size_t begin, size_t end) {
    return kleidicv_rgb_to_yuv420sp_stripe_u8(src, src_stride, y_dst, y_stride,
                                              uv_dst, uv_stride, width, height,
                                              color_format, begin, end);
  };
  return parallel_batches(callback, mt, (height + 1) / 2);
}

kleidicv_error_t kleidicv_thread_rgb_to_yuv_u8(
    const uint8_t *src, size_t src_stride, uint8_t *dst, size_t dst_stride,
    size_t width, size_t height, kleidicv_color_conversion_t color_format,
    kleidicv_thread_multithreading mt) {
  // Extract the base format
  const size_t base_format = static_cast<size_t>(
      color_format & KLEIDICV_COLOR_CONVERSION_YUV_FMT_MASK);
  if (base_format == KLEIDICV_COLOR_CONVERSION_FMT_YUV444) {
    return kleidicv_thread_unary_op_impl(kleidicv_rgb_to_yuv444_u8, mt, src,
                                         src_stride, dst, dst_stride, width,
                                         height, color_format);
  }

  if (base_format == KLEIDICV_COLOR_CONVERSION_FMT_YUV422) {
    return kleidicv_thread_unary_op_impl(kleidicv_rgb_to_yuv422_u8, mt, src,
                                         src_stride, dst, dst_stride, width,
                                         height, color_format);
  }

  auto callback = [=](size_t begin, size_t end) {
    return kleidicv_rgb_to_yuv420p_stripe_u8(src, src_stride, dst, dst_stride,
                                             width, height, color_format, begin,
                                             end);
  };
  return parallel_batches(callback, mt, (height + 1) / 2);
}

kleidicv_error_t kleidicv_thread_yuv_semiplanar_to_rgb_u8(
    const uint8_t *src_y, size_t src_y_stride, const uint8_t *src_uv,
    size_t src_uv_stride, uint8_t *dst, size_t dst_stride, size_t width,
    size_t height, kleidicv_color_conversion_t color_format,
    kleidicv_thread_multithreading mt) {
  if (src_y == nullptr || src_uv == nullptr || dst == nullptr) {
    return KLEIDICV_ERROR_NULL_POINTER;
  }

  auto callback = [=](size_t begin, size_t end) {
    size_t row_begin = begin * 2;
    size_t row_end = std::min<size_t>(height, end * 2);
    size_t row_uv = begin;
    return kleidicv_yuv_semiplanar_to_rgb_u8(
        src_y + row_begin * src_y_stride, src_y_stride,
        src_uv + row_uv * src_uv_stride, src_uv_stride,
        dst + row_begin * dst_stride, dst_stride, width, row_end - row_begin,
        color_format);
  };
  return parallel_batches(callback, mt, (height + 1) / 2);
}

template <typename T>
void reduce_min(const T *values, size_t count, T *result) {
  if (!result) {
    return;
  }
  *result = std::numeric_limits<T>::max();
  for (size_t i = 0; i < count; ++i) {
    *result = std::min(*result, values[i]);
  }
}

template <typename T>
void reduce_max(const T *values, size_t count, T *result) {
  if (!result) {
    return;
  }
  *result = std::numeric_limits<T>::lowest();
  for (size_t i = 0; i < count; ++i) {
    *result = std::max(*result, values[i]);
  }
}

template <typename ScalarType, typename FunctionType>
kleidicv_error_t parallel_min_max(FunctionType min_max_func,
                                  const ScalarType *src, size_t src_stride,
                                  size_t width, size_t height,
                                  ScalarType *p_min_value,
                                  ScalarType *p_max_value,
                                  kleidicv_thread_multithreading mt) {
  const size_t value_count = std::max<size_t>(1, height);
  kleidicv::thread_internal::HeapArray<ScalarType> min_values;
  kleidicv::thread_internal::HeapArray<ScalarType> max_values;
  if ((p_min_value &&
       !min_values.allocate_and_fill(value_count,
                                     std::numeric_limits<ScalarType>::max())) ||
      (p_max_value &&
       !max_values.allocate_and_fill(
           value_count, std::numeric_limits<ScalarType>::lowest()))) {
    return KLEIDICV_ERROR_ALLOCATION;
  }

  auto callback = [&](size_t begin, size_t end) {
    return min_max_func(src + begin * (src_stride / sizeof(ScalarType)),
                        src_stride, width, end - begin,
                        p_min_value ? min_values.data() + begin : nullptr,
                        p_max_value ? max_values.data() + begin : nullptr);
  };

  auto return_val = parallel_batches(callback, mt, height);
  reduce_min(min_values.data(), height, p_min_value);
  reduce_max(max_values.data(), height, p_max_value);
  return return_val;
}

#define DEFINE_KLEIDICV_THREAD_MIN_MAX(suffix, type)                           \
  kleidicv_error_t kleidicv_thread_min_max_##suffix(                           \
      const type *src, size_t src_stride, size_t width, size_t height,         \
      type *p_min_value, type *p_max_value,                                    \
      kleidicv_thread_multithreading mt) {                                     \
    return parallel_min_max(kleidicv_min_max_##suffix, src, src_stride, width, \
                            height, p_min_value, p_max_value, mt);             \
  }

DEFINE_KLEIDICV_THREAD_MIN_MAX(u8, uint8_t);
DEFINE_KLEIDICV_THREAD_MIN_MAX(s8, int8_t);
DEFINE_KLEIDICV_THREAD_MIN_MAX(u16, uint16_t);
DEFINE_KLEIDICV_THREAD_MIN_MAX(s16, int16_t);
DEFINE_KLEIDICV_THREAD_MIN_MAX(s32, int32_t);
DEFINE_KLEIDICV_THREAD_MIN_MAX(f32, float);

template <typename ScalarType, typename Compare>
void reduce_offset(const ScalarType *src, size_t src_stride,
                   const size_t *offsets, size_t count, size_t *result,
                   Compare compare) {
  if (!result) {
    return;
  }
  *result = 0;
  for (size_t i = 0; i < count; ++i) {
    const size_t offset = offsets[i] + i * src_stride;
    if (compare(src[offset / sizeof(ScalarType)],
                src[*result / sizeof(ScalarType)])) {
      *result = offset;
    }
  }
}

template <typename ScalarType, typename FunctionType>
kleidicv_error_t parallel_min_max_loc(FunctionType min_max_loc_func,
                                      const ScalarType *src, size_t src_stride,
                                      size_t width, size_t height,
                                      size_t *p_min_offset,
                                      size_t *p_max_offset,
                                      kleidicv_thread_multithreading mt) {
  const size_t value_count = std::max<size_t>(1, height);
  kleidicv::thread_internal::HeapArray<size_t> min_offsets;
  kleidicv::thread_internal::HeapArray<size_t> max_offsets;
  if ((p_min_offset && !min_offsets.allocate_and_fill(value_count, 0)) ||
      (p_max_offset && !max_offsets.allocate_and_fill(value_count, 0))) {
    return KLEIDICV_ERROR_ALLOCATION;
  }

  auto callback = [&](size_t begin, size_t end) {
    return min_max_loc_func(
        src + begin * (src_stride / sizeof(ScalarType)), src_stride, width,
        end - begin, p_min_offset ? min_offsets.data() + begin : nullptr,
        p_max_offset ? max_offsets.data() + begin : nullptr);
  };
  auto return_val = parallel_batches(callback, mt, height);
  reduce_offset(src, src_stride, min_offsets.data(), height, p_min_offset,
                [](ScalarType lhs, ScalarType rhs) { return lhs < rhs; });
  reduce_offset(src, src_stride, max_offsets.data(), height, p_max_offset,
                [](ScalarType lhs, ScalarType rhs) { return lhs > rhs; });
  return return_val;
}

#define DEFINE_KLEIDICV_THREAD_MIN_MAX_LOC(suffix, type)                 \
  kleidicv_error_t kleidicv_thread_min_max_loc_##suffix(                 \
      const type *src, size_t src_stride, size_t width, size_t height,   \
      size_t *p_min_offset, size_t *p_max_offset,                        \
      kleidicv_thread_multithreading mt) {                               \
    return parallel_min_max_loc(kleidicv_min_max_loc_##suffix, src,      \
                                src_stride, width, height, p_min_offset, \
                                p_max_offset, mt);                       \
  }

DEFINE_KLEIDICV_THREAD_MIN_MAX_LOC(u8, uint8_t);

kleidicv_error_t kleidicv_thread_gaussian_blur_u8(
    const uint8_t *src, size_t src_stride, uint8_t *dst, size_t dst_stride,
    size_t width, size_t height, size_t channels, size_t kernel_width,
    size_t kernel_height, float sigma_x, float sigma_y,
    kleidicv_border_type_t border_type, kleidicv_thread_multithreading mt) {
  const auto validation = kleidicv::gaussian_blur_validate(
      src, src_stride, dst, dst_stride, width, height, channels, kernel_width,
      kernel_height, sigma_x, sigma_y, border_type);
  if (validation.error != KLEIDICV_OK) {
    return validation.error;
  }

  if (kernel_width <= 9 || kernel_width == 15 || kernel_width == 21) {
#if KLEIDICV_ENABLE_SME
    if (kHwCapsHasSme) {
      auto callback = [=](size_t y_begin, size_t y_end) {
        auto sme_callback = [=]() {
          return kleidicv_gaussian_blur_fixed_stripe_u8_sme(
              src, src_stride, dst, dst_stride, width, height, y_begin, y_end,
              channels, kernel_width, kernel_height, sigma_x, sigma_y,
              validation.fixed_border_type);
        };

        auto sme_call_result_pair = try_to_run_sme_thread(sme_callback);
        if (sme_call_result_pair.did_run) {
          return sme_call_result_pair.error;
        }

        return kleidicv_gaussian_blur_fixed_stripe_u8(
            src, src_stride, dst, dst_stride, width, height, y_begin, y_end,
            channels, kernel_width, kernel_height, sigma_x, sigma_y,
            validation.fixed_border_type);
      };
      return parallel_batches(callback, mt, height);
    }
#endif
    auto callback = [=](size_t y_begin, size_t y_end) {
      return kleidicv_gaussian_blur_fixed_stripe_u8(
          src, src_stride, dst, dst_stride, width, height, y_begin, y_end,
          channels, kernel_width, kernel_height, sigma_x, sigma_y,
          validation.fixed_border_type);
    };
    return parallel_batches(callback, mt, height);
  }
  auto callback = [=](size_t y_begin, size_t y_end) {
    return kleidicv_gaussian_blur_arbitrary_stripe_u8(
        src, src_stride, dst, dst_stride, width, height, y_begin, y_end,
        channels, kernel_width, kernel_height, sigma_x, sigma_y,
        validation.fixed_border_type);
  };
  return parallel_batches(callback, mt, height);
}

kleidicv_error_t kleidicv_thread_separable_filter_2d_u8(
    const uint8_t *src, size_t src_stride, uint8_t *dst, size_t dst_stride,
    size_t width, size_t height, size_t channels, const uint8_t *kernel_x,
    size_t kernel_width, const uint8_t *kernel_y, size_t kernel_height,
    kleidicv_border_type_t border_type, kleidicv_thread_multithreading mt) {
  const auto validation = kleidicv::separable_filter_2d_validate(
      src, src_stride, dst, dst_stride, width, height, channels, kernel_x,
      kernel_width, kernel_y, kernel_height, border_type);
  if (validation.error != KLEIDICV_OK) {
    return validation.error;
  }

  auto callback = [=](size_t y_begin, size_t y_end) {
    return kleidicv_separable_filter_2d_stripe_u8(
        src, src_stride, dst, dst_stride, width, height, y_begin, y_end,
        channels, kernel_x, kernel_width, kernel_y, kernel_height,
        validation.fixed_border_type);
  };
  return parallel_batches(callback, mt, height);
}

kleidicv_error_t kleidicv_thread_separable_filter_2d_u16(
    const uint16_t *src, size_t src_stride, uint16_t *dst, size_t dst_stride,
    size_t width, size_t height, size_t channels, const uint16_t *kernel_x,
    size_t kernel_width, const uint16_t *kernel_y, size_t kernel_height,
    kleidicv_border_type_t border_type, kleidicv_thread_multithreading mt) {
  const auto validation = kleidicv::separable_filter_2d_validate(
      src, src_stride, dst, dst_stride, width, height, channels, kernel_x,
      kernel_width, kernel_y, kernel_height, border_type);
  if (validation.error != KLEIDICV_OK) {
    return validation.error;
  }

  auto callback = [=](size_t y_begin, size_t y_end) {
    return kleidicv_separable_filter_2d_stripe_u16(
        src, src_stride, dst, dst_stride, width, height, y_begin, y_end,
        channels, kernel_x, kernel_width, kernel_y, kernel_height,
        validation.fixed_border_type);
  };
  return parallel_batches(callback, mt, height);
}

kleidicv_error_t kleidicv_thread_blur_and_downsample_u8(
    const uint8_t *src, size_t src_stride, size_t src_width, size_t src_height,
    uint8_t *dst, size_t dst_stride, size_t channels,
    kleidicv_border_type_t border_type, kleidicv_thread_multithreading mt) {
  const auto validation = kleidicv::blur_and_downsample_validate(
      src, src_stride, src_width, src_height, dst, dst_stride, channels,
      border_type);
  if (validation.error != KLEIDICV_OK) {
    return validation.error;
  }

  auto callback = [=](size_t y_begin, size_t y_end) {
    return kleidicv_blur_and_downsample_stripe_u8(
        src, src_stride, src_width, src_height, dst, dst_stride, y_begin, y_end,
        channels, validation.fixed_border_type);
  };
  return parallel_batches(callback, mt, src_height);
}

kleidicv_error_t kleidicv_thread_sobel_3x3_horizontal_s16_u8(
    const uint8_t *src, size_t src_stride, int16_t *dst, size_t dst_stride,
    size_t width, size_t height, size_t channels,
    kleidicv_thread_multithreading mt) {
  if (kleidicv_error_t err = kleidicv::sobel_validate(
          src, src_stride, dst, dst_stride, width, height, channels)) {
    return err;
  }

#if KLEIDICV_ENABLE_SME
  if (kHwCapsHasSme) {
    auto callback = [=](size_t y_begin, size_t y_end) {
      auto sme_callback = [=]() {
        return kleidicv_sobel_3x3_horizontal_stripe_s16_u8_sme(
            src, src_stride, dst, dst_stride, width, height, y_begin, y_end,
            channels);
      };

      auto sme_call_result_pair = try_to_run_sme_thread(sme_callback);
      if (sme_call_result_pair.did_run) {
        return sme_call_result_pair.error;
      }

      return kleidicv_sobel_3x3_horizontal_stripe_s16_u8(
          src, src_stride, dst, dst_stride, width, height, y_begin, y_end,
          channels);
    };
    return parallel_batches(callback, mt, height);
  }
#endif

  auto callback = [=](size_t y_begin, size_t y_end) {
    return kleidicv_sobel_3x3_horizontal_stripe_s16_u8(
        src, src_stride, dst, dst_stride, width, height, y_begin, y_end,
        channels);
  };
  return parallel_batches(callback, mt, height);
}

kleidicv_error_t kleidicv_thread_median_blur_u8(
    const uint8_t *src, size_t src_stride, uint8_t *dst, size_t dst_stride,
    size_t width, size_t height, size_t channels, size_t kernel_width,
    size_t kernel_height, kleidicv_border_type_t border_type,
    kleidicv_thread_multithreading mt) {
  const auto validation = kleidicv::median_blur_validate(
      src, src_stride, dst, dst_stride, width, height, channels, kernel_width,
      kernel_height, border_type);
  if (validation.error != KLEIDICV_OK) {
    return validation.error;
  }

  if (kernel_width <= 7) {
#if KLEIDICV_ENABLE_SME
    if (kHwCapsHasSme) {
      auto callback = [=](size_t y_begin, size_t y_end) {
        auto sme_callback = [=]() {
          return kleidicv_median_blur_sorting_network_stripe_u8_sme(
              src, src_stride, dst, dst_stride, width, height, y_begin, y_end,
              channels, kernel_width, kernel_height,
              validation.fixed_border_type);
        };

        auto sme_call_result_pair = try_to_run_sme_thread(sme_callback);
        if (sme_call_result_pair.did_run) {
          return sme_call_result_pair.error;
        }

        return kleidicv_median_blur_sorting_network_stripe_u8(
            src, src_stride, dst, dst_stride, width, height, y_begin, y_end,
            channels, kernel_width, kernel_height,
            validation.fixed_border_type);
      };
      return parallel_batches(callback, mt, height);
    }
#endif

    auto callback = [=](size_t y_begin, size_t y_end) {
      return kleidicv_median_blur_sorting_network_stripe_u8(
          src, src_stride, dst, dst_stride, width, height, y_begin, y_end,
          channels, kernel_width, kernel_height, validation.fixed_border_type);
    };
    return parallel_batches(callback, mt, height);
  }

  if (kernel_width > 7 && kernel_width <= 15) {
    auto callback = [=](size_t y_begin, size_t y_end) {
      return kleidicv_median_blur_small_hist_stripe_u8(
          src, src_stride, dst, dst_stride, width, height, y_begin, y_end,
          channels, kernel_width, kernel_height, validation.fixed_border_type);
    };
    return parallel_batches(callback, mt, height);
  }

  auto callback = [=](size_t y_begin, size_t y_end) {
    return kleidicv_median_blur_large_hist_stripe_u8(
        src, src_stride, dst, dst_stride, width, height, y_begin, y_end,
        channels, kernel_width, kernel_height, validation.fixed_border_type);
  };
  return parallel_batches(callback, mt, height);
}

kleidicv_error_t kleidicv_thread_median_blur_s16(
    const int16_t *src, size_t src_stride, int16_t *dst, size_t dst_stride,
    size_t width, size_t height, size_t channels, size_t kernel_width,
    size_t kernel_height, kleidicv_border_type_t border_type,
    kleidicv_thread_multithreading mt) {
  const auto validation = kleidicv::median_blur_validate(
      src, src_stride, dst, dst_stride, width, height, channels, kernel_width,
      kernel_height, border_type);
  if (validation.error != KLEIDICV_OK) {
    return validation.error;
  }

#if KLEIDICV_ENABLE_SME
  if (kHwCapsHasSme) {
    auto callback = [=](size_t y_begin, size_t y_end) {
      auto sme_callback = [=]() {
        return kleidicv_median_blur_sorting_network_stripe_s16_sme(
            src, src_stride, dst, dst_stride, width, height, y_begin, y_end,
            channels, kernel_width, kernel_height,
            validation.fixed_border_type);
      };

      auto sme_call_result_pair = try_to_run_sme_thread(sme_callback);
      if (sme_call_result_pair.did_run) {
        return sme_call_result_pair.error;
      }

      return kleidicv_median_blur_sorting_network_stripe_s16(
          src, src_stride, dst, dst_stride, width, height, y_begin, y_end,
          channels, kernel_width, kernel_height, validation.fixed_border_type);
    };
    return parallel_batches(callback, mt, height);
  }
#endif

  auto callback = [=](size_t y_begin, size_t y_end) {
    return kleidicv_median_blur_sorting_network_stripe_s16(
        src, src_stride, dst, dst_stride, width, height, y_begin, y_end,
        channels, kernel_width, kernel_height, validation.fixed_border_type);
  };
  return parallel_batches(callback, mt, height);
}

kleidicv_error_t kleidicv_thread_median_blur_u16(
    const uint16_t *src, size_t src_stride, uint16_t *dst, size_t dst_stride,
    size_t width, size_t height, size_t channels, size_t kernel_width,
    size_t kernel_height, kleidicv_border_type_t border_type,
    kleidicv_thread_multithreading mt) {
  const auto validation = kleidicv::median_blur_validate(
      src, src_stride, dst, dst_stride, width, height, channels, kernel_width,
      kernel_height, border_type);
  if (validation.error != KLEIDICV_OK) {
    return validation.error;
  }

#if KLEIDICV_ENABLE_SME
  if (kHwCapsHasSme) {
    auto callback = [=](size_t y_begin, size_t y_end) {
      auto sme_callback = [=]() {
        return kleidicv_median_blur_sorting_network_stripe_u16_sme(
            src, src_stride, dst, dst_stride, width, height, y_begin, y_end,
            channels, kernel_width, kernel_height,
            validation.fixed_border_type);
      };

      auto sme_call_result_pair = try_to_run_sme_thread(sme_callback);
      if (sme_call_result_pair.did_run) {
        return sme_call_result_pair.error;
      }

      return kleidicv_median_blur_sorting_network_stripe_u16(
          src, src_stride, dst, dst_stride, width, height, y_begin, y_end,
          channels, kernel_width, kernel_height, validation.fixed_border_type);
    };
    return parallel_batches(callback, mt, height);
  }
#endif

  auto callback = [=](size_t y_begin, size_t y_end) {
    return kleidicv_median_blur_sorting_network_stripe_u16(
        src, src_stride, dst, dst_stride, width, height, y_begin, y_end,
        channels, kernel_width, kernel_height, validation.fixed_border_type);
  };
  return parallel_batches(callback, mt, height);
}

kleidicv_error_t kleidicv_thread_median_blur_f32(
    const float *src, size_t src_stride, float *dst, size_t dst_stride,
    size_t width, size_t height, size_t channels, size_t kernel_width,
    size_t kernel_height, kleidicv_border_type_t border_type,
    kleidicv_thread_multithreading mt) {
  const auto validation = kleidicv::median_blur_validate(
      src, src_stride, dst, dst_stride, width, height, channels, kernel_width,
      kernel_height, border_type);
  if (validation.error != KLEIDICV_OK) {
    return validation.error;
  }

#if KLEIDICV_ENABLE_SME
  if (kHwCapsHasSme) {
    auto callback = [=](size_t y_begin, size_t y_end) {
      auto sme_callback = [=]() {
        return kleidicv_median_blur_sorting_network_stripe_f32_sme(
            src, src_stride, dst, dst_stride, width, height, y_begin, y_end,
            channels, kernel_width, kernel_height,
            validation.fixed_border_type);
      };

      auto sme_call_result_pair = try_to_run_sme_thread(sme_callback);
      if (sme_call_result_pair.did_run) {
        return sme_call_result_pair.error;
      }

      return kleidicv_median_blur_sorting_network_stripe_f32(
          src, src_stride, dst, dst_stride, width, height, y_begin, y_end,
          channels, kernel_width, kernel_height, validation.fixed_border_type);
    };
    return parallel_batches(callback, mt, height);
  }
#endif
  auto callback = [=](size_t y_begin, size_t y_end) {
    return kleidicv_median_blur_sorting_network_stripe_f32(
        src, src_stride, dst, dst_stride, width, height, y_begin, y_end,
        channels, kernel_width, kernel_height, validation.fixed_border_type);
  };
  return parallel_batches(callback, mt, height);
}

kleidicv_error_t kleidicv_thread_sobel_3x3_vertical_s16_u8(
    const uint8_t *src, size_t src_stride, int16_t *dst, size_t dst_stride,
    size_t width, size_t height, size_t channels,
    kleidicv_thread_multithreading mt) {
  if (kleidicv_error_t err = kleidicv::sobel_validate(
          src, src_stride, dst, dst_stride, width, height, channels)) {
    return err;
  }

#if KLEIDICV_ENABLE_SME
  if (kHwCapsHasSme) {
    auto callback = [=](size_t y_begin, size_t y_end) {
      auto sme_callback = [=]() {
        return kleidicv_sobel_3x3_vertical_stripe_s16_u8_sme(
            src, src_stride, dst, dst_stride, width, height, y_begin, y_end,
            channels);
      };

      auto sme_call_result_pair = try_to_run_sme_thread(sme_callback);
      if (sme_call_result_pair.did_run) {
        return sme_call_result_pair.error;
      }

      return kleidicv_sobel_3x3_vertical_stripe_s16_u8(
          src, src_stride, dst, dst_stride, width, height, y_begin, y_end,
          channels);
    };
    return parallel_batches(callback, mt, height);
  }
#endif

  auto callback = [=](size_t y_begin, size_t y_end) {
    return kleidicv_sobel_3x3_vertical_stripe_s16_u8(src, src_stride, dst,
                                                     dst_stride, width, height,
                                                     y_begin, y_end, channels);
  };
  return parallel_batches(callback, mt, height);
}

kleidicv_error_t kleidicv_thread_scharr_interleaved_s16_u8(
    const uint8_t *src, size_t src_stride, size_t src_width, size_t src_height,
    size_t src_channels, int16_t *dst, size_t dst_stride,
    kleidicv_thread_multithreading mt) {
  if (kleidicv_error_t err = kleidicv::scharr_interleaved_validate(
          src, src_stride, src_width, src_height, src_channels, dst,
          dst_stride)) {
    return err;
  }

  auto callback = [=](size_t y_begin, size_t y_end) {
    return kleidicv_scharr_interleaved_stripe_s16_u8(
        src, src_stride, src_width, src_height, src_channels, dst, dst_stride,
        y_begin, y_end);
  };

  // height is decremented by 2 as the result has less rows.
  return parallel_batches(callback, mt, src_height - 2);
}

inline kleidicv_error_t kleidicv_thread_resize_linear_fixed_scale_u8(
    const uint8_t *src, size_t src_stride, size_t src_width, size_t src_height,
    uint8_t *dst, size_t dst_stride,
    kleidicv::ResizeLinearFixedScaleStripeU8 stripe_function,
    kleidicv::ResizeLinearFixedScaleStripeU8 sme_stripe_function,
    kleidicv_thread_multithreading mt) {
#if KLEIDICV_ENABLE_SME
  if (kHwCapsHasSme) {
    auto callback = [=](size_t y_begin, size_t y_end) {
      const size_t stripe_y_end = std::min<size_t>(src_height, y_end + 1);
      auto sme_callback = [=]() {
        return sme_stripe_function(src, src_stride, src_width, src_height,
                                   y_begin, stripe_y_end, dst, dst_stride);
      };

      auto sme_call_result_pair = try_to_run_sme_thread(sme_callback);
      if (sme_call_result_pair.did_run) {
        return sme_call_result_pair.error;
      }

      return stripe_function(src, src_stride, src_width, src_height, y_begin,
                             stripe_y_end, dst, dst_stride);
    };
    return parallel_batches(callback, mt, std::max<size_t>(1, src_height - 1));
  }
#else
  static_cast<void>(sme_stripe_function);
#endif

  auto callback = [=](size_t y_begin, size_t y_end) {
    return stripe_function(src, src_stride, src_width, src_height, y_begin,
                           std::min<size_t>(src_height, y_end + 1), dst,
                           dst_stride);
  };
  return parallel_batches(callback, mt, std::max<size_t>(1, src_height - 1));
}

kleidicv_error_t kleidicv_thread_resize_linear_u8(
    const uint8_t *src, size_t src_stride, size_t src_width, size_t src_height,
    uint8_t *dst, size_t dst_stride, size_t dst_width, size_t dst_height,
    size_t channels, kleidicv_thread_multithreading mt) {
  if (!kleidicv::resize_linear_u8_is_implemented(
          src_width, src_height, dst_width, dst_height, channels)) {
    return KLEIDICV_ERROR_NOT_IMPLEMENTED;
  }

  CHECK_POINTER_AND_STRIDE(src, src_stride, src_height);
  CHECK_POINTER_AND_STRIDE(dst, dst_stride, dst_height);

  if (src_width == 0 || src_height == 0) {
    return KLEIDICV_OK;
  }

  if (kleidicv::resize_linear_u8_is_2x2(src_width, src_height, dst_width,
                                        dst_height, channels)) {
    return kleidicv_thread_resize_linear_fixed_scale_u8(
        src, src_stride, src_width, src_height, dst, dst_stride,
        kleidicv_resize_2x2_stripe_u8, kleidicv_resize_2x2_stripe_u8_sme, mt);
  }
  if (kleidicv::resize_linear_u8_is_4x4(src_width, src_height, dst_width,
                                        dst_height, channels)) {
    return kleidicv_thread_resize_linear_fixed_scale_u8(
        src, src_stride, src_width, src_height, dst, dst_stride,
        kleidicv_resize_4x4_stripe_u8, kleidicv_resize_4x4_stripe_u8_sme, mt);
  }

#if KLEIDICV_ENABLE_SME
  if (kHwCapsHasSme) {
    auto callback = [=](size_t y_begin, size_t y_end) {
      auto sme_callback = [=]() {
        return kleidicv::resize_linear_stripe_u8<true>(
            src, src_stride, src_width, src_height, y_begin, y_end, dst,
            dst_stride, dst_width, dst_height, channels);
      };

      auto sme_call_result_pair = try_to_run_sme_thread(sme_callback);
      if (sme_call_result_pair.did_run) {
        return sme_call_result_pair.error;
      }

      return kleidicv::resize_linear_stripe_u8<false>(
          src, src_stride, src_width, src_height, y_begin, y_end, dst,
          dst_stride, dst_width, dst_height, channels);
    };
    return parallel_batches(callback, mt, dst_height);
  }
#endif

  auto callback = [=](size_t y_begin, size_t y_end) {
    return kleidicv::resize_linear_stripe_u8<false>(
        src, src_stride, src_width, src_height, y_begin, y_end, dst, dst_stride,
        dst_width, dst_height, channels);
  };
  return parallel_batches(callback, mt, dst_height);
}

kleidicv_error_t kleidicv_thread_resize_linear_f32(
    const float *src, size_t src_stride, size_t src_width, size_t src_height,
    float *dst, size_t dst_stride, size_t dst_width, size_t dst_height,
    size_t channels, kleidicv_thread_multithreading mt) {
  if (!kleidicv::resize_linear_f32_is_implemented(
          src_width, src_height, dst_width, dst_height, channels)) {
    return KLEIDICV_ERROR_NOT_IMPLEMENTED;
  }

#if KLEIDICV_ENABLE_SME
  if (kHwCapsHasSme) {
    auto callback = [=](size_t y_begin, size_t y_end) {
      auto sme_callback = [=]() {
        return kleidicv_resize_linear_stripe_f32_sme(
            src, src_stride, src_width, src_height, y_begin,
            std::min<size_t>(src_height, y_end + 1), dst, dst_stride, dst_width,
            dst_height);
      };

      auto sme_call_result_pair = try_to_run_sme_thread(sme_callback);
      if (sme_call_result_pair.did_run) {
        return sme_call_result_pair.error;
      }

      return kleidicv_resize_linear_stripe_f32(
          src, src_stride, src_width, src_height, y_begin,
          std::min<size_t>(src_height, y_end + 1), dst, dst_stride, dst_width,
          dst_height);
    };
    return parallel_batches(callback, mt, std::max<size_t>(1, src_height - 1));
  }
#endif

  auto callback = [=](size_t y_begin, size_t y_end) {
    return kleidicv_resize_linear_stripe_f32(
        src, src_stride, src_width, src_height, y_begin,
        std::min<size_t>(src_height, y_end + 1), dst, dst_stride, dst_width,
        dst_height);
  };
  return parallel_batches(callback, mt, std::max<size_t>(1, src_height - 1));
}

kleidicv_error_t kleidicv_thread_remap_s16_u8(
    const uint8_t *src, size_t src_stride, size_t src_width, size_t src_height,
    uint8_t *dst, size_t dst_stride, size_t dst_width, size_t dst_height,
    size_t channels, const int16_t *mapxy, size_t mapxy_stride,
    kleidicv_border_type_t border_type, const uint8_t *border_value,
    kleidicv_thread_multithreading mt) {
  if (!kleidicv::remap_s16_is_implemented<uint8_t>(src_stride, src_width,
                                                   src_height, dst_width,
                                                   border_type, channels)) {
    return KLEIDICV_ERROR_NOT_IMPLEMENTED;
  }
  auto callback = [=](size_t begin, size_t end) {
    return kleidicv_remap_s16_u8(
        src, src_stride, src_width, src_height,
        dst + begin * dst_stride / sizeof(uint8_t), dst_stride, dst_width,
        end - begin, channels,
        mapxy + static_cast<ptrdiff_t>(begin * mapxy_stride / sizeof(int16_t)),
        mapxy_stride, border_type, border_value);
  };
  return parallel_batches(callback, mt, dst_height);
}

kleidicv_error_t kleidicv_thread_remap_s16_u16(
    const uint16_t *src, size_t src_stride, size_t src_width, size_t src_height,
    uint16_t *dst, size_t dst_stride, size_t dst_width, size_t dst_height,
    size_t channels, const int16_t *mapxy, size_t mapxy_stride,
    kleidicv_border_type_t border_type, const uint16_t *border_value,
    kleidicv_thread_multithreading mt) {
  if (!kleidicv::remap_s16_is_implemented<uint16_t>(src_stride, src_width,
                                                    src_height, dst_width,
                                                    border_type, channels)) {
    return KLEIDICV_ERROR_NOT_IMPLEMENTED;
  }
  auto callback = [=](size_t begin, size_t end) {
    return kleidicv_remap_s16_u16(
        src, src_stride, src_width, src_height,
        dst + static_cast<ptrdiff_t>(begin * dst_stride / sizeof(uint16_t)),
        dst_stride, dst_width, end - begin, channels,
        mapxy + static_cast<ptrdiff_t>(begin * mapxy_stride / sizeof(int16_t)),
        mapxy_stride, border_type, border_value);
  };
  return parallel_batches(callback, mt, dst_height);
}

kleidicv_error_t kleidicv_thread_remap_s16point5_u8(
    const uint8_t *src, size_t src_stride, size_t src_width, size_t src_height,
    uint8_t *dst, size_t dst_stride, size_t dst_width, size_t dst_height,
    size_t channels, const int16_t *mapxy, size_t mapxy_stride,
    const uint16_t *mapfrac, size_t mapfrac_stride,
    kleidicv_border_type_t border_type, const uint8_t *border_value,
    kleidicv_thread_multithreading mt) {
  if (!kleidicv::remap_s16point5_is_implemented<uint8_t>(
          src_stride, src_width, src_height, dst_width, border_type,
          channels)) {
    return KLEIDICV_ERROR_NOT_IMPLEMENTED;
  }
  auto callback = [=](size_t begin, size_t end) {
    return kleidicv_remap_s16point5_u8(
        src, src_stride, src_width, src_height,
        dst + begin * dst_stride / sizeof(uint8_t), dst_stride, dst_width,
        end - begin, channels,
        mapxy + static_cast<ptrdiff_t>(begin * mapxy_stride / sizeof(int16_t)),
        mapxy_stride,
        mapfrac +
            static_cast<ptrdiff_t>(begin * mapfrac_stride / sizeof(uint16_t)),
        mapfrac_stride, border_type, border_value);
  };
  return parallel_batches(callback, mt, dst_height);
}

kleidicv_error_t kleidicv_thread_remap_s16point5_u16(
    const uint16_t *src, size_t src_stride, size_t src_width, size_t src_height,
    uint16_t *dst, size_t dst_stride, size_t dst_width, size_t dst_height,
    size_t channels, const int16_t *mapxy, size_t mapxy_stride,
    const uint16_t *mapfrac, size_t mapfrac_stride,
    kleidicv_border_type_t border_type, const uint16_t *border_value,
    kleidicv_thread_multithreading mt) {
  if (!kleidicv::remap_s16point5_is_implemented<uint16_t>(
          src_stride, src_width, src_height, dst_width, border_type,
          channels)) {
    return KLEIDICV_ERROR_NOT_IMPLEMENTED;
  }
  auto callback = [=](size_t begin, size_t end) {
    return kleidicv_remap_s16point5_u16(
        src, src_stride, src_width, src_height,
        dst + static_cast<ptrdiff_t>(begin * dst_stride / sizeof(uint16_t)),
        dst_stride, dst_width, end - begin, channels,
        mapxy + static_cast<ptrdiff_t>(begin * mapxy_stride / sizeof(int16_t)),
        mapxy_stride,
        mapfrac +
            static_cast<ptrdiff_t>(begin * mapfrac_stride / sizeof(uint16_t)),
        mapfrac_stride, border_type, border_value);
  };
  return parallel_batches(callback, mt, dst_height);
}

kleidicv_error_t kleidicv_thread_remap_f32_u8(
    const uint8_t *src, size_t src_stride, size_t src_width, size_t src_height,
    uint8_t *dst, size_t dst_stride, size_t dst_width, size_t dst_height,
    size_t channels, const float *mapx, size_t mapx_stride, const float *mapy,
    size_t mapy_stride, kleidicv_interpolation_type_t interpolation,
    kleidicv_border_type_t border_type, const uint8_t *border_value,
    kleidicv_thread_multithreading mt) {
  if (!kleidicv::remap_f32_is_implemented<uint8_t>(
          src_stride, src_width, src_height, dst_width, dst_height, border_type,
          channels, interpolation)) {
    return KLEIDICV_ERROR_NOT_IMPLEMENTED;
  }
  auto callback = [=](size_t begin, size_t end) {
    return kleidicv_remap_f32_u8(
        src, src_stride, src_width, src_height,
        dst + static_cast<ptrdiff_t>(begin * dst_stride / sizeof(uint8_t)),
        dst_stride, dst_width, end - begin, channels,
        mapx + static_cast<ptrdiff_t>(begin * mapx_stride / sizeof(float)),
        mapx_stride,
        mapy + static_cast<ptrdiff_t>(begin * mapy_stride / sizeof(float)),
        mapy_stride, interpolation, border_type, border_value);
  };
  return parallel_batches(callback, mt, dst_height);
}

kleidicv_error_t kleidicv_thread_remap_f32_u16(
    const uint16_t *src, size_t src_stride, size_t src_width, size_t src_height,
    uint16_t *dst, size_t dst_stride, size_t dst_width, size_t dst_height,
    size_t channels, const float *mapx, size_t mapx_stride, const float *mapy,
    size_t mapy_stride, kleidicv_interpolation_type_t interpolation,
    kleidicv_border_type_t border_type, const uint16_t *border_value,
    kleidicv_thread_multithreading mt) {
  if (!kleidicv::remap_f32_is_implemented<uint16_t>(
          src_stride, src_width, src_height, dst_width, dst_height, border_type,
          channels, interpolation)) {
    return KLEIDICV_ERROR_NOT_IMPLEMENTED;
  }
  auto callback = [=](size_t begin, size_t end) {
    return kleidicv_remap_f32_u16(
        src, src_stride, src_width, src_height,
        dst + static_cast<ptrdiff_t>(begin * dst_stride / sizeof(uint16_t)),
        dst_stride, dst_width, end - begin, channels,
        mapx + static_cast<ptrdiff_t>(begin * mapx_stride / sizeof(float)),
        mapx_stride,
        mapy + static_cast<ptrdiff_t>(begin * mapy_stride / sizeof(float)),
        mapy_stride, interpolation, border_type, border_value);
  };
  return parallel_batches(callback, mt, dst_height);
}

kleidicv_error_t kleidicv_thread_warp_perspective_u8(
    const uint8_t *src, size_t src_stride, size_t src_width, size_t src_height,
    uint8_t *dst, size_t dst_stride, size_t dst_width, size_t dst_height,
    const float transformation[9], size_t channels,
    kleidicv_interpolation_type_t interpolation,
    kleidicv_border_type_t border_type, const uint8_t *border_value,
    kleidicv_thread_multithreading mt) {
  if (!kleidicv::warp_perspective_is_implemented<uint8_t>(
          dst_width, channels, interpolation, border_type)) {
    return KLEIDICV_ERROR_NOT_IMPLEMENTED;
  }

  auto callback = [=](size_t y_begin, size_t y_end) {
    return kleidicv_warp_perspective_stripe_u8(
        src, src_stride, src_width, src_height, dst, dst_stride, dst_width,
        dst_height, y_begin, y_end, transformation, channels, interpolation,
        border_type, border_value);
  };
  return parallel_batches(callback, mt, dst_height);
}
