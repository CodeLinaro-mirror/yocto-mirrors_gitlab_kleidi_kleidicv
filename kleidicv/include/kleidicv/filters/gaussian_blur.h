// SPDX-FileCopyrightText: 2023 - 2024 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#ifndef KLEIDICV_FILTERS_GAUSSIAN_BLUR_H
#define KLEIDICV_FILTERS_GAUSSIAN_BLUR_H

#include "kleidicv/config.h"
#include "kleidicv/types.h"

namespace kleidicv {

namespace neon {

kleidicv_error_t gaussian_blur_3x3_u8(const uint8_t *src, size_t src_stride,
                                      uint8_t *dst, size_t dst_stride,
                                      size_t width, size_t height,
                                      size_t channels,
                                      kleidicv_border_type_t border_type,
                                      kleidicv_filter_context_t *context);

kleidicv_error_t gaussian_blur_5x5_u8(const uint8_t *src, size_t src_stride,
                                      uint8_t *dst, size_t dst_stride,
                                      size_t width, size_t height,
                                      size_t channels,
                                      kleidicv_border_type_t border_type,
                                      kleidicv_filter_context_t *context);

kleidicv_error_t gaussian_blur_7x7_u8(const uint8_t *src, size_t src_stride,
                                      uint8_t *dst, size_t dst_stride,
                                      size_t width, size_t height,
                                      size_t channels,
                                      kleidicv_border_type_t border_type,
                                      kleidicv_filter_context_t *context);

}  // namespace neon

namespace sve2 {

kleidicv_error_t gaussian_blur_3x3_u8(const uint8_t *src, size_t src_stride,
                                      uint8_t *dst, size_t dst_stride,
                                      size_t width, size_t height,
                                      size_t channels,
                                      kleidicv_border_type_t border_type,
                                      kleidicv_filter_context_t *context);

kleidicv_error_t gaussian_blur_5x5_u8(const uint8_t *src, size_t src_stride,
                                      uint8_t *dst, size_t dst_stride,
                                      size_t width, size_t height,
                                      size_t channels,
                                      kleidicv_border_type_t border_type,
                                      kleidicv_filter_context_t *context);

kleidicv_error_t gaussian_blur_7x7_u8(const uint8_t *src, size_t src_stride,
                                      uint8_t *dst, size_t dst_stride,
                                      size_t width, size_t height,
                                      size_t channels,
                                      kleidicv_border_type_t border_type,
                                      kleidicv_filter_context_t *context);

}  // namespace sve2

namespace sme2 {

kleidicv_error_t gaussian_blur_3x3_u8(const uint8_t *src, size_t src_stride,
                                      uint8_t *dst, size_t dst_stride,
                                      size_t width, size_t height,
                                      size_t channels,
                                      kleidicv_border_type_t border_type,
                                      kleidicv_filter_context_t *context);

kleidicv_error_t gaussian_blur_5x5_u8(const uint8_t *src, size_t src_stride,
                                      uint8_t *dst, size_t dst_stride,
                                      size_t width, size_t height,
                                      size_t channels,
                                      kleidicv_border_type_t border_type,
                                      kleidicv_filter_context_t *context);

kleidicv_error_t gaussian_blur_7x7_u8(const uint8_t *src, size_t src_stride,
                                      uint8_t *dst, size_t dst_stride,
                                      size_t width, size_t height,
                                      size_t channels,
                                      kleidicv_border_type_t border_type,
                                      kleidicv_filter_context_t *context);

}  // namespace sme2

}  // namespace kleidicv

#endif  // KLEIDICV_FILTERS_GAUSSIAN_BLUR_H
