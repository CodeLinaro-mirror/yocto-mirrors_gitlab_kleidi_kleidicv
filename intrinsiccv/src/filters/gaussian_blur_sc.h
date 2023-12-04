// SPDX-FileCopyrightText: 2023 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#ifndef INTRINSICCV_GAUSSIAN_BLUR_SC_H
#define INTRINSICCV_GAUSSIAN_BLUR_SC_H

#include <limits>

#include "intrinsiccv.h"
#include "sve2.h"

namespace intrinsiccv::sve2 {

// Primary template for Gaussian Blur approximation filters.
template <typename ScalarType, size_t KernelSize>
class DiscreteGaussianBlur;

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
      INTRINSICCV_STREAMING_COMPATIBLE {
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

    svuint16x2_t interleaved;
    interleaved = svset2(interleaved, 0, acc_u16_b);
    interleaved = svset2(interleaved, 1, acc_u16_t);
    svst2(pg, &dst[0], interleaved);
  }

  // Applies horizontal filtering vector using SIMD operations.
  //
  // DST = 1/256 * [ SRC0, SRC1, SRC2, SRC3, SRC4 ] * [ 1, 4, 6, 4, 1 ]T
  void horizontal_vector_path(svbool_t pg, svuint16_t src_0, svuint16_t src_1,
                              svuint16_t src_2, svuint16_t src_3,
                              svuint16_t src_4, DestinationType *dst) const
      INTRINSICCV_STREAMING_COMPATIBLE {
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
  void horizontal_scalar_path(BufferType src[5], DestinationType *dst) const
      INTRINSICCV_STREAMING_COMPATIBLE {
    auto acc = src[0] + src[4] + 4 * (src[1] + src[3]) + 6 * src[2];
    dst[0] = rounding_shift_right(acc, 8);
  }
};  // end of class DiscreteGaussianBlur<uint8_t, 5>

template <typename ScalarType, size_t KernelSize>
void discrete_gaussian_blur(const ScalarType *src, size_t src_stride,
                            ScalarType *dst, size_t dst_stride, size_t width,
                            size_t height, size_t channels,
                            intrinsiccv_border_type_t border_type,
                            const intrinsiccv_filter_params_t *params)
    INTRINSICCV_STREAMING_COMPATIBLE {
  using GaussianBlurFilterType = DiscreteGaussianBlur<ScalarType, KernelSize>;

  Rectangle rect{width, height};
  Rows<const ScalarType> src_rows{src, src_stride, channels};
  Rows<ScalarType> dst_rows{dst, dst_stride, channels};

  auto workspace =
      reinterpret_cast<SeparableFilterWorkspace *>(params->workspace);

  GaussianBlurFilterType blur;
  SeparableFilter<GaussianBlurFilterType, KernelSize> filter{blur};
  workspace->process(rect, src_rows, dst_rows, channels, border_type, filter);
}

}  // namespace intrinsiccv::sve2

#endif  // INTRINSICCV_GAUSSIAN_BLUR_SC_H
