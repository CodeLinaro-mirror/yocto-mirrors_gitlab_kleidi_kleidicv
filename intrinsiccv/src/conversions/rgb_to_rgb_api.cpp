// SPDX-FileCopyrightText: 2023 - 2024 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#include "intrinsiccv/conversions/rgb_to_rgb.h"
#include "intrinsiccv/dispatch.h"
#include "intrinsiccv/intrinsiccv.h"
#include "intrinsiccv/types.h"

namespace intrinsiccv {

#define INTRINSICCV_DEFINE_C_API(name, partialname)             \
  INTRINSICCV_MULTIVERSION_C_API(                               \
      name, intrinsiccv::neon::partialname,                     \
      INTRINSICCV_SVE2_IMPL_IF(intrinsiccv::sve2::partialname), \
      intrinsiccv::sme2::partialname)

INTRINSICCV_DEFINE_C_API(intrinsiccv_rgb_to_bgr_u8, rgb_to_bgr_u8);
INTRINSICCV_DEFINE_C_API(intrinsiccv_rgba_to_bgra_u8, rgba_to_bgra_u8);
INTRINSICCV_DEFINE_C_API(intrinsiccv_rgb_to_bgra_u8, rgb_to_bgra_u8);
INTRINSICCV_DEFINE_C_API(intrinsiccv_rgb_to_rgba_u8, rgb_to_rgba_u8);
INTRINSICCV_DEFINE_C_API(intrinsiccv_rgba_to_bgr_u8, rgba_to_bgr_u8);
INTRINSICCV_DEFINE_C_API(intrinsiccv_rgba_to_rgb_u8, rgba_to_rgb_u8);

extern "C" {

using INTRINSICCV_TARGET_NAMESPACE::CopyRows;
using INTRINSICCV_TARGET_NAMESPACE::Rectangle;
using INTRINSICCV_TARGET_NAMESPACE::Rows;

static intrinsiccv_error_t intrinsiccv_rgb_to_rgb_u8_impl(
    const uint8_t *src, size_t src_stride, uint8_t *dst, size_t dst_stride,
    size_t width, size_t height) {
  CHECK_POINTER_AND_STRIDE(src, src_stride);
  CHECK_POINTER_AND_STRIDE(dst, dst_stride);
  CHECK_IMAGE_SIZE(width, height);

  Rectangle rect{width, height};
  Rows<const uint8_t> src_rows{src, src_stride, 3 /* RGB */};
  Rows<uint8_t> dst_rows{dst, dst_stride, 3 /* BGR */};
  CopyRows<uint8_t>::copy_rows(rect, src_rows, dst_rows);
  return INTRINSICCV_OK;
}

decltype(intrinsiccv_rgb_to_rgb_u8_impl) *intrinsiccv_rgb_to_rgb_u8 =
    intrinsiccv_rgb_to_rgb_u8_impl;

static intrinsiccv_error_t intrinsiccv_rgba_to_rgba_u8_impl(
    const uint8_t *src, size_t src_stride, uint8_t *dst, size_t dst_stride,
    size_t width, size_t height) {
  CHECK_POINTER_AND_STRIDE(src, src_stride);
  CHECK_POINTER_AND_STRIDE(dst, dst_stride);
  CHECK_IMAGE_SIZE(width, height);

  Rectangle rect{width, height};
  Rows<const uint8_t> src_rows{src, src_stride, 4 /* RGBA */};
  Rows<uint8_t> dst_rows{dst, dst_stride, 4 /* RGBA */};
  CopyRows<uint8_t>::copy_rows(rect, src_rows, dst_rows);
  return INTRINSICCV_OK;
}

decltype(intrinsiccv_rgba_to_rgba_u8_impl) *intrinsiccv_rgba_to_rgba_u8 =
    intrinsiccv_rgba_to_rgba_u8_impl;

}  // extern "C"

}  // namespace intrinsiccv
