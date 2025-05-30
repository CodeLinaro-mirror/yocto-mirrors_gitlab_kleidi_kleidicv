// SPDX-FileCopyrightText: 2023 - 2025 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#ifndef KLEIDICV_FILTERS_MEDIAN_BLUR_H
#define KLEIDICV_FILTERS_MEDIAN_BLUR_H

#include <utility>

#include "kleidicv/config.h"
#include "kleidicv/kleidicv.h"
#include "kleidicv/types.h"
#include "kleidicv/workspace/border_types.h"

extern "C" {

// For internal use only. See instead kleidicv_median_blur_stripe_s8.
// find a median across an image. The stripe is defined by the
// range (y_begin, y_end].
KLEIDICV_API_DECLARATION(kleidicv_median_blur_stripe_s8, const int8_t *src,
                         size_t src_stride, int8_t *dst, size_t dst_stride,
                         size_t width, size_t height, size_t y_begin,
                         size_t y_end, size_t channels, size_t kernel_width,
                         size_t kernel_height,
                         kleidicv::FixedBorderType border_type);

// For internal use only. See instead kleidicv_median_blur_stripe_u8.
// find a median across an image. The stripe is defined by the
// range (y_begin, y_end].
KLEIDICV_API_DECLARATION(kleidicv_median_blur_stripe_u8, const uint8_t *src,
                         size_t src_stride, uint8_t *dst, size_t dst_stride,
                         size_t width, size_t height, size_t y_begin,
                         size_t y_end, size_t channels, size_t kernel_width,
                         size_t kernel_height,
                         kleidicv::FixedBorderType border_type);

// For internal use only. See instead kleidicv_median_blur_stripe_s16.
// Filter a horizontal stripe across an image. The stripe is defined by the
// range (y_begin, y_end].
KLEIDICV_API_DECLARATION(kleidicv_median_blur_stripe_s16, const int16_t *src,
                         size_t src_stride, int16_t *dst, size_t dst_stride,
                         size_t width, size_t height, size_t y_begin,
                         size_t y_end, size_t channels, size_t kernel_width,
                         size_t kernel_height,
                         kleidicv::FixedBorderType border_type);

// For internal use only. See instead kleidicv_median_blur_stripe_u16.
// Filter a horizontal stripe across an image. The stripe is defined by the
// range (y_begin, y_end].
KLEIDICV_API_DECLARATION(kleidicv_median_blur_stripe_u16, const uint16_t *src,
                         size_t src_stride, uint16_t *dst, size_t dst_stride,
                         size_t width, size_t height, size_t y_begin,
                         size_t y_end, size_t channels, size_t kernel_width,
                         size_t kernel_height,
                         kleidicv::FixedBorderType border_type);

// For internal use only. See instead kleidicv_median_blur_stripe_s32.
// Filter a horizontal stripe across an image. The stripe is defined by the
// range (y_begin, y_end].
KLEIDICV_API_DECLARATION(kleidicv_median_blur_stripe_s32, const int32_t *src,
                         size_t src_stride, int32_t *dst, size_t dst_stride,
                         size_t width, size_t height, size_t y_begin,
                         size_t y_end, size_t channels, size_t kernel_width,
                         size_t kernel_height,
                         kleidicv::FixedBorderType border_type);

// For internal use only. See instead kleidicv_median_blur_stripe_u32.
// Filter a horizontal stripe across an image. The stripe is defined by the
// range (y_begin, y_end].
KLEIDICV_API_DECLARATION(kleidicv_median_blur_stripe_u32, const uint32_t *src,
                         size_t src_stride, uint32_t *dst, size_t dst_stride,
                         size_t width, size_t height, size_t y_begin,
                         size_t y_end, size_t channels, size_t kernel_width,
                         size_t kernel_height,
                         kleidicv::FixedBorderType border_type);

// For internal use only. See instead kleidicv_median_blur_stripe_f32.
// Filter a horizontal stripe across an image. The stripe is defined by the
// range (y_begin, y_end].
KLEIDICV_API_DECLARATION(kleidicv_median_blur_stripe_f32, const float *src,
                         size_t src_stride, float *dst, size_t dst_stride,
                         size_t width, size_t height, size_t y_begin,
                         size_t y_end, size_t channels, size_t kernel_width,
                         size_t kernel_height,
                         kleidicv::FixedBorderType border_type);
}

namespace kleidicv {
template <typename T>
inline kleidicv_error_t check_ptrs_strides_imagesizes(const T *src,
                                                      size_t src_stride, T *dst,
                                                      size_t dst_stride,
                                                      size_t width,
                                                      size_t height) {
  CHECK_POINTER_AND_STRIDE(src, src_stride, height);
  CHECK_POINTER_AND_STRIDE(dst, dst_stride, height);
  CHECK_IMAGE_SIZE(width, height);
  return KLEIDICV_OK;
}
template <typename T>
inline std::pair<kleidicv_error_t, FixedBorderType> median_blur_is_implemented(
    const T *src, size_t src_stride, T *dst, size_t dst_stride, size_t width,
    size_t height, size_t channels, size_t kernel_width, size_t kernel_height,
    kleidicv_border_type_t border_type) {
  auto image_check = check_ptrs_strides_imagesizes(src, src_stride, dst,
                                                   dst_stride, width, height);
  if (image_check != KLEIDICV_OK) {
    return std::make_pair(image_check, FixedBorderType{});
  }

  auto fixed_border_type = kleidicv::get_fixed_border_type(border_type);
  if ((src != dst) && (channels <= KLEIDICV_MAXIMUM_CHANNEL_COUNT) &&
      (kernel_width == kernel_height) && (height >= kernel_height - 1) &&
      (width >= kernel_width - 1) &&
      ((kernel_width == 5) || (kernel_width == 7)) &&
      fixed_border_type.has_value()) {
    return std::make_pair(KLEIDICV_OK, *fixed_border_type);
  }

  return std::make_pair(KLEIDICV_ERROR_NOT_IMPLEMENTED, FixedBorderType{});
}

}  // namespace kleidicv

#endif  // KLEIDICV_FILTERS_MEDIAN_BLUR_H
