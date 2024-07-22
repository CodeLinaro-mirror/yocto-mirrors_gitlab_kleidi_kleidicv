// SPDX-FileCopyrightText: 2024 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#include "kleidicv_thread/kleidicv_thread.h"

#include <algorithm>
#include <limits>
#include <vector>

#include "kleidicv/kleidicv.h"

struct kleidicv_thread_yuv_sp_to_rgb_u8_data {
  const uint8_t *src_y;
  size_t src_y_stride;
  const uint8_t *src_uv;
  size_t src_uv_stride;
  uint8_t *dst;
  size_t dst_stride;
  size_t width;
  size_t height;
  bool is_nv21;
};

static kleidicv_error_t kleidicv_thread_yuv_sp_to_rgb_u8_callback(
    unsigned task_begin, unsigned task_end, void *void_data) {
  auto *data =
      reinterpret_cast<kleidicv_thread_yuv_sp_to_rgb_u8_data *>(void_data);

  size_t row_begin = size_t{task_begin} * 2;
  size_t row_end = std::min<size_t>(data->height, size_t{task_end} * 2);
  size_t row_uv = task_begin;

  return kleidicv_yuv_sp_to_rgb_u8(
      data->src_y + row_begin * data->src_y_stride, data->src_y_stride,
      data->src_uv + row_uv * data->src_uv_stride, data->src_uv_stride,
      data->dst + row_begin * data->dst_stride, data->dst_stride, data->width,
      row_end - row_begin, data->is_nv21);
}

kleidicv_error_t kleidicv_thread_yuv_sp_to_rgb_u8(
    const uint8_t *src_y, size_t src_y_stride, const uint8_t *src_uv,
    size_t src_uv_stride, uint8_t *dst, size_t dst_stride, size_t width,
    size_t height, bool is_nv21, kleidicv_thread_multithreading mt) {
  kleidicv_thread_yuv_sp_to_rgb_u8_data callback_data = {
      src_y,      src_y_stride, src_uv, src_uv_stride, dst,
      dst_stride, width,        height, is_nv21};
  return mt.parallel(kleidicv_thread_yuv_sp_to_rgb_u8_callback, &callback_data,
                     mt.parallel_data, (height + 1) / 2);
}

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
                                     std::numeric_limits<ScalarType>::min());

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
    *p_max_value = std::numeric_limits<ScalarType>::min();
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
