// SPDX-FileCopyrightText: 2023 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#ifndef INTRINSICCV_FILTERS_SOBEL_H
#define INTRINSICCV_FILTERS_SOBEL_H

#include "intrinsiccv.h"

namespace intrinsiccv {

namespace neon {
intrinsiccv_error_t sobel_3x3_horizontal_s16_u8(const uint8_t *src,
                                                size_t src_stride, int16_t *dst,
                                                size_t dst_stride, size_t width,
                                                size_t height, size_t channels);
intrinsiccv_error_t sobel_3x3_vertical_s16_u8(const uint8_t *src,
                                              size_t src_stride, int16_t *dst,
                                              size_t dst_stride, size_t width,
                                              size_t height, size_t channels);
}  // namespace neon

namespace sve2 {
intrinsiccv_error_t sobel_3x3_horizontal_s16_u8(const uint8_t *src,
                                                size_t src_stride, int16_t *dst,
                                                size_t dst_stride, size_t width,
                                                size_t height, size_t channels);
intrinsiccv_error_t sobel_3x3_vertical_s16_u8(const uint8_t *src,
                                              size_t src_stride, int16_t *dst,
                                              size_t dst_stride, size_t width,
                                              size_t height, size_t channels);
}  // namespace sve2

namespace sme2 {
intrinsiccv_error_t sobel_3x3_horizontal_s16_u8(const uint8_t *src,
                                                size_t src_stride, int16_t *dst,
                                                size_t dst_stride, size_t width,
                                                size_t height, size_t channels);
intrinsiccv_error_t sobel_3x3_vertical_s16_u8(const uint8_t *src,
                                              size_t src_stride, int16_t *dst,
                                              size_t dst_stride, size_t width,
                                              size_t height, size_t channels);
}  // namespace sme2

}  // namespace intrinsiccv

#endif  // INTRINSICCV_FILTERS_SOBEL_H
