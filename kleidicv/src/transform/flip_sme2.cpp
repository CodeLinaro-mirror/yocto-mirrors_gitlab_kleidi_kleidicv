// SPDX-FileCopyrightText: 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#include "flip_sc.h"
#include "kleidicv/transform/flip.h"

namespace kleidicv::sme2 {

KLEIDICV_LOCALLY_STREAMING KLEIDICV_TARGET_FN_ATTRS kleidicv_error_t
flip_work_items(const void *src, size_t src_stride, size_t width, size_t height,
                void *dst, size_t dst_stride, kleidicv_flip_mode_t flip_mode,
                size_t pixel_size, size_t work_items_begin,
                size_t work_items_end) {
  return flip_work_items_impl(src, src_stride, width, height, dst, dst_stride,
                              flip_mode, pixel_size, work_items_begin,
                              work_items_end);
}

}  // namespace kleidicv::sme2
