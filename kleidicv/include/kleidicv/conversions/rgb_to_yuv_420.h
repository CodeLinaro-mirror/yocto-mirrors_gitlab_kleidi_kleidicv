// SPDX-FileCopyrightText: 2025 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#ifndef KLEIDICV_CONVERSIONS_RGB_TO_YUV420_H
#define KLEIDICV_CONVERSIONS_RGB_TO_YUV420_H

#include <utility>

#include "kleidicv/config.h"
#include "kleidicv/kleidicv.h"
#include "kleidicv/types.h"

extern "C" {
// For internal use only. See instead `kleidicv_rgb_to_yuv420_sp_u8`.
// Converts a stripe (range of rows) of an RGB image to a semi-planar YUV420
// image format (NV12 or NV21). The stripe is defined by the range [begin, end].
KLEIDICV_API_DECLARATION(kleidicv_rgb_to_yuv420_sp_stripe_u8,
                         const uint8_t *src, size_t src_stride, uint8_t *y_dst,
                         size_t y_stride, uint8_t *uv_dst, size_t uv_stride,
                         size_t width, size_t height, bool is_nv21,
                         size_t begin, size_t end);

// For internal use only. See instead `kleidicv_rgba_to_yuv420_sp_u8`.
// Converts a stripe (range of rows) of an RGBA image to a semi-planar YUV420
// image format (NV12 or NV21). The stripe is defined by the range [begin, end].
KLEIDICV_API_DECLARATION(kleidicv_rgba_to_yuv420_sp_stripe_u8,
                         const uint8_t *src, size_t src_stride, uint8_t *y_dst,
                         size_t y_stride, uint8_t *uv_dst, size_t uv_stride,
                         size_t width, size_t height, bool is_nv21,
                         size_t begin, size_t end);

// For internal use only. See instead `kleidicv_bgr_to_yuv420_sp_u8`.
// Converts a stripe (range of rows) of a BGR image to a semi-planar YUV420
// image format (NV12 or NV21). The stripe is defined by the range [begin, end].
KLEIDICV_API_DECLARATION(kleidicv_bgr_to_yuv420_sp_stripe_u8,
                         const uint8_t *src, size_t src_stride, uint8_t *y_dst,
                         size_t y_stride, uint8_t *uv_dst, size_t uv_stride,
                         size_t width, size_t height, bool is_nv21,
                         size_t begin, size_t end);

// For internal use only. See instead `kleidicv_bgra_to_yuv420_sp_u8`.
// Converts a stripe (range of rows) of a BGRA image to a semi-planar YUV420
// image format (NV12 or NV21). The stripe is defined by the range [begin, end].
KLEIDICV_API_DECLARATION(kleidicv_bgra_to_yuv420_sp_stripe_u8,
                         const uint8_t *src, size_t src_stride, uint8_t *y_dst,
                         size_t y_stride, uint8_t *uv_dst, size_t uv_stride,
                         size_t width, size_t height, bool is_nv21,
                         size_t begin, size_t end);

// For internal use only. See instead `kleidicv_rgb_to_yuv420_p_u8`.
// Converts a stripe (range of rows) of an RGB image to a planar YUV420 image
// format (IYUV or YV12). The stripe is defined by the range [begin, end].
KLEIDICV_API_DECLARATION(kleidicv_rgb_to_yuv420_p_stripe_u8, const uint8_t *src,
                         size_t src_stride, uint8_t *dst, size_t dst_stride,
                         size_t width, size_t height, bool is_yv12,
                         size_t begin, size_t end);

// For internal use only. See instead `kleidicv_rgba_to_yuv420_p_u8`.
// Converts a stripe (range of rows) of an RGBA image to a planar YUV420
// image format (IYUV or YV12). The stripe is defined by the range [begin, end].
KLEIDICV_API_DECLARATION(kleidicv_rgba_to_yuv420_p_stripe_u8,
                         const uint8_t *src, size_t src_stride, uint8_t *dst,
                         size_t dst_stride, size_t width, size_t height,
                         bool is_yv12, size_t begin, size_t end);

// For internal use only. See instead `kleidicv_bgr_to_yuv420_p_u8`.
// Converts a stripe (range of rows) of a BGR image to a planar YUV420 image
// format (IYUV or YV12). The stripe is defined by the range [begin, end].
KLEIDICV_API_DECLARATION(kleidicv_bgr_to_yuv420_p_stripe_u8, const uint8_t *src,
                         size_t src_stride, uint8_t *dst, size_t dst_stride,
                         size_t width, size_t height, bool is_yv12,
                         size_t begin, size_t end);

// For internal use only. See instead `kleidicv_bgra_to_yuv420_p_u8`.
// Converts a stripe (range of rows) of a BGRA image to a planar YUV420
// image format (IYUV or YV12). The stripe is defined by the range [begin, end].
KLEIDICV_API_DECLARATION(kleidicv_bgra_to_yuv420_p_stripe_u8,
                         const uint8_t *src, size_t src_stride, uint8_t *dst,
                         size_t dst_stride, size_t width, size_t height,
                         bool is_yv12, size_t begin, size_t end);
}
namespace kleidicv {
namespace neon {
kleidicv_error_t rgb_to_yuv420_sp_stripe_u8(const uint8_t *src,
                                            size_t src_stride, uint8_t *y_dst,
                                            size_t y_stride, uint8_t *uv_dst,
                                            size_t uv_stride, size_t width,
                                            size_t height, bool is_nv21,
                                            size_t begin, size_t end);

kleidicv_error_t rgba_to_yuv420_sp_stripe_u8(const uint8_t *src,
                                             size_t src_stride, uint8_t *y_dst,
                                             size_t y_stride, uint8_t *uv_dst,
                                             size_t uv_stride, size_t width,
                                             size_t height, bool is_nv21,
                                             size_t begin, size_t end);

kleidicv_error_t bgr_to_yuv420_sp_stripe_u8(const uint8_t *src,
                                            size_t src_stride, uint8_t *y_dst,
                                            size_t y_stride, uint8_t *uv_dst,
                                            size_t uv_stride, size_t width,
                                            size_t height, bool is_nv21,
                                            size_t begin, size_t end);

kleidicv_error_t bgra_to_yuv420_sp_stripe_u8(const uint8_t *src,
                                             size_t src_stride, uint8_t *y_dst,
                                             size_t y_stride, uint8_t *uv_dst,
                                             size_t uv_stride, size_t width,
                                             size_t height, bool is_nv21,
                                             size_t begin, size_t end);

kleidicv_error_t rgb_to_yuv420_p_stripe_u8(const uint8_t *src,
                                           size_t src_stride, uint8_t *dst,
                                           size_t dst_stride, size_t width,
                                           size_t height, bool is_yv12,
                                           size_t begin, size_t end);

kleidicv_error_t rgba_to_yuv420_p_stripe_u8(const uint8_t *src,
                                            size_t src_stride, uint8_t *dst,
                                            size_t dst_stride, size_t width,
                                            size_t height, bool is_yv12,
                                            size_t begin, size_t end);

kleidicv_error_t bgr_to_yuv420_p_stripe_u8(const uint8_t *src,
                                           size_t src_stride, uint8_t *dst,
                                           size_t dst_stride, size_t width,
                                           size_t height, bool is_yv12,
                                           size_t begin, size_t end);

kleidicv_error_t bgra_to_yuv420_p_stripe_u8(const uint8_t *src,
                                            size_t src_stride, uint8_t *dst,
                                            size_t dst_stride, size_t width,
                                            size_t height, bool is_yv12,
                                            size_t begin, size_t end);

}  // namespace neon

namespace sve2 {
kleidicv_error_t rgb_to_yuv420_sp_stripe_u8(const uint8_t *src,
                                            size_t src_stride, uint8_t *y_dst,
                                            size_t y_stride, uint8_t *uv_dst,
                                            size_t uv_stride, size_t width,
                                            size_t height, bool is_nv21,
                                            size_t begin, size_t end);

kleidicv_error_t rgba_to_yuv420_sp_stripe_u8(const uint8_t *src,
                                             size_t src_stride, uint8_t *y_dst,
                                             size_t y_stride, uint8_t *uv_dst,
                                             size_t uv_stride, size_t width,
                                             size_t height, bool is_nv21,
                                             size_t begin, size_t end);

kleidicv_error_t bgr_to_yuv420_sp_stripe_u8(const uint8_t *src,
                                            size_t src_stride, uint8_t *y_dst,
                                            size_t y_stride, uint8_t *uv_dst,
                                            size_t uv_stride, size_t width,
                                            size_t height, bool is_nv21,
                                            size_t begin, size_t end);

kleidicv_error_t bgra_to_yuv420_sp_stripe_u8(const uint8_t *src,
                                             size_t src_stride, uint8_t *y_dst,
                                             size_t y_stride, uint8_t *uv_dst,
                                             size_t uv_stride, size_t width,
                                             size_t height, bool is_nv21,
                                             size_t begin, size_t end);

kleidicv_error_t rgb_to_yuv420_p_stripe_u8(const uint8_t *src,
                                           size_t src_stride, uint8_t *dst,
                                           size_t dst_stride, size_t width,
                                           size_t height, bool is_yv12,
                                           size_t begin, size_t end);

kleidicv_error_t rgba_to_yuv420_p_stripe_u8(const uint8_t *src,
                                            size_t src_stride, uint8_t *dst,
                                            size_t dst_stride, size_t width,
                                            size_t height, bool is_yv12,
                                            size_t begin, size_t end);

kleidicv_error_t bgr_to_yuv420_p_stripe_u8(const uint8_t *src,
                                           size_t src_stride, uint8_t *dst,
                                           size_t dst_stride, size_t width,
                                           size_t height, bool is_yv12,
                                           size_t begin, size_t end);

kleidicv_error_t bgra_to_yuv420_p_stripe_u8(const uint8_t *src,
                                            size_t src_stride, uint8_t *dst,
                                            size_t dst_stride, size_t width,
                                            size_t height, bool is_yv12,
                                            size_t begin, size_t end);

}  // namespace sve2

namespace sme {
kleidicv_error_t rgb_to_yuv420_sp_stripe_u8(const uint8_t *src,
                                            size_t src_stride, uint8_t *y_dst,
                                            size_t y_stride, uint8_t *uv_dst,
                                            size_t uv_stride, size_t width,
                                            size_t height, bool is_nv21,
                                            size_t begin, size_t end);

kleidicv_error_t rgba_to_yuv420_sp_stripe_u8(const uint8_t *src,
                                             size_t src_stride, uint8_t *y_dst,
                                             size_t y_stride, uint8_t *uv_dst,
                                             size_t uv_stride, size_t width,
                                             size_t height, bool is_nv21,
                                             size_t begin, size_t end);

kleidicv_error_t bgr_to_yuv420_sp_stripe_u8(const uint8_t *src,
                                            size_t src_stride, uint8_t *y_dst,
                                            size_t y_stride, uint8_t *uv_dst,
                                            size_t uv_stride, size_t width,
                                            size_t height, bool is_nv21,
                                            size_t begin, size_t end);

kleidicv_error_t bgra_to_yuv420_sp_stripe_u8(const uint8_t *src,
                                             size_t src_stride, uint8_t *y_dst,
                                             size_t y_stride, uint8_t *uv_dst,
                                             size_t uv_stride, size_t width,
                                             size_t height, bool is_nv21,
                                             size_t begin, size_t end);

kleidicv_error_t rgb_to_yuv420_p_stripe_u8(const uint8_t *src,
                                           size_t src_stride, uint8_t *dst,
                                           size_t dst_stride, size_t width,
                                           size_t height, bool is_yv12,
                                           size_t begin, size_t end);

kleidicv_error_t rgba_to_yuv420_p_stripe_u8(const uint8_t *src,
                                            size_t src_stride, uint8_t *dst,
                                            size_t dst_stride, size_t width,
                                            size_t height, bool is_yv12,
                                            size_t begin, size_t end);

kleidicv_error_t bgr_to_yuv420_p_stripe_u8(const uint8_t *src,
                                           size_t src_stride, uint8_t *dst,
                                           size_t dst_stride, size_t width,
                                           size_t height, bool is_yv12,
                                           size_t begin, size_t end);

kleidicv_error_t bgra_to_yuv420_p_stripe_u8(const uint8_t *src,
                                            size_t src_stride, uint8_t *dst,
                                            size_t dst_stride, size_t width,
                                            size_t height, bool is_yv12,
                                            size_t begin, size_t end);

}  // namespace sme

}  // namespace kleidicv

#endif  // KLEIDICV_CONVERSIONS_RGB_TO_YUV420_H
