// SPDX-FileCopyrightText: 2023 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#include "gaussian_blur_sc.h"
#include "kleidicv/filters/gaussian_blur.h"

namespace kleidicv::sme2 {

KLEIDICV_LOCALLY_STREAMING KLEIDICV_TARGET_FN_ATTRS kleidicv_error_t
gaussian_blur_3x3_u8(const uint8_t *src, size_t src_stride, uint8_t *dst,
                     size_t dst_stride, size_t width, size_t height,
                     size_t channels, kleidicv_border_type_t border_type,
                     kleidicv_filter_context_t *context) {
  return discrete_gaussian_blur<uint8_t, 3>(src, src_stride, dst, dst_stride,
                                            width, height, channels,
                                            border_type, context);
}

KLEIDICV_LOCALLY_STREAMING KLEIDICV_TARGET_FN_ATTRS kleidicv_error_t
gaussian_blur_5x5_u8(const uint8_t *src, size_t src_stride, uint8_t *dst,
                     size_t dst_stride, size_t width, size_t height,
                     size_t channels, kleidicv_border_type_t border_type,
                     kleidicv_filter_context_t *context) {
  return discrete_gaussian_blur<uint8_t, 5>(src, src_stride, dst, dst_stride,
                                            width, height, channels,
                                            border_type, context);
}

}  // namespace kleidicv::sme2
