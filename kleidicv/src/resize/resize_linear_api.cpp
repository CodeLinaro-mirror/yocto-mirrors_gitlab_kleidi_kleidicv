// SPDX-FileCopyrightText: 2024 - 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#include "kleidicv/dispatch.h"
#include "kleidicv/kleidicv.h"
#include "kleidicv/resize/resize_linear.h"
#include "kleidicv/utils.h"

#define KLEIDICV_DEFINE_C_API_ALL(name, called_name)      \
  KLEIDICV_MULTIVERSION_C_API(                            \
      name, &kleidicv::neon::called_name,                 \
      KLEIDICV_SVE2_IMPL_IF(kleidicv::sve2::called_name), \
      &kleidicv::sme::called_name, &kleidicv::sme2::called_name)

#define KLEIDICV_DEFINE_C_API_NEON(name, called_name)                      \
  KLEIDICV_MULTIVERSION_C_API(name, &kleidicv::neon::called_name, nullptr, \
                              nullptr, nullptr)

KLEIDICV_DEFINE_C_API_ALL(kleidicv_resize_2x2_stripe_u8,
                          kleidicv_resize_2x2_stripe_u8);
KLEIDICV_DEFINE_C_API_ALL(kleidicv_resize_4x4_stripe_u8,
                          kleidicv_resize_4x4_stripe_u8);
KLEIDICV_DEFINE_C_API_NEON(kleidicv_resize_r2_u8,
                           kleidicv_resize_generic_stripe_u8<2>);
KLEIDICV_DEFINE_C_API_NEON(kleidicv_resize_r3_u8,
                           kleidicv_resize_generic_stripe_u8<3>);

KLEIDICV_DEFINE_C_API_ALL(kleidicv_resize_linear_stripe_f32,
                          kleidicv_resize_linear_stripe_f32);

extern "C" {

kleidicv_error_t kleidicv_resize_linear_u8(const uint8_t *src,
                                           size_t src_stride, size_t src_width,
                                           size_t src_height, uint8_t *dst,
                                           size_t dst_stride, size_t dst_width,
                                           size_t dst_height) {
  if (!kleidicv::resize_linear_u8_is_implemented(src_width, src_height,
                                                 dst_width, dst_height)) {
    return KLEIDICV_ERROR_NOT_IMPLEMENTED;
  }
  return kleidicv_resize_linear_stripe_u8(
      src, src_stride, src_width, src_height, 0,
      std::min(src_height, dst_height), dst, dst_stride, dst_width, dst_height);
}

kleidicv_error_t kleidicv_resize_linear_stripe_u8(
    const uint8_t *src, size_t src_stride, size_t src_width, size_t src_height,
    size_t y_begin, size_t y_end, uint8_t *dst, size_t dst_stride,
    size_t dst_width, size_t dst_height) {
  CHECK_POINTER_AND_STRIDE(src, src_stride, src_height);
  CHECK_POINTER_AND_STRIDE(dst, dst_stride, dst_height);
  CHECK_IMAGE_SIZE(dst_width, dst_height);

  if (src_width == 0 || src_height == 0) {
    return KLEIDICV_OK;
  }

  if (src_width * 2 == dst_width && src_height * 2 == dst_height) {
    return kleidicv_resize_2x2_stripe_u8(src, src_stride, src_width, src_height,
                                         y_begin, y_end, dst, dst_stride);
  }
  if (src_width * 4 == dst_width && src_height * 4 == dst_height) {
    return kleidicv_resize_4x4_stripe_u8(src, src_stride, src_width, src_height,
                                         y_begin, y_end, dst, dst_stride);
  }

  if (dst_width * 2 >= src_width) {
    return kleidicv_resize_r2_u8(src, src_stride, src_width, src_height,
                                 y_begin, y_end, dst, dst_stride, dst_width,
                                 dst_height);
  }
  return kleidicv_resize_r3_u8(src, src_stride, src_width, src_height, y_begin,
                               y_end, dst, dst_stride, dst_width, dst_height);
}

kleidicv_error_t kleidicv_resize_linear_f32(const float *src, size_t src_stride,
                                            size_t src_width, size_t src_height,
                                            float *dst, size_t dst_stride,
                                            size_t dst_width,
                                            size_t dst_height) {
  if (!kleidicv::resize_linear_f32_is_implemented(src_width, src_height,
                                                  dst_width, dst_height)) {
    return KLEIDICV_ERROR_NOT_IMPLEMENTED;
  }
  return kleidicv_resize_linear_stripe_f32(src, src_stride, src_width,
                                           src_height, 0, src_height, dst,
                                           dst_stride, dst_width, dst_height);
}

}  // extern "C"
