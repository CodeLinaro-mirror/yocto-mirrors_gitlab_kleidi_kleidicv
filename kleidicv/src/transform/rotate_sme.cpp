// SPDX-FileCopyrightText: 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#include <arm_sme.h>

#include <climits>

#include "kleidicv/transform/rotate.h"
#include "kleidicv/utils.h"
#include "transpose_sme_common.h"

namespace kleidicv::sme {

#define CHECKPTR(type, p, stride, height)                               \
  do {                                                                  \
    CHECK_POINTER_AND_STRIDE(reinterpret_cast<const type *>(p), stride, \
                             height);                                   \
    if (KLEIDICV_TARGET_NAMESPACE::is_misaligned<const type>(           \
            reinterpret_cast<uintptr_t>(p))) {                          \
      return KLEIDICV_ERROR_ALIGNMENT;                                  \
    }                                                                   \
  } while (false)

template <size_t kPixelSize>
static inline kleidicv_error_t rotate_by_angle(
    const uint8_t *src, size_t src_stride, uint8_t *dst, size_t dst_stride,
    size_t src_width, size_t src_height, int normalized_angle) {
  if (normalized_angle == 90) {
    return rotate_cw<kPixelSize>(src, src_stride, dst, dst_stride, src_width,
                                 src_height);
  }
  return rotate_ccw<kPixelSize>(src, src_stride, dst, dst_stride, src_width,
                                src_height);
}

// Most of the complexity comes from parameter checking.
// NOLINTBEGIN(readability-function-cognitive-complexity)
KLEIDICV_LOCALLY_STREAMING KLEIDICV_TARGET_FN_ATTRS kleidicv_error_t
rotate(const void *src_void, size_t src_stride, size_t src_width,
       size_t src_height, void *dst_void, size_t dst_stride, int angle,
       size_t pixel_size) {
  if (!rotate_is_implemented(src_void, dst_void, angle, pixel_size)) {
    return KLEIDICV_ERROR_NOT_IMPLEMENTED;
  }

  CHECK_IMAGE_SIZE(src_width, src_height);
  const uint8_t *src = reinterpret_cast<const uint8_t *>(src_void);
  uint8_t *dst = reinterpret_cast<uint8_t *>(dst_void);
  const int normalized_angle = (angle == 270) ? -90 : angle;

  switch (pixel_size) {
    case 1:
      CHECK_POINTERS(src, dst);
      return rotate_by_angle<1>(src, src_stride, dst, dst_stride, src_width,
                                src_height, normalized_angle);
    case 2:
      CHECKPTR(uint16_t, src, src_stride, src_height);
      CHECKPTR(uint16_t, dst, dst_stride, src_width);
      return rotate_by_angle<2>(src, src_stride, dst, dst_stride, src_width,
                                src_height, normalized_angle);
    case 3:
      CHECK_POINTERS(src, dst);
      return KLEIDICV_ERROR_NOT_IMPLEMENTED;
    case 4:
      CHECKPTR(uint32_t, src, src_stride, src_height);
      CHECKPTR(uint32_t, dst, dst_stride, src_width);
      return rotate_by_angle<4>(src, src_stride, dst, dst_stride, src_width,
                                src_height, normalized_angle);
    case 6:
      CHECK_POINTERS(src, dst);
      return KLEIDICV_ERROR_NOT_IMPLEMENTED;
    case 8:
      CHECKPTR(uint64_t, src, src_stride, src_height);
      CHECKPTR(uint64_t, dst, dst_stride, src_width);
      return rotate_by_angle<8>(src, src_stride, dst, dst_stride, src_width,
                                src_height, normalized_angle);
    default:
      CHECK_POINTERS(src, dst);
      return KLEIDICV_ERROR_NOT_IMPLEMENTED;
  }
}
// NOLINTEND(readability-function-cognitive-complexity)

}  // namespace kleidicv::sme
