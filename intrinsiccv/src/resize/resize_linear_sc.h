// SPDX-FileCopyrightText: 2024 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#ifndef INTRINSICCV_RESIZE_LINEAR_SC_H
#define INTRINSICCV_RESIZE_LINEAR_SC_H

#include "intrinsiccv/intrinsiccv.h"
#include "intrinsiccv/sve2.h"

namespace INTRINSICCV_TARGET_NAMESPACE {

INTRINSICCV_TARGET_FN_ATTRS static intrinsiccv_error_t resize_2x2_u8_sc(
    const uint8_t *src, size_t src_stride, size_t src_width, size_t src_height,
    uint8_t *dst, size_t dst_stride) INTRINSICCV_STREAMING_COMPATIBLE {
  if (src_width == 0 || src_height == 0) {
    return INTRINSICCV_OK;
  }

  size_t dst_width = src_width * 2;
  size_t dst_height = src_height * 2;

  auto lerp1d_scalar = [](uint8_t near, uint8_t far)
                           INTRINSICCV_STREAMING_COMPATIBLE {
                             return (near * 3 + far + 2) >> 2;
                           };

  auto lerp1d_vector = [](svuint8_t near,
                          svuint8_t far) INTRINSICCV_STREAMING_COMPATIBLE {
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
                          svuint8_t far) INTRINSICCV_STREAMING_COMPATIBLE {
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
#define ISC INTRINSICCV_STREAMING_COMPATIBLE

  // Handle top or bottom edge
  auto process_edge_row = [src_width, lerp1d_vector](const uint8_t *src_row,
                                                     uint8_t *dst_row) ISC {
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
                         uint8_t *dst_row1) INTRINSICCV_STREAMING_COMPATIBLE {
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
                   dst + dst_stride * (src_height * 2 - 1));

  return INTRINSICCV_OK;
}

INTRINSICCV_TARGET_FN_ATTRS static intrinsiccv_error_t resize_linear_u8_sc(
    const uint8_t *src, size_t src_stride, size_t src_width, size_t src_height,
    uint8_t *dst, size_t dst_stride, size_t dst_width,
    size_t dst_height) INTRINSICCV_STREAMING_COMPATIBLE {
  CHECK_POINTER_AND_STRIDE(src, src_stride);
  CHECK_POINTER_AND_STRIDE(dst, dst_stride);
  CHECK_IMAGE_SIZE(dst_width, dst_height);

  if (src_width * 2 == dst_width && src_height * 2 == dst_height) {
    return resize_2x2_u8_sc(src, src_stride, src_width, src_height, dst,
                            dst_stride);
  }
  return INTRINSICCV_ERROR_NOT_IMPLEMENTED;
}

}  // namespace INTRINSICCV_TARGET_NAMESPACE

#endif  // INTRINSICCV_RESIZE_SC_H
