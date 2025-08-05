// SPDX-FileCopyrightText: 2025 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#include "yuv_p_to_rgb_sc.h"

namespace kleidicv::sve2 {

KLEIDICV_TARGET_FN_ATTRS
kleidicv_error_t yuv_p_to_rgb_stripe_u8(const uint8_t *src, size_t src_stride,
                                        uint8_t *dst, size_t dst_stride,
                                        size_t width, size_t height,
                                        bool v_first, size_t begin,
                                        size_t end) {
  return yuv_p_to_rgb_stripe_u8_sc(src, src_stride, dst, dst_stride, width,
                                   height, v_first, begin, end);
}

KLEIDICV_TARGET_FN_ATTRS
kleidicv_error_t yuv_p_to_rgba_stripe_u8(const uint8_t *src, size_t src_stride,
                                         uint8_t *dst, size_t dst_stride,
                                         size_t width, size_t height,
                                         bool v_first, size_t begin,
                                         size_t end) {
  return yuv_p_to_rgba_stripe_u8_sc(src, src_stride, dst, dst_stride, width,
                                    height, v_first, begin, end);
}

KLEIDICV_TARGET_FN_ATTRS kleidicv_error_t yuv_p_to_bgr_stripe_u8(
    const uint8_t *src, size_t src_stride, uint8_t *dst, size_t dst_stride,
    size_t width, size_t height, bool v_first, size_t begin, size_t end) {
  return yuv_p_to_bgr_stripe_u8_sc(src, src_stride, dst, dst_stride, width,
                                   height, v_first, begin, end);
}

KLEIDICV_TARGET_FN_ATTRS kleidicv_error_t yuv_p_to_bgra_stripe_u8(
    const uint8_t *src, size_t src_stride, uint8_t *dst, size_t dst_stride,
    size_t width, size_t height, bool v_first, size_t begin, size_t end) {
  return yuv_p_to_bgra_stripe_u8_sc(src, src_stride, dst, dst_stride, width,
                                    height, v_first, begin, end);
}

}  // namespace kleidicv::sve2
