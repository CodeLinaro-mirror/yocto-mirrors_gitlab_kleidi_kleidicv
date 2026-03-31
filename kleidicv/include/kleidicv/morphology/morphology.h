// SPDX-FileCopyrightText: 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#ifndef KLEIDICV_MORPHOLOGY_MORPHOLOGY_H
#define KLEIDICV_MORPHOLOGY_MORPHOLOGY_H

#include "kleidicv/ctypes.h"
#include "kleidicv/morphology/workspace.h"
#include "kleidicv/utils.h"

namespace kleidicv {

namespace neon {

template <typename T>
kleidicv_error_t dilate(const T *src, size_t src_stride, T *dst,
                        size_t dst_stride, size_t width, size_t height,
                        size_t channels, size_t kernel_width,
                        size_t kernel_height, size_t anchor_x, size_t anchor_y,
                        kleidicv_border_type_t border_type,
                        const uint8_t *border_value, size_t iterations);

template <typename T>
kleidicv_error_t erode(const T *src, size_t src_stride, T *dst,
                       size_t dst_stride, size_t width, size_t height,
                       size_t channels, size_t kernel_width,
                       size_t kernel_height, size_t anchor_x, size_t anchor_y,
                       kleidicv_border_type_t border_type,
                       const uint8_t *border_value, size_t iterations);

}  // namespace neon

namespace sve2 {

template <typename T>
kleidicv_error_t dilate(const T *src, size_t src_stride, T *dst,
                        size_t dst_stride, size_t width, size_t height,
                        size_t channels, size_t kernel_width,
                        size_t kernel_height, size_t anchor_x, size_t anchor_y,
                        kleidicv_border_type_t border_type,
                        const uint8_t *border_value, size_t iterations);

template <typename T>
kleidicv_error_t erode(const T *src, size_t src_stride, T *dst,
                       size_t dst_stride, size_t width, size_t height,
                       size_t channels, size_t kernel_width,
                       size_t kernel_height, size_t anchor_x, size_t anchor_y,
                       kleidicv_border_type_t border_type,
                       const uint8_t *border_value, size_t iterations);

}  // namespace sve2

namespace sme {

template <typename T>
kleidicv_error_t dilate(const T *src, size_t src_stride, T *dst,
                        size_t dst_stride, size_t width, size_t height,
                        size_t channels, size_t kernel_width,
                        size_t kernel_height, size_t anchor_x, size_t anchor_y,
                        kleidicv_border_type_t border_type,
                        const uint8_t *border_value, size_t iterations);

template <typename T>
kleidicv_error_t erode(const T *src, size_t src_stride, T *dst,
                       size_t dst_stride, size_t width, size_t height,
                       size_t channels, size_t kernel_width,
                       size_t kernel_height, size_t anchor_x, size_t anchor_y,
                       kleidicv_border_type_t border_type,
                       const uint8_t *border_value, size_t iterations);

}  // namespace sme

namespace sme2 {

template <typename T>
kleidicv_error_t dilate(const T *src, size_t src_stride, T *dst,
                        size_t dst_stride, size_t width, size_t height,
                        size_t channels, size_t kernel_width,
                        size_t kernel_height, size_t anchor_x, size_t anchor_y,
                        kleidicv_border_type_t border_type,
                        const uint8_t *border_value, size_t iterations);

template <typename T>
kleidicv_error_t erode(const T *src, size_t src_stride, T *dst,
                       size_t dst_stride, size_t width, size_t height,
                       size_t channels, size_t kernel_width,
                       size_t kernel_height, size_t anchor_x, size_t anchor_y,
                       kleidicv_border_type_t border_type,
                       const uint8_t *border_value, size_t iterations);

}  // namespace sme2

}  // namespace kleidicv

namespace KLEIDICV_TARGET_NAMESPACE {

inline bool morphology_is_implemented(size_t width, size_t height,
                                      size_t kernel_width, size_t kernel_height,
                                      size_t channels) KLEIDICV_STREAMING {
  if (channels > KLEIDICV_MAXIMUM_CHANNEL_COUNT) {
    return false;
  }

  if (width < kernel_width - 1 || height < kernel_height - 1) {
    return false;
  }

  return true;
}

}  // namespace KLEIDICV_TARGET_NAMESPACE

#endif  // KLEIDICV_MORPHOLOGY_MORPHOLOGY_H
