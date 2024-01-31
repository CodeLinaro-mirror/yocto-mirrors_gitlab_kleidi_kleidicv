// SPDX-FileCopyrightText: 2023 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#ifndef INTRINSICCV_CONVERSIONS_GRAY_TO_RGB_H
#define INTRINSICCV_CONVERSIONS_GRAY_TO_RGB_H

#include "intrinsiccv.h"

namespace intrinsiccv {

namespace neon {

intrinsiccv_error_t gray_to_rgb_u8(const uint8_t *src, size_t src_stride,
                                   uint8_t *dst, size_t dst_stride,
                                   size_t width, size_t height);

intrinsiccv_error_t gray_to_rgba_u8(const uint8_t *src, size_t src_stride,
                                    uint8_t *dst, size_t dst_stride,
                                    size_t width, size_t height);

}  // namespace neon

namespace sve2 {

intrinsiccv_error_t gray_to_rgb_u8(const uint8_t *src, size_t src_stride,
                                   uint8_t *dst, size_t dst_stride,
                                   size_t width, size_t height);

intrinsiccv_error_t gray_to_rgba_u8(const uint8_t *src, size_t src_stride,
                                    uint8_t *dst, size_t dst_stride,
                                    size_t width, size_t height);

}  // namespace sve2

namespace sme2 {

intrinsiccv_error_t gray_to_rgb_u8(const uint8_t *src, size_t src_stride,
                                   uint8_t *dst, size_t dst_stride,
                                   size_t width, size_t height);

intrinsiccv_error_t gray_to_rgba_u8(const uint8_t *src, size_t src_stride,
                                    uint8_t *dst, size_t dst_stride,
                                    size_t width, size_t height);

}  // namespace sme2

}  // namespace intrinsiccv

#endif  // INTRINSICCV_CONVERSIONS_GRAY_TO_RGB_H
