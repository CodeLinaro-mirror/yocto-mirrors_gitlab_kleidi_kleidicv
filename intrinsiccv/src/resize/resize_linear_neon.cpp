// SPDX-FileCopyrightText: 2024 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#include "intrinsiccv/intrinsiccv.h"
#include "intrinsiccv/neon.h"
#include "intrinsiccv/resize/resize_linear.h"

namespace intrinsiccv::neon {

template <uint8_t P, uint8_t Q, uint8_t Bias, uint8_t Shift>
uint8x8_t lerp2d_vector_p_q_q_1(uint8x8_t a, uint8x8_t b, uint8x8_t c,
                                uint8x8_t d) {
  // b + c
  uint16x8_t b_c = vaddl_u8(b, c);

  // a * p
  uint16x8_t ap = vmull_u8(a, vdup_n_u8(P));

  // a * p + (b + c) * q
  uint16x8_t ap_bcq = vmlaq_u16(ap, b_c, vdupq_n_u16(Q));

  // d + bias
  uint16x8_t d_bias = vaddl_u8(d, vdup_n_u8(Bias));

  // a * p + (b + c) * q + d + bias
  uint16x8_t ap_bcq_d_bias = vaddq_u16(ap_bcq, d_bias);

  // (a * p + (b + c) * q + d + bias) >> shift
  uint8x8_t result = vshrn_n_u16(ap_bcq_d_bias, Shift);
  return result;
}

template <uint8_t P, uint8_t Q, uint8_t R, uint8_t Bias, uint8_t Shift>
uint8x8_t lerp2d_vector_p_q_q_r(uint8x8_t a, uint8x8_t b, uint8x8_t c,
                                uint8x8_t d) {
  // b + c
  uint16x8_t b_c = vaddl_u8(b, c);

  // a * p
  uint16x8_t ap = vmull_u8(a, vdup_n_u8(P));

  // d * r
  uint16x8_t dr = vmull_u8(d, vdup_n_u8(R));

  // a * p + (b + c) * q
  uint16x8_t ap_bcq = vmlaq_u16(ap, b_c, vdupq_n_u16(Q));

  // d * r + bias
  uint16x8_t dr_bias = vaddq_u16(dr, vdupq_n_u16(Bias));

  // a * p + (b + c) * q + d * r + bias
  uint16x8_t ap_bcq_dr_bias = vaddq_u16(ap_bcq, dr_bias);

  // (a * p + (b + c) * q + d * r + bias) >> shift
  uint8x8_t result = vshrn_n_u16(ap_bcq_dr_bias, Shift);
  return result;
}

KLEIDICV_TARGET_FN_ATTRS static intrinsiccv_error_t resize_2x2_u8(
    const uint8_t *src, size_t src_stride, size_t src_width, size_t src_height,
    uint8_t *dst, size_t dst_stride) {
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

  auto lerp2d_vector = [](uint8x8_t a, uint8x8_t b, uint8x8_t c, uint8x8_t d) {
    return lerp2d_vector_p_q_q_1<9, 3, 8, 4>(a, b, c, d);
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

  return KLEIDICV_OK;
}

KLEIDICV_TARGET_FN_ATTRS static intrinsiccv_error_t resize_4x4_u8(
    const uint8_t *src, size_t src_stride, size_t src_width, size_t src_height,
    uint8_t *dst, size_t dst_stride) {
  size_t dst_width = src_width * 4, dst_height = src_height * 4;

  auto lerp1d_scalar = [](uint8_t coeff_a, uint8_t a, uint8_t coeff_b,
                          uint8_t b) {
    return (coeff_a * a + coeff_b * b + 4) >> 3;
  };
  auto lerp1d_vector = [](uint8_t coeff_a_scalar, uint8x8_t a,
                          uint8_t coeff_b_scalar, uint8x8_t b) {
    uint8x8_t coeff_a = vdup_n_u8(coeff_a_scalar);
    uint8x8_t coeff_b = vdup_n_u8(coeff_b_scalar);
    uint16x8_t four = vdupq_n_u16(4);

    // a * coeff_a
    uint16x8_t a1 = vmull_u8(a, coeff_a);

    // b * coeff_b
    uint16x8_t b1 = vmull_u8(b, coeff_b);

    // a * coeff_a + b * coeff_b
    uint16x8_t a1_b1 = vaddq_u16(a1, b1);

    // a * coeff_a + b * coeff_b + 4
    uint16x8_t a1_b1_4 = vaddq_u16(a1_b1, four);

    // (a * coeff_a + b * coeff_b + 4) / 8
    uint8x8_t result = vshrn_n_u16(a1_b1_4, 3);

    return result;
  };
  auto lerp2d_scalar = [](uint8_t coeff_a, uint8_t a, uint8_t coeff_b,
                          uint8_t b, uint8_t coeff_c, uint8_t c,
                          uint8_t coeff_d, uint8_t d) {
    return (coeff_a * a + coeff_b * b + coeff_c * c + coeff_d * d + 32) >> 6;
  };
  auto lerp2d_vector = [](uint8_t coeff_a_scalar, uint8x8_t a,
                          uint8_t coeff_b_scalar, uint8x8_t b,
                          uint8_t coeff_c_scalar, uint8x8_t c,
                          uint8_t coeff_d_scalar, uint8x8_t d) {
    uint8x8_t coeff_a = vdup_n_u8(coeff_a_scalar);
    uint8x8_t coeff_b = vdup_n_u8(coeff_b_scalar);
    uint8x8_t coeff_c = vdup_n_u8(coeff_c_scalar);
    uint8x8_t coeff_d = vdup_n_u8(coeff_d_scalar);
    uint16x8_t thirtytwo = vdupq_n_u16(32);

    // a * coeff_a
    uint16x8_t a1 = vmull_u8(a, coeff_a);

    // b * coeff_b
    uint16x8_t b1 = vmull_u8(b, coeff_b);

    // c * coeff_c
    uint16x8_t c1 = vmull_u8(c, coeff_c);

    // d * coeff_d
    uint16x8_t d1 = vmull_u8(d, coeff_d);

    // a * coeff_a + b * coeff_b
    uint16x8_t a1_b1 = vaddq_u16(a1, b1);

    // c * coeff_c + d * coeff_d
    uint16x8_t c1_d1 = vaddq_u16(c1, d1);

    // a * coeff_a + b * coeff_b + c * coeff_c + d * coeff_d
    uint16x8_t a1_b1_c1_d1 = vaddq_u16(a1_b1, c1_d1);

    // a * coeff_a + b * coeff_b + c * coeff_c + d * coeff_d + 32
    uint16x8_t a1_b1_c1_d1_32 = vaddq_u16(a1_b1_c1_d1, thirtytwo);

    // (a * coeff_a + b * coeff_b + c * coeff_c + d * coeff_d + 32) / 64
    uint8x8_t result = vshrn_n_u16(a1_b1_c1_d1_32, 6);
    return result;
  };
  // Handle top or bottom edge
  auto process_edge_row = [src_width, dst_width, lerp1d_scalar, lerp1d_vector](
                              const uint8_t *src_row, uint8_t *dst_row) {
    // Left elements
    dst_row[1] = dst_row[0] = src_row[0];

    // Right elements
    dst_row[dst_width - 1] = dst_row[dst_width - 2] = src_row[src_width - 1];

    // Middle elements
    size_t src_x = 0;
    for (; src_x + sizeof(uint8x8_t) < src_width; src_x += sizeof(uint8x8_t)) {
      size_t dst_x = src_x * 4 + 2;
      uint8x8_t a = vld1_u8(src_row + src_x);
      uint8x8_t b = vld1_u8(src_row + src_x + 1);
      uint8x8x4_t interpolated = {
          lerp1d_vector(7, a, 1, b), lerp1d_vector(5, a, 3, b),
          lerp1d_vector(3, a, 5, b), lerp1d_vector(1, a, 7, b)};

      vst4_u8(dst_row + dst_x, interpolated);
    }
    for (; src_x + 1 < src_width; ++src_x) {
      size_t dst_x = src_x * 4 + 2;
      const uint8_t a = src_row[src_x], b = src_row[src_x + 1];
      dst_row[dst_x + 0] = lerp1d_scalar(7, a, 1, b);
      dst_row[dst_x + 1] = lerp1d_scalar(5, a, 3, b);
      dst_row[dst_x + 2] = lerp1d_scalar(3, a, 5, b);
      dst_row[dst_x + 3] = lerp1d_scalar(1, a, 7, b);
    }
  };

  auto process_row = [src_width, dst_width, lerp1d_scalar, lerp2d_scalar,
                      lerp2d_vector](const uint8_t *src_row0,
                                     const uint8_t *src_row1, uint8_t *dst_row0,
                                     uint8_t *dst_row1, uint8_t *dst_row2,
                                     uint8_t *dst_row3) {
    auto lerp2d_vector_49_7_7_1 = [](uint8x8_t a, uint8x8_t b, uint8x8_t c,
                                     uint8x8_t d) {
      return lerp2d_vector_p_q_q_1<49, 7, 32, 6>(a, b, c, d);
    };
    auto lerp2d_vector_25_15_15_9 = [](uint8x8_t a, uint8x8_t b, uint8x8_t c,
                                       uint8x8_t d) {
      return lerp2d_vector_p_q_q_r<25, 15, 9, 32, 6>(a, b, c, d);
    };

    // Left elements
    const uint8_t s0l = src_row0[0], s1l = src_row1[0];
    dst_row0[0] = dst_row0[1] = lerp1d_scalar(7, s0l, 1, s1l);
    dst_row1[0] = dst_row1[1] = lerp1d_scalar(5, s0l, 3, s1l);
    dst_row2[0] = dst_row2[1] = lerp1d_scalar(3, s0l, 5, s1l);
    dst_row3[0] = dst_row3[1] = lerp1d_scalar(1, s0l, 7, s1l);

    // Right elements
    const size_t s0r = src_row0[src_width - 1], s1r = src_row1[src_width - 1];
    const size_t dr0 = dst_width - 2;
    const size_t dr1 = dst_width - 1;
    dst_row0[dr0] = dst_row0[dr1] = lerp1d_scalar(7, s0r, 1, s1r);
    dst_row1[dr0] = dst_row1[dr1] = lerp1d_scalar(5, s0r, 3, s1r);
    dst_row2[dr0] = dst_row2[dr1] = lerp1d_scalar(3, s0r, 5, s1r);
    dst_row3[dr0] = dst_row3[dr1] = lerp1d_scalar(1, s0r, 7, s1r);

    // Middle elements
    size_t src_x = 0;
    for (; src_x + sizeof(uint8x8_t) < src_width; src_x += sizeof(uint8x8_t)) {
      size_t dst_x = src_x * 4 + 2;

      uint8x8_t a = vld1_u8(src_row0 + src_x);
      uint8x8_t b = vld1_u8(src_row0 + src_x + 1);
      uint8x8_t c = vld1_u8(src_row1 + src_x);
      uint8x8_t d = vld1_u8(src_row1 + src_x + 1);

      vst4_u8(dst_row0 + dst_x, (uint8x8x4_t{
                                    lerp2d_vector_49_7_7_1(a, b, c, d),
                                    lerp2d_vector(35, a, 21, b, 5, c, 3, d),
                                    lerp2d_vector(21, a, 35, b, 3, c, 5, d),
                                    lerp2d_vector_49_7_7_1(b, a, d, c),
                                }));
      vst4_u8(dst_row1 + dst_x, (uint8x8x4_t{
                                    lerp2d_vector(35, a, 5, b, 21, c, 3, d),
                                    lerp2d_vector_25_15_15_9(a, b, c, d),
                                    lerp2d_vector_25_15_15_9(b, a, d, c),
                                    lerp2d_vector(5, a, 35, b, 3, c, 21, d),
                                }));
      vst4_u8(dst_row2 + dst_x, (uint8x8x4_t{
                                    lerp2d_vector(21, a, 3, b, 35, c, 5, d),
                                    lerp2d_vector_25_15_15_9(c, a, d, b),
                                    lerp2d_vector_25_15_15_9(d, b, c, a),
                                    lerp2d_vector(3, a, 21, b, 5, c, 35, d),
                                }));
      vst4_u8(dst_row3 + dst_x, (uint8x8x4_t{
                                    lerp2d_vector_49_7_7_1(c, a, d, b),
                                    lerp2d_vector(5, a, 3, b, 35, c, 21, d),
                                    lerp2d_vector(3, a, 5, b, 21, c, 35, d),
                                    lerp2d_vector_49_7_7_1(d, b, c, a),
                                }));
    }
    for (; src_x + 1 < src_width; ++src_x) {
      size_t dst_x = src_x * 4 + 2;
      const uint8_t a = src_row0[src_x], b = src_row0[src_x + 1],
                    c = src_row1[src_x], d = src_row1[src_x + 1];

      dst_row0[dst_x + 0] = lerp2d_scalar(49, a, 7, b, 7, c, 1, d);
      dst_row0[dst_x + 1] = lerp2d_scalar(35, a, 21, b, 5, c, 3, d);
      dst_row0[dst_x + 2] = lerp2d_scalar(21, a, 35, b, 3, c, 5, d);
      dst_row0[dst_x + 3] = lerp2d_scalar(7, a, 49, b, 1, c, 7, d);
      dst_row1[dst_x + 0] = lerp2d_scalar(35, a, 5, b, 21, c, 3, d);
      dst_row1[dst_x + 1] = lerp2d_scalar(25, a, 15, b, 15, c, 9, d);
      dst_row1[dst_x + 2] = lerp2d_scalar(15, a, 25, b, 9, c, 15, d);
      dst_row1[dst_x + 3] = lerp2d_scalar(5, a, 35, b, 3, c, 21, d);
      dst_row2[dst_x + 0] = lerp2d_scalar(21, a, 3, b, 35, c, 5, d);
      dst_row2[dst_x + 1] = lerp2d_scalar(15, a, 9, b, 25, c, 15, d);
      dst_row2[dst_x + 2] = lerp2d_scalar(9, a, 15, b, 15, c, 25, d);
      dst_row2[dst_x + 3] = lerp2d_scalar(3, a, 21, b, 5, c, 35, d);
      dst_row3[dst_x + 0] = lerp2d_scalar(7, a, 1, b, 49, c, 7, d);
      dst_row3[dst_x + 1] = lerp2d_scalar(5, a, 3, b, 35, c, 21, d);
      dst_row3[dst_x + 2] = lerp2d_scalar(3, a, 5, b, 21, c, 35, d);
      dst_row3[dst_x + 3] = lerp2d_scalar(1, a, 7, b, 7, c, 49, d);
    }
  };

  // Top rows
  process_edge_row(src, dst);
  memcpy(dst + dst_stride, dst, dst_stride);

  // Middle rows
  for (size_t src_y = 0; src_y + 1 < src_height; ++src_y) {
    size_t dst_y = src_y * 4 + 2;
    const uint8_t *src_row0 = src + src_stride * src_y;
    const uint8_t *src_row1 = src_row0 + src_stride;
    uint8_t *dst_row0 = dst + dst_stride * dst_y;
    uint8_t *dst_row1 = dst_row0 + dst_stride;
    uint8_t *dst_row2 = dst_row1 + dst_stride;
    uint8_t *dst_row3 = dst_row2 + dst_stride;

    process_row(src_row0, src_row1, dst_row0, dst_row1, dst_row2, dst_row3);
  }

  // Bottom rows
  process_edge_row(src + src_stride * (src_height - 1),
                   dst + dst_stride * (dst_height - 2));
  memcpy(dst + dst_stride * (dst_height - 1),
         dst + dst_stride * (dst_height - 2), dst_stride);

  return KLEIDICV_OK;
}

KLEIDICV_TARGET_FN_ATTRS
intrinsiccv_error_t resize_linear_u8(const uint8_t *src, size_t src_stride,
                                     size_t src_width, size_t src_height,
                                     uint8_t *dst, size_t dst_stride,
                                     size_t dst_width, size_t dst_height) {
  CHECK_POINTER_AND_STRIDE(src, src_stride);
  CHECK_POINTER_AND_STRIDE(dst, dst_stride);
  CHECK_IMAGE_SIZE(dst_width, dst_height);

  if (src_width == 0 || src_height == 0) {
    return KLEIDICV_OK;
  }
  if (src_width * 2 == dst_width && src_height * 2 == dst_height) {
    return resize_2x2_u8(src, src_stride, src_width, src_height, dst,
                         dst_stride);
  }
  if (src_width * 4 == dst_width && src_height * 4 == dst_height) {
    return resize_4x4_u8(src, src_stride, src_width, src_height, dst,
                         dst_stride);
  }
  return KLEIDICV_ERROR_NOT_IMPLEMENTED;
}

}  // namespace intrinsiccv::neon
