// SPDX-FileCopyrightText: 2025 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#include "kleidicv/conversions/rgb_to_yuv_420.h"
#include "rgb_to_yuv420_sc.h"

namespace kleidicv::sme {

KLEIDICV_LOCALLY_STREAMING KLEIDICV_TARGET_FN_ATTRS kleidicv_error_t
rgb_to_yuv420_p_stripe_u8(const uint8_t *src, size_t src_stride, uint8_t *dst,
                          size_t dst_stride, size_t width, size_t height,
                          bool is_yv12, size_t begin, size_t end) {
  CHECK_POINTER_AND_STRIDE(src, src_stride, height);
  CHECK_POINTER_AND_STRIDE(dst, dst_stride, (height * 3 + 1) / 2);
  CHECK_IMAGE_SIZE(width, height);
  uint8_t *uv_dst = dst + dst_stride * height;
  return RGBxorBGRxToYUV420<false, true, false>::rgb2yuv420_operation_sc(
      src, src_stride, dst, dst_stride, uv_dst, dst_stride, width, height,
      is_yv12, begin, end);
}

KLEIDICV_LOCALLY_STREAMING KLEIDICV_TARGET_FN_ATTRS kleidicv_error_t
rgba_to_yuv420_p_stripe_u8(const uint8_t *src, size_t src_stride, uint8_t *dst,
                           size_t dst_stride, size_t width, size_t height,
                           bool is_yv12, size_t begin, size_t end) {
  CHECK_POINTER_AND_STRIDE(src, src_stride, height);
  CHECK_POINTER_AND_STRIDE(dst, dst_stride, (height * 3 + 1) / 2);
  CHECK_IMAGE_SIZE(width, height);
  uint8_t *uv_dst = dst + dst_stride * height;
  return RGBxorBGRxToYUV420<true, true, false>::rgb2yuv420_operation_sc(
      src, src_stride, dst, dst_stride, uv_dst, dst_stride, width, height,
      is_yv12, begin, end);
}

KLEIDICV_LOCALLY_STREAMING KLEIDICV_TARGET_FN_ATTRS kleidicv_error_t
bgr_to_yuv420_p_stripe_u8(const uint8_t *src, size_t src_stride, uint8_t *dst,
                          size_t dst_stride, size_t width, size_t height,
                          bool is_yv12, size_t begin, size_t end) {
  CHECK_POINTER_AND_STRIDE(src, src_stride, height);
  CHECK_POINTER_AND_STRIDE(dst, dst_stride, (height * 3 + 1) / 2);
  CHECK_IMAGE_SIZE(width, height);
  uint8_t *uv_dst = dst + dst_stride * height;
  return RGBxorBGRxToYUV420<false, false, false>::rgb2yuv420_operation_sc(
      src, src_stride, dst, dst_stride, uv_dst, dst_stride, width, height,
      is_yv12, begin, end);
}

KLEIDICV_LOCALLY_STREAMING KLEIDICV_TARGET_FN_ATTRS kleidicv_error_t
bgra_to_yuv420_p_stripe_u8(const uint8_t *src, size_t src_stride, uint8_t *dst,
                           size_t dst_stride, size_t width, size_t height,
                           bool is_yv12, size_t begin, size_t end) {
  CHECK_POINTER_AND_STRIDE(src, src_stride, height);
  CHECK_POINTER_AND_STRIDE(dst, dst_stride, (height * 3 + 1) / 2);
  CHECK_IMAGE_SIZE(width, height);
  uint8_t *uv_dst = dst + dst_stride * height;
  return RGBxorBGRxToYUV420<true, false, false>::rgb2yuv420_operation_sc(
      src, src_stride, dst, dst_stride, uv_dst, dst_stride, width, height,
      is_yv12, begin, end);
}

}  // namespace kleidicv::sme
