// SPDX-FileCopyrightText: 2023 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#ifndef KLEIDICV_FILTERS_SOBEL_H
#define KLEIDICV_FILTERS_SOBEL_H

#include "kleidicv/kleidicv.h"

namespace kleidicv {

namespace neon {
kleidicv_error_t sobel_3x3_horizontal_s16_u8(const uint8_t *src,
                                             size_t src_stride, int16_t *dst,
                                             size_t dst_stride, size_t width,
                                             size_t height, size_t channels);
kleidicv_error_t sobel_3x3_vertical_s16_u8(const uint8_t *src,
                                           size_t src_stride, int16_t *dst,
                                           size_t dst_stride, size_t width,
                                           size_t height, size_t channels);
}  // namespace neon

namespace sve2 {
kleidicv_error_t sobel_3x3_horizontal_s16_u8(const uint8_t *src,
                                             size_t src_stride, int16_t *dst,
                                             size_t dst_stride, size_t width,
                                             size_t height, size_t channels);
kleidicv_error_t sobel_3x3_vertical_s16_u8(const uint8_t *src,
                                           size_t src_stride, int16_t *dst,
                                           size_t dst_stride, size_t width,
                                           size_t height, size_t channels);
}  // namespace sve2

namespace sme2 {
kleidicv_error_t sobel_3x3_horizontal_s16_u8(const uint8_t *src,
                                             size_t src_stride, int16_t *dst,
                                             size_t dst_stride, size_t width,
                                             size_t height, size_t channels);
kleidicv_error_t sobel_3x3_vertical_s16_u8(const uint8_t *src,
                                           size_t src_stride, int16_t *dst,
                                           size_t dst_stride, size_t width,
                                           size_t height, size_t channels);
}  // namespace sme2

}  // namespace kleidicv

#endif  // KLEIDICV_FILTERS_SOBEL_H
