// SPDX-FileCopyrightText: 2023 - 2024 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#include "kleidicv/dispatch.h"
#include "kleidicv/filters/sobel.h"
#include "kleidicv/kleidicv.h"

#define KLEIDICV_DEFINE_C_API(name, partialname)           \
  KLEIDICV_MULTIVERSION_C_API(                             \
      name, &kleidicv::neon::partialname,                  \
      KLEIDICV_SVE2_IMPL_IF(&kleidicv::sve2::partialname), \
      &kleidicv::sme2::partialname)

KLEIDICV_DEFINE_C_API(kleidicv_sobel_3x3_horizontal_stripe_s16_u8,
                      sobel_3x3_horizontal_stripe_s16_u8);
KLEIDICV_DEFINE_C_API(kleidicv_sobel_3x3_vertical_stripe_s16_u8,
                      sobel_3x3_vertical_stripe_s16_u8);

namespace kleidicv {
static kleidicv_error_t sobel_3x3_horizontal_s16_u8(
    const uint8_t *src, size_t src_stride, int16_t *dst, size_t dst_stride,
    size_t width, size_t height, size_t channels) {
  return kleidicv_sobel_3x3_horizontal_stripe_s16_u8(
      src, src_stride, dst, dst_stride, width, height, 0, height, channels);
}
static kleidicv_error_t sobel_3x3_vertical_s16_u8(
    const uint8_t *src, size_t src_stride, int16_t *dst, size_t dst_stride,
    size_t width, size_t height, size_t channels) {
  return kleidicv_sobel_3x3_vertical_stripe_s16_u8(
      src, src_stride, dst, dst_stride, width, height, 0, height, channels);
}
}  // namespace kleidicv

KLEIDICV_MULTIVERSION_C_API(kleidicv_sobel_3x3_horizontal_s16_u8,
                            &kleidicv::sobel_3x3_horizontal_s16_u8, nullptr,
                            nullptr);
KLEIDICV_MULTIVERSION_C_API(kleidicv_sobel_3x3_vertical_s16_u8,
                            &kleidicv::sobel_3x3_vertical_s16_u8, nullptr,
                            nullptr);
