// SPDX-FileCopyrightText: 2023 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#include "resize/resize.h"
#include "resize_sc.h"

namespace intrinsiccv::sme2 {

INTRINSICCV_LOCALLY_STREAMING INTRINSICCV_TARGET_FN_ATTS void
resize_to_quarter_u8(const uint8_t *src, size_t src_stride, size_t src_width,
                     size_t src_height, uint8_t *dst, size_t dst_stride,
                     size_t dst_width, size_t dst_height) {
  intrinsiccv::sve2::resize_to_quarter_u8_sc(src, src_stride, src_width,
                                             src_height, dst, dst_stride,
                                             dst_width, dst_height);
}

}  // namespace intrinsiccv::sme2
