// SPDX-FileCopyrightText: 2023 - 2025 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#ifndef KLEIDICV_CONVERSIONS_YUV_420_TO_RGB_H
#define KLEIDICV_CONVERSIONS_YUV_420_TO_RGB_H

#include "kleidicv/kleidicv.h"

extern "C" {

// For internal use only. See instead kleidicv_yuv_p_to_rgb_u8.
// Converts a stripe (range of rows) of a planar YUV420 image (I420 or YV12)
// to RGB format. The stripe is defined by the range [begin, end].
KLEIDICV_API_DECLARATION(kleidicv_yuv_p_to_rgb_stripe_u8, const uint8_t *src,
                         size_t src_stride, uint8_t *dst, size_t dst_stride,
                         size_t width, size_t height, bool is_yv12,
                         size_t begin, size_t end);

// For internal use only. See instead kleidicv_yuv_p_to_rgba_u8.
// Converts a stripe (range of rows) of a planar YUV420 image (I420 or YV12)
// to RGBA format. The stripe is defined by the range [begin, end].
KLEIDICV_API_DECLARATION(kleidicv_yuv_p_to_rgba_stripe_u8, const uint8_t *src,
                         size_t src_stride, uint8_t *dst, size_t dst_stride,
                         size_t width, size_t height, bool is_yv12,
                         size_t begin, size_t end);

// For internal use only. See instead kleidicv_yuv_p_to_bgr_u8.
// Converts a stripe (range of rows) of a planar YUV420 image (I420 or YV12)
// to BGR format. The stripe is defined by the range [begin, end].
KLEIDICV_API_DECLARATION(kleidicv_yuv_p_to_bgr_stripe_u8, const uint8_t *src,
                         size_t src_stride, uint8_t *dst, size_t dst_stride,
                         size_t width, size_t height, bool is_yv12,
                         size_t begin, size_t end);

// For internal use only. See instead kleidicv_yuv_p_to_bgra_u8.
// Converts a stripe (range of rows) of a planar YUV420 image (I420 or YV12)
// to BGRA format. The stripe is defined by the range [begin, end].
KLEIDICV_API_DECLARATION(kleidicv_yuv_p_to_bgra_stripe_u8, const uint8_t *src,
                         size_t src_stride, uint8_t *dst, size_t dst_stride,
                         size_t width, size_t height, bool is_yv12,
                         size_t begin, size_t end);
}

namespace kleidicv {
namespace neon {
kleidicv_error_t yuv_sp_to_rgb_u8(const uint8_t *src_y, size_t src_y_stride,
                                  const uint8_t *src_uv, size_t src_uv_stride,
                                  uint8_t *dst, size_t dst_stride, size_t width,
                                  size_t height, bool is_nv21);

kleidicv_error_t yuv_sp_to_rgba_u8(const uint8_t *src_y, size_t src_y_stride,
                                   const uint8_t *src_uv, size_t src_uv_stride,
                                   uint8_t *dst, size_t dst_stride,
                                   size_t width, size_t height, bool is_nv21);

kleidicv_error_t yuv_sp_to_bgr_u8(const uint8_t *src_y, size_t src_y_stride,
                                  const uint8_t *src_uv, size_t src_uv_stride,
                                  uint8_t *dst, size_t dst_stride, size_t width,
                                  size_t height, bool is_nv21);

kleidicv_error_t yuv_sp_to_bgra_u8(const uint8_t *src_y, size_t src_y_stride,
                                   const uint8_t *src_uv, size_t src_uv_stride,
                                   uint8_t *dst, size_t dst_stride,
                                   size_t width, size_t height, bool is_nv21);

kleidicv_error_t yuv_p_to_rgb_stripe_u8(const uint8_t *src, size_t src_stride,
                                        uint8_t *dst, size_t dst_stride,
                                        size_t width, size_t height,
                                        bool is_yv12, size_t begin, size_t end);

kleidicv_error_t yuv_p_to_rgba_stripe_u8(const uint8_t *src, size_t src_stride,
                                         uint8_t *dst, size_t dst_stride,
                                         size_t width, size_t height,
                                         bool is_yv12, size_t begin,
                                         size_t end);

kleidicv_error_t yuv_p_to_bgr_stripe_u8(const uint8_t *src, size_t src_stride,
                                        uint8_t *dst, size_t dst_stride,
                                        size_t width, size_t height,
                                        bool is_yv12, size_t begin, size_t end);

kleidicv_error_t yuv_p_to_bgra_stripe_u8(const uint8_t *src, size_t src_stride,
                                         uint8_t *dst, size_t dst_stride,
                                         size_t width, size_t height,
                                         bool is_yv12, size_t begin,
                                         size_t end);
}  // namespace neon

namespace sve2 {
kleidicv_error_t yuv_sp_to_rgb_u8(const uint8_t *src_y, size_t src_y_stride,
                                  const uint8_t *src_uv, size_t src_uv_stride,
                                  uint8_t *dst, size_t dst_stride, size_t width,
                                  size_t height, bool is_nv21);

kleidicv_error_t yuv_sp_to_rgba_u8(const uint8_t *src_y, size_t src_y_stride,
                                   const uint8_t *src_uv, size_t src_uv_stride,
                                   uint8_t *dst, size_t dst_stride,
                                   size_t width, size_t height, bool is_nv21);

kleidicv_error_t yuv_sp_to_bgr_u8(const uint8_t *src_y, size_t src_y_stride,
                                  const uint8_t *src_uv, size_t src_uv_stride,
                                  uint8_t *dst, size_t dst_stride, size_t width,
                                  size_t height, bool is_nv21);

kleidicv_error_t yuv_sp_to_bgra_u8(const uint8_t *src_y, size_t src_y_stride,
                                   const uint8_t *src_uv, size_t src_uv_stride,
                                   uint8_t *dst, size_t dst_stride,
                                   size_t width, size_t height, bool is_nv21);

kleidicv_error_t yuv_p_to_rgb_stripe_u8(const uint8_t *src, size_t src_stride,
                                        uint8_t *dst, size_t dst_stride,
                                        size_t width, size_t height,
                                        bool is_yv12, size_t begin, size_t end);

kleidicv_error_t yuv_p_to_rgba_stripe_u8(const uint8_t *src, size_t src_stride,
                                         uint8_t *dst, size_t dst_stride,
                                         size_t width, size_t height,
                                         bool is_yv12, size_t begin,
                                         size_t end);

kleidicv_error_t yuv_p_to_bgr_stripe_u8(const uint8_t *src, size_t src_stride,
                                        uint8_t *dst, size_t dst_stride,
                                        size_t width, size_t height,
                                        bool is_yv12, size_t begin, size_t end);

kleidicv_error_t yuv_p_to_bgra_stripe_u8(const uint8_t *src, size_t src_stride,
                                         uint8_t *dst, size_t dst_stride,
                                         size_t width, size_t height,
                                         bool is_yv12, size_t begin,
                                         size_t end);
}  // namespace sve2

namespace sme {
kleidicv_error_t yuv_sp_to_rgb_u8(const uint8_t *src_y, size_t src_y_stride,
                                  const uint8_t *src_uv, size_t src_uv_stride,
                                  uint8_t *dst, size_t dst_stride, size_t width,
                                  size_t height, bool is_nv21);

kleidicv_error_t yuv_sp_to_rgba_u8(const uint8_t *src_y, size_t src_y_stride,
                                   const uint8_t *src_uv, size_t src_uv_stride,
                                   uint8_t *dst, size_t dst_stride,
                                   size_t width, size_t height, bool is_nv21);

kleidicv_error_t yuv_sp_to_bgr_u8(const uint8_t *src_y, size_t src_y_stride,
                                  const uint8_t *src_uv, size_t src_uv_stride,
                                  uint8_t *dst, size_t dst_stride, size_t width,
                                  size_t height, bool is_nv21);

kleidicv_error_t yuv_sp_to_bgra_u8(const uint8_t *src_y, size_t src_y_stride,
                                   const uint8_t *src_uv, size_t src_uv_stride,
                                   uint8_t *dst, size_t dst_stride,
                                   size_t width, size_t height, bool is_nv21);

kleidicv_error_t yuv_p_to_rgb_stripe_u8(const uint8_t *src, size_t src_stride,
                                        uint8_t *dst, size_t dst_stride,
                                        size_t width, size_t height,
                                        bool is_yv12, size_t begin, size_t end);

kleidicv_error_t yuv_p_to_rgba_stripe_u8(const uint8_t *src, size_t src_stride,
                                         uint8_t *dst, size_t dst_stride,
                                         size_t width, size_t height,
                                         bool is_yv12, size_t begin,
                                         size_t end);

kleidicv_error_t yuv_p_to_bgr_stripe_u8(const uint8_t *src, size_t src_stride,
                                        uint8_t *dst, size_t dst_stride,
                                        size_t width, size_t height,
                                        bool is_yv12, size_t begin, size_t end);

kleidicv_error_t yuv_p_to_bgra_stripe_u8(const uint8_t *src, size_t src_stride,
                                         uint8_t *dst, size_t dst_stride,
                                         size_t width, size_t height,
                                         bool is_yv12, size_t begin,
                                         size_t end);
}  // namespace sme

}  // namespace kleidicv

#endif  // KLEIDICV_CONVERSIONS_YUV_420_TO_RGB_H
