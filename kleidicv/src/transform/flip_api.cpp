// SPDX-FileCopyrightText: 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#include "kleidicv/dispatch.h"
#include "kleidicv/kleidicv.h"
#include "kleidicv/transform/flip.h"

KLEIDICV_MULTIVERSION_C_API_WITH_SME(kleidicv_flip_work_items,
                                     &kleidicv::neon::flip_work_items,
                                     &kleidicv::sve2::flip_work_items,
                                     &kleidicv::sme::flip_work_items,
                                     &kleidicv::sme2::flip_work_items);

namespace {

template <auto &WorkItemFlipFunction>
kleidicv_error_t flip(const void *src, size_t src_stride, size_t width,
                      size_t height, void *dst, size_t dst_stride,
                      kleidicv_flip_mode_t flip_mode, size_t pixel_size) {
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
  return WorkItemFlipFunction(src, src_stride, width, height, dst, dst_stride,
                              flip_mode, pixel_size, 0, work_item_count);
}

}  // namespace

extern "C" {

kleidicv_error_t kleidicv_flip(const void *src, size_t src_stride, size_t width,
                               size_t height, void *dst, size_t dst_stride,
                               kleidicv_flip_mode_t flip_mode,
                               size_t pixel_size) {
  return flip<kleidicv_flip_work_items>(src, src_stride, width, height, dst,
                                        dst_stride, flip_mode, pixel_size);
}

kleidicv_error_t kleidicv_flip_sme(const void *src, size_t src_stride,
                                   size_t width, size_t height, void *dst,
                                   size_t dst_stride,
                                   kleidicv_flip_mode_t flip_mode,
                                   size_t pixel_size) {
  return flip<kleidicv_flip_work_items_sme>(src, src_stride, width, height, dst,
                                            dst_stride, flip_mode, pixel_size);
}

}  // extern "C"
