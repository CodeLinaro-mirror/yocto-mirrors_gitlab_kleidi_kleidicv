// SPDX-FileCopyrightText: 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#include <arm_sme.h>

#include <climits>

#include "kleidicv/transform/transpose.h"
#include "kleidicv/utils.h"
#include "rotate_transpose_common_sme.h"

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

// Most of the complexity comes from parameter checking.
// NOLINTBEGIN(readability-function-cognitive-complexity)
KLEIDICV_LOCALLY_STREAMING KLEIDICV_TARGET_FN_ATTRS kleidicv_error_t
transpose(const void *src_void, size_t src_stride, void *dst_void,
          size_t dst_stride, size_t width, size_t height, size_t pixel_size) {
  CHECK_IMAGE_SIZE(width, height);
  const uint8_t *src = reinterpret_cast<const uint8_t *>(src_void);
  uint8_t *dst = reinterpret_cast<uint8_t *>(dst_void);

  if (src == dst && width != height) {
    // Inplace transpose only implemented if width and height are the same
    return KLEIDICV_ERROR_NOT_IMPLEMENTED;
  }

  switch (pixel_size) {
    case 1:
      CHECK_POINTERS(src, dst);
      return transpose<1>(src, src_stride, dst, dst_stride, width, height);
    case 2:
      CHECKPTR(uint16_t, src, src_stride, height);
      CHECKPTR(uint16_t, dst, dst_stride, width);
      return transpose<2>(src, src_stride, dst, dst_stride, width, height);
    case 3:
      CHECK_POINTERS(src, dst);
      return transpose<3>(src, src_stride, dst, dst_stride, width, height);
    case 4:
      CHECKPTR(uint32_t, src, src_stride, height);
      CHECKPTR(uint32_t, dst, dst_stride, width);
      return transpose<4>(src, src_stride, dst, dst_stride, width, height);
    case 6:
      CHECKPTR(uint16_t, src, src_stride, height);
      CHECKPTR(uint16_t, dst, dst_stride, width);
      return transpose<6>(src, src_stride, dst, dst_stride, width, height);
    case 8:
      CHECKPTR(uint64_t, src, src_stride, height);
      CHECKPTR(uint64_t, dst, dst_stride, width);
      return transpose<8>(src, src_stride, dst, dst_stride, width, height);
    default:
      CHECK_POINTERS(src, dst);
      return KLEIDICV_ERROR_NOT_IMPLEMENTED;
  }
}
// NOLINTEND(readability-function-cognitive-complexity)

}  // namespace kleidicv::sme
