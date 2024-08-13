// SPDX-FileCopyrightText: 2024 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#include "kleidicv/filters/separable_filter_2d.h"
#include "separable_filter_2d_sc.h"

namespace kleidicv::sme2 {

KLEIDICV_LOCALLY_STREAMING KLEIDICV_TARGET_FN_ATTRS kleidicv_error_t
separable_filter_2d_stripe_u8(const uint8_t *src, size_t src_stride,
                              uint8_t *dst, size_t dst_stride, size_t width,
                              size_t height, size_t y_begin, size_t y_end,
                              size_t channels, const uint8_t *kernel_x,
                              size_t kernel_width, const uint8_t *kernel_y,
                              size_t kernel_height,
                              kleidicv_border_type_t border_type,
                              kleidicv_filter_context_t *context) {
  return separable_filter_2d_stripe_u8_sc(
      src, src_stride, dst, dst_stride, width, height, y_begin, y_end, channels,
      kernel_x, kernel_width, kernel_y, kernel_height, border_type, context);
}

}  // namespace kleidicv::sme2
