// SPDX-FileCopyrightText: 2025 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#include "kleidicv/conversions/rgb_to_yuv_420.h"
#include "kleidicv/dispatch.h"
#include "kleidicv/kleidicv.h"

#define KLEIDICV_DEFINE_C_API(name, partialname)                  \
  KLEIDICV_MULTIVERSION_C_API(name, &kleidicv::neon::partialname, \
                              &kleidicv::sve2::partialname,       \
                              &kleidicv::sme::partialname, nullptr)

KLEIDICV_DEFINE_C_API(kleidicv_rgb_to_yuv420_p_stripe_u8,
                      rgb_to_yuv420_p_stripe_u8);

KLEIDICV_DEFINE_C_API(kleidicv_rgba_to_yuv420_p_stripe_u8,
                      rgba_to_yuv420_p_stripe_u8);

KLEIDICV_DEFINE_C_API(kleidicv_bgr_to_yuv420_p_stripe_u8,
                      bgr_to_yuv420_p_stripe_u8);

KLEIDICV_DEFINE_C_API(kleidicv_bgra_to_yuv420_p_stripe_u8,
                      bgra_to_yuv420_p_stripe_u8);

extern "C" {

kleidicv_error_t kleidicv_rgb_to_yuv420_p_u8(const uint8_t *src,
                                             size_t src_stride, uint8_t *dst,
                                             size_t dst_stride, size_t width,
                                             size_t height, bool is_yv12) {
  return kleidicv_rgb_to_yuv420_p_stripe_u8(src, src_stride, dst, dst_stride,
                                            width, height, is_yv12, 0, height);
}

kleidicv_error_t kleidicv_rgba_to_yuv420_p_u8(const uint8_t *src,
                                              size_t src_stride, uint8_t *dst,
                                              size_t dst_stride, size_t width,
                                              size_t height, bool is_yv12) {
  return kleidicv_rgba_to_yuv420_p_stripe_u8(src, src_stride, dst, dst_stride,
                                             width, height, is_yv12, 0, height);
}

kleidicv_error_t kleidicv_bgr_to_yuv420_p_u8(const uint8_t *src,
                                             size_t src_stride, uint8_t *dst,
                                             size_t dst_stride, size_t width,
                                             size_t height, bool is_yv12) {
  return kleidicv_bgr_to_yuv420_p_stripe_u8(src, src_stride, dst, dst_stride,
                                            width, height, is_yv12, 0, height);
}

kleidicv_error_t kleidicv_bgra_to_yuv420_p_u8(const uint8_t *src,
                                              size_t src_stride, uint8_t *dst,
                                              size_t dst_stride, size_t width,
                                              size_t height, bool is_yv12) {
  return kleidicv_bgra_to_yuv420_p_stripe_u8(src, src_stride, dst, dst_stride,
                                             width, height, is_yv12, 0, height);
}

}  // extern "C"
