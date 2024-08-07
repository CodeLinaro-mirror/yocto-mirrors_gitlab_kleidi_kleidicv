// SPDX-FileCopyrightText: 2023 - 2024 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#include "kleidicv/conversions/gray_to_rgb.h"
#include "kleidicv/dispatch.h"
#include "kleidicv/kleidicv.h"

#define KLEIDICV_DEFINE_C_API(name, partialname)           \
  KLEIDICV_MULTIVERSION_C_API(                             \
      name, &kleidicv::neon::partialname,                  \
      KLEIDICV_SVE2_IMPL_IF(&kleidicv::sve2::partialname), \
      &kleidicv::sme2::partialname)

KLEIDICV_DEFINE_C_API(kleidicv_auto_backend_gray_to_rgb_u8, gray_to_rgb_u8);
KLEIDICV_DEFINE_C_API(kleidicv_gray_to_rgba_u8, gray_to_rgba_u8);

kleidicv_error_t kleidicv_gray_to_rgb_u8(const uint8_t *src, size_t src_stride,
                                         uint8_t *dst, size_t dst_stride,
                                         size_t width, size_t height,
                                         kleidicv_backend_t backend) {
  switch (backend) {
    case KLEIDICV_BACKEND_AUTO:
      return kleidicv_auto_backend_gray_to_rgb_u8(src, src_stride, dst,
                                                  dst_stride, width, height);
    case KLEIDICV_BACKEND_NEON:
      return kleidicv::neon::gray_to_rgb_u8(src, src_stride, dst, dst_stride,
                                            width, height);
#ifdef KLEIDICV_HAVE_SVE2
    case KLEIDICV_BACKEND_SVE2:
      return kleidicv::sve2::gray_to_rgb_u8(src, src_stride, dst, dst_stride,
                                            width, height);
#endif

#ifdef KLEIDICV_HAVE_SME2
    case KLEIDICV_BACKEND_SME2:
      return kleidicv::sme2::gray_to_rgb_u8(src, src_stride, dst, dst_stride,
                                            width, height);
#endif
    default:
      return KLEIDICV_ERROR_NOT_IMPLEMENTED;
  }
}
