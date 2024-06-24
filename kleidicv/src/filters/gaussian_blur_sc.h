// SPDX-FileCopyrightText: 2023 - 2024 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#ifndef KLEIDICV_GAUSSIAN_BLUR_SC_H
#define KLEIDICV_GAUSSIAN_BLUR_SC_H

#include <limits>

#include "kleidicv/kleidicv.h"
#include "kleidicv/separable_filter_15x15_sc.h"
#include "kleidicv/separable_filter_3x3_sc.h"
#include "kleidicv/separable_filter_5x5_sc.h"
#include "kleidicv/separable_filter_7x7_sc.h"
#include "kleidicv/sve2.h"

namespace KLEIDICV_TARGET_NAMESPACE {

// Primary template for Gaussian Blur approximation filters.
template <typename ScalarType, size_t KernelSize>
class DiscreteGaussianBlur;

// Template for 3x3 Gaussian Blur approximation filters.
//
//             [ 1, 2, 1 ]          [ 1 ]
//  F = 1/16 * [ 2, 4, 2 ] = 1/16 * [ 2 ] * [ 1, 2, 1 ]
//             [ 1, 2, 1 ]          [ 1 ]
template <>
class DiscreteGaussianBlur<uint8_t, 3> {
 public:
  using SourceType = uint8_t;
  using BufferType = uint16_t;
  using DestinationType = uint8_t;

  // Applies vertical filtering vector using SIMD operations.
  //
  // DST = [ SRC0, SRC1, SRC2 ] * [ 1, 2, 1 ]T
  void vertical_vector_path(svbool_t pg, svuint8_t src_0, svuint8_t src_1,
                            svuint8_t src_2, BufferType *dst) const
      KLEIDICV_STREAMING_COMPATIBLE {
    svuint16_t acc_0_2_b = svaddlb_u16(src_0, src_2);
    svuint16_t acc_0_2_t = svaddlt_u16(src_0, src_2);

    svuint16_t acc_1_b = svshllb_n_u16(src_1, 1);
    svuint16_t acc_1_t = svshllt_n_u16(src_1, 1);

    svuint16_t acc_u16_b = svadd_u16_x(pg, acc_0_2_b, acc_1_b);
    svuint16_t acc_u16_t = svadd_u16_x(pg, acc_0_2_t, acc_1_t);

    svuint16x2_t interleaved = svcreate2(acc_u16_b, acc_u16_t);
    svst2(pg, &dst[0], interleaved);
  }

  // Applies horizontal filtering vector using SIMD operations.
  //
  // DST = 1/16 * [ SRC0, SRC1, SRC2 ] * [ 1, 2, 1 ]T
  void horizontal_vector_path(svbool_t pg, svuint16_t src_0, svuint16_t src_1,
                              svuint16_t src_2, DestinationType *dst) const
      KLEIDICV_STREAMING_COMPATIBLE {
    svuint16_t acc_0_2 = svhadd_u16_x(pg, src_0, src_2);

    svuint16_t acc = svadd_u16_x(pg, acc_0_2, src_1);
    acc = svrshr_x(pg, acc, 3);

    svst1b(pg, &dst[0], acc);
  }

  // Applies horizontal filtering vector using scalar operations.
  //
  // DST = 1/16 * [ SRC0, SRC1, SRC2 ] * [ 1, 2, 1 ]T
  void horizontal_scalar_path(const BufferType src[3], DestinationType *dst)
      const KLEIDICV_STREAMING_COMPATIBLE {
    auto acc = src[0] + 2 * src[1] + src[2];
    dst[0] = rounding_shift_right(acc, 4);
  }
};  // end of class DiscreteGaussianBlur<uint8_t, 3>

// Template for 5x5 Gaussian Blur approximation filters.
//
//              [ 1,  4,  6,  4, 1 ]           [ 1 ]
//              [ 4, 16, 24, 16, 4 ]           [ 4 ]
//  F = 1/256 * [ 6, 24, 36, 24, 6 ] = 1/256 * [ 6 ] * [ 1,  4,  6,  4, 1 ]
//              [ 4, 16, 24, 16, 4 ]           [ 4 ]
//              [ 1,  4,  6,  4, 1 ]           [ 1 ]
template <>
class DiscreteGaussianBlur<uint8_t, 5> {
 public:
  using SourceType = uint8_t;
  using BufferType = uint16_t;
  using DestinationType = uint8_t;

  // Applies vertical filtering vector using SIMD operations.
  //
  // DST = [ SRC0, SRC1, SRC2, SRC3, SRC4 ] * [ 1, 4, 6, 4, 1 ]T
  void vertical_vector_path(svbool_t pg, svuint8_t src_0, svuint8_t src_1,
                            svuint8_t src_2, svuint8_t src_3, svuint8_t src_4,
                            BufferType *dst) const
      KLEIDICV_STREAMING_COMPATIBLE {
    svuint8_t const_6_u8 = svdup_n_u8(6);
    svuint16_t const_4_u16 = svdup_n_u16(4);

    svuint16_t acc_0_4_b = svaddlb_u16(src_0, src_4);
    svuint16_t acc_0_4_t = svaddlt_u16(src_0, src_4);
    svuint16_t acc_1_3_b = svaddlb_u16(src_1, src_3);
    svuint16_t acc_1_3_t = svaddlt_u16(src_1, src_3);

    svuint16_t acc_u16_b = svmlalb_u16(acc_0_4_b, src_2, const_6_u8);
    svuint16_t acc_u16_t = svmlalt_u16(acc_0_4_t, src_2, const_6_u8);
    acc_u16_b = svmad_u16_x(pg, acc_1_3_b, const_4_u16, acc_u16_b);
    acc_u16_t = svmad_u16_x(pg, acc_1_3_t, const_4_u16, acc_u16_t);

    svuint16x2_t interleaved = svcreate2(acc_u16_b, acc_u16_t);
    svst2(pg, &dst[0], interleaved);
  }

  // Applies horizontal filtering vector using SIMD operations.
  //
  // DST = 1/256 * [ SRC0, SRC1, SRC2, SRC3, SRC4 ] * [ 1, 4, 6, 4, 1 ]T
  void horizontal_vector_path(svbool_t pg, svuint16_t src_0, svuint16_t src_1,
                              svuint16_t src_2, svuint16_t src_3,
                              svuint16_t src_4, DestinationType *dst) const
      KLEIDICV_STREAMING_COMPATIBLE {
    svuint16_t const_4_u16 = svdup_n_u16(4);
    svuint16_t const_6_u16 = svdup_n_u16(6);

    svuint16_t acc_0_4 = svadd_x(pg, src_0, src_4);
    svuint16_t acc_1_3 = svadd_x(pg, src_1, src_3);
    svuint16_t acc = svmad_u16_x(pg, src_2, const_6_u16, acc_0_4);
    acc = svmad_u16_x(pg, acc_1_3, const_4_u16, acc);
    acc = svrshr_x(pg, acc, 8);
    svst1b(pg, &dst[0], acc);
  }

  // Applies horizontal filtering vector using scalar operations.
  //
  // DST = 1/256 * [ SRC0, SRC1, SRC2, SRC3, SRC4 ] * [ 1, 4, 6, 4, 1 ]T
  void horizontal_scalar_path(const BufferType src[5], DestinationType *dst)
      const KLEIDICV_STREAMING_COMPATIBLE {
    auto acc = src[0] + src[4] + 4 * (src[1] + src[3]) + 6 * src[2];
    dst[0] = rounding_shift_right(acc, 8);
  }
};  // end of class DiscreteGaussianBlur<uint8_t, 5>

// Template for 7x7 Gaussian Blur approximation filters.
//
//               [  4,  14,  28,  36,  28,  14,  4 ]
//               [ 14,  49,  98, 126,  98,  49, 14 ]
//               [ 28,  98, 196, 252, 196,  98, 28 ]
//  F = 1/4096 * [ 36, 126, 252, 324, 252, 126, 36 ] =
//               [ 28,  98, 196, 252, 196,  98, 28 ]
//               [ 14,  49,  98, 126,  98,  49, 14 ]
//               [  4,  14,  28,  36,  28,  14,  4 ]
//
//               [  2 ]
//               [  7 ]
//               [ 14 ]
//  = 1/4096  *  [ 18 ] * [ 2, 7, 14, 18, 14, 7, 2 ]
//               [ 14 ]
//               [  7 ]
//               [  2 ]
template <>
class DiscreteGaussianBlur<uint8_t, 7> {
 public:
  using SourceType = uint8_t;
  using BufferType = uint16_t;
  using DestinationType = uint8_t;

  // Applies vertical filtering vector using SIMD operations.
  //
  // DST = [ SRC0, SRC1, SRC2, SRC3, SRC4, SRC5, SRC6 ] *
  //     * [ 2, 7, 14, 18, 14, 7, 2 ]T
  void vertical_vector_path(
      svbool_t pg, svuint8_t src_0, svuint8_t src_1, svuint8_t src_2,
      svuint8_t src_3, svuint8_t src_4, svuint8_t src_5, svuint8_t src_6,
      BufferType *dst) const KLEIDICV_STREAMING_COMPATIBLE {
    svuint16_t const_7_u16 = svdup_n_u16(7);
    svuint16_t const_9_u16 = svdup_n_u16(9);

    svuint16_t acc_0_6_b = svaddlb_u16(src_0, src_6);
    svuint16_t acc_0_6_t = svaddlt_u16(src_0, src_6);

    svuint16_t acc_1_5_b = svaddlb_u16(src_1, src_5);
    svuint16_t acc_1_5_t = svaddlt_u16(src_1, src_5);

    svuint16_t acc_2_4_b = svaddlb_u16(src_2, src_4);
    svuint16_t acc_2_4_t = svaddlt_u16(src_2, src_4);

    svuint16_t acc_3_b = svmovlb_u16(src_3);
    svuint16_t acc_3_t = svmovlt_u16(src_3);

    svuint16_t acc_0_2_4_6_b =
        svmla_u16_x(pg, acc_0_6_b, acc_2_4_b, const_7_u16);
    svuint16_t acc_0_2_4_6_t =
        svmla_u16_x(pg, acc_0_6_t, acc_2_4_t, const_7_u16);

    svuint16_t acc_0_2_3_4_6_b =
        svmla_u16_x(pg, acc_0_2_4_6_b, acc_3_b, const_9_u16);
    svuint16_t acc_0_2_3_4_6_t =
        svmla_u16_x(pg, acc_0_2_4_6_t, acc_3_t, const_9_u16);
    acc_0_2_3_4_6_b = svlsl_n_u16_x(pg, acc_0_2_3_4_6_b, 1);
    acc_0_2_3_4_6_t = svlsl_n_u16_x(pg, acc_0_2_3_4_6_t, 1);

    svuint16_t acc_0_1_2_3_4_5_6_b =
        svmla_u16_x(pg, acc_0_2_3_4_6_b, acc_1_5_b, const_7_u16);
    svuint16_t acc_0_1_2_3_4_5_6_t =
        svmla_u16_x(pg, acc_0_2_3_4_6_t, acc_1_5_t, const_7_u16);

    svuint16x2_t interleaved =
        svcreate2(acc_0_1_2_3_4_5_6_b, acc_0_1_2_3_4_5_6_t);
    svst2(pg, &dst[0], interleaved);
  }

  // Applies horizontal filtering vector using SIMD operations.
  //
  // DST = 1/4096 * [ SRC0, SRC1, SRC2, SRC3, SRC4, SRC5, SRC6 ] *
  //              * [ 2, 7, 14, 18, 14, 7, 2 ]T
  void horizontal_vector_path(
      svbool_t pg, svuint16_t src_0, svuint16_t src_1, svuint16_t src_2,
      svuint16_t src_3, svuint16_t src_4, svuint16_t src_5, svuint16_t src_6,
      DestinationType *dst) const KLEIDICV_STREAMING_COMPATIBLE {
    svuint16_t const_7_u16 = svdup_n_u16(7);
    svuint16_t const_9_u16 = svdup_n_u16(9);
    svuint32_t const_7_u32 = svdup_n_u32(7);

    svuint32_t acc_0_6_b = svaddlb_u32(src_0, src_6);
    svuint32_t acc_0_6_t = svaddlt_u32(src_0, src_6);

    svuint32_t acc_1_5_b = svaddlb_u32(src_1, src_5);
    svuint32_t acc_1_5_t = svaddlt_u32(src_1, src_5);

    svuint16_t acc_2_4 = svadd_u16_x(pg, src_2, src_4);

    svuint32_t acc_0_2_4_6_b = svmlalb_u32(acc_0_6_b, acc_2_4, const_7_u16);
    svuint32_t acc_0_2_4_6_t = svmlalt_u32(acc_0_6_t, acc_2_4, const_7_u16);

    svuint32_t acc_0_2_3_4_6_b = svmlalb_u32(acc_0_2_4_6_b, src_3, const_9_u16);
    svuint32_t acc_0_2_3_4_6_t = svmlalt_u32(acc_0_2_4_6_t, src_3, const_9_u16);

    acc_0_2_3_4_6_b = svlsl_n_u32_x(pg, acc_0_2_3_4_6_b, 1);
    acc_0_2_3_4_6_t = svlsl_n_u32_x(pg, acc_0_2_3_4_6_t, 1);

    svuint32_t acc_0_1_2_3_4_5_6_b =
        svmla_u32_x(pg, acc_0_2_3_4_6_b, acc_1_5_b, const_7_u32);
    svuint32_t acc_0_1_2_3_4_5_6_t =
        svmla_u32_x(pg, acc_0_2_3_4_6_t, acc_1_5_t, const_7_u32);

    svuint16_t acc_0_1_2_3_4_5_6_u16_b =
        svrshrnb_n_u32(acc_0_1_2_3_4_5_6_b, 12);
    svuint16_t acc_0_1_2_3_4_5_6_u16 =
        svrshrnt_n_u32(acc_0_1_2_3_4_5_6_u16_b, acc_0_1_2_3_4_5_6_t, 12);

    svst1b(pg, &dst[0], acc_0_1_2_3_4_5_6_u16);
  }

  // Applies horizontal filtering vector using scalar operations.
  //
  // DST = 1/4096 * [ SRC0, SRC1, SRC2, SRC3, SRC4, SRC5, SRC6 ] *
  //              * [ 2, 7, 14, 18, 14, 7, 2 ]T
  void horizontal_scalar_path(const BufferType src[7], DestinationType *dst)
      const KLEIDICV_STREAMING_COMPATIBLE {
    uint32_t acc = src[0] * 2 + src[1] * 7 + src[2] * 14 + src[3] * 18 +
                   src[4] * 14 + src[5] * 7 + src[6] * 2;
    dst[0] = rounding_shift_right(acc, 12);
  }
};  // end of class DiscreteGaussianBlur<uint8_t, 7>

// Template for 15x15 Gaussian Blur approximation filters.
//
//                  [  16,   44,  100,  192 ...  192,  100,   44,  16 ]
//                  [  44,  121,  275,  528 ...  528,  275,  121,  44 ]
//                  [ 100,  275,  625, 1200 ... 1200,  625,  275, 100 ]
//                  [ 192,  528, 1200, 2304 ... 2304, 1200,  528, 192 ]
//  F = 1/1048576 * [  |     |     |     |  ...   |     |     |    |  ] =
//                  [ 192,  528, 1200, 2304 ... 2304, 1200,  528, 192 ]
//                  [ 100,  275,  625, 1200 ... 1200,  625,  275, 100 ]
//                  [  44,  121,  275,  528 ...  528,  275,  121,  44 ]
//                  [  16,   44,  100,  192 ...  192,  100,   44,  16 ]
//
//                  [   4 ]
//                  [  11 ]
//                  [  25 ]
//                  [  48 ]
//                  [  81 ]
//                  [ 118 ]
//                  [ 146 ]
//  = 1/1048576  *  [ 158 ] * [4,11,25,48,81,118,146,158,146,118,81,48,25,11,4]
//                  [ 146 ]
//                  [ 118 ]
//                  [  81 ]
//                  [  48 ]
//                  [  25 ]
//                  [  11 ]
//                  [   4 ]
template <>
class DiscreteGaussianBlur<uint8_t, 15> {
 public:
  using SourceType = uint8_t;
  using BufferType = uint32_t;
  using DestinationType = uint8_t;

  // Applies vertical filtering vector using SIMD operations.
  //
  // DST = [ SRC0, SRC1, SRC2, SRC3...SRC11, SRC12, SRC13, SRC14 ] *
  //     * [ 4, 11, 25, 48 ... 48, 25, 11, 4 ]T
  void vertical_vector_path(
      svbool_t pg, svuint8_t src_0, svuint8_t src_1, svuint8_t src_2,
      svuint8_t src_3, svuint8_t src_4, svuint8_t src_5, svuint8_t src_6,
      svuint8_t src_7, svuint8_t src_8, svuint8_t src_9, svuint8_t src_10,
      svuint8_t src_11, svuint8_t src_12, svuint8_t src_13, svuint8_t src_14,
      BufferType *dst) const KLEIDICV_STREAMING_COMPATIBLE {
    svuint16_t acc_7_b = svmovlb_u16(src_7);
    svuint16_t acc_7_t = svmovlt_u16(src_7);

    svuint16_t acc_1_13_b = svaddlb_u16(src_1, src_13);
    svuint16_t acc_1_13_t = svaddlt_u16(src_1, src_13);

    svuint16_t acc_2_12_b = svaddlb_u16(src_2, src_12);
    svuint16_t acc_2_12_t = svaddlt_u16(src_2, src_12);

    svuint16_t acc_6_8_b = svaddlb_u16(src_6, src_8);
    svuint16_t acc_6_8_t = svaddlt_u16(src_6, src_8);

    svuint16_t acc_5_9_b = svaddlb_u16(src_5, src_9);
    svuint16_t acc_5_9_t = svaddlt_u16(src_5, src_9);

    svuint16_t acc_0_14_b = svaddlb_u16(src_0, src_14);
    svuint16_t acc_0_14_t = svaddlt_u16(src_0, src_14);

    svuint16_t acc_3_11_b = svaddlb_u16(src_3, src_11);
    svuint16_t acc_3_11_t = svaddlt_u16(src_3, src_11);

    svuint16_t acc_4_10_b = svaddlb_u16(src_4, src_10);
    svuint16_t acc_4_10_t = svaddlt_u16(src_4, src_10);

    acc_0_14_b = svlsl_n_u16_x(pg, acc_0_14_b, 2);
    acc_0_14_t = svlsl_n_u16_x(pg, acc_0_14_t, 2);

    acc_3_11_b = svlsl_n_u16_x(pg, acc_3_11_b, 2);
    acc_3_11_t = svlsl_n_u16_x(pg, acc_3_11_t, 2);

    acc_4_10_b = svmul_n_u16_x(pg, acc_4_10_b, 81);
    acc_4_10_t = svmul_n_u16_x(pg, acc_4_10_t, 81);

    svuint16_t acc_1_3_11_13_b = svadd_u16_x(pg, acc_3_11_b, acc_1_13_b);
    svuint16_t acc_1_3_11_13_t = svadd_u16_x(pg, acc_3_11_t, acc_1_13_t);
    acc_1_3_11_13_b = svmla_n_u16_x(pg, acc_3_11_b, acc_1_3_11_13_b, 11);
    acc_1_3_11_13_t = svmla_n_u16_x(pg, acc_3_11_t, acc_1_3_11_13_t, 11);

    svuint16_t acc_0_1_3_11_13_14_b =
        svadd_u16_x(pg, acc_1_3_11_13_b, acc_0_14_b);
    svuint16_t acc_0_1_3_11_13_14_t =
        svadd_u16_x(pg, acc_1_3_11_13_t, acc_0_14_t);

    svuint16_t acc_2_4_10_12_b = svmla_n_u16_x(pg, acc_4_10_b, acc_2_12_b, 25);
    svuint16_t acc_2_4_10_12_t = svmla_n_u16_x(pg, acc_4_10_t, acc_2_12_t, 25);

    svuint32_t acc_b_b = svaddlb_u32(acc_2_4_10_12_b, acc_0_1_3_11_13_14_b);
    svuint32_t acc_b_t = svaddlb_u32(acc_2_4_10_12_t, acc_0_1_3_11_13_14_t);
    svuint32_t acc_t_b = svaddlt_u32(acc_2_4_10_12_b, acc_0_1_3_11_13_14_b);
    svuint32_t acc_t_t = svaddlt_u32(acc_2_4_10_12_t, acc_0_1_3_11_13_14_t);

    acc_b_b = svmlalb_n_u32(acc_b_b, acc_6_8_b, 146);
    acc_b_t = svmlalb_n_u32(acc_b_t, acc_6_8_t, 146);
    acc_t_b = svmlalt_n_u32(acc_t_b, acc_6_8_b, 146);
    acc_t_t = svmlalt_n_u32(acc_t_t, acc_6_8_t, 146);

    acc_b_b = svmlalb_n_u32(acc_b_b, acc_5_9_b, 118);
    acc_b_t = svmlalb_n_u32(acc_b_t, acc_5_9_t, 118);
    acc_t_b = svmlalt_n_u32(acc_t_b, acc_5_9_b, 118);
    acc_t_t = svmlalt_n_u32(acc_t_t, acc_5_9_t, 118);

    acc_b_b = svmlalb_n_u32(acc_b_b, acc_7_b, 158);
    acc_b_t = svmlalb_n_u32(acc_b_t, acc_7_t, 158);
    acc_t_b = svmlalt_n_u32(acc_t_b, acc_7_b, 158);
    acc_t_t = svmlalt_n_u32(acc_t_t, acc_7_t, 158);

    svuint32x4_t interleaved =
        svcreate4_u32(acc_b_b, acc_b_t, acc_t_b, acc_t_t);
    svst4_u32(pg, &dst[0], interleaved);
  }

  // Applies horizontal filtering vector using SIMD operations.
  //
  // DST = 1/1048576 * [ SRC0, SRC1, SRC2, SRC3...SRC11, SRC12, SRC13, SRC14 ] *
  //                 * [ 4, 11, 25, 48 ... 48, 25, 11, 4 ]T
  void horizontal_vector_path(
      svbool_t pg, svuint32_t src_0, svuint32_t src_1, svuint32_t src_2,
      svuint32_t src_3, svuint32_t src_4, svuint32_t src_5, svuint32_t src_6,
      svuint32_t src_7, svuint32_t src_8, svuint32_t src_9, svuint32_t src_10,
      svuint32_t src_11, svuint32_t src_12, svuint32_t src_13,
      svuint32_t src_14,
      DestinationType *dst) const KLEIDICV_STREAMING_COMPATIBLE {
    svuint32_t acc_1_13 = svadd_u32_x(pg, src_1, src_13);
    svuint32_t acc_2_12 = svadd_u32_x(pg, src_2, src_12);
    svuint32_t acc_6_8 = svadd_u32_x(pg, src_6, src_8);
    svuint32_t acc_5_9 = svadd_u32_x(pg, src_5, src_9);
    svuint32_t acc_0_14 = svadd_u32_x(pg, src_0, src_14);
    svuint32_t acc_3_11 = svadd_u32_x(pg, src_3, src_11);
    svuint32_t acc_4_10 = svadd_u32_x(pg, src_4, src_10);

    acc_0_14 = svlsl_n_u32_x(pg, acc_0_14, 2);
    acc_3_11 = svlsl_n_u32_x(pg, acc_3_11, 2);
    acc_4_10 = svmul_n_u32_x(pg, acc_4_10, 81);

    svuint32_t acc_1_3_11_13 = svadd_u32_x(pg, acc_3_11, acc_1_13);
    acc_1_3_11_13 = svmla_n_u32_x(pg, acc_3_11, acc_1_3_11_13, 11);
    svuint32_t acc_0_1_3_11_13_14 = svadd_u32_x(pg, acc_1_3_11_13, acc_0_14);
    svuint32_t acc_2_4_10_12 = svmla_n_u32_x(pg, acc_4_10, acc_2_12, 25);

    svuint32_t acc = svadd_u32_x(pg, acc_2_4_10_12, acc_0_1_3_11_13_14);
    acc = svmla_n_u32_x(pg, acc, acc_6_8, 146);
    acc = svmla_n_u32_x(pg, acc, acc_5_9, 118);
    acc = svmla_n_u32_x(pg, acc, src_7, 158);
    acc = svrshr_n_u32_x(pg, acc, 20);
    svst1b_u32(pg, &dst[0], acc);
  }

  // Applies horizontal filtering vector using scalar operations.
  //
  // DST = 1/1048576 * [ SRC0, SRC1, SRC2, SRC3...SRC11, SRC12, SRC13, SRC14 ] *
  //                 * [ 4, 11, 25, 48 ... 48, 25, 11, 4 ]T
  void horizontal_scalar_path(const BufferType src[15], DestinationType *dst)
      const KLEIDICV_STREAMING_COMPATIBLE {
    uint32_t acc = (static_cast<uint32_t>(src[3]) + src[11]) * 4;
    acc += (acc + src[1] + src[13]) * 11;
    acc += (src[0] + src[14]) * 4 + (src[2] + src[12]) * 25 +
           (src[4] + src[10]) * 81;
    acc += (src[5] + src[9]) * 118 + (src[6] + src[8]) * 146 + src[7] * 158;
    dst[0] = rounding_shift_right(acc, 20);
  }
};  // end of class DiscreteGaussianBlur<uint8_t, 15>

template <typename ScalarType, size_t KernelSize>
kleidicv_error_t discrete_gaussian_blur(
    const ScalarType *src, size_t src_stride, ScalarType *dst,
    size_t dst_stride, size_t width, size_t height, size_t channels,
    kleidicv_border_type_t border_type,
    kleidicv_filter_context_t *context) KLEIDICV_STREAMING_COMPATIBLE {
  CHECK_POINTERS(context);
  CHECK_POINTER_AND_STRIDE(src, src_stride, height);
  CHECK_POINTER_AND_STRIDE(dst, dst_stride, height);
  CHECK_IMAGE_SIZE(width, height);

  if (width < KernelSize - 1 || height < KernelSize - 1) {
    return KLEIDICV_ERROR_NOT_IMPLEMENTED;
  }

  if (channels > KLEIDICV_MAXIMUM_CHANNEL_COUNT) {
    return KLEIDICV_ERROR_RANGE;
  }

  auto *workspace = reinterpret_cast<SeparableFilterWorkspace *>(context);

  if constexpr (KernelSize == 15) {
    if (workspace->intermediate_size() < sizeof(uint32_t)) {
      return KLEIDICV_ERROR_CONTEXT_MISMATCH;
    }
  }

  if (workspace->channels() < channels) {
    return KLEIDICV_ERROR_CONTEXT_MISMATCH;
  }

  Rectangle rect{width, height};
  const Rectangle &context_rect = workspace->image_size();
  if (context_rect.width() < width || context_rect.height() < height) {
    return KLEIDICV_ERROR_CONTEXT_MISMATCH;
  }

  auto fixed_border_type = get_fixed_border_type(border_type);

  if (!fixed_border_type) {
    return KLEIDICV_ERROR_NOT_IMPLEMENTED;
  }

  using GaussianBlurFilterType = DiscreteGaussianBlur<ScalarType, KernelSize>;

  GaussianBlurFilterType blur;
  SeparableFilter<GaussianBlurFilterType, KernelSize> filter{blur};

  Rows<const ScalarType> src_rows{src, src_stride, channels};
  Rows<ScalarType> dst_rows{dst, dst_stride, channels};
  workspace->process(rect, src_rows, dst_rows, channels, *fixed_border_type,
                     filter);
  return KLEIDICV_OK;
}

#define KLEIDICV_GAUSSIAN_BLUR_WRAPPER(size, ...)              \
  if (kernel_width == size) {                                  \
    return discrete_gaussian_blur<uint8_t, size>(__VA_ARGS__); \
  }

#define KLEIDICV_GENERATE_GAUSSIAN_BLUR_WRAPPERS(...) \
  KLEIDICV_GAUSSIAN_BLUR_WRAPPER(3, __VA_ARGS__)      \
  KLEIDICV_GAUSSIAN_BLUR_WRAPPER(5, __VA_ARGS__)      \
  KLEIDICV_GAUSSIAN_BLUR_WRAPPER(7, __VA_ARGS__)      \
  KLEIDICV_GAUSSIAN_BLUR_WRAPPER(15, __VA_ARGS__)

kleidicv_error_t gaussian_blur_u8_entry(
    const uint8_t *src, size_t src_stride, uint8_t *dst, size_t dst_stride,
    size_t width, size_t height, size_t channels, size_t kernel_width,
    size_t kernel_height, float sigma_x, float sigma_y,
    kleidicv_border_type_t border_type,
    kleidicv_filter_context_t *context) KLEIDICV_STREAMING_COMPATIBLE {
  if (kernel_width != kernel_height) {
    return KLEIDICV_ERROR_NOT_IMPLEMENTED;
  }

  if (sigma_x != 0.0 || sigma_y != 0.0) {
    return KLEIDICV_ERROR_NOT_IMPLEMENTED;
  }

  KLEIDICV_GENERATE_GAUSSIAN_BLUR_WRAPPERS(src, src_stride, dst, dst_stride,
                                           width, height, channels, border_type,
                                           context)

  return KLEIDICV_ERROR_NOT_IMPLEMENTED;
}

}  // namespace KLEIDICV_TARGET_NAMESPACE

#endif  // KLEIDICV_GAUSSIAN_BLUR_SC_H
