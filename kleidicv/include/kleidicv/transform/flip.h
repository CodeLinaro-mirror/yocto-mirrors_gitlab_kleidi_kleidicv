// SPDX-FileCopyrightText: 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#ifndef KLEIDICV_TRANSFORM_FLIP_H
#define KLEIDICV_TRANSFORM_FLIP_H

#include <cstddef>

#include "kleidicv/kleidicv.h"
#include "kleidicv/utils.h"

extern "C" {
// For internal use only. See instead kleidicv_flip.
//
// [work_items_begin, work_items_end) identifies independent units of work.
//
// For out-of-place flips in any mode, each work item processes one source row.
// For in-place horizontal flips, each work item processes one row.
//
// In-place vertical and both-axes flips use mirrored row pairs, allowing one
// backend call to process several pairs while moving inward. Each work item
// owns the row at its index and the mirrored row on the opposite side of the
// image, except for the centre-row case described below.
//
// For odd-height in-place vertical flips, the centre row is unchanged and
// has no work item. For odd-height in-place both-axes flips, the final work
// item for the whole image owns only the centre row.
//
// Public API arguments must be validated before calling this function.
KLEIDICV_API_DECLARATION(kleidicv_flip_work_items, const void *src,
                         size_t src_stride, size_t width, size_t height,
                         void *dst, size_t dst_stride,
                         kleidicv_flip_mode_t flip_mode, size_t pixel_size,
                         size_t work_items_begin, size_t work_items_end);

// SME-preferred counterpart to kleidicv_flip_work_items with the same contract.
KLEIDICV_API_DECLARATION(kleidicv_flip_work_items_sme, const void *src,
                         size_t src_stride, size_t width, size_t height,
                         void *dst, size_t dst_stride,
                         kleidicv_flip_mode_t flip_mode, size_t pixel_size,
                         size_t work_items_begin, size_t work_items_end);
}  // extern "C"

namespace kleidicv {

template <typename ScalarType>
static inline kleidicv_error_t flip_validate(const void *src_void,
                                             size_t src_stride, size_t width,
                                             size_t height, void *dst_void,
                                             size_t dst_stride,
                                             kleidicv_flip_mode_t flip_mode) {
  MAKE_POINTER_CHECK_ALIGNMENT(const ScalarType, src, src_void);
  MAKE_POINTER_CHECK_ALIGNMENT(ScalarType, dst, dst_void);
  CHECK_POINTER_AND_STRIDE(src, src_stride, height);
  CHECK_POINTER_AND_STRIDE(dst, dst_stride, height);
  CHECK_IMAGE_SIZE(width, height);

  if (src == dst && src_stride != dst_stride) {
    return KLEIDICV_ERROR_RANGE;
  }

  switch (flip_mode) {
    case KLEIDICV_FLIP_HORIZONTAL:
    case KLEIDICV_FLIP_VERTICAL:
    case KLEIDICV_FLIP_BOTH:
      return KLEIDICV_OK;
    default:
      return KLEIDICV_ERROR_NOT_IMPLEMENTED;
  }
}

static inline kleidicv_error_t flip_validate(const void *src, size_t src_stride,
                                             size_t width, size_t height,
                                             void *dst, size_t dst_stride,
                                             kleidicv_flip_mode_t flip_mode,
                                             size_t pixel_size) {
  switch (pixel_size) {
    case sizeof(uint8_t):
    case sizeof(uint8_t) * 3:
      return flip_validate<uint8_t>(src, src_stride, width, height, dst,
                                    dst_stride, flip_mode);
    case sizeof(uint16_t):
    case sizeof(uint16_t) * 3:
      return flip_validate<uint16_t>(src, src_stride, width, height, dst,
                                     dst_stride, flip_mode);
    case sizeof(uint32_t):
      return flip_validate<uint32_t>(src, src_stride, width, height, dst,
                                     dst_stride, flip_mode);
    case sizeof(uint64_t):
      return flip_validate<uint64_t>(src, src_stride, width, height, dst,
                                     dst_stride, flip_mode);
    default:
      return KLEIDICV_ERROR_NOT_IMPLEMENTED;
  }
}

static inline size_t flip_work_item_count(size_t height, bool in_place,
                                          kleidicv_flip_mode_t flip_mode) {
  if (!in_place || flip_mode == KLEIDICV_FLIP_HORIZONTAL) {
    return height;
  }

  // In-place vertical flip: exclude the unchanged centre row for odd heights
  if (flip_mode == KLEIDICV_FLIP_VERTICAL) {
    return height / 2;
  }

  // In-place both-axes flip: include the centre row for odd heights
  return (height + 1) / 2;
}

namespace neon {
kleidicv_error_t flip_work_items(const void *src, size_t src_stride,
                                 size_t width, size_t height, void *dst,
                                 size_t dst_stride,
                                 kleidicv_flip_mode_t flip_mode,
                                 size_t pixel_size, size_t work_items_begin,
                                 size_t work_items_end);
}  // namespace neon

namespace sve2 {
kleidicv_error_t flip_work_items(const void *src, size_t src_stride,
                                 size_t width, size_t height, void *dst,
                                 size_t dst_stride,
                                 kleidicv_flip_mode_t flip_mode,
                                 size_t pixel_size, size_t work_items_begin,
                                 size_t work_items_end);
}  // namespace sve2

namespace sme {
kleidicv_error_t flip_work_items(const void *src, size_t src_stride,
                                 size_t width, size_t height, void *dst,
                                 size_t dst_stride,
                                 kleidicv_flip_mode_t flip_mode,
                                 size_t pixel_size, size_t work_items_begin,
                                 size_t work_items_end);
}  // namespace sme

namespace sme2 {
kleidicv_error_t flip_work_items(const void *src, size_t src_stride,
                                 size_t width, size_t height, void *dst,
                                 size_t dst_stride,
                                 kleidicv_flip_mode_t flip_mode,
                                 size_t pixel_size, size_t work_items_begin,
                                 size_t work_items_end);
}  // namespace sme2

}  // namespace kleidicv

#endif  // KLEIDICV_TRANSFORM_FLIP_H
