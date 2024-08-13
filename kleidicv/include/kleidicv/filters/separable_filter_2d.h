// SPDX-FileCopyrightText: 2024 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#ifndef KLEIDICV_FILTERS_SEPARABLE_FILTER_2D_H
#define KLEIDICV_FILTERS_SEPARABLE_FILTER_2D_H

#include "kleidicv/config.h"
#include "kleidicv/kleidicv.h"
#include "kleidicv/types.h"

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
                         kleidicv_border_type_t border_type,
                         kleidicv_filter_context_t *context);
// For internal use only. See instead kleidicv_separable_filter_2d_u16.
// Filter a horizontal stripe across an image. The stripe is defined by the
// range (y_begin, y_end].
KLEIDICV_API_DECLARATION(kleidicv_separable_filter_2d_stripe_u16,
                         const uint16_t *src, size_t src_stride, uint16_t *dst,
                         size_t dst_stride, size_t width, size_t height,
                         size_t y_begin, size_t y_end, size_t channels,
                         const uint16_t *kernel_x, size_t kernel_width,
                         const uint16_t *kernel_y, size_t kernel_height,
                         kleidicv_border_type_t border_type,
                         kleidicv_filter_context_t *context);
}

namespace kleidicv {

namespace neon {

kleidicv_error_t separable_filter_2d_stripe_u8(
    const uint8_t *src, size_t src_stride, uint8_t *dst, size_t dst_stride,
    size_t width, size_t height, size_t y_begin, size_t y_end, size_t channels,
    const uint8_t *kernel_x, size_t kernel_width, const uint8_t *kernel_y,
    size_t kernel_height, kleidicv_border_type_t border_type,
    kleidicv_filter_context_t *context);

kleidicv_error_t separable_filter_2d_stripe_u16(
    const uint16_t *src, size_t src_stride, uint16_t *dst, size_t dst_stride,
    size_t width, size_t height, size_t y_begin, size_t y_end, size_t channels,
    const uint16_t *kernel_x, size_t kernel_width, const uint16_t *kernel_y,
    size_t kernel_height, kleidicv_border_type_t border_type,
    kleidicv_filter_context_t *context);

}  // namespace neon

namespace sve2 {

kleidicv_error_t separable_filter_2d_stripe_u8(
    const uint8_t *src, size_t src_stride, uint8_t *dst, size_t dst_stride,
    size_t width, size_t height, size_t y_begin, size_t y_end, size_t channels,
    const uint8_t *kernel_x, size_t kernel_width, const uint8_t *kernel_y,
    size_t kernel_height, kleidicv_border_type_t border_type,
    kleidicv_filter_context_t *context);

kleidicv_error_t separable_filter_2d_stripe_u16(
    const uint16_t *src, size_t src_stride, uint16_t *dst, size_t dst_stride,
    size_t width, size_t height, size_t y_begin, size_t y_end, size_t channels,
    const uint16_t *kernel_x, size_t kernel_width, const uint16_t *kernel_y,
    size_t kernel_height, kleidicv_border_type_t border_type,
    kleidicv_filter_context_t *context);

}  // namespace sve2

namespace sme2 {

kleidicv_error_t separable_filter_2d_stripe_u8(
    const uint8_t *src, size_t src_stride, uint8_t *dst, size_t dst_stride,
    size_t width, size_t height, size_t y_begin, size_t y_end, size_t channels,
    const uint8_t *kernel_x, size_t kernel_width, const uint8_t *kernel_y,
    size_t kernel_height, kleidicv_border_type_t border_type,
    kleidicv_filter_context_t *context);

kleidicv_error_t separable_filter_2d_stripe_u16(
    const uint16_t *src, size_t src_stride, uint16_t *dst, size_t dst_stride,
    size_t width, size_t height, size_t y_begin, size_t y_end, size_t channels,
    const uint16_t *kernel_x, size_t kernel_width, const uint16_t *kernel_y,
    size_t kernel_height, kleidicv_border_type_t border_type,
    kleidicv_filter_context_t *context);

}  // namespace sme2

}  // namespace kleidicv

#endif  // KLEIDICV_FILTERS_SEPARABLE_FILTER_2D_H
