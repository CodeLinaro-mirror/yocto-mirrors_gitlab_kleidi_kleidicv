// SPDX-FileCopyrightText: 2023 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#ifndef INTRINSICCV_FILTERS_GAUSSIAN_BLUR_H
#define INTRINSICCV_FILTERS_GAUSSIAN_BLUR_H

#include "config.h"
#include "types.h"

namespace intrinsiccv {

namespace neon {

void gaussian_blur_3x3_u8(const uint8_t *src, size_t src_stride, uint8_t *dst,
                          size_t dst_stride, size_t width, size_t height,
                          size_t channels,
                          intrinsiccv_border_type_t border_type,
                          const intrinsiccv_filter_params_t *params);

void gaussian_blur_5x5_u8(const uint8_t *src, size_t src_stride, uint8_t *dst,
                          size_t dst_stride, size_t width, size_t height,
                          size_t channels,
                          intrinsiccv_border_type_t border_type,
                          const intrinsiccv_filter_params_t *params);

}  // namespace neon

namespace sve2 {

void gaussian_blur_5x5_u8(const uint8_t *src, size_t src_stride, uint8_t *dst,
                          size_t dst_stride, size_t width, size_t height,
                          size_t channels,
                          intrinsiccv_border_type_t border_type,
                          const intrinsiccv_filter_params_t *params);

}  // namespace sve2

namespace sme2 {

void gaussian_blur_5x5_u8(const uint8_t *src, size_t src_stride, uint8_t *dst,
                          size_t dst_stride, size_t width, size_t height,
                          size_t channels,
                          intrinsiccv_border_type_t border_type,
                          const intrinsiccv_filter_params_t *params);

}  // namespace sme2

}  // namespace intrinsiccv

#endif  // INTRINSICCV_FILTERS_GAUSSIAN_BLUR_H
