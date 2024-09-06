// SPDX-FileCopyrightText: 2024 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#include "kleidicv_thread/kleidicv_thread.h"

#include <algorithm>
#include <functional>
#include <limits>
#include <vector>

#include "kleidicv/filters/gaussian_blur.h"
#include "kleidicv/filters/separable_filter_2d.h"
#include "kleidicv/filters/sobel.h"
#include "kleidicv/kleidicv.h"
#include "kleidicv/resize/resize_linear.h"

typedef std::function<kleidicv_error_t(unsigned, unsigned)> FunctionCallback;

static kleidicv_error_t kleidicv_thread_std_function_callback(
    unsigned task_begin, unsigned task_end, void *data) {
  auto *callback = reinterpret_cast<FunctionCallback *>(data);
  return (*callback)(task_begin, task_end);
}

template <typename SrcT, typename DstT, typename F, typename... Args>
inline kleidicv_error_t kleidicv_thread_unary_op_impl(
    F f, kleidicv_thread_multithreading mt, const SrcT *src, size_t src_stride,
    DstT *dst, size_t dst_stride, size_t width, size_t height, Args... args) {
  FunctionCallback callback = [=](unsigned task_begin, unsigned task_end) {
    return f(src + task_begin * src_stride / sizeof(SrcT), src_stride,
             dst + task_begin * dst_stride / sizeof(DstT), dst_stride, width,
             task_end - task_begin, args...);
  };
  return mt.parallel(kleidicv_thread_std_function_callback, &callback,
                     mt.parallel_data, height);
}

template <typename SrcT, typename DstT, typename F, typename... Args>
inline kleidicv_error_t kleidicv_thread_binary_op_impl(
    F f, kleidicv_thread_multithreading mt, const SrcT *src_a,
    size_t src_a_stride, const SrcT *src_b, size_t src_b_stride, DstT *dst,
    size_t dst_stride, size_t width, size_t height, Args... args) {
  FunctionCallback callback = [=](unsigned task_begin, unsigned task_end) {
    return f(src_a + task_begin * src_a_stride / sizeof(SrcT), src_a_stride,
             src_b + task_begin * src_b_stride / sizeof(SrcT), src_b_stride,
             dst + task_begin * dst_stride / sizeof(DstT), dst_stride, width,
             task_end - task_begin, args...);
  };
  return mt.parallel(kleidicv_thread_std_function_callback, &callback,
                     mt.parallel_data, height);
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
KLEIDICV_THREAD_UNARY_OP_IMPL(yuv_to_bgr_u8, uint8_t, uint8_t);
KLEIDICV_THREAD_UNARY_OP_IMPL(yuv_to_rgb_u8, uint8_t, uint8_t);
KLEIDICV_THREAD_UNARY_OP_IMPL(bgr_to_yuv_u8, uint8_t, uint8_t);
KLEIDICV_THREAD_UNARY_OP_IMPL(rgb_to_yuv_u8, uint8_t, uint8_t);
KLEIDICV_THREAD_UNARY_OP_IMPL(bgra_to_yuv_u8, uint8_t, uint8_t);
KLEIDICV_THREAD_UNARY_OP_IMPL(rgba_to_yuv_u8, uint8_t, uint8_t);
KLEIDICV_THREAD_UNARY_OP_IMPL(exp_f32, float, float);
KLEIDICV_THREAD_UNARY_OP_IMPL(float_conversion_f32_s8, float, int8_t);
KLEIDICV_THREAD_UNARY_OP_IMPL(float_conversion_f32_u8, float, uint8_t);
KLEIDICV_THREAD_UNARY_OP_IMPL(float_conversion_s8_f32, int8_t, float);
KLEIDICV_THREAD_UNARY_OP_IMPL(float_conversion_u8_f32, uint8_t, float);

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
                                          float scale, float shift,
                                          kleidicv_thread_multithreading mt) {
  return kleidicv_thread_unary_op_impl(kleidicv_scale_u8, mt, src, src_stride,
                                       dst, dst_stride, width, height, scale,
                                       shift);
}

kleidicv_error_t kleidicv_thread_scale_f32(const float *src, size_t src_stride,
                                           float *dst, size_t dst_stride,
                                           size_t width, size_t height,
                                           float scale, float shift,
                                           kleidicv_thread_multithreading mt) {
  return kleidicv_thread_unary_op_impl(kleidicv_scale_f32, mt, src, src_stride,
                                       dst, dst_stride, width, height, scale,
                                       shift);
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

template <typename F>
inline kleidicv_error_t kleidicv_thread_yuv_sp_to_rgb_u8_impl(
    F f, const uint8_t *src_y, size_t src_y_stride, const uint8_t *src_uv,
    size_t src_uv_stride, uint8_t *dst, size_t dst_stride, size_t width,
    size_t height, bool is_nv21, kleidicv_thread_multithreading mt) {
  FunctionCallback callback = [=](unsigned task_begin, unsigned task_end) {
    size_t row_begin = size_t{task_begin} * 2;
    size_t row_end = std::min<size_t>(height, size_t{task_end} * 2);
    size_t row_uv = task_begin;
    return f(src_y + row_begin * src_y_stride, src_y_stride,
             src_uv + row_uv * src_uv_stride, src_uv_stride,
             dst + row_begin * dst_stride, dst_stride, width,
             row_end - row_begin, is_nv21);
  };
  return mt.parallel(kleidicv_thread_std_function_callback, &callback,
                     mt.parallel_data, (height + 1) / 2);
}

#define YUV_SP_TO_RGB(suffix)                                               \
  kleidicv_error_t kleidicv_thread_##suffix(                                \
      const uint8_t *src_y, size_t src_y_stride, const uint8_t *src_uv,     \
      size_t src_uv_stride, uint8_t *dst, size_t dst_stride, size_t width,  \
      size_t height, bool is_nv21, kleidicv_thread_multithreading mt) {     \
    return kleidicv_thread_yuv_sp_to_rgb_u8_impl(                           \
        kleidicv_##suffix, src_y, src_y_stride, src_uv, src_uv_stride, dst, \
        dst_stride, width, height, is_nv21, mt);                            \
  }

YUV_SP_TO_RGB(yuv_sp_to_bgr_u8);
YUV_SP_TO_RGB(yuv_sp_to_bgra_u8);
YUV_SP_TO_RGB(yuv_sp_to_rgb_u8);
YUV_SP_TO_RGB(yuv_sp_to_rgba_u8);

template <typename ScalarType, typename FunctionType>
struct parallel_min_max_data {
  FunctionType min_max_func;
  const ScalarType *src;
  size_t src_stride;
  size_t width;
  ScalarType *p_min_value;
  ScalarType *p_max_value;
};

template <typename ScalarType, typename FunctionType>
static kleidicv_error_t kleidicv_thread_min_max_callback(unsigned task_begin,
                                                         unsigned task_end,
                                                         void *void_data) {
  auto *data =
      reinterpret_cast<parallel_min_max_data<ScalarType, FunctionType> *>(
          void_data);

  return data->min_max_func(
      data->src + task_begin * (data->src_stride / sizeof(ScalarType)),
      data->src_stride, data->width, task_end - task_begin,
      data->p_min_value ? data->p_min_value + task_begin : nullptr,
      data->p_max_value ? data->p_max_value + task_begin : nullptr);
}

template <typename ScalarType, typename FunctionType>
kleidicv_error_t parallel_min_max(FunctionType min_max_func,
                                  const ScalarType *src, size_t src_stride,
                                  size_t width, size_t height,
                                  ScalarType *p_min_value,
                                  ScalarType *p_max_value,
                                  kleidicv_thread_multithreading mt) {
  std::vector<ScalarType> min_values(height,
                                     std::numeric_limits<ScalarType>::max());
  std::vector<ScalarType> max_values(height,
                                     std::numeric_limits<ScalarType>::lowest());

  parallel_min_max_data<ScalarType, FunctionType> callback_data = {
      min_max_func,
      src,
      src_stride,
      width,
      p_min_value ? min_values.data() : nullptr,
      p_max_value ? max_values.data() : nullptr};

  auto return_val =
      mt.parallel(kleidicv_thread_min_max_callback<ScalarType, FunctionType>,
                  &callback_data, mt.parallel_data, height);

  if (p_min_value) {
    *p_min_value = std::numeric_limits<ScalarType>::max();
    for (ScalarType m : min_values) {
      if (m < *p_min_value) {
        *p_min_value = m;
      }
    }
  }
  if (p_max_value) {
    *p_max_value = std::numeric_limits<ScalarType>::lowest();
    for (ScalarType m : max_values) {
      if (m > *p_max_value) {
        *p_max_value = m;
      }
    }
  }
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

template <typename ScalarType, typename FunctionType>
struct parallel_min_max_loc_data {
  FunctionType min_max_loc_func;
  const ScalarType *src;
  size_t src_stride;
  size_t width;
  size_t *p_min_offset;
  size_t *p_max_offset;
};

template <typename ScalarType, typename FunctionType>
static kleidicv_error_t kleidicv_thread_min_max_loc_callback(
    unsigned task_begin, unsigned task_end, void *void_data) {
  auto *data =
      reinterpret_cast<parallel_min_max_loc_data<ScalarType, FunctionType> *>(
          void_data);

  return data->min_max_loc_func(
      data->src + task_begin * (data->src_stride / sizeof(ScalarType)),
      data->src_stride, data->width, task_end - task_begin,
      data->p_min_offset ? data->p_min_offset + task_begin : nullptr,
      data->p_max_offset ? data->p_max_offset + task_begin : nullptr);
}

template <typename ScalarType, typename FunctionType>
kleidicv_error_t parallel_min_max_loc(FunctionType min_max_loc_func,
                                      const ScalarType *src, size_t src_stride,
                                      size_t width, size_t height,
                                      size_t *p_min_offset,
                                      size_t *p_max_offset,
                                      kleidicv_thread_multithreading mt) {
  std::vector<size_t> min_offsets(height, 0);
  std::vector<size_t> max_offsets(height, 0);

  parallel_min_max_loc_data<ScalarType, FunctionType> callback_data = {
      min_max_loc_func,
      src,
      src_stride,
      width,
      p_min_offset ? min_offsets.data() : nullptr,
      p_max_offset ? max_offsets.data() : nullptr};

  auto return_val = mt.parallel(
      kleidicv_thread_min_max_loc_callback<ScalarType, FunctionType>,
      &callback_data, mt.parallel_data, height);

  if (p_min_offset) {
    *p_min_offset = 0;
    for (size_t i = 0; i < min_offsets.size(); ++i) {
      size_t offs = min_offsets[i] + i * src_stride;
      if (src[offs / sizeof(ScalarType)] <
          src[*p_min_offset / sizeof(ScalarType)]) {
        *p_min_offset = offs;
      }
    }
  }
  if (p_max_offset) {
    *p_max_offset = 0;
    for (size_t i = 0; i < max_offsets.size(); ++i) {
      size_t offs = max_offsets[i] + i * src_stride;
      if (src[offs / sizeof(ScalarType)] >
          src[*p_max_offset / sizeof(ScalarType)]) {
        *p_max_offset = offs;
      }
    }
  }
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

template <typename F>
kleidicv_error_t kleidicv_thread_filter(F filter, size_t width, size_t height,
                                        size_t channels, size_t kernel_width,
                                        size_t kernel_height,
                                        kleidicv_filter_context_t *context,
                                        kleidicv_thread_multithreading mt) {
  FunctionCallback callback = [=](unsigned y_begin, unsigned y_end) {
    // The context contains a buffer that can only fit a single row, so can't be
    // shared between threads. Since we don't know how many threads there are,
    // create and destroy a context every time this callback is called. Only use
    // the context argument for the first thread.
    bool create_context = 0 != y_begin;
    kleidicv_filter_context_t *thread_context = context;
    if (create_context) {
      kleidicv_error_t context_create_result = kleidicv_filter_context_create(
          &thread_context, channels, kernel_width, kernel_height, width,
          height);
      // Excluded from coverage because it's impractical to test this.
      // MockMallocToFail can't be used because malloc is used in thread setup.
      // GCOVR_EXCL_START
      if (KLEIDICV_OK != context_create_result) {
        return context_create_result;
      }
      // GCOVR_EXCL_STOP
    }

    kleidicv_error_t result = filter(y_begin, y_end, thread_context);

    if (create_context) {
      kleidicv_error_t context_release_result =
          kleidicv_filter_context_release(thread_context);
      if (KLEIDICV_OK == result) {
        result = context_release_result;
      }
    }
    return result;
  };
  return mt.parallel(kleidicv_thread_std_function_callback, &callback,
                     mt.parallel_data, height);
}

kleidicv_error_t kleidicv_thread_gaussian_blur_u8(
    const uint8_t *src, size_t src_stride, uint8_t *dst, size_t dst_stride,
    size_t width, size_t height, size_t channels, size_t kernel_width,
    size_t kernel_height, float sigma_x, float sigma_y,
    kleidicv_border_type_t border_type, kleidicv_filter_context_t *context,
    kleidicv_thread_multithreading mt) {
  auto callback = [=](size_t y_begin, size_t y_end,
                      kleidicv_filter_context_t *thread_context) {
    return kleidicv_gaussian_blur_stripe_u8(
        src, src_stride, dst, dst_stride, width, height, y_begin, y_end,
        channels, kernel_width, kernel_height, sigma_x, sigma_y, border_type,
        thread_context);
  };
  return kleidicv_thread_filter(callback, width, height, channels, kernel_width,
                                kernel_height, context, mt);
}

kleidicv_error_t kleidicv_thread_separable_filter_2d_u8(
    const uint8_t *src, size_t src_stride, uint8_t *dst, size_t dst_stride,
    size_t width, size_t height, size_t channels, const uint8_t *kernel_x,
    size_t kernel_width, const uint8_t *kernel_y, size_t kernel_height,
    kleidicv_border_type_t border_type, kleidicv_filter_context_t *context,
    kleidicv_thread_multithreading mt) {
  auto callback = [=](size_t y_begin, size_t y_end,
                      kleidicv_filter_context_t *thread_context) {
    return kleidicv_separable_filter_2d_stripe_u8(
        src, src_stride, dst, dst_stride, width, height, y_begin, y_end,
        channels, kernel_x, kernel_width, kernel_y, kernel_height, border_type,
        thread_context);
  };
  return kleidicv_thread_filter(callback, width, height, channels, kernel_width,
                                kernel_height, context, mt);
}

kleidicv_error_t kleidicv_thread_separable_filter_2d_u16(
    const uint16_t *src, size_t src_stride, uint16_t *dst, size_t dst_stride,
    size_t width, size_t height, size_t channels, const uint16_t *kernel_x,
    size_t kernel_width, const uint16_t *kernel_y, size_t kernel_height,
    kleidicv_border_type_t border_type, kleidicv_filter_context_t *context,
    kleidicv_thread_multithreading mt) {
  auto callback = [=](size_t y_begin, size_t y_end,
                      kleidicv_filter_context_t *thread_context) {
    return kleidicv_separable_filter_2d_stripe_u16(
        src, src_stride, dst, dst_stride, width, height, y_begin, y_end,
        channels, kernel_x, kernel_width, kernel_y, kernel_height, border_type,
        thread_context);
  };
  return kleidicv_thread_filter(callback, width, height, channels, kernel_width,
                                kernel_height, context, mt);
}

kleidicv_error_t kleidicv_thread_separable_filter_2d_s16(
    const int16_t *src, size_t src_stride, int16_t *dst, size_t dst_stride,
    size_t width, size_t height, size_t channels, const int16_t *kernel_x,
    size_t kernel_width, const int16_t *kernel_y, size_t kernel_height,
    kleidicv_border_type_t border_type, kleidicv_filter_context_t *context,
    kleidicv_thread_multithreading mt) {
  auto callback = [=](size_t y_begin, size_t y_end,
                      kleidicv_filter_context_t *thread_context) {
    return kleidicv_separable_filter_2d_stripe_s16(
        src, src_stride, dst, dst_stride, width, height, y_begin, y_end,
        channels, kernel_x, kernel_width, kernel_y, kernel_height, border_type,
        thread_context);
  };
  return kleidicv_thread_filter(callback, width, height, channels, kernel_width,
                                kernel_height, context, mt);
}

kleidicv_error_t kleidicv_thread_sobel_3x3_horizontal_s16_u8(
    const uint8_t *src, size_t src_stride, int16_t *dst, size_t dst_stride,
    size_t width, size_t height, size_t channels,
    kleidicv_thread_multithreading mt) {
  FunctionCallback callback = [=](unsigned y_begin, unsigned y_end) {
    return kleidicv_sobel_3x3_horizontal_stripe_s16_u8(
        src, src_stride, dst, dst_stride, width, height, y_begin, y_end,
        channels);
  };
  return mt.parallel(kleidicv_thread_std_function_callback, &callback,
                     mt.parallel_data, height);
}

kleidicv_error_t kleidicv_thread_sobel_3x3_vertical_s16_u8(
    const uint8_t *src, size_t src_stride, int16_t *dst, size_t dst_stride,
    size_t width, size_t height, size_t channels,
    kleidicv_thread_multithreading mt) {
  FunctionCallback callback = [=](unsigned y_begin, unsigned y_end) {
    return kleidicv_sobel_3x3_vertical_stripe_s16_u8(src, src_stride, dst,
                                                     dst_stride, width, height,
                                                     y_begin, y_end, channels);
  };
  return mt.parallel(kleidicv_thread_std_function_callback, &callback,
                     mt.parallel_data, height);
}

kleidicv_error_t kleidicv_thread_resize_to_quarter_u8(
    const uint8_t *src, size_t src_stride, size_t src_width, size_t src_height,
    uint8_t *dst, size_t dst_stride, size_t dst_width, size_t dst_height,
    kleidicv_thread_multithreading mt) {
  FunctionCallback callback = [=](unsigned task_begin, unsigned task_end) {
    size_t src_begin = size_t{task_begin} * 2;
    size_t src_end = std::min<size_t>(src_height, size_t{task_end} * 2);
    size_t dst_begin = task_begin;
    size_t dst_end = std::min<size_t>(dst_height, task_end);

    // half of odd height is rounded towards zero?
    if (dst_begin == dst_end) {
      return KLEIDICV_OK;
    }

    return kleidicv_resize_to_quarter_u8(
        src + src_begin * src_stride, src_stride, src_width,
        src_end - src_begin, dst + dst_begin * dst_stride, dst_stride,
        dst_width, dst_end - dst_begin);
  };
  return mt.parallel(kleidicv_thread_std_function_callback, &callback,
                     mt.parallel_data, (src_height + 1) / 2);
}

kleidicv_error_t kleidicv_thread_resize_linear_u8(
    const uint8_t *src, size_t src_stride, size_t src_width, size_t src_height,
    uint8_t *dst, size_t dst_stride, size_t dst_width, size_t dst_height,
    kleidicv_thread_multithreading mt) {
  FunctionCallback callback = [=](unsigned y_begin, unsigned y_end) {
    return kleidicv_resize_linear_stripe_u8(
        src, src_stride, src_width, src_height, y_begin,
        std::min<size_t>(src_height, y_end + 1), dst, dst_stride, dst_width,
        dst_height);
  };
  return mt.parallel(kleidicv_thread_std_function_callback, &callback,
                     mt.parallel_data, std::max<size_t>(1, src_height - 1));
}

kleidicv_error_t kleidicv_thread_resize_linear_f32(
    const float *src, size_t src_stride, size_t src_width, size_t src_height,
    float *dst, size_t dst_stride, size_t dst_width, size_t dst_height,
    kleidicv_thread_multithreading mt) {
  FunctionCallback callback = [=](unsigned y_begin, unsigned y_end) {
    return kleidicv_resize_linear_stripe_f32(
        src, src_stride, src_width, src_height, y_begin,
        std::min<size_t>(src_height, y_end + 1), dst, dst_stride, dst_width,
        dst_height);
  };
  return mt.parallel(kleidicv_thread_std_function_callback, &callback,
                     mt.parallel_data, std::max<size_t>(1, src_height - 1));
}
