// SPDX-FileCopyrightText: 2023 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#include "filters/gaussian_blur.h"
#include "gaussian_blur_sc.h"

namespace intrinsiccv::sve2 {

INTRINSICCV_TARGET_FN_ATTRS
intrinsiccv_error_t gaussian_blur_5x5_u8(
    const uint8_t *src, size_t src_stride, uint8_t *dst, size_t dst_stride,
    size_t width, size_t height, size_t channels,
    intrinsiccv_border_type_t border_type,
    const intrinsiccv_filter_params_t *params) {
  return discrete_gaussian_blur<uint8_t, 5>(src, src_stride, dst, dst_stride,
                                            width, height, channels,
                                            border_type, params);
}

}  // namespace intrinsiccv::sve2
