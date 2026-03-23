// SPDX-FileCopyrightText: 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#include <arm_sme.h>

#include <algorithm>
#include <climits>

#include "kleidicv/arithmetics/transpose.h"
#include "kleidicv/utils.h"
#include "transpose_sme_common.h"

namespace kleidicv::sme {

// For 3channels, the indirect load is needed for interleaving:
/*                } else if constexpr (Channels == 3) {
          svuint8x3_t c = svld3(pred, p);
          svwrite_hor_za8_m(0, row_index + 0 * za_channel_padding,
   svptrue_b8(), svget3(c, 0)); svwrite_hor_za8_m(0, row_index + 1 *
   za_channel_padding, svptrue_b8(), svget3(c, 1)); svwrite_hor_za8_m(0,
   row_index + 2 * za_channel_padding, svptrue_b8(), svget3(c, 2));
        }*/

// src:          0  1  2  3  4  5
//              10 11 12 13 14 15
//
// transpose:    0 10 20 30 40 50
//               1 11 21 31 41 51
// -- load hor, store ver
// rotate cw:    50 40 30 20 10  0
//               51 41 31 21 11  1
// -- load hor/reversed, store ver
// rotate ccw:    5 15 25 35 45 55
//                4 14 24 34 44 54
// -- load hor, store ver/reversed

// unroll 128-bit blocks!

// inplace: ONLY FOR TRANSPOSE NOW!!!
// we can use the SV regs for temporary storage!
// transpose: load A, transpose B to A, transpose loaded to B
// rotate: 4way: load A, rotate B to A, rotate C to B, rotate D to
// C, rotate loaded to D

#define CHECKPTR(type, p, stride, height)                               \
  do {                                                                  \
    CHECK_POINTER_AND_STRIDE(reinterpret_cast<const type *>(p), stride, \
                             height);                                   \
    if (KLEIDICV_TARGET_NAMESPACE::is_misaligned<const type>(           \
            reinterpret_cast<uintptr_t>(p))) {                          \
      return KLEIDICV_ERROR_ALIGNMENT;                                  \
    }                                                                   \
  } while (false)

KLEIDICV_LOCALLY_STREAMING KLEIDICV_TARGET_FN_ATTRS kleidicv_error_t
transpose(const void *src, size_t src_stride, void *dst, size_t dst_stride,
          size_t width, size_t height, size_t pixel_size) {
  CHECK_IMAGE_SIZE(width, height);

  if (src == dst) {
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
    case 4:
      CHECKPTR(uint32_t, src, src_stride, height);
      CHECKPTR(uint32_t, dst, dst_stride, width);
      return transpose<4>(src, src_stride, dst, dst_stride, width, height);
    case 8:
      CHECKPTR(uint64_t, src, src_stride, height);
      CHECKPTR(uint64_t, dst, dst_stride, width);
      return transpose<8>(src, src_stride, dst, dst_stride, width, height);
    default:
      CHECK_POINTERS(src, dst);
      return KLEIDICV_ERROR_NOT_IMPLEMENTED;
  }
}
}  // namespace kleidicv::sme
