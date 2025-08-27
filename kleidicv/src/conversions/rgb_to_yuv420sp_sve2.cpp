// SPDX-FileCopyrightText: 2025 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#include "kleidicv/conversions/rgb_to_yuv_420.h"
#include "rgb_to_yuv420_sc.h"

namespace kleidicv::sve2 {

KLEIDICV_TARGET_FN_ATTRS
kleidicv_error_t rgb_to_yuv420_sp_stripe_u8(const uint8_t *src,
                                            size_t src_stride, uint8_t *y_dst,
                                            size_t y_stride, uint8_t *uv_dst,
                                            size_t uv_stride, size_t width,
                                            size_t height, bool is_nv21,
                                            size_t begin, size_t end) {
  CHECK_POINTER_AND_STRIDE(src, src_stride, height);
  CHECK_POINTER_AND_STRIDE(y_dst, y_stride, height);
  CHECK_POINTER_AND_STRIDE(uv_dst, uv_stride, (height + 1) / 2);
  CHECK_IMAGE_SIZE(width, height);
  return RGBxorBGRxToYUV420<false, true, true>::rgb2yuv420_operation_sc(
      src, src_stride, y_dst, y_stride, uv_dst, uv_stride, width, height,
      is_nv21, begin, end);
}

KLEIDICV_TARGET_FN_ATTRS
kleidicv_error_t rgba_to_yuv420_sp_stripe_u8(const uint8_t *src,
                                             size_t src_stride, uint8_t *y_dst,
                                             size_t y_stride, uint8_t *uv_dst,
                                             size_t uv_stride, size_t width,
                                             size_t height, bool is_nv21,
                                             size_t begin, size_t end) {
  CHECK_POINTER_AND_STRIDE(src, src_stride, height);
  CHECK_POINTER_AND_STRIDE(y_dst, y_stride, height);
  CHECK_POINTER_AND_STRIDE(uv_dst, uv_stride, (height + 1) / 2);
  CHECK_IMAGE_SIZE(width, height);
  return RGBxorBGRxToYUV420<true, true, true>::rgb2yuv420_operation_sc(
      src, src_stride, y_dst, y_stride, uv_dst, uv_stride, width, height,
      is_nv21, begin, end);
}

KLEIDICV_TARGET_FN_ATTRS
kleidicv_error_t bgr_to_yuv420_sp_stripe_u8(const uint8_t *src,
                                            size_t src_stride, uint8_t *y_dst,
                                            size_t y_stride, uint8_t *uv_dst,
                                            size_t uv_stride, size_t width,
                                            size_t height, bool is_nv21,
                                            size_t begin, size_t end) {
  CHECK_POINTER_AND_STRIDE(src, src_stride, height);
  CHECK_POINTER_AND_STRIDE(y_dst, y_stride, height);
  CHECK_POINTER_AND_STRIDE(uv_dst, uv_stride, (height + 1) / 2);
  CHECK_IMAGE_SIZE(width, height);
  return RGBxorBGRxToYUV420<false, false, true>::rgb2yuv420_operation_sc(
      src, src_stride, y_dst, y_stride, uv_dst, uv_stride, width, height,
      is_nv21, begin, end);
}

KLEIDICV_TARGET_FN_ATTRS
kleidicv_error_t bgra_to_yuv420_sp_stripe_u8(const uint8_t *src,
                                             size_t src_stride, uint8_t *y_dst,
                                             size_t y_stride, uint8_t *uv_dst,
                                             size_t uv_stride, size_t width,
                                             size_t height, bool is_nv21,
                                             size_t begin, size_t end) {
  CHECK_POINTER_AND_STRIDE(src, src_stride, height);
  CHECK_POINTER_AND_STRIDE(y_dst, y_stride, height);
  CHECK_POINTER_AND_STRIDE(uv_dst, uv_stride, (height + 1) / 2);
  CHECK_IMAGE_SIZE(width, height);
  return RGBxorBGRxToYUV420<true, false, true>::rgb2yuv420_operation_sc(
      src, src_stride, y_dst, y_stride, uv_dst, uv_stride, width, height,
      is_nv21, begin, end);
}

}  // namespace kleidicv::sve2
