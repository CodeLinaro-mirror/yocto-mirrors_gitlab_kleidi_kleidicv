// SPDX-FileCopyrightText: 2023 - 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#ifndef KLEIDICV_FILTERS_SOBEL_H
#define KLEIDICV_FILTERS_SOBEL_H

#include "kleidicv/kleidicv.h"
#include "kleidicv/utils.h"

extern "C" {
// For internal use only. See instead kleidicv_sobel_3x3_horizontal_s16_u8.
// Filter a horizontal stripe across an image. The stripe is defined by the
// range [y_begin, y_end).
KLEIDICV_API_DECLARATION(kleidicv_sobel_3x3_horizontal_stripe_s16_u8,
                         const uint8_t *src, size_t src_stride, int16_t *dst,
                         size_t dst_stride, size_t width, size_t height,
                         size_t y_begin, size_t y_end, size_t channels);
// For internal use only. See instead kleidicv_sobel_3x3_horizontal_s16_u8.
// Filter a horizontal stripe across an image. The stripe is defined by the
// range [y_begin, y_end).
KLEIDICV_API_DECLARATION(kleidicv_sobel_3x3_horizontal_stripe_s16_u8_sme,
                         const uint8_t *src, size_t src_stride, int16_t *dst,
                         size_t dst_stride, size_t width, size_t height,
                         size_t y_begin, size_t y_end, size_t channels);
// For internal use only. See instead kleidicv_sobel_3x3_vertical_s16_u8.
// Filter a horizontal stripe across an image. The stripe is defined by the
// range [y_begin, y_end).
KLEIDICV_API_DECLARATION(kleidicv_sobel_3x3_vertical_stripe_s16_u8,
                         const uint8_t *src, size_t src_stride, int16_t *dst,
                         size_t dst_stride, size_t width, size_t height,
                         size_t y_begin, size_t y_end, size_t channels);
// For internal use only. See instead kleidicv_sobel_3x3_vertical_s16_u8.
// Filter a horizontal stripe across an image. The stripe is defined by the
// range [y_begin, y_end).
KLEIDICV_API_DECLARATION(kleidicv_sobel_3x3_vertical_stripe_s16_u8_sme,
                         const uint8_t *src, size_t src_stride, int16_t *dst,
                         size_t dst_stride, size_t width, size_t height,
                         size_t y_begin, size_t y_end, size_t channels);
}

namespace kleidicv {

inline kleidicv_error_t sobel_validate(const uint8_t *src, size_t src_stride,
                                       int16_t *dst, size_t dst_stride,
                                       size_t width, size_t height,
                                       size_t channels) {
  if (width < 2 || height < 2) {
    return KLEIDICV_ERROR_NOT_IMPLEMENTED;
  }

  CHECK_POINTER_AND_STRIDE(src, src_stride, height);
  CHECK_POINTER_AND_STRIDE(dst, dst_stride, height);
  CHECK_IMAGE_SIZE(width, height);

  if (channels > KLEIDICV_MAXIMUM_CHANNEL_COUNT) {
    return KLEIDICV_ERROR_NOT_IMPLEMENTED;
  }

  return KLEIDICV_OK;
}

namespace neon {
kleidicv_error_t sobel_3x3_horizontal_stripe_s16_u8(
    const uint8_t *src, size_t src_stride, int16_t *dst, size_t dst_stride,
    size_t width, size_t height, size_t y_begin, size_t y_end, size_t channels);
kleidicv_error_t sobel_3x3_vertical_stripe_s16_u8(
    const uint8_t *src, size_t src_stride, int16_t *dst, size_t dst_stride,
    size_t width, size_t height, size_t y_begin, size_t y_end, size_t channels);
}  // namespace neon

namespace sve2 {
kleidicv_error_t sobel_3x3_horizontal_stripe_s16_u8(
    const uint8_t *src, size_t src_stride, int16_t *dst, size_t dst_stride,
    size_t width, size_t height, size_t y_begin, size_t y_end, size_t channels);
kleidicv_error_t sobel_3x3_vertical_stripe_s16_u8(
    const uint8_t *src, size_t src_stride, int16_t *dst, size_t dst_stride,
    size_t width, size_t height, size_t y_begin, size_t y_end, size_t channels);
}  // namespace sve2

namespace sme {
kleidicv_error_t sobel_3x3_horizontal_stripe_s16_u8(
    const uint8_t *src, size_t src_stride, int16_t *dst, size_t dst_stride,
    size_t width, size_t height, size_t y_begin, size_t y_end, size_t channels);
kleidicv_error_t sobel_3x3_vertical_stripe_s16_u8(
    const uint8_t *src, size_t src_stride, int16_t *dst, size_t dst_stride,
    size_t width, size_t height, size_t y_begin, size_t y_end, size_t channels);
}  // namespace sme

}  // namespace kleidicv

#endif  // KLEIDICV_FILTERS_SOBEL_H
