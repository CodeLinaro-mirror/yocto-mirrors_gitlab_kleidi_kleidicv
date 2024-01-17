// SPDX-FileCopyrightText: 2023 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#include "sobel_sc.h"

namespace intrinsiccv::sme2 {

INTRINSICCV_TARGET_FN_ATTS void sobel_3x3_horizontal_s16_u8(
    const uint8_t *src, size_t src_stride, int16_t *dst, size_t dst_stride,
    size_t width, size_t height, size_t channels) {
  intrinsiccv::sve2::sobel_3x3_horizontal_s16_u8_sc(
      src, src_stride, dst, dst_stride, width, height, channels);
}

INTRINSICCV_TARGET_FN_ATTS void sobel_3x3_vertical_s16_u8(
    const uint8_t *src, size_t src_stride, int16_t *dst, size_t dst_stride,
    size_t width, size_t height, size_t channels) {
  intrinsiccv::sve2::sobel_3x3_vertical_s16_u8_sc(
      src, src_stride, dst, dst_stride, width, height, channels);
}

}  // namespace intrinsiccv::sme2
