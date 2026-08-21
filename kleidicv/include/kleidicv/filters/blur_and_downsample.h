// SPDX-FileCopyrightText: 2024 - 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#ifndef KLEIDICV_FILTERS_BLUR_AND_DOWNSAMPLE_H
#define KLEIDICV_FILTERS_BLUR_AND_DOWNSAMPLE_H

#include "kleidicv/config.h"
#include "kleidicv/kleidicv.h"
#include "kleidicv/types.h"
#include "kleidicv/utils.h"
#include "kleidicv/workspace/border_types.h"

extern "C" {
// For internal use only. See instead kleidicv_blur_and_downsample_u8.
// Blurs and downsamples a horizontal stripe across an image. The stripe is
// defined by the range [y_begin, y_end).
KLEIDICV_API_DECLARATION(kleidicv_blur_and_downsample_stripe_u8,
                         const uint8_t *src, size_t src_stride,
                         size_t src_width, size_t src_height, uint8_t *dst,
                         size_t dst_stride, size_t y_begin, size_t y_end,
                         size_t channels,
                         kleidicv::FixedBorderType fixed_border_type);
}

namespace kleidicv {

inline kleidicv_error_t blur_and_downsample_checks(
    const uint8_t *src, size_t src_stride, size_t src_width, size_t src_height,
    uint8_t *dst, size_t dst_stride) {
  CHECK_POINTER_AND_STRIDE(src, src_stride, src_height);
  CHECK_POINTER_AND_STRIDE(dst, dst_stride, (src_height + 1) / 2);
  CHECK_IMAGE_SIZE(src_width, src_height);

  return KLEIDICV_OK;
}

inline FixedBorderTypeValidationResult blur_and_downsample_validate(
    const uint8_t *src, size_t src_stride, size_t src_width, size_t src_height,
    uint8_t *dst, size_t dst_stride, size_t channels,
    kleidicv_border_type_t border_type) {
  if (src_width < 4 || src_height < 4 || channels < 1 ||
      channels > KLEIDICV_MAXIMUM_CHANNEL_COUNT) {
    return {KLEIDICV_ERROR_NOT_IMPLEMENTED, FixedBorderType{}};
  }

  auto fixed_border_type = get_fixed_border_type(border_type);
  if (!fixed_border_type) {
    return {KLEIDICV_ERROR_NOT_IMPLEMENTED, FixedBorderType{}};
  }

  const kleidicv_error_t check_error = blur_and_downsample_checks(
      src, src_stride, src_width, src_height, dst, dst_stride);
  if (check_error != KLEIDICV_OK) {
    return {check_error, FixedBorderType{}};
  }

  return {KLEIDICV_OK, *fixed_border_type};
}

namespace neon {

kleidicv_error_t kleidicv_blur_and_downsample_stripe_u8(
    const uint8_t *src, size_t src_stride, size_t src_width, size_t src_height,
    uint8_t *dst, size_t dst_stride, size_t y_begin, size_t y_end,
    size_t channels, FixedBorderType fixed_border_type);

}  // namespace neon

namespace sve2 {

kleidicv_error_t kleidicv_blur_and_downsample_stripe_u8(
    const uint8_t *src, size_t src_stride, size_t src_width, size_t src_height,
    uint8_t *dst, size_t dst_stride, size_t y_begin, size_t y_end,
    size_t channels, FixedBorderType fixed_border_type);

}  // namespace sve2

namespace sme {

kleidicv_error_t kleidicv_blur_and_downsample_stripe_u8(
    const uint8_t *src, size_t src_stride, size_t src_width, size_t src_height,
    uint8_t *dst, size_t dst_stride, size_t y_begin, size_t y_end,
    size_t channels, FixedBorderType fixed_border_type);

}  // namespace sme

}  // namespace kleidicv

#endif  // KLEIDICV_FILTERS_BLUR_AND_DOWNSAMPLE_H
