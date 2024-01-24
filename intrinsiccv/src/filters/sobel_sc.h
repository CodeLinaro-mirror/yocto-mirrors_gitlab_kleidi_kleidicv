// SPDX-FileCopyrightText: 2023 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#ifndef INTRINSICCV_SOBEL_SC_H
#define INTRINSICCV_SOBEL_SC_H

#include "filters/sobel.h"
#include "intrinsiccv.h"
#include "sve2.h"

namespace intrinsiccv::sve2 {

// Template for 3x3 Sobel filters which calculate horizontal derivative
// approximations, often denoted as Gx.
//
//      [ -1, 0, 1 ]   [ 1 ]
//  F = [ -2, 0, 2 ] = [ 2 ] * [ -1,  0, 1 ]
//      [ -1, 0, 1 ]   [ 1 ]
template <typename T>
class HorizontalSobel3x3;

// 3x3 Sobel filter for uint8_t types which calculates horizontal derivative
// approximations, often denoted as Gx.
template <>
class HorizontalSobel3x3<uint8_t> {
 public:
  using SourceType = uint8_t;
  using BufferType = int16_t;
  using DestinationType = int16_t;

  // Applies vertical filtering vector using SIMD operations.
  //
  // DST = [ SRC0, SRC1, SRC2 ] * [ 1, 2, 1 ]T
  void vertical_vector_path(svbool_t pg, svuint8_t src_0, svuint8_t src_1,
                            svuint8_t src_2, BufferType *dst) const
      INTRINSICCV_STREAMING_COMPATIBLE {
    svuint16_t acc_u16_b = svaddlb(src_0, src_2);
    svuint16_t acc_u16_t = svaddlt(src_0, src_2);
    acc_u16_b = svmlalb(acc_u16_b, src_1, svdup_n_u8(2));
    acc_u16_t = svmlalt(acc_u16_t, src_1, svdup_n_u8(2));

    svint16x2_t interleaved =
        svcreate2(svreinterpret_s16(acc_u16_b), svreinterpret_s16(acc_u16_t));
    svst2(pg, &dst[0], interleaved);
  }

  // Applies horizontal filtering vector using SIMD operations.
  //
  // DST = [ SRC0, SRC1, SRC2 ] * [ -1, 0, 1 ]T
  void horizontal_vector_path(
      svbool_t pg, svint16_t src_0, svint16_t /* src_1 */, svint16_t src_2,
      DestinationType *dst) const INTRINSICCV_STREAMING_COMPATIBLE {
    svst1(pg, &dst[0], svsub_x(pg, src_2, src_0));
  }

  // Applies horizontal filtering vector using scalar operations.
  //
  // DST = [ SRC0, SRC1, SRC2 ] * [ -1, 0, 1 ]T
  void horizontal_scalar_path(BufferType src[3], DestinationType *dst) const
      INTRINSICCV_STREAMING_COMPATIBLE {
    dst[0] = src[2] - src[0];
  }
};  // end of class HorizontalSobel3x3<uint8_t>

// Template for 3x3 Sobel filters which calculate vertical derivative
// approximations, often denoted as Gy.
//
//      [ -1, -2, 1 ]   [ -1 ]
//  F = [  0,  0, 0 ] = [  0 ] * [ 1,  2, 1 ]
//      [  1,  2, 1 ]   [  1 ]
template <typename T>
class VerticalSobel3x3;

// 3x3 Sobel filter for uint8_t types which calculates vertical derivative
// approximations, often denoted as Gy.
template <>
class VerticalSobel3x3<uint8_t> {
 public:
  using SourceType = uint8_t;
  using BufferType = int16_t;
  using DestinationType = int16_t;

  // Applies vertical filtering vector using SIMD operations.
  //
  // DST = [ SRC0, SRC1, SRC2 ] * [ -1, 0, 1 ]T
  void vertical_vector_path(svbool_t pg, svuint8_t src_0, svuint8_t /* src_1 */,
                            svuint8_t src_2, BufferType *dst) const
      INTRINSICCV_STREAMING_COMPATIBLE {
    svuint16_t acc_u16_b = svsublb(src_2, src_0);
    svuint16_t acc_u16_t = svsublt(src_2, src_0);

    svint16x2_t interleaved =
        svcreate2(svreinterpret_s16(acc_u16_b), svreinterpret_s16(acc_u16_t));
    svst2(pg, &dst[0], interleaved);
  }

  // Applies horizontal filtering vector using SIMD operations.
  //
  // DST = [ SRC0, SRC1, SRC2 ] * [ 1, 2, 1 ]T
  void horizontal_vector_path(svbool_t pg, svint16_t src_0, svint16_t src_1,
                              svint16_t src_2, DestinationType *dst) const
      INTRINSICCV_STREAMING_COMPATIBLE {
    svint16_t acc = svadd_x(pg, src_0, src_2);
    acc = svmad_s16_x(pg, src_1, svdup_n_s16(2), acc);
    svst1(pg, &dst[0], acc);
  }

  // Applies horizontal filtering vector using scalar operations.
  //
  // DST = [ SRC0, SRC1, SRC2 ] * [ 1, 2, 1 ]T
  void horizontal_scalar_path(BufferType src[3], DestinationType *dst) const
      INTRINSICCV_STREAMING_COMPATIBLE {
    dst[0] = src[0] + 2 * src[1] + src[2];
  }
};  // end of class VerticalSobel3x3<uint8_t>

INTRINSICCV_TARGET_FN_ATTRS
static void sobel_3x3_horizontal_s16_u8_sc(
    const uint8_t *src, size_t src_stride, int16_t *dst, size_t dst_stride,
    size_t width, size_t height,
    size_t channels) INTRINSICCV_STREAMING_COMPATIBLE {
  Rectangle rect{width, height};
  Rows<const uint8_t> src_rows{src, src_stride, channels};
  Rows<int16_t> dst_rows{dst, dst_stride, channels};

  auto workspace =
      SeparableFilterWorkspace::create(rect, channels, sizeof(int16_t));
  if (!workspace) {
    return;
  }

  HorizontalSobel3x3<uint8_t> horizontal_sobel;
  sve2::SeparableFilter3x3<HorizontalSobel3x3<uint8_t>> filter{
      horizontal_sobel};
  workspace->process(rect, src_rows, dst_rows, channels,
                     INTRINSICCV_BORDER_TYPE_REPLICATE, filter);
}

INTRINSICCV_TARGET_FN_ATTRS
static void sobel_3x3_vertical_s16_u8_sc(
    const uint8_t *src, size_t src_stride, int16_t *dst, size_t dst_stride,
    size_t width, size_t height,
    size_t channels) INTRINSICCV_STREAMING_COMPATIBLE {
  Rectangle rect{width, height};
  Rows<const uint8_t> src_rows{src, src_stride, channels};
  Rows<int16_t> dst_rows{dst, dst_stride, channels};

  auto workspace =
      SeparableFilterWorkspace::create(rect, channels, sizeof(int16_t));
  if (!workspace) {
    return;
  }

  VerticalSobel3x3<uint8_t> vertical_sobel;
  sve2::SeparableFilter3x3<VerticalSobel3x3<uint8_t>> filter{vertical_sobel};
  workspace->process(rect, src_rows, dst_rows, channels,
                     INTRINSICCV_BORDER_TYPE_REPLICATE, filter);
}

}  // namespace intrinsiccv::sve2

#endif  // INTRINSICCV_SOBEL_SC_H
