// SPDX-FileCopyrightText: 2023 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#ifndef INTRINSICCV_RESIZE_RESIZE_H
#define INTRINSICCV_RESIZE_RESIZE_H

#include "intrinsiccv.h"

namespace intrinsiccv {

namespace neon {
void resize_to_quarter_u8(const uint8_t *src, size_t src_stride,
                          size_t src_width, size_t src_height, uint8_t *dst,
                          size_t dst_stride, size_t dst_width,
                          size_t dst_height);
}  // namespace neon

namespace sve2 {
void resize_to_quarter_u8(const uint8_t *src, size_t src_stride,
                          size_t src_width, size_t src_height, uint8_t *dst,
                          size_t dst_stride, size_t dst_width,
                          size_t dst_height);
}  // namespace sve2

namespace sme2 {
void resize_to_quarter_u8(const uint8_t *src, size_t src_stride,
                          size_t src_width, size_t src_height, uint8_t *dst,
                          size_t dst_stride, size_t dst_width,
                          size_t dst_height);
}  // namespace sme2

}  // namespace intrinsiccv

#endif  // INTRINSICCV_RESIZE_RESIZE_H
