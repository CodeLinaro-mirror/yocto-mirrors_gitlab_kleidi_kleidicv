// SPDX-FileCopyrightText: 2024 - 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#ifndef KLEIDICV_FILTERS_SEPARABLE_FILTER_2D_H
#define KLEIDICV_FILTERS_SEPARABLE_FILTER_2D_H

#include "kleidicv/config.h"
#include "kleidicv/kleidicv.h"
#include "kleidicv/types.h"
#include "kleidicv/workspace/border_types.h"

extern "C" {
// For internal use only. See instead kleidicv_separable_filter_2d_u8.
// Filter a horizontal stripe across an image. The stripe is defined by the
// range (y_begin, y_end].
KLEIDICV_API_DECLARATION(kleidicv_separable_filter_2d_stripe_u8,
                         const uint8_t *src, size_t src_stride, uint8_t *dst,
                         size_t dst_stride, size_t width, size_t height,
                         size_t y_begin, size_t y_end, size_t channels,
                         const uint8_t *kernel_x, size_t kernel_width,
                         const uint8_t *kernel_y, size_t kernel_height,
                         kleidicv::FixedBorderType border_type);
// For internal use only. See instead kleidicv_separable_filter_2d_u16.
// Filter a horizontal stripe across an image. The stripe is defined by the
// range (y_begin, y_end].
KLEIDICV_API_DECLARATION(kleidicv_separable_filter_2d_stripe_u16,
                         const uint16_t *src, size_t src_stride, uint16_t *dst,
                         size_t dst_stride, size_t width, size_t height,
                         size_t y_begin, size_t y_end, size_t channels,
                         const uint16_t *kernel_x, size_t kernel_width,
                         const uint16_t *kernel_y, size_t kernel_height,
                         kleidicv::FixedBorderType border_type);
// For internal use only. See instead kleidicv_separable_filter_2d_s16.
// Filter a horizontal stripe across an image. The stripe is defined by the
// range (y_begin, y_end].
KLEIDICV_API_DECLARATION(kleidicv_separable_filter_2d_stripe_s16,
                         const int16_t *src, size_t src_stride, int16_t *dst,
                         size_t dst_stride, size_t width, size_t height,
                         size_t y_begin, size_t y_end, size_t channels,
                         const int16_t *kernel_x, size_t kernel_width,
                         const int16_t *kernel_y, size_t kernel_height,
                         kleidicv::FixedBorderType border_type);
}

namespace kleidicv {

inline bool separable_filter_2d_is_implemented(size_t width, size_t height,
                                               size_t kernel_width,
                                               size_t kernel_height) {
  if (kernel_width != 5 || kernel_height != 5) {
    return false;
  }

  if (width < kernel_width - 1 || height < kernel_width - 1) {
    return false;
  }

  return true;
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
