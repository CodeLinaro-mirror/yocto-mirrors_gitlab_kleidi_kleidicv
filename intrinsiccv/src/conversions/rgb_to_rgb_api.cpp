// SPDX-FileCopyrightText: 2023 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#include "conversions/rgb_to_rgb.h"
#include "dispatch.h"
#include "intrinsiccv.h"
#include "types.h"

namespace intrinsiccv {

#define INTRINSICCV_DEFINE_C_API(name)                                         \
  static IFuncImpls name##_impls_builder(void) {                               \
    IFuncImpls impls;                                                          \
    INTRINSICCV_ADD_NEON_IMPL(intrinsiccv::neon::name);                        \
    INTRINSICCV_ADD_SVE2_IMPL_IF(intrinsiccv::sve2::name);                     \
    INTRINSICCV_ADD_SME2_IMPL(intrinsiccv::sme2::name);                        \
    return impls;                                                              \
  }                                                                            \
  INTRINSICCV_MULTIVERSION_C_API(                                              \
      name, name##_impls_builder, void, const uint8_t *src, size_t src_stride, \
      uint8_t *dst, size_t dst_stride, size_t width, size_t height)

INTRINSICCV_DEFINE_C_API(rgb_to_bgr_u8);
INTRINSICCV_DEFINE_C_API(rgba_to_bgra_u8);
INTRINSICCV_DEFINE_C_API(rgb_to_bgra_u8);
INTRINSICCV_DEFINE_C_API(rgb_to_rgba_u8);
INTRINSICCV_DEFINE_C_API(rgba_to_bgr_u8);
INTRINSICCV_DEFINE_C_API(rgba_to_rgb_u8);

extern "C" {

void INTRINSICCV_C_API(rgb_to_rgb_u8)(const uint8_t *src, size_t src_stride,
                                      uint8_t *dst, size_t dst_stride,
                                      size_t width, size_t height) {
  Rectangle rect{width, height};
  Rows<const uint8_t> src_rows{src, src_stride, 3 /* RGB */};
  Rows<uint8_t> dst_rows{dst, dst_stride, 3 /* BGR */};
  CopyRows<uint8_t>::copy_rows(rect, src_rows, dst_rows);
}

void INTRINSICCV_C_API(rgba_to_rgba_u8)(const uint8_t *src, size_t src_stride,
                                        uint8_t *dst, size_t dst_stride,
                                        size_t width, size_t height) {
  Rectangle rect{width, height};
  Rows<const uint8_t> src_rows{src, src_stride, 4 /* RGBA */};
  Rows<uint8_t> dst_rows{dst, dst_stride, 4 /* RGBA */};
  CopyRows<uint8_t>::copy_rows(rect, src_rows, dst_rows);
}

}  // extern "C"

}  // namespace intrinsiccv
