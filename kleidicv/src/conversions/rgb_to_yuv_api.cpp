// SPDX-FileCopyrightText: 2024 - 2025 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#include "kleidicv/conversions/rgb_to_yuv.h"
#include "kleidicv/dispatch.h"
#include "kleidicv/kleidicv.h"
#include "kleidicv/utils.h"

#define KLEIDICV_DEFINE_C_API_WITH_SME2(name, partialname)              \
  KLEIDICV_MULTIVERSION_C_API(                                          \
      name, &kleidicv::neon::partialname, &kleidicv::sve2::partialname, \
      &kleidicv::sme::partialname, &kleidicv::sme2::partialname)

#define KLEIDICV_DEFINE_C_API_WITHOUT_SME2(name, partialname)     \
  KLEIDICV_MULTIVERSION_C_API(name, &kleidicv::neon::partialname, \
                              &kleidicv::sve2::partialname,       \
                              &kleidicv::sme::partialname, nullptr)

KLEIDICV_DEFINE_C_API_WITH_SME2(kleidicv_rgb_to_yuv444_u8, rgb_to_yuv444_u8);
KLEIDICV_DEFINE_C_API_WITH_SME2(kleidicv_rgb_to_yuv420p_stripe_u8,
                                rgb_to_yuv420p_stripe_u8);
KLEIDICV_DEFINE_C_API_WITHOUT_SME2(kleidicv_rgb_to_yuv422_u8, rgb_to_yuv422_u8);
KLEIDICV_DEFINE_C_API_WITHOUT_SME2(kleidicv_rgb_to_yuv420sp_stripe_u8,
                                   rgb_to_yuv420sp_stripe_u8);

extern "C" {

kleidicv_error_t kleidicv_rgb_to_yuv_u8(
    const uint8_t *src, size_t src_stride, uint8_t *dst, size_t dst_stride,
    size_t width, size_t height, kleidicv_color_conversion_t color_format) {
  // Extract the base format
  const size_t base_format = static_cast<size_t>(
      color_format & KLEIDICV_COLOR_CONVERSION_YUV_FMT_MASK);
  if (base_format == KLEIDICV_COLOR_CONVERSION_FMT_YUV444) {
    return kleidicv_rgb_to_yuv444_u8(src, src_stride, dst, dst_stride, width,
                                     height, color_format);
  }
  if (base_format == KLEIDICV_COLOR_CONVERSION_FMT_YUV422) {
    return kleidicv_rgb_to_yuv422_u8(src, src_stride, dst, dst_stride, width,
                                     height, color_format);
  }

  // YUV420 uses 2x2 chroma blocks spanning adjacent rows, so the stripe API
  // provides each invocation with contiguous rows and the proper per-plane
  // offsets for that slice.
  CHECK_POINTER_AND_STRIDE(src, src_stride, height);
  CHECK_POINTER_AND_STRIDE(dst, dst_stride, (height * 3 + 1) / 2);
  CHECK_IMAGE_SIZE(width, height);
  return kleidicv_rgb_to_yuv420p_stripe_u8(
      src, src_stride, dst, dst_stride, width, height, color_format, 0, height);
}

kleidicv_error_t kleidicv_rgb_to_yuv_semiplanar_u8(
    const uint8_t *src, size_t src_stride, uint8_t *y_dst, size_t y_stride,
    uint8_t *uv_dst, size_t uv_stride, size_t width, size_t height,
    kleidicv_color_conversion_t color_format) {
  CHECK_POINTER_AND_STRIDE(src, src_stride, height);
  CHECK_POINTER_AND_STRIDE(y_dst, y_stride, height);
  CHECK_POINTER_AND_STRIDE(uv_dst, uv_stride, (height + 1) / 2);
  CHECK_IMAGE_SIZE(width, height);
  return kleidicv_rgb_to_yuv420sp_stripe_u8(src, src_stride, y_dst, y_stride,
                                            uv_dst, uv_stride, width, height,
                                            color_format, 0, height);
}

}  // extern "C"
