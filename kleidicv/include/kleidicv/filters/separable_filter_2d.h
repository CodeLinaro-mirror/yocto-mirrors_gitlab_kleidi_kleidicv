// SPDX-FileCopyrightText: 2024 - 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#ifndef KLEIDICV_FILTERS_SEPARABLE_FILTER_2D_H
#define KLEIDICV_FILTERS_SEPARABLE_FILTER_2D_H

#include "kleidicv/config.h"
#include "kleidicv/kleidicv.h"
#include "kleidicv/types.h"
#include "kleidicv/utils.h"
#include "kleidicv/workspace/border_types.h"

extern "C" {
// For internal use only. See instead kleidicv_separable_filter_2d_u8.
// Filter a horizontal stripe across an image. The stripe is defined by the
// range [y_begin, y_end).
KLEIDICV_API_DECLARATION(kleidicv_separable_filter_2d_stripe_u8,
                         const uint8_t *src, size_t src_stride, uint8_t *dst,
                         size_t dst_stride, size_t width, size_t height,
                         size_t y_begin, size_t y_end, size_t channels,
                         const uint8_t *kernel_x, size_t kernel_width,
                         const uint8_t *kernel_y, size_t kernel_height,
                         kleidicv::FixedBorderType border_type);
// For internal use only. See instead kleidicv_separable_filter_2d_u16.
// Filter a horizontal stripe across an image. The stripe is defined by the
// range [y_begin, y_end).
KLEIDICV_API_DECLARATION(kleidicv_separable_filter_2d_stripe_u16,
                         const uint16_t *src, size_t src_stride, uint16_t *dst,
                         size_t dst_stride, size_t width, size_t height,
                         size_t y_begin, size_t y_end, size_t channels,
                         const uint16_t *kernel_x, size_t kernel_width,
                         const uint16_t *kernel_y, size_t kernel_height,
                         kleidicv::FixedBorderType border_type);
}

namespace kleidicv {

template <typename T>
kleidicv_error_t separable_filter_2d_checks(const T *src, size_t src_stride,
                                            T *dst, size_t dst_stride,
                                            size_t width, size_t height,
                                            const T *kernel_x,
                                            const T *kernel_y) {
  CHECK_POINTER_AND_STRIDE(src, src_stride, height);
  CHECK_POINTER_AND_STRIDE(dst, dst_stride, height);
  CHECK_IMAGE_SIZE(width, height);
  CHECK_POINTERS(kernel_x, kernel_y);

  return KLEIDICV_OK;
}

template <typename T>
FixedBorderTypeValidationResult separable_filter_2d_validate(
    const T *src, size_t src_stride, T *dst, size_t dst_stride, size_t width,
    size_t height, size_t channels, const T *kernel_x, size_t kernel_width,
    const T *kernel_y, size_t kernel_height,
    kleidicv_border_type_t border_type) {
  if (kernel_width != 5 || kernel_height != 5) {
    return {KLEIDICV_ERROR_NOT_IMPLEMENTED, FixedBorderType{}};
  }

  if (width < kernel_width - 1 || height < kernel_width - 1) {
    return {KLEIDICV_ERROR_NOT_IMPLEMENTED, FixedBorderType{}};
  }

  auto fixed_border_type = get_fixed_border_type(border_type);
  if (!fixed_border_type) {
    return {KLEIDICV_ERROR_NOT_IMPLEMENTED, FixedBorderType{}};
  }

  const kleidicv_error_t check_error = separable_filter_2d_checks(
      src, src_stride, dst, dst_stride, width, height, kernel_x, kernel_y);
  if (check_error != KLEIDICV_OK) {
    return {check_error, FixedBorderType{}};
  }

  if (channels > KLEIDICV_MAXIMUM_CHANNEL_COUNT) {
    return {KLEIDICV_ERROR_NOT_IMPLEMENTED, FixedBorderType{}};
  }

  return {KLEIDICV_OK, *fixed_border_type};
}

namespace neon {

template <typename T>
kleidicv_error_t separable_filter_2d_stripe(
    const T *src, size_t src_stride, T *dst, size_t dst_stride, size_t width,
    size_t height, size_t y_begin, size_t y_end, size_t channels,
    const T *kernel_x, size_t kernel_width, const T *kernel_y,
    size_t kernel_height, FixedBorderType border_type);

}  // namespace neon

namespace sve2 {

template <typename T>
kleidicv_error_t separable_filter_2d_stripe(
    const T *src, size_t src_stride, T *dst, size_t dst_stride, size_t width,
    size_t height, size_t y_begin, size_t y_end, size_t channels,
    const T *kernel_x, size_t kernel_width, const T *kernel_y,
    size_t kernel_height, FixedBorderType border_type);

}  // namespace sve2

namespace sme {

template <typename T>
kleidicv_error_t separable_filter_2d_stripe(
    const T *src, size_t src_stride, T *dst, size_t dst_stride, size_t width,
    size_t height, size_t y_begin, size_t y_end, size_t channels,
    const T *kernel_x, size_t kernel_width, const T *kernel_y,
    size_t kernel_height, FixedBorderType border_type);

}  // namespace sme

}  // namespace kleidicv

#endif  // KLEIDICV_FILTERS_SEPARABLE_FILTER_2D_H
