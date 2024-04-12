// SPDX-FileCopyrightText: 2024 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#include "intrinsiccv/resize/resize_linear.h"
#include "resize_linear_sc.h"

namespace intrinsiccv::sve2 {
KLEIDICV_TARGET_FN_ATTRS
intrinsiccv_error_t resize_linear_u8(const uint8_t *src, size_t src_stride,
                                     size_t src_width, size_t src_height,
                                     uint8_t *dst, size_t dst_stride,
                                     size_t dst_width, size_t dst_height) {
  return resize_linear_u8_sc(src, src_stride, src_width, src_height, dst,
                             dst_stride, dst_width, dst_height);
}

}  // namespace intrinsiccv::sve2
