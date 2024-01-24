// SPDX-FileCopyrightText: 2023 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#include "rgb_to_rgb_sc.h"

namespace intrinsiccv::sme2 {

INTRINSICCV_LOCALLY_STREAMING INTRINSICCV_TARGET_FN_ATTRS void rgb_to_bgr_u8(
    const uint8_t *src, size_t src_stride, uint8_t *dst, size_t dst_stride,
    size_t width, size_t height) {
  intrinsiccv::sve2::rgb_to_bgr_u8_sc(src, src_stride, dst, dst_stride, width,
                                      height);
}

INTRINSICCV_LOCALLY_STREAMING INTRINSICCV_TARGET_FN_ATTRS void rgba_to_bgra_u8(
    const uint8_t *src, size_t src_stride, uint8_t *dst, size_t dst_stride,
    size_t width, size_t height) {
  intrinsiccv::sve2::rgba_to_bgra_u8_sc(src, src_stride, dst, dst_stride, width,
                                        height);
}

INTRINSICCV_LOCALLY_STREAMING INTRINSICCV_TARGET_FN_ATTRS void rgb_to_bgra_u8(
    const uint8_t *src, size_t src_stride, uint8_t *dst, size_t dst_stride,
    size_t width, size_t height) {
  intrinsiccv::sve2::rgb_to_bgra_u8_sc(src, src_stride, dst, dst_stride, width,
                                       height);
}

INTRINSICCV_LOCALLY_STREAMING INTRINSICCV_TARGET_FN_ATTRS void rgb_to_rgba_u8(
    const uint8_t *src, size_t src_stride, uint8_t *dst, size_t dst_stride,
    size_t width, size_t height) {
  intrinsiccv::sve2::rgb_to_rgba_u8_sc(src, src_stride, dst, dst_stride, width,
                                       height);
}

INTRINSICCV_LOCALLY_STREAMING INTRINSICCV_TARGET_FN_ATTRS void rgba_to_bgr_u8(
    const uint8_t *src, size_t src_stride, uint8_t *dst, size_t dst_stride,
    size_t width, size_t height) {
  intrinsiccv::sve2::rgba_to_bgr_u8_sc(src, src_stride, dst, dst_stride, width,
                                       height);
}

INTRINSICCV_LOCALLY_STREAMING INTRINSICCV_TARGET_FN_ATTRS void rgba_to_rgb_u8(
    const uint8_t *src, size_t src_stride, uint8_t *dst, size_t dst_stride,
    size_t width, size_t height) {
  intrinsiccv::sve2::rgba_to_rgb_u8_sc(src, src_stride, dst, dst_stride, width,
                                       height);
}

}  // namespace intrinsiccv::sme2
