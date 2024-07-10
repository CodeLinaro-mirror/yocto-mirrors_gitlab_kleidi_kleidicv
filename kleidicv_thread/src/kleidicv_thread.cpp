// SPDX-FileCopyrightText: 2024 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#include "kleidicv_thread/kleidicv_thread.h"

#include <algorithm>

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
