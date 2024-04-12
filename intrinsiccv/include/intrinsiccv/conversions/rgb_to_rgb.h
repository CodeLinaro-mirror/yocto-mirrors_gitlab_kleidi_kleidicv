// SPDX-FileCopyrightText: 2023 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#ifndef KLEIDICV_CONVERSIONS_RGB_TO_RGB_H
#define KLEIDICV_CONVERSIONS_RGB_TO_RGB_H

#include "intrinsiccv/intrinsiccv.h"

namespace intrinsiccv {

namespace neon {

intrinsiccv_error_t rgb_to_bgr_u8(const uint8_t *src, size_t src_stride,
                                  uint8_t *dst, size_t dst_stride, size_t width,
                                  size_t height);
intrinsiccv_error_t rgba_to_bgra_u8(const uint8_t *src, size_t src_stride,
                                    uint8_t *dst, size_t dst_stride,
                                    size_t width, size_t height);
intrinsiccv_error_t rgb_to_bgra_u8(const uint8_t *src, size_t src_stride,
                                   uint8_t *dst, size_t dst_stride,
                                   size_t width, size_t height);
intrinsiccv_error_t rgb_to_rgba_u8(const uint8_t *src, size_t src_stride,
                                   uint8_t *dst, size_t dst_stride,
                                   size_t width, size_t height);
intrinsiccv_error_t rgba_to_bgr_u8(const uint8_t *src, size_t src_stride,
                                   uint8_t *dst, size_t dst_stride,
                                   size_t width, size_t height);
intrinsiccv_error_t rgba_to_rgb_u8(const uint8_t *src, size_t src_stride,
                                   uint8_t *dst, size_t dst_stride,
                                   size_t width, size_t height);

}  // namespace neon

namespace sve2 {

intrinsiccv_error_t rgb_to_bgr_u8(const uint8_t *src, size_t src_stride,
                                  uint8_t *dst, size_t dst_stride, size_t width,
                                  size_t height);
intrinsiccv_error_t rgba_to_bgra_u8(const uint8_t *src, size_t src_stride,
                                    uint8_t *dst, size_t dst_stride,
                                    size_t width, size_t height);
intrinsiccv_error_t rgb_to_bgra_u8(const uint8_t *src, size_t src_stride,
                                   uint8_t *dst, size_t dst_stride,
                                   size_t width, size_t height);
intrinsiccv_error_t rgb_to_rgba_u8(const uint8_t *src, size_t src_stride,
                                   uint8_t *dst, size_t dst_stride,
                                   size_t width, size_t height);
intrinsiccv_error_t rgba_to_bgr_u8(const uint8_t *src, size_t src_stride,
                                   uint8_t *dst, size_t dst_stride,
                                   size_t width, size_t height);
intrinsiccv_error_t rgba_to_rgb_u8(const uint8_t *src, size_t src_stride,
                                   uint8_t *dst, size_t dst_stride,
                                   size_t width, size_t height);

}  // namespace sve2

namespace sme2 {

intrinsiccv_error_t rgb_to_bgr_u8(const uint8_t *src, size_t src_stride,
                                  uint8_t *dst, size_t dst_stride, size_t width,
                                  size_t height);

intrinsiccv_error_t rgba_to_bgra_u8(const uint8_t *src, size_t src_stride,
                                    uint8_t *dst, size_t dst_stride,
                                    size_t width, size_t height);

intrinsiccv_error_t rgb_to_bgra_u8(const uint8_t *src, size_t src_stride,
                                   uint8_t *dst, size_t dst_stride,
                                   size_t width, size_t height);

intrinsiccv_error_t rgb_to_rgba_u8(const uint8_t *src, size_t src_stride,
                                   uint8_t *dst, size_t dst_stride,
                                   size_t width, size_t height);

intrinsiccv_error_t rgba_to_bgr_u8(const uint8_t *src, size_t src_stride,
                                   uint8_t *dst, size_t dst_stride,
                                   size_t width, size_t height);

intrinsiccv_error_t rgba_to_rgb_u8(const uint8_t *src, size_t src_stride,
                                   uint8_t *dst, size_t dst_stride,
                                   size_t width, size_t height);

}  // namespace sme2

}  // namespace intrinsiccv

#endif  // KLEIDICV_CONVERSIONS_RGB_TO_RGB_H
