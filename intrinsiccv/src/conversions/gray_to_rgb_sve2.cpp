// SPDX-FileCopyrightText: 2023 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#include "gray_to_rgb_sc.h"

namespace intrinsiccv::sve2 {

INTRINSICCV_TARGET_FN_ATTS void gray_to_rgb_u8(const uint8_t *src,
                                               size_t src_stride, uint8_t *dst,
                                               size_t dst_stride, size_t width,
                                               size_t height) {
  gray_to_rgb_u8_sc(src, src_stride, dst, dst_stride, width, height);
}

INTRINSICCV_TARGET_FN_ATTS void gray_to_rgba_u8(const uint8_t *src,
                                                size_t src_stride, uint8_t *dst,
                                                size_t dst_stride, size_t width,
                                                size_t height) {
  gray_to_rgba_u8_sc(src, src_stride, dst, dst_stride, width, height);
}

}  // namespace intrinsiccv::sve2
