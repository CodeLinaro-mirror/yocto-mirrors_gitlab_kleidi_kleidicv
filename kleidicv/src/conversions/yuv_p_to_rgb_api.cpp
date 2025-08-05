// SPDX-FileCopyrightText: 2025 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#include "kleidicv/conversions/yuv_420_to_rgb.h"
#include "kleidicv/dispatch.h"
#include "kleidicv/kleidicv.h"

#define KLEIDICV_DEFINE_C_API(name, partialname)                  \
  KLEIDICV_MULTIVERSION_C_API(name, &kleidicv::neon::partialname, \
                              &kleidicv::sve2::partialname,       \
                              &kleidicv::sme::partialname, nullptr)

KLEIDICV_DEFINE_C_API(kleidicv_yuv_p_to_rgb_stripe_u8, yuv_p_to_rgb_stripe_u8);

KLEIDICV_DEFINE_C_API(kleidicv_yuv_p_to_bgr_stripe_u8, yuv_p_to_bgr_stripe_u8);

KLEIDICV_DEFINE_C_API(kleidicv_yuv_p_to_rgba_stripe_u8,
                      yuv_p_to_rgba_stripe_u8);

KLEIDICV_DEFINE_C_API(kleidicv_yuv_p_to_bgra_stripe_u8,
                      yuv_p_to_bgra_stripe_u8);

extern "C" {

kleidicv_error_t kleidicv_yuv_p_to_rgb_u8(const uint8_t *src, size_t src_stride,
                                          uint8_t *dst, size_t dst_stride,
                                          size_t width, size_t height,
                                          bool v_first) {
  return kleidicv_yuv_p_to_rgb_stripe_u8(src, src_stride, dst, dst_stride,
                                         width, height, v_first, 0, height);
}

kleidicv_error_t kleidicv_yuv_p_to_bgr_u8(const uint8_t *src, size_t src_stride,
                                          uint8_t *dst, size_t dst_stride,
                                          size_t width, size_t height,
                                          bool v_first) {
  return kleidicv_yuv_p_to_bgr_stripe_u8(src, src_stride, dst, dst_stride,
                                         width, height, v_first, 0, height);
}

kleidicv_error_t kleidicv_yuv_p_to_rgba_u8(const uint8_t *src,
                                           size_t src_stride, uint8_t *dst,
                                           size_t dst_stride, size_t width,
                                           size_t height, bool v_first) {
  return kleidicv_yuv_p_to_rgba_stripe_u8(src, src_stride, dst, dst_stride,
                                          width, height, v_first, 0, height);
}

kleidicv_error_t kleidicv_yuv_p_to_bgra_u8(const uint8_t *src,
                                           size_t src_stride, uint8_t *dst,
                                           size_t dst_stride, size_t width,
                                           size_t height, bool v_first) {
  return kleidicv_yuv_p_to_bgra_stripe_u8(src, src_stride, dst, dst_stride,
                                          width, height, v_first, 0, height);
}

}  // extern "C"
