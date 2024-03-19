// SPDX-FileCopyrightText: 2024 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#include "intrinsiccv/intrinsiccv.h"
#include "intrinsiccv/neon.h"
#include "intrinsiccv/resize/resize_linear.h"

namespace intrinsiccv::neon {

INTRINSICCV_TARGET_FN_ATTRS static intrinsiccv_error_t resize_2x2_u8(
    const uint8_t *src, size_t src_stride, size_t src_width, size_t src_height,
    uint8_t *dst, size_t dst_stride) {
  if (src_width == 0 || src_height == 0) {
    return INTRINSICCV_OK;
  }

  size_t dst_width = src_width * 2;

  auto lerp1d_scalar = [](uint8_t near, uint8_t far) {
    return (near * 3 + far + 2) >> 2;
  };

  auto lerp1d_vector = [](uint8x8_t near, uint8x8_t far) {
    uint8x8_t three = vdup_n_u8(3);
    uint8x8_t two = vdup_n_u8(2);

    // near * 3
    uint16x8_t near3 = vmull_u8(near, three);

    // far + 2
    uint16x8_t far_2 = vaddl_u8(far, two);

    // near * 3 + far * 2
    uint16x8_t near3_far_2 = vaddq_u16(near3, far_2);

    // (near * 3 + far * 2) / 4
    uint8x8_t near3_far_2_div4 = vshrn_n_u16(near3_far_2, 2);

    return near3_far_2_div4;
  };

  auto lerp2d_scalar = [](uint8_t near, uint8_t mid_a, uint8_t mid_b,
                          uint8_t far) {
    return (near * 9 + (mid_a + mid_b) * 3 + far + 8) >> 4;
  };

  auto lerp2d_vector = [](uint8x8_t near, uint8x8_t mid_a, uint8x8_t mid_b,
                          uint8x8_t far) {
    uint8x8_t nine = vdup_n_u8(9);
    uint16x8_t three = vdupq_n_u16(3);
    uint8x8_t eight = vdup_n_u8(8);

    // mid_a + mid_b
    uint16x8_t mid = vaddl_u8(mid_a, mid_b);

    // near * 9
    uint16x8_t near9 = vmull_u8(near, nine);

    // near * 9 + (mid_a + mid_b) * 3
    uint16x8_t near9_mid3 = vmlaq_u16(near9, mid, three);

    // far + 8
    uint16x8_t far_8 = vaddl_u8(far, eight);

    // near * 9 + (mid_a + mid_b) * 3 + far + 8
    uint16x8_t near9_mid3_far_8 = vaddq_u16(near9_mid3, far_8);

    // (near * 9 + (mid_a + mid_b) * 3 + far + 8) / 16
    uint8x8_t near9_mid3_far_8_div16 = vshrn_n_u16(near9_mid3_far_8, 4);
    return near9_mid3_far_8_div16;
  };

  // Handle top or bottom edge
  auto process_edge_row = [src_width, dst_width, lerp1d_scalar, lerp1d_vector](
                              const uint8_t *src_row, uint8_t *dst_row) {
    // Left element
    dst_row[0] = src_row[0];

    // Right element
    dst_row[dst_width - 1] = src_row[src_width - 1];

    // Middle elements
    size_t src_x = 0;
    for (; src_x + sizeof(uint8x8_t) < src_width; src_x += sizeof(uint8x8_t)) {
      size_t dst_x = src_x * 2 + 1;
      uint8x8_t src_left = vld1_u8(src_row + src_x);
      uint8x8_t src_right = vld1_u8(src_row + src_x + 1);

      uint8x8_t dst_left = lerp1d_vector(src_left, src_right);
      uint8x8_t dst_right = lerp1d_vector(src_right, src_left);

      vst2_u8(dst_row + dst_x, (uint8x8x2_t{dst_left, dst_right}));
    }
    for (; src_x + 1 < src_width; ++src_x) {
      size_t dst_x = src_x * 2 + 1;
      const uint8_t src_left = src_row[src_x], src_right = src_row[src_x + 1];
      dst_row[dst_x] = lerp1d_scalar(src_left, src_right);
      dst_row[dst_x + 1] = lerp1d_scalar(src_right, src_left);
    }
  };

  auto process_row = [src_width, dst_width, lerp1d_scalar, lerp2d_scalar,
                      lerp2d_vector](const uint8_t *src_row0,
                                     const uint8_t *src_row1, uint8_t *dst_row0,
                                     uint8_t *dst_row1) {
    // Left element
    dst_row0[0] = lerp1d_scalar(src_row0[0], src_row1[0]);
    dst_row1[0] = lerp1d_scalar(src_row1[0], src_row0[0]);

    // Right element
    dst_row0[dst_width - 1] =
        lerp1d_scalar(src_row0[src_width - 1], src_row1[src_width - 1]);
    dst_row1[dst_width - 1] =
        lerp1d_scalar(src_row1[src_width - 1], src_row0[src_width - 1]);

    // Middle elements
    size_t src_x = 0;
    for (; src_x + sizeof(uint8x8_t) < src_width; src_x += sizeof(uint8x8_t)) {
      size_t dst_x = src_x * 2 + 1;

      uint8x8_t src_tl = vld1_u8(src_row0 + src_x);
      uint8x8_t src_tr = vld1_u8(src_row0 + src_x + 1);
      uint8x8_t src_bl = vld1_u8(src_row1 + src_x);
      uint8x8_t src_br = vld1_u8(src_row1 + src_x + 1);

      uint8x8_t dst_tl = lerp2d_vector(src_tl, src_tr, src_bl, src_br);
      uint8x8_t dst_tr = lerp2d_vector(src_tr, src_tl, src_br, src_bl);
      uint8x8_t dst_bl = lerp2d_vector(src_bl, src_tl, src_br, src_tr);
      uint8x8_t dst_br = lerp2d_vector(src_br, src_tr, src_bl, src_tl);

      vst2_u8(dst_row0 + dst_x, (uint8x8x2_t{dst_tl, dst_tr}));
      vst2_u8(dst_row1 + dst_x, (uint8x8x2_t{dst_bl, dst_br}));
    }
    for (; src_x + 1 < src_width; ++src_x) {
      size_t dst_x = src_x * 2 + 1;
      const uint8_t src_tl = src_row0[src_x], src_tr = src_row0[src_x + 1],
                    src_bl = src_row1[src_x], src_br = src_row1[src_x + 1];
      dst_row0[dst_x] = lerp2d_scalar(src_tl, src_tr, src_bl, src_br);
      dst_row0[dst_x + 1] = lerp2d_scalar(src_tr, src_tl, src_br, src_bl);
      dst_row1[dst_x] = lerp2d_scalar(src_bl, src_tl, src_br, src_tr);
      dst_row1[dst_x + 1] = lerp2d_scalar(src_br, src_tr, src_bl, src_tl);
    }
  };

  // Top row
  process_edge_row(src, dst);

  // Middle rows
  for (size_t src_y = 0; src_y + 1 < src_height; ++src_y) {
    size_t dst_y = src_y * 2 + 1;
    const uint8_t *src_row0 = src + src_stride * src_y;
    const uint8_t *src_row1 = src_row0 + src_stride;
    uint8_t *dst_row0 = dst + dst_stride * dst_y;
    uint8_t *dst_row1 = dst_row0 + dst_stride;

    process_row(src_row0, src_row1, dst_row0, dst_row1);
  }

  // Bottom row
  process_edge_row(src + src_stride * (src_height - 1),
                   dst + dst_stride * (src_height * 2 - 1));

  return INTRINSICCV_OK;
}

INTRINSICCV_TARGET_FN_ATTRS
intrinsiccv_error_t resize_linear_u8(const uint8_t *src, size_t src_stride,
                                     size_t src_width, size_t src_height,
                                     uint8_t *dst, size_t dst_stride,
                                     size_t dst_width, size_t dst_height) {
  CHECK_POINTER_AND_STRIDE(src, src_stride);
  CHECK_POINTER_AND_STRIDE(dst, dst_stride);
  CHECK_IMAGE_SIZE(dst_width, dst_height);

  if (src_width * 2 == dst_width && src_height * 2 == dst_height) {
    return resize_2x2_u8(src, src_stride, src_width, src_height, dst,
                         dst_stride);
  }
  return INTRINSICCV_ERROR_NOT_IMPLEMENTED;
}

}  // namespace intrinsiccv::neon
