// SPDX-FileCopyrightText: 2024 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#ifndef KLEIDICV_RESIZE_LINEAR_SC_H
#define KLEIDICV_RESIZE_LINEAR_SC_H

#include <arm_sve.h>

#include "kleidicv/kleidicv.h"
#include "kleidicv/sve2.h"

namespace KLEIDICV_TARGET_NAMESPACE {

KLEIDICV_TARGET_FN_ATTRS static kleidicv_error_t resize_2x2_u8_sc(
    const uint8_t *src, size_t src_stride, size_t src_width, size_t src_height,
    uint8_t *dst, size_t dst_stride) KLEIDICV_STREAMING_COMPATIBLE {
  size_t dst_width = src_width * 2;
  size_t dst_height = src_height * 2;

  auto lerp1d_scalar =
      [](uint8_t near, uint8_t far)
          KLEIDICV_STREAMING_COMPATIBLE { return (near * 3 + far + 2) >> 2; };

  auto lerp1d_vector = [](svuint8_t near,
                          svuint8_t far) KLEIDICV_STREAMING_COMPATIBLE {
    // near * 3
    svuint16_t near3b = svmullb(near, uint8_t{3});
    svuint16_t near3t = svmullt(near, uint8_t{3});

    // near * 3 + far
    svuint16_t near3_far_b = svaddwb(near3b, far);
    svuint16_t near3_far_t = svaddwt(near3t, far);

    // near * 3 + far + 2
    svuint16_t near3_far_2b = svaddwb(near3_far_b, uint8_t{2});
    svuint16_t near3_far_2t = svaddwt(near3_far_t, uint8_t{2});

    // (near * 3 + far + 2) / 4
    svuint8_t near3_far_2_div4 = svshrnb_n_u16(near3_far_2b, 2);
    near3_far_2_div4 = svshrnt_n_u16(near3_far_2_div4, near3_far_2t, 2);
    return near3_far_2_div4;
  };

  auto lerp2d_vector = [](svbool_t pg, svuint8_t near, svuint8_t mid_a,
                          svuint8_t mid_b,
                          svuint8_t far) KLEIDICV_STREAMING_COMPATIBLE {
    // near * 9
    svuint16_t near9b = svmullb(near, uint8_t{9});
    svuint16_t near9t = svmullt(near, uint8_t{9});

    // mid_a + mid_b
    svuint16_t midb = svaddlb(mid_a, mid_b);
    svuint16_t midt = svaddlt(mid_a, mid_b);

    // near * 9 + (mid_a + mid_b) * 3
    svuint16_t near9_mid3b = svmla_x(pg, near9b, midb, uint16_t{3});
    svuint16_t near9_mid3t = svmla_x(pg, near9t, midt, uint16_t{3});

    // near * 9 + (mid_a + mid_b) * 3 + far
    svuint16_t near9_mid3_far_b = svaddwb(near9_mid3b, far);
    svuint16_t near9_mid3_far_t = svaddwt(near9_mid3t, far);

    // near * 9 + (mid_a + mid_b) * 3 + far + 8
    svuint16_t near9_mid3_far_8b = svaddwb(near9_mid3_far_b, uint8_t{8});
    svuint16_t near9_mid3_far_8t = svaddwt(near9_mid3_far_t, uint8_t{8});

    // (near * 9 + (mid_a + mid_b) * 3 + far + 8) / 16
    svuint8_t near9_mid3_far_8_div16 = svshrnb_n_u16(near9_mid3_far_8b, 4);
    near9_mid3_far_8_div16 =
        svshrnt_n_u16(near9_mid3_far_8_div16, near9_mid3_far_8t, 4);
    return near9_mid3_far_8_div16;
  };

  // Work-around for clang-format oddness.
#define KSC KLEIDICV_STREAMING_COMPATIBLE

  // Handle top or bottom edge
  auto process_edge_row = [src_width, lerp1d_vector](const uint8_t *src_row,
                                                     uint8_t *dst_row) KSC {
    for (size_t src_x = 0; src_x + 1 < src_width; src_x += svcntb()) {
      size_t dst_x = src_x * 2 + 1;

      svbool_t pg = svwhilelt_b8(src_x + 1, src_width);

      svuint8_t src_left = svld1_u8(pg, src_row + src_x);
      svuint8_t src_right = svld1_u8(pg, src_row + src_x + 1);

      svuint8_t dst_left = lerp1d_vector(src_left, src_right);
      svuint8_t dst_right = lerp1d_vector(src_right, src_left);

      svst2_u8(pg, dst_row + dst_x, svcreate2(dst_left, dst_right));
    }
  };

  auto process_row = [src_width, lerp2d_vector](
                         const uint8_t *src_row0, const uint8_t *src_row1,
                         uint8_t *dst_row0,
                         uint8_t *dst_row1) KLEIDICV_STREAMING_COMPATIBLE {
    // Middle elements
    for (size_t src_x = 0; src_x + 1 < src_width; src_x += svcntb()) {
      size_t dst_x = src_x * 2 + 1;

      svbool_t pg = svwhilelt_b8(src_x + 1, src_width);

      svuint8_t src_tl = svld1_u8(pg, src_row0 + src_x);
      svuint8_t src_tr = svld1_u8(pg, src_row0 + src_x + 1);
      svuint8_t src_bl = svld1_u8(pg, src_row1 + src_x);
      svuint8_t src_br = svld1_u8(pg, src_row1 + src_x + 1);

      svuint8_t dst_tl = lerp2d_vector(pg, src_tl, src_tr, src_bl, src_br);
      svuint8_t dst_tr = lerp2d_vector(pg, src_tr, src_tl, src_br, src_bl);
      svuint8_t dst_bl = lerp2d_vector(pg, src_bl, src_tl, src_br, src_tr);
      svuint8_t dst_br = lerp2d_vector(pg, src_br, src_tr, src_bl, src_tl);

      svst2_u8(pg, dst_row0 + dst_x, svcreate2(dst_tl, dst_tr));
      svst2_u8(pg, dst_row1 + dst_x, svcreate2(dst_bl, dst_br));
    }
  };

  // Corners
  dst[0] = src[0];
  dst[dst_width - 1] = src[src_width - 1];
  dst[dst_stride * (dst_height - 1)] = src[src_stride * (src_height - 1)];
  dst[dst_stride * (dst_height - 1) + dst_width - 1] =
      src[src_stride * (src_height - 1) + src_width - 1];

  // Left & right edge
  for (size_t src_y = 0; src_y + 1 < src_height; ++src_y) {
    size_t dst_y = src_y * 2 + 1;
    const uint8_t *src_row0 = src + src_stride * src_y;
    const uint8_t *src_row1 = src_row0 + src_stride;
    uint8_t *dst_row0 = dst + dst_stride * dst_y;
    uint8_t *dst_row1 = dst_row0 + dst_stride;

    // Left edge
    dst_row0[0] = lerp1d_scalar(src_row0[0], src_row1[0]);
    dst_row1[0] = lerp1d_scalar(src_row1[0], src_row0[0]);

    // Right edge
    dst_row0[dst_width - 1] =
        lerp1d_scalar(src_row0[src_width - 1], src_row1[src_width - 1]);
    dst_row1[dst_width - 1] =
        lerp1d_scalar(src_row1[src_width - 1], src_row0[src_width - 1]);
  }

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
                   dst + dst_stride * (dst_height - 1));

  return KLEIDICV_OK;
}

KLEIDICV_TARGET_FN_ATTRS static kleidicv_error_t resize_4x4_u8_sc(
    const uint8_t *src, size_t src_stride, size_t src_width, size_t src_height,
    uint8_t *dst, size_t dst_stride) KLEIDICV_STREAMING_COMPATIBLE {
  size_t dst_width = src_width * 4;
  size_t dst_height = src_height * 4;

  auto lerp1d_scalar =
      [](uint8_t p, uint8_t a, uint8_t q, uint8_t b)
          KLEIDICV_STREAMING_COMPATIBLE { return (p * a + q * b + 4) >> 3; };

  auto lerp1d_vector = [](uint8_t p, svuint8_t a, uint8_t q, svuint8_t b) KSC {
    // bias
    svuint16_t top = svdup_u16(4);

    // bias + a * p
    svuint16_t bot = svmlalb(top, a, p);
    top = svmlalt(top, a, p);

    // bias + a * p + b * q
    bot = svmlalb(bot, b, q);
    top = svmlalt(top, b, q);

    // (bias + a * p + b * q) / 8
    svuint8_t result = svshrnb(bot, 3ULL);
    result = svshrnt(result, top, 3ULL);
    return result;
  };

  auto lerp2d_vector = [](uint8_t p, svuint8_t a, uint8_t q, svuint8_t b,
                          uint8_t r, svuint8_t c, uint8_t s, svuint8_t d) KSC {
    // bias
    svuint16_t top = svdup_u16(32);

    // bias + a * p
    svuint16_t bot = svmlalb(top, a, p);
    top = svmlalt(top, a, p);

    // bias + a * p + b * q
    bot = svmlalb(bot, b, q);
    top = svmlalt(top, b, q);

    // bias + a * p + b * q + c * r
    bot = svmlalb(bot, c, r);
    top = svmlalt(top, c, r);

    // bias + a * p + b * q + c * r + d * s
    bot = svmlalb(bot, d, s);
    top = svmlalt(top, d, s);

    // (bias + a * p + b * q + c * r + d * s) / 64
    svuint8_t result = svshrnt(svshrnb(bot, 6ULL), top, 6ULL);
    return result;
  };

  // Handle top or bottom edge
  auto process_edge_row = [src_width, lerp1d_vector](const uint8_t *src_row,
                                                     uint8_t *dst_row) KSC {
    for (size_t src_x = 0; src_x + 1 < src_width; src_x += svcntb()) {
      size_t dst_x = src_x * 4 + 2;
      svbool_t pg = svwhilelt_b8(src_x + 1, src_width);
      svuint8_t a = svld1_u8(pg, src_row + src_x);
      svuint8_t b = svld1_u8(pg, src_row + src_x + 1);
      svst4_u8(pg, dst_row + dst_x,
               svcreate4(lerp1d_vector(7, a, 1, b), lerp1d_vector(5, a, 3, b),
                         lerp1d_vector(3, a, 5, b), lerp1d_vector(1, a, 7, b)));
    }
  };

  auto process_row = [src_width, lerp2d_vector](
                         const uint8_t *src_row0, const uint8_t *src_row1,
                         uint8_t *dst_row0, uint8_t *dst_row1,
                         uint8_t *dst_row2,
                         uint8_t *dst_row3) KLEIDICV_STREAMING_COMPATIBLE {
    // Middle elements
    for (size_t src_x = 0; src_x + 1 < src_width; src_x += svcntb()) {
      size_t dst_x = src_x * 4 + 2;

      svbool_t pg = svwhilelt_b8(src_x + 1, src_width);

      svuint8_t a = svld1_u8(pg, src_row0 + src_x);
      svuint8_t b = svld1_u8(pg, src_row0 + src_x + 1);
      svuint8_t c = svld1_u8(pg, src_row1 + src_x);
      svuint8_t d = svld1_u8(pg, src_row1 + src_x + 1);

      svst4_u8(pg, dst_row0 + dst_x,
               (svcreate4(lerp2d_vector(49, a, 7, b, 7, c, 1, d),
                          lerp2d_vector(35, a, 21, b, 5, c, 3, d),
                          lerp2d_vector(21, a, 35, b, 3, c, 5, d),
                          lerp2d_vector(49, b, 7, a, 7, d, 1, c))));

      svst4_u8(pg, dst_row1 + dst_x,
               (svcreate4(lerp2d_vector(35, a, 5, b, 21, c, 3, d),
                          lerp2d_vector(25, a, 15, b, 15, c, 9, d),
                          lerp2d_vector(15, a, 25, b, 9, c, 15, d),
                          lerp2d_vector(5, a, 35, b, 3, c, 21, d))));
      svst4_u8(pg, dst_row2 + dst_x,
               (svcreate4(lerp2d_vector(21, a, 3, b, 35, c, 5, d),
                          lerp2d_vector(15, a, 9, b, 25, c, 15, d),
                          lerp2d_vector(9, a, 15, b, 15, c, 25, d),
                          lerp2d_vector(3, a, 21, b, 5, c, 35, d))));
      svst4_u8(pg, dst_row3 + dst_x,
               (svcreate4(lerp2d_vector(49, c, 7, a, 7, d, 1, b),
                          lerp2d_vector(5, a, 3, b, 35, c, 21, d),
                          lerp2d_vector(3, a, 5, b, 21, c, 35, d),
                          lerp2d_vector(49, d, 7, b, 7, c, 1, a))));
    }
  };

  // Corners
  auto set_corner = [dst, dst_stride](size_t left_column, size_t top_row,
                                      uint8_t value) KSC {
    dst[dst_stride * top_row + left_column] = value;
    dst[dst_stride * top_row + left_column + 1] = value;
    dst[dst_stride * (top_row + 1) + left_column] = value;
    dst[dst_stride * (top_row + 1) + left_column + 1] = value;
  };
  set_corner(0, 0, src[0]);
  set_corner(dst_width - 2, 0, src[src_width - 1]);
  set_corner(0, dst_height - 2, src[src_stride * (src_height - 1)]);
  set_corner(dst_width - 2, dst_height - 2,
             src[src_stride * (src_height - 1) + src_width - 1]);

  // Left & right edge
  for (size_t src_y = 0; src_y + 1 < src_height; ++src_y) {
    size_t dst_y = src_y * 4 + 2;
    const uint8_t *src_row0 = src + src_stride * src_y;
    const uint8_t *src_row1 = src_row0 + src_stride;
    uint8_t *dst_row0 = dst + dst_stride * dst_y;
    uint8_t *dst_row1 = dst_row0 + dst_stride;
    uint8_t *dst_row2 = dst_row1 + dst_stride;
    uint8_t *dst_row3 = dst_row2 + dst_stride;

    // Left elements
    const uint8_t s0l = src_row0[0], s1l = src_row1[0];
    dst_row0[0] = dst_row0[1] = lerp1d_scalar(7, s0l, 1, s1l);
    dst_row1[0] = dst_row1[1] = lerp1d_scalar(5, s0l, 3, s1l);
    dst_row2[0] = dst_row2[1] = lerp1d_scalar(3, s0l, 5, s1l);
    dst_row3[0] = dst_row3[1] = lerp1d_scalar(1, s0l, 7, s1l);

    // Right elements
    const uint8_t s0r = src_row0[src_width - 1], s1r = src_row1[src_width - 1];
    const size_t dr0 = dst_width - 2;
    const size_t dr1 = dst_width - 1;
    dst_row0[dr0] = dst_row0[dr1] = lerp1d_scalar(7, s0r, 1, s1r);
    dst_row1[dr0] = dst_row1[dr1] = lerp1d_scalar(5, s0r, 3, s1r);
    dst_row2[dr0] = dst_row2[dr1] = lerp1d_scalar(3, s0r, 5, s1r);
    dst_row3[dr0] = dst_row3[dr1] = lerp1d_scalar(1, s0r, 7, s1r);
  }

  auto copy_dst_row = [src_width](const uint8_t *dst_from,
                                  uint8_t *dst_to) KSC {
    for (size_t i = 0; i < src_width; i += svcntb()) {
      svbool_t pg = svwhilelt_b8(i, src_width);
      svst4(pg, dst_to + i * 4, svld4(pg, dst_from + i * 4));
    }
  };

  // Top row
  process_edge_row(src, dst);
  copy_dst_row(dst, dst + dst_stride);

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

  // Bottom row
  process_edge_row(src + src_stride * (src_height - 1),
                   dst + dst_stride * (dst_height - 2));
  copy_dst_row(dst + dst_stride * (dst_height - 2),
               dst + dst_stride * (dst_height - 1));

  return KLEIDICV_OK;
}

KLEIDICV_TARGET_FN_ATTRS static kleidicv_error_t resize_2x2_f32_sc(
    const float *src, size_t src_stride, size_t src_width, size_t src_height,
    float *dst, size_t dst_stride) KLEIDICV_STREAMING_COMPATIBLE {
  size_t dst_width = src_width * 2;
  size_t dst_height = src_height * 2;
  src_stride /= sizeof(float);
  dst_stride /= sizeof(float);

  auto lerp1d_scalar = [](float near, float far) KLEIDICV_STREAMING_COMPATIBLE {
    return near * 0.75F + far * 0.25F;
  };

  auto lerp1d_vector = [](svbool_t pg, svfloat32_t near,
                          svfloat32_t far) KLEIDICV_STREAMING_COMPATIBLE {
    return svmla_n_f32_x(pg, svmul_n_f32_x(pg, near, 0.75F), far, 0.25F);
  };

  auto lerp2d_vector = [](svbool_t pg, svfloat32_t near, svfloat32_t mid_a,
                          svfloat32_t mid_b,
                          svfloat32_t far) KLEIDICV_STREAMING_COMPATIBLE {
    return svmla_n_f32_x(
        pg,
        svmla_n_f32_x(
            pg,
            svmla_n_f32_x(pg, svmul_n_f32_x(pg, near, 0.5625F), mid_a, 0.1875F),
            mid_b, 0.1875F),
        far, 0.0625F);
  };

  // Work-around for clang-format oddness.
#define KSC KLEIDICV_STREAMING_COMPATIBLE

  // Handle top or bottom edge
  auto process_edge_row = [src_width, lerp1d_vector](const float *src_row,
                                                     float *dst_row) KSC {
    for (size_t src_x = 0; src_x + 1 < src_width; src_x += svcntw()) {
      size_t dst_x = src_x * 2 + 1;

      svbool_t pg = svwhilelt_b32(src_x + 1, src_width);

      svfloat32_t a = svld1_f32(pg, src_row + src_x);
      svfloat32_t b = svld1_f32(pg, src_row + src_x + 1);

      svst2_f32(pg, dst_row + dst_x,
                svcreate2(lerp1d_vector(pg, a, b), lerp1d_vector(pg, b, a)));
    }
  };

  auto process_row = [src_width, lerp2d_vector](
                         const float *src_row0, const float *src_row1,
                         float *dst_row0, float *dst_row1) KSC {
    // Middle elements
    for (size_t src_x = 0; src_x + 1 < src_width; src_x += svcntw()) {
      size_t dst_x = src_x * 2 + 1;

      svbool_t pg = svwhilelt_b32(src_x + 1, src_width);

      svfloat32_t a = svld1_f32(pg, src_row0 + src_x);
      svfloat32_t b = svld1_f32(pg, src_row0 + src_x + 1);
      svfloat32_t c = svld1_f32(pg, src_row1 + src_x);
      svfloat32_t d = svld1_f32(pg, src_row1 + src_x + 1);

      svst2_f32(pg, dst_row0 + dst_x,
                svcreate2(lerp2d_vector(pg, a, b, c, d),
                          lerp2d_vector(pg, b, a, d, c)));
      svst2_f32(pg, dst_row1 + dst_x,
                svcreate2(lerp2d_vector(pg, c, a, d, b),
                          lerp2d_vector(pg, d, b, c, a)));
    }
  };

  // Corners
  dst[0] = src[0];
  dst[dst_width - 1] = src[src_width - 1];
  dst[dst_stride * (dst_height - 1)] = src[src_stride * (src_height - 1)];
  dst[dst_stride * (dst_height - 1) + dst_width - 1] =
      src[src_stride * (src_height - 1) + src_width - 1];

  // Left & right edge
  for (size_t src_y = 0; src_y + 1 < src_height; ++src_y) {
    size_t dst_y = src_y * 2 + 1;
    const float *src_row0 = src + src_stride * src_y;
    const float *src_row1 = src_row0 + src_stride;
    float *dst_row0 = dst + dst_stride * dst_y;
    float *dst_row1 = dst_row0 + dst_stride;

    // Left edge
    dst_row0[0] = lerp1d_scalar(src_row0[0], src_row1[0]);
    dst_row1[0] = lerp1d_scalar(src_row1[0], src_row0[0]);

    // Right edge
    dst_row0[dst_width - 1] =
        lerp1d_scalar(src_row0[src_width - 1], src_row1[src_width - 1]);
    dst_row1[dst_width - 1] =
        lerp1d_scalar(src_row1[src_width - 1], src_row0[src_width - 1]);
  }

  // Top row
  process_edge_row(src, dst);

  // Middle rows
  for (size_t src_y = 0; src_y + 1 < src_height; ++src_y) {
    size_t dst_y = src_y * 2 + 1;
    const float *src_row0 = src + src_stride * src_y;
    const float *src_row1 = src_row0 + src_stride;
    float *dst_row0 = dst + dst_stride * dst_y;
    float *dst_row1 = dst_row0 + dst_stride;

    process_row(src_row0, src_row1, dst_row0, dst_row1);
  }

  // Bottom row
  process_edge_row(src + src_stride * (src_height - 1),
                   dst + dst_stride * (dst_height - 1));

  return KLEIDICV_OK;
}

KLEIDICV_TARGET_FN_ATTRS static kleidicv_error_t resize_4x4_f32_sc(
    const float *src, size_t src_stride, size_t src_width, size_t src_height,
    float *dst, size_t dst_stride) KLEIDICV_STREAMING_COMPATIBLE {
  size_t dst_width = src_width * 4;
  size_t dst_height = src_height * 4;
  src_stride /= sizeof(float);
  dst_stride /= sizeof(float);

  auto lerp1d_scalar =
      [](float p, float a, float q, float b)
          KLEIDICV_STREAMING_COMPATIBLE { return p * a + q * b; };

  auto lerp1d_vector =
      [](svbool_t pg, float p, svfloat32_t a, float q, svfloat32_t b)
          KSC { return svmla_n_f32_x(pg, svmul_n_f32_x(pg, a, p), b, q); };

  auto lerp2d_vector = [](svbool_t pg, float p, svfloat32_t a, float q,
                          svfloat32_t b, float r, svfloat32_t c, float s,
                          svfloat32_t d) KSC {
    return svmla_n_f32_x(
        pg,
        svmla_n_f32_x(pg, svmla_n_f32_x(pg, svmul_n_f32_x(pg, a, p), b, q), c,
                      r),
        d, s);
  };

  // Handle top or bottom edge
  auto process_edge_row = [src_width, lerp1d_vector](const float *src_row,
                                                     float *dst_row) KSC {
    for (size_t src_x = 0; src_x + 1 < src_width; src_x += svcntw()) {
      size_t dst_x = src_x * 4 + 2;
      svbool_t pg = svwhilelt_b32(src_x + 1, src_width);
      svfloat32_t a = svld1_f32(pg, src_row + src_x);
      svfloat32_t b = svld1_f32(pg, src_row + src_x + 1);
      svst4_f32(pg, dst_row + dst_x,
                svcreate4(lerp1d_vector(pg, 0.875F, a, 0.125F, b),
                          lerp1d_vector(pg, 0.625F, a, 0.375F, b),
                          lerp1d_vector(pg, 0.375F, a, 0.625F, b),
                          lerp1d_vector(pg, 0.125F, a, 0.875F, b)));
    }
  };

  auto process_row = [src_width, lerp1d_vector, lerp2d_vector](
                         const float *src_row0, const float *src_row1,
                         float *dst_row0, float *dst_row1, float *dst_row2,
                         float *dst_row3) KLEIDICV_STREAMING_COMPATIBLE {
    // Middle elements
    for (size_t src_x = 0; src_x + 1 < src_width; src_x += svcntw()) {
      size_t dst_x = src_x * 4 + 2;

      svbool_t pg = svwhilelt_b32(src_x + 1, src_width);

      svfloat32_t a = svld1_f32(pg, src_row0 + src_x);
      svfloat32_t b = svld1_f32(pg, src_row0 + src_x + 1);
      svfloat32_t c = svld1_f32(pg, src_row1 + src_x);
      svfloat32_t d = svld1_f32(pg, src_row1 + src_x + 1);

      svfloat32x4_t dst_a =
          svcreate4(lerp2d_vector(pg, 0.765625F, a, 0.109375F, b, 0.109375F, c,
                                  0.015625F, d),
                    lerp2d_vector(pg, 0.546875F, a, 0.328125F, b, 0.078125F, c,
                                  0.046875F, d),
                    lerp2d_vector(pg, 0.328125F, a, 0.546875F, b, 0.046875F, c,
                                  0.078125F, d),
                    lerp2d_vector(pg, 0.109375F, a, 0.765625F, b, 0.015625F, c,
                                  0.109375F, d));
      svfloat32x4_t dst_d =
          svcreate4(lerp2d_vector(pg, 0.109375F, a, 0.015625F, b, 0.765625F, c,
                                  0.109375F, d),
                    lerp2d_vector(pg, 0.078125F, a, 0.046875F, b, 0.546875F, c,
                                  0.328125F, d),
                    lerp2d_vector(pg, 0.046875F, a, 0.078125F, b, 0.328125F, c,
                                  0.546875F, d),
                    lerp2d_vector(pg, 0.015625F, a, 0.109375F, b, 0.109375F, c,
                                  0.765625F, d));
      const float one_3rd = 0.3333333333333333F;
      const float two_3rd = 0.6666666666666667F;
      svst4_f32(pg, dst_row0 + dst_x, dst_a);
      svst4_f32(pg, dst_row1 + dst_x,
                svcreate4(lerp1d_vector(pg, two_3rd, svget4(dst_a, 0), one_3rd,
                                        svget4(dst_d, 0)),
                          lerp1d_vector(pg, two_3rd, svget4(dst_a, 1), one_3rd,
                                        svget4(dst_d, 1)),
                          lerp1d_vector(pg, two_3rd, svget4(dst_a, 2), one_3rd,
                                        svget4(dst_d, 2)),
                          lerp1d_vector(pg, two_3rd, svget4(dst_a, 3), one_3rd,
                                        svget4(dst_d, 3))));
      svst4_f32(pg, dst_row2 + dst_x,
                svcreate4(lerp1d_vector(pg, one_3rd, svget4(dst_a, 0), two_3rd,
                                        svget4(dst_d, 0)),
                          lerp1d_vector(pg, one_3rd, svget4(dst_a, 1), two_3rd,
                                        svget4(dst_d, 1)),
                          lerp1d_vector(pg, one_3rd, svget4(dst_a, 2), two_3rd,
                                        svget4(dst_d, 2)),
                          lerp1d_vector(pg, one_3rd, svget4(dst_a, 3), two_3rd,
                                        svget4(dst_d, 3))));
      svst4_f32(pg, dst_row3 + dst_x, dst_d);
    }
  };

  // Corners
  auto set_corner = [dst, dst_stride](size_t left_column, size_t top_row,
                                      float value) KSC {
    dst[dst_stride * top_row + left_column] = value;
    dst[dst_stride * top_row + left_column + 1] = value;
    dst[dst_stride * (top_row + 1) + left_column] = value;
    dst[dst_stride * (top_row + 1) + left_column + 1] = value;
  };
  set_corner(0, 0, src[0]);
  set_corner(dst_width - 2, 0, src[src_width - 1]);
  set_corner(0, dst_height - 2, src[src_stride * (src_height - 1)]);
  set_corner(dst_width - 2, dst_height - 2,
             src[src_stride * (src_height - 1) + src_width - 1]);

  // Left & right edge
  for (size_t src_y = 0; src_y + 1 < src_height; ++src_y) {
    size_t dst_y = src_y * 4 + 2;
    const float *src_row0 = src + src_stride * src_y;
    const float *src_row1 = src_row0 + src_stride;
    float *dst_row0 = dst + dst_stride * dst_y;
    float *dst_row1 = dst_row0 + dst_stride;
    float *dst_row2 = dst_row1 + dst_stride;
    float *dst_row3 = dst_row2 + dst_stride;

    // Left elements
    const float s0l = src_row0[0], s1l = src_row1[0];
    dst_row0[0] = dst_row0[1] = lerp1d_scalar(0.875F, s0l, 0.125F, s1l);
    dst_row1[0] = dst_row1[1] = lerp1d_scalar(0.625F, s0l, 0.375F, s1l);
    dst_row2[0] = dst_row2[1] = lerp1d_scalar(0.375F, s0l, 0.625F, s1l);
    dst_row3[0] = dst_row3[1] = lerp1d_scalar(0.125F, s0l, 0.875F, s1l);

    // Right elements
    const float s0r = src_row0[src_width - 1], s1r = src_row1[src_width - 1];
    const size_t dr0 = dst_width - 2;
    const size_t dr1 = dst_width - 1;
    dst_row0[dr0] = dst_row0[dr1] = lerp1d_scalar(0.875F, s0r, 0.125F, s1r);
    dst_row1[dr0] = dst_row1[dr1] = lerp1d_scalar(0.625F, s0r, 0.375F, s1r);
    dst_row2[dr0] = dst_row2[dr1] = lerp1d_scalar(0.375F, s0r, 0.625F, s1r);
    dst_row3[dr0] = dst_row3[dr1] = lerp1d_scalar(0.125F, s0r, 0.875F, s1r);
  }

  auto copy_dst_row = [src_width](const float *dst_from, float *dst_to) KSC {
    for (size_t i = 0; i < src_width; i += svcntw()) {
      svbool_t pg = svwhilelt_b32(i, src_width);
      svst4(pg, dst_to + i * 4, svld4(pg, dst_from + i * 4));
    }
  };

  // Top row
  process_edge_row(src, dst);
  copy_dst_row(dst, dst + dst_stride);

  // Middle rows
  for (size_t src_y = 0; src_y + 1 < src_height; ++src_y) {
    size_t dst_y = src_y * 4 + 2;
    const float *src_row0 = src + src_stride * src_y;
    const float *src_row1 = src_row0 + src_stride;
    float *dst_row0 = dst + dst_stride * dst_y;
    float *dst_row1 = dst_row0 + dst_stride;
    float *dst_row2 = dst_row1 + dst_stride;
    float *dst_row3 = dst_row2 + dst_stride;

    process_row(src_row0, src_row1, dst_row0, dst_row1, dst_row2, dst_row3);
  }

  // Bottom row
  process_edge_row(src + src_stride * (src_height - 1),
                   dst + dst_stride * (dst_height - 2));
  copy_dst_row(dst + dst_stride * (dst_height - 2),
               dst + dst_stride * (dst_height - 1));

  return KLEIDICV_OK;
}

KLEIDICV_TARGET_FN_ATTRS static kleidicv_error_t resize_8x8_f32_sc(
    const float *src, size_t src_stride, size_t src_width, size_t src_height,
    float *dst, size_t dst_stride) KLEIDICV_STREAMING_COMPATIBLE {
  size_t dst_width = src_width * 8;
  size_t dst_height = src_height * 8;
  src_stride /= sizeof(float);
  dst_stride /= sizeof(float);

  svuint32_t indices_1a, indices_1b, indices_2a, indices_2b, indices_3a,
      indices_3b, indices_4a, indices_4b;
  {
    svuint32_t tmp_2x = svreinterpret_u32_u64(svindex_u64(0, 0x100000001UL));
    svuint32_t tmp_4x = svzip1(tmp_2x, tmp_2x);  // 0, 0, 0, 0, 1, 1, 1, 1, ...
    indices_1a = svzip1(tmp_4x, tmp_4x);  // 8 times 0, then 8 times 1, ...
    indices_2a = svzip2(tmp_4x, tmp_4x);
    // next section, e.g. in case of 512-bit regs (=16 x F32), it is 4, 4, 4, 4,
    // 5, 5, 5, 5, ...
    tmp_4x = svzip2(tmp_2x, tmp_2x);
    indices_3a = svzip1(tmp_4x, tmp_4x);
    indices_4a = svzip2(tmp_4x, tmp_4x);

    // same as above, just all numbers are bigger by one
    tmp_2x = svreinterpret_u32_u64(svindex_u64(0x100000001UL, 0x100000001UL));
    tmp_4x = svzip1(tmp_2x, tmp_2x);      // 1, 1, 1, 1, ...
    indices_1b = svzip1(tmp_4x, tmp_4x);  // 8 times 1, then 8 times 2, ...
    indices_2b = svzip2(tmp_4x, tmp_4x);
    // next section, e.g. in case of 512-bit regs (=16 x F32), it is 5, 5, 5, 5,
    // 6, 6, 6, 6, ...
    tmp_4x = svzip2(tmp_2x, tmp_2x);
    indices_3b = svzip1(tmp_4x, tmp_4x);
    indices_4b = svzip2(tmp_4x, tmp_4x);
  }

  svfloat32_t coeffs_a, coeffs_b;
  {
    // Prepare 1/16, 3/16, 5/16, ..., 15/16, repeated
    svuint32_t linear = svindex_u32(1, 2);
    svfloat32_t repetitive_float =  // mod 16
        svcvt_f32_x(svptrue_b32(), svand_n_u32_m(svptrue_b32(), linear, 0x0F));
    coeffs_b = svdiv_n_f32_x(svptrue_b32(), repetitive_float, 16.0F);
    coeffs_a = svsub_x(svptrue_b32(), svdup_f32(1.0F), coeffs_b);
  }
  auto lerp1d_vector =
      [](svbool_t pg, float p, svfloat32_t a, float q, svfloat32_t b)
          KSC { return svmla_n_f32_x(pg, svmul_n_f32_x(pg, a, p), b, q); };

  auto index_and_lerp1d = [&coeffs_a, &coeffs_b](
                              svbool_t pg, svuint32_t indices_a,
                              svuint32_t indices_b, svfloat32_t src) KSC {
    return svmla_f32_x(pg, svmul_f32_x(pg, svtbl(src, indices_a), coeffs_a),
                       svtbl(src, indices_b), coeffs_b);
  };

  // Handle top or bottom edge
  auto process_edge_row = [src_width, index_and_lerp1d, &indices_1a,
                           &indices_1b, &indices_2a, &indices_2b, &indices_3a,
                           &indices_3b, &indices_4a,
                           &indices_4b](const float *src_row, float *dst_row,
                                        size_t dst_stride) KSC {
    for (size_t src_x = 0; src_x + 1 < src_width; src_x += svcntw() / 2) {
      svbool_t pg = svwhilelt_b32(src_x, src_width);
      svfloat32_t svsrc = svld1_f32(pg, src_row + src_x);

      size_t dst_length = 8 * (src_width - src_x - 1);
      svbool_t pg_1 = svwhilelt_b32(0UL, dst_length);
      svbool_t pg_2 = svwhilelt_b32(svcntw(), dst_length);
      svbool_t pg_3 = svwhilelt_b32(2 * svcntw(), dst_length);
      svbool_t pg_4 = svwhilelt_b32(3 * svcntw(), dst_length);

      float *pDst1 = dst_row + src_x * 8 + 4;
      float *pDst2 = pDst1 + dst_stride;
      float *pDst3 = pDst2 + dst_stride;
      float *pDst4 = pDst3 + dst_stride;
      svfloat32_t dst = index_and_lerp1d(pg_1, indices_1a, indices_1b, svsrc);
      svst1(pg_1, pDst1, dst);
      svst1(pg_1, pDst2, dst);
      svst1(pg_1, pDst3, dst);
      svst1(pg_1, pDst4, dst);

      dst = index_and_lerp1d(pg_2, indices_2a, indices_2b, svsrc);
      svst1_vnum(pg_2, pDst1, 1, dst);
      svst1_vnum(pg_2, pDst2, 1, dst);
      svst1_vnum(pg_2, pDst3, 1, dst);
      svst1_vnum(pg_2, pDst4, 1, dst);

      dst = index_and_lerp1d(pg_3, indices_3a, indices_3b, svsrc);
      svst1_vnum(pg_3, pDst1, 2, dst);
      svst1_vnum(pg_3, pDst2, 2, dst);
      svst1_vnum(pg_3, pDst3, 2, dst);
      svst1_vnum(pg_3, pDst4, 2, dst);

      dst = index_and_lerp1d(pg_4, indices_4a, indices_4b, svsrc);
      svst1_vnum(pg_4, pDst1, 3, dst);
      svst1_vnum(pg_4, pDst2, 3, dst);
      svst1_vnum(pg_4, pDst3, 3, dst);
      svst1_vnum(pg_4, pDst4, 3, dst);
    }
  };

  svfloat32_t coeffs_p = svmul_n_f32_x(svptrue_b32(), coeffs_a, 15.0 / 16);
  svfloat32_t coeffs_q = svmul_n_f32_x(svptrue_b32(), coeffs_b, 15.0 / 16);
  svfloat32_t coeffs_r = svmul_n_f32_x(svptrue_b32(), coeffs_a, 1.0 / 16);
  svfloat32_t coeffs_s = svmul_n_f32_x(svptrue_b32(), coeffs_b, 1.0 / 16);

  auto index_and_lerp2d = [&coeffs_p, &coeffs_q, &coeffs_r, &coeffs_s](
                              svbool_t pg, svuint32_t indices_a,
                              svuint32_t indices_b, svfloat32_t src0,
                              svfloat32_t src1) KSC {
    return svmla_f32_x(
        pg,
        svmla_f32_x(
            pg,
            svmla_f32_x(pg, svmul_f32_x(pg, svtbl(src0, indices_a), coeffs_p),
                        svtbl(src0, indices_b), coeffs_q),
            svtbl(src1, indices_a), coeffs_r),
        svtbl(src1, indices_b), coeffs_s);
  };

  auto process_row = [src_width, index_and_lerp2d, lerp1d_vector, &indices_1a,
                      &indices_1b, &indices_2a, &indices_2b, &indices_3a,
                      &indices_3b, &indices_4a, &indices_4b](
                         const float *src_row0, const float *src_row1,
                         float *dst_row0,
                         size_t dst_stride) KLEIDICV_STREAMING_COMPATIBLE {
    // Middle elements
    float *dst_row1 = dst_row0 + dst_stride;
    float *dst_row2 = dst_row1 + dst_stride;
    float *dst_row3 = dst_row2 + dst_stride;
    float *dst_row4 = dst_row3 + dst_stride;
    float *dst_row5 = dst_row4 + dst_stride;
    float *dst_row6 = dst_row5 + dst_stride;
    float *dst_row7 = dst_row6 + dst_stride;
    for (size_t src_x = 0; src_x + 1 < src_width; src_x += svcntw() / 2) {
      size_t dst_x = src_x * 8 + 4;

      svbool_t pg = svwhilelt_b32(src_x, src_width);
      svfloat32_t src_0 = svld1_f32(pg, src_row0 + src_x);
      svfloat32_t src_1 = svld1_f32(pg, src_row1 + src_x);

      size_t dst_length = 8 * (src_width - src_x - 1);
      svbool_t pg_1 = svwhilelt_b32(0UL, dst_length);
      svbool_t pg_2 = svwhilelt_b32(svcntw(), dst_length);
      svbool_t pg_3 = svwhilelt_b32(2 * svcntw(), dst_length);
      svbool_t pg_4 = svwhilelt_b32(3 * svcntw(), dst_length);

      float *pDst0 = dst_row0 + dst_x;
      float *pDst1 = dst_row1 + dst_x;
      float *pDst2 = dst_row2 + dst_x;
      float *pDst3 = dst_row3 + dst_x;
      float *pDst4 = dst_row4 + dst_x;
      float *pDst5 = dst_row5 + dst_x;
      float *pDst6 = dst_row6 + dst_x;
      float *pDst7 = dst_row7 + dst_x;

      svfloat32_t dst_0 =
          index_and_lerp2d(pg_1, indices_1a, indices_1b, src_0, src_1);
      svst1(pg_1, pDst0, dst_0);
      svfloat32_t dst_7 =
          index_and_lerp2d(pg_1, indices_1a, indices_1b, src_1, src_0);
      svst1(pg_1, pDst7, dst_7);
      svst1(pg_1, pDst1, lerp1d_vector(pg_1, 6.0 / 7, dst_0, 1.0 / 7, dst_7));
      svst1(pg_1, pDst2, lerp1d_vector(pg_1, 5.0 / 7, dst_0, 2.0 / 7, dst_7));
      svst1(pg_1, pDst3, lerp1d_vector(pg_1, 4.0 / 7, dst_0, 3.0 / 7, dst_7));
      svst1(pg_1, pDst4, lerp1d_vector(pg_1, 3.0 / 7, dst_0, 4.0 / 7, dst_7));
      svst1(pg_1, pDst5, lerp1d_vector(pg_1, 2.0 / 7, dst_0, 5.0 / 7, dst_7));
      svst1(pg_1, pDst6, lerp1d_vector(pg_1, 1.0 / 7, dst_0, 6.0 / 7, dst_7));

      dst_0 = index_and_lerp2d(pg_2, indices_2a, indices_2b, src_0, src_1);
      svst1_vnum(pg_2, pDst0, 1, dst_0);
      dst_7 = index_and_lerp2d(pg_2, indices_2a, indices_2b, src_1, src_0);
      svst1_vnum(pg_2, pDst7, 1, dst_7);
      svst1_vnum(pg_2, pDst1, 1,
                 lerp1d_vector(pg_2, 6.0 / 7, dst_0, 1.0 / 7, dst_7));
      svst1_vnum(pg_2, pDst2, 1,
                 lerp1d_vector(pg_2, 5.0 / 7, dst_0, 2.0 / 7, dst_7));
      svst1_vnum(pg_2, pDst3, 1,
                 lerp1d_vector(pg_2, 4.0 / 7, dst_0, 3.0 / 7, dst_7));
      svst1_vnum(pg_2, pDst4, 1,
                 lerp1d_vector(pg_2, 3.0 / 7, dst_0, 4.0 / 7, dst_7));
      svst1_vnum(pg_2, pDst5, 1,
                 lerp1d_vector(pg_2, 2.0 / 7, dst_0, 5.0 / 7, dst_7));
      svst1_vnum(pg_2, pDst6, 1,
                 lerp1d_vector(pg_2, 1.0 / 7, dst_0, 6.0 / 7, dst_7));

      dst_0 = index_and_lerp2d(pg_3, indices_3a, indices_3b, src_0, src_1);
      svst1_vnum(pg_3, pDst0, 2, dst_0);
      dst_7 = index_and_lerp2d(pg_3, indices_3a, indices_3b, src_1, src_0);
      svst1_vnum(pg_3, pDst7, 2, dst_7);
      svst1_vnum(pg_3, pDst1, 2,
                 lerp1d_vector(pg_3, 6.0 / 7, dst_0, 1.0 / 7, dst_7));
      svst1_vnum(pg_3, pDst2, 2,
                 lerp1d_vector(pg_3, 5.0 / 7, dst_0, 2.0 / 7, dst_7));
      svst1_vnum(pg_3, pDst3, 2,
                 lerp1d_vector(pg_3, 4.0 / 7, dst_0, 3.0 / 7, dst_7));
      svst1_vnum(pg_3, pDst4, 2,
                 lerp1d_vector(pg_3, 3.0 / 7, dst_0, 4.0 / 7, dst_7));
      svst1_vnum(pg_3, pDst5, 2,
                 lerp1d_vector(pg_3, 2.0 / 7, dst_0, 5.0 / 7, dst_7));
      svst1_vnum(pg_3, pDst6, 2,
                 lerp1d_vector(pg_3, 1.0 / 7, dst_0, 6.0 / 7, dst_7));

      dst_0 = index_and_lerp2d(pg_4, indices_4a, indices_4b, src_0, src_1);
      svst1_vnum(pg_4, pDst0, 3, dst_0);
      dst_7 = index_and_lerp2d(pg_4, indices_4a, indices_4b, src_1, src_0);
      svst1_vnum(pg_4, pDst7, 3, dst_7);
      svst1_vnum(pg_4, pDst1, 3,
                 lerp1d_vector(pg_4, 6.0 / 7, dst_0, 1.0 / 7, dst_7));
      svst1_vnum(pg_4, pDst2, 3,
                 lerp1d_vector(pg_4, 5.0 / 7, dst_0, 2.0 / 7, dst_7));
      svst1_vnum(pg_4, pDst3, 3,
                 lerp1d_vector(pg_4, 4.0 / 7, dst_0, 3.0 / 7, dst_7));
      svst1_vnum(pg_4, pDst4, 3,
                 lerp1d_vector(pg_4, 3.0 / 7, dst_0, 4.0 / 7, dst_7));
      svst1_vnum(pg_4, pDst5, 3,
                 lerp1d_vector(pg_4, 2.0 / 7, dst_0, 5.0 / 7, dst_7));
      svst1_vnum(pg_4, pDst6, 3,
                 lerp1d_vector(pg_4, 1.0 / 7, dst_0, 6.0 / 7, dst_7));
    }
  };

  // Corners
  auto set_corner = [dst, dst_stride](size_t left_column, size_t top_row,
                                      float value) KSC {
    float *row = dst + dst_stride * top_row + left_column;
    for (size_t i = 0; i < 4; ++i) {
      for (size_t j = 0; j < 4; ++j) {
        row[j] = value;
      }
      row += dst_stride;
    }
  };
  set_corner(0, 0, src[0]);
  set_corner(dst_width - 4, 0, src[src_width - 1]);
  set_corner(0, dst_height - 4, src[src_stride * (src_height - 1)]);
  set_corner(dst_width - 4, dst_height - 4,
             src[src_stride * (src_height - 1) + src_width - 1]);

  // Left & right edge
  for (size_t src_y = 0; src_y + 1 < src_height; ++src_y) {
    float *pDst = dst + dst_stride * (src_y * 8 + 4);
    const float *src_row0 = src + src_stride * src_y;
    const float *src_row1 = src_row0 + src_stride;
    const float s0l = src_row0[0], s1l = src_row1[0];
    const float s0r = src_row0[src_width - 1], s1r = src_row1[src_width - 1];
    svbool_t pg = svptrue_pat_b32(SV_VL4);  // write 4 elements
    for (size_t i = 0; i < 8; ++i) {
      svst1(pg, pDst,
            lerp1d_vector(pg, static_cast<float>(15 - i * 2) / 16.0F,
                          svdup_f32_x(pg, s0l),
                          static_cast<float>(i * 2 + 1) / 16.0F,
                          svdup_f32_x(pg, s1l)));
      svst1(pg, pDst + dst_width - 4,
            lerp1d_vector(pg, static_cast<float>(15 - i * 2) / 16.0F,
                          svdup_f32_x(pg, s0r),
                          static_cast<float>(i * 2 + 1) / 16.0F,
                          svdup_f32_x(pg, s1r)));
      pDst += dst_stride;
    }
  }

  // Top rows
  process_edge_row(src, dst, dst_stride);

  // Middle rows
  for (size_t src_y = 0; src_y + 1 < src_height; ++src_y) {
    size_t dst_y = src_y * 8 + 4;
    const float *src_row0 = src + src_stride * src_y;
    const float *src_row1 = src_row0 + src_stride;
    process_row(src_row0, src_row1, dst + dst_stride * dst_y, dst_stride);
  }

  // Bottom rows
  process_edge_row(src + src_stride * (src_height - 1),
                   dst + dst_stride * (dst_height - 4), dst_stride);

  return KLEIDICV_OK;
}

KLEIDICV_TARGET_FN_ATTRS static kleidicv_error_t resize_linear_u8_sc(
    const uint8_t *src, size_t src_stride, size_t src_width, size_t src_height,
    uint8_t *dst, size_t dst_stride, size_t dst_width,
    size_t dst_height) KLEIDICV_STREAMING_COMPATIBLE {
  CHECK_POINTER_AND_STRIDE(src, src_stride, src_height);
  CHECK_POINTER_AND_STRIDE(dst, dst_stride, dst_height);
  CHECK_IMAGE_SIZE(dst_width, dst_height);

  if (src_width == 0 || src_height == 0) {
    return KLEIDICV_OK;
  }
  if (src_width * 2 == dst_width && src_height * 2 == dst_height) {
    return resize_2x2_u8_sc(src, src_stride, src_width, src_height, dst,
                            dst_stride);
  }
  if (src_width * 4 == dst_width && src_height * 4 == dst_height) {
    return resize_4x4_u8_sc(src, src_stride, src_width, src_height, dst,
                            dst_stride);
  }
  return KLEIDICV_ERROR_NOT_IMPLEMENTED;
}

KLEIDICV_TARGET_FN_ATTRS static kleidicv_error_t resize_linear_f32_sc(
    const float *src, size_t src_stride, size_t src_width, size_t src_height,
    float *dst, size_t dst_stride, size_t dst_width,
    size_t dst_height) KLEIDICV_STREAMING_COMPATIBLE {
  CHECK_POINTER_AND_STRIDE(src, src_stride, src_height);
  CHECK_POINTER_AND_STRIDE(dst, dst_stride, dst_height);
  CHECK_IMAGE_SIZE(dst_width, dst_height);

  if (src_width == 0 || src_height == 0) {
    return KLEIDICV_OK;
  }
  if (src_width * 2 == dst_width && src_height * 2 == dst_height) {
    return resize_2x2_f32_sc(src, src_stride, src_width, src_height, dst,
                             dst_stride);
  }
  if (src_width * 4 == dst_width && src_height * 4 == dst_height) {
    return resize_4x4_f32_sc(src, src_stride, src_width, src_height, dst,
                             dst_stride);
  }
  if (src_width * 8 == dst_width && src_height * 8 == dst_height &&
      svcntw() >= 8) {
    return resize_8x8_f32_sc(src, src_stride, src_width, src_height, dst,
                             dst_stride);
  }
  return KLEIDICV_ERROR_NOT_IMPLEMENTED;
}

}  // namespace KLEIDICV_TARGET_NAMESPACE

#endif  // KLEIDICV_RESIZE_SC_H
