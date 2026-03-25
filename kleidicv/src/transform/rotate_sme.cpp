// SPDX-FileCopyrightText: 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#include "kleidicv/transform/rotate.h"
#include "kleidicv/types.h"
#include "transpose_sme_common.h"

namespace kleidicv::sme {

KLEIDICV_LOCALLY_STREAMING KLEIDICV_TARGET_FN_ATTRS kleidicv_error_t
rotate(const void *src_void, size_t src_stride, size_t src_width,
       size_t src_height, void *dst_void, size_t dst_stride, int angle,
       size_t element_size) {
  if (!rotate_is_implemented(src_void, dst_void, angle, element_size)) {
    return KLEIDICV_ERROR_NOT_IMPLEMENTED;
  }

  return kleidicv::neon::rotate(src_void, src_stride, src_width, src_height,
                                dst_void, dst_stride, angle, element_size);
  /*
  MAKE_POINTER_CHECK_ALIGNMENT(const uint8_t, src, src_void);
  MAKE_POINTER_CHECK_ALIGNMENT(uint8_t, dst, dst_void);
  CHECK_POINTER_AND_STRIDE(src, src_stride, src_height);
  CHECK_POINTER_AND_STRIDE(dst, dst_stride, src_width);
  CHECK_IMAGE_SIZE(src_width, src_height);

  const int normalized_angle = (angle == 270) ? -90 : angle;
  Rectangle rect{src_width, src_height};
  Rows<const uint8_t> src_rows{src, src_stride};
  Rows<uint8_t> dst_rows{dst, dst_stride};

    if (normalized_angle == 90) {
      rotate_u8_cw_out_of_place_sc(src_rows, dst_rows, rect);
    } else {
      rotate_u8_ccw_out_of_place_sc(src_rows, dst_rows, rect);
    }
  */
  return KLEIDICV_OK;
}

}  // namespace kleidicv::sme
