// SPDX-FileCopyrightText: 2024 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#ifndef KLEIDICV_RESIZE_LINEAR_SC_H
#define KLEIDICV_RESIZE_LINEAR_SC_H

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

  auto process_row = [src_width, lerp2d_vector](
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

      svst4_f32(pg, dst_row0 + dst_x,
                (svcreate4(lerp2d_vector(pg, 0.765625F, a, 0.109375F, b,
                                         0.109375F, c, 0.015625F, d),
                           lerp2d_vector(pg, 0.546875F, a, 0.328125F, b,
                                         0.078125F, c, 0.046875F, d),
                           lerp2d_vector(pg, 0.328125F, a, 0.546875F, b,
                                         0.046875F, c, 0.078125F, d),
                           lerp2d_vector(pg, 0.109375F, a, 0.765625F, b,
                                         0.015625F, c, 0.109375F, d))));

      svst4_f32(pg, dst_row1 + dst_x,
                (svcreate4(lerp2d_vector(pg, 0.546875F, a, 0.078125F, b,
                                         0.328125F, c, 0.046875F, d),
                           lerp2d_vector(pg, 0.390625F, a, 0.234375F, b,
                                         0.234375F, c, 0.140625F, d),
                           lerp2d_vector(pg, 0.234375F, a, 0.390625F, b,
                                         0.140625F, c, 0.234375F, d),
                           lerp2d_vector(pg, 0.078125F, a, 0.546875F, b,
                                         0.046875F, c, 0.328125F, d))));
      svst4_f32(pg, dst_row2 + dst_x,
                (svcreate4(lerp2d_vector(pg, 0.328125F, a, 0.046875F, b,
                                         0.546875F, c, 0.078125F, d),
                           lerp2d_vector(pg, 0.234375F, a, 0.140625F, b,
                                         0.390625F, c, 0.234375F, d),
                           lerp2d_vector(pg, 0.140625F, a, 0.234375F, b,
                                         0.234375F, c, 0.390625F, d),
                           lerp2d_vector(pg, 0.046875F, a, 0.328125F, b,
                                         0.078125F, c, 0.546875F, d))));
      svst4_f32(pg, dst_row3 + dst_x,
                (svcreate4(lerp2d_vector(pg, 0.109375F, a, 0.015625F, b,
                                         0.765625F, c, 0.109375F, d),
                           lerp2d_vector(pg, 0.078125F, a, 0.046875F, b,
                                         0.546875F, c, 0.328125F, d),
                           lerp2d_vector(pg, 0.046875F, a, 0.078125F, b,
                                         0.328125F, c, 0.546875F, d),
                           lerp2d_vector(pg, 0.015625F, a, 0.109375F, b,
                                         0.109375F, c, 0.765625F, d))));
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
  return KLEIDICV_ERROR_NOT_IMPLEMENTED;
}

}  // namespace KLEIDICV_TARGET_NAMESPACE

#endif  // KLEIDICV_RESIZE_SC_H
