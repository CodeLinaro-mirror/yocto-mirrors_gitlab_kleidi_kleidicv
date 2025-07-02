// SPDX-FileCopyrightText: 2023 - 2025 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#ifndef KLEIDICV_GAUSSIAN_BLUR_SC_H
#define KLEIDICV_GAUSSIAN_BLUR_SC_H

#include <cassert>
#include <cstddef>

#include "border_generic_sc.h"
#include "kleidicv/filters/gaussian_blur.h"
#include "kleidicv/filters/separable_filter_15x15_sc.h"
#include "kleidicv/filters/separable_filter_21x21_sc.h"
#include "kleidicv/filters/separable_filter_3x3_sc.h"
#include "kleidicv/filters/separable_filter_5x5_sc.h"
#include "kleidicv/filters/separable_filter_7x7_sc.h"
#include "kleidicv/filters/sigma.h"
#include "kleidicv/workspace/separable.h"

namespace KLEIDICV_TARGET_NAMESPACE {

// Primary template for Gaussian Blur filters.
template <typename ScalarType, size_t KernelSize, bool IsBinomial>
class GaussianBlur;

// Template for 3x3 Gaussian Blur binomial filters.
//
//             [ 1, 2, 1 ]          [ 1 ]
//  F = 1/16 * [ 2, 4, 2 ] = 1/16 * [ 2 ] * [ 1, 2, 1 ]
//             [ 1, 2, 1 ]          [ 1 ]
template <>
class GaussianBlur<uint8_t, 3, true> {
 public:
  using SourceType = uint8_t;
  using BufferType = uint16_t;
  using DestinationType = uint8_t;

  // Applies vertical filtering vector using SIMD operations.
  //
  // DST = [ SRC0, SRC1, SRC2 ] * [ 1, 2, 1 ]T
  void vertical_vector_path(
      svbool_t pg, std::reference_wrapper<svuint8_t> src[3],
      BufferType *dst) const KLEIDICV_STREAMING_COMPATIBLE {
    svuint16_t acc_0_2_b = svaddlb_u16(src[0], src[2]);
    svuint16_t acc_0_2_t = svaddlt_u16(src[0], src[2]);

    svuint16_t acc_1_b = svshllb_n_u16(src[1], 1);
    svuint16_t acc_1_t = svshllt_n_u16(src[1], 1);

    svuint16_t acc_u16_b = svadd_u16_x(pg, acc_0_2_b, acc_1_b);
    svuint16_t acc_u16_t = svadd_u16_x(pg, acc_0_2_t, acc_1_t);

    svuint16x2_t interleaved = svcreate2(acc_u16_b, acc_u16_t);
    svst2(pg, &dst[0], interleaved);
  }

  // Applies horizontal filtering vector using SIMD operations.
  //
  // DST = 1/16 * [ SRC0, SRC1, SRC2 ] * [ 1, 2, 1 ]T
  void horizontal_vector_path(
      svbool_t pg, std::reference_wrapper<svuint16_t> src[3],
      DestinationType *dst) const KLEIDICV_STREAMING_COMPATIBLE {
    svuint16_t acc_0_2 = svhadd_u16_x(pg, src[0], src[2]);

    svuint16_t acc = svadd_u16_x(pg, acc_0_2, src[1]);
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
};  // end of class GaussianBlur<uint8_t, 3, true>

// Template for 5x5 Gaussian Blur binomial filters.
//
//              [ 1,  4,  6,  4, 1 ]           [ 1 ]
//              [ 4, 16, 24, 16, 4 ]           [ 4 ]
//  F = 1/256 * [ 6, 24, 36, 24, 6 ] = 1/256 * [ 6 ] * [ 1,  4,  6,  4, 1 ]
//              [ 4, 16, 24, 16, 4 ]           [ 4 ]
//              [ 1,  4,  6,  4, 1 ]           [ 1 ]
template <>
class GaussianBlur<uint8_t, 5, true> {
 public:
  using SourceType = uint8_t;
  using BufferType = uint16_t;
  using DestinationType = uint8_t;

  // Applies vertical filtering vector using SIMD operations.
  //
  // DST = [ SRC0, SRC1, SRC2, SRC3, SRC4 ] * [ 1, 4, 6, 4, 1 ]T
  void vertical_vector_path(
      svbool_t pg, std::reference_wrapper<svuint8_t> src[5],
      BufferType *dst) const KLEIDICV_STREAMING_COMPATIBLE {
    svuint16_t acc_0_4_b = svaddlb_u16(src[0], src[4]);
    svuint16_t acc_0_4_t = svaddlt_u16(src[0], src[4]);
    svuint16_t acc_1_3_b = svaddlb_u16(src[1], src[3]);
    svuint16_t acc_1_3_t = svaddlt_u16(src[1], src[3]);

    svuint16_t acc_u16_b = svmlalb_n_u16(acc_0_4_b, src[2], 6);
    svuint16_t acc_u16_t = svmlalt_n_u16(acc_0_4_t, src[2], 6);
    acc_u16_b = svmla_n_u16_x(pg, acc_u16_b, acc_1_3_b, 4);
    acc_u16_t = svmla_n_u16_x(pg, acc_u16_t, acc_1_3_t, 4);

    svuint16x2_t interleaved = svcreate2(acc_u16_b, acc_u16_t);
    svst2(pg, &dst[0], interleaved);
  }

  // Applies horizontal filtering vector using SIMD operations.
  //
  // DST = 1/256 * [ SRC0, SRC1, SRC2, SRC3, SRC4 ] * [ 1, 4, 6, 4, 1 ]T
  void horizontal_vector_path(
      svbool_t pg, std::reference_wrapper<svuint16_t> src[5],
      DestinationType *dst) const KLEIDICV_STREAMING_COMPATIBLE {
    svuint16_t acc_0_4 = svadd_x(pg, src[0], src[4]);
    svuint16_t acc_1_3 = svadd_x(pg, src[1], src[3]);
    svuint16_t acc = svmla_n_u16_x(pg, acc_0_4, src[2], 6);
    acc = svmla_n_u16_x(pg, acc, acc_1_3, 4);
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
};  // end of class GaussianBlur<uint8_t, 5, true>

// Template for 7x7 Gaussian Blur binomial filters.
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
class GaussianBlur<uint8_t, 7, true> {
 public:
  using SourceType = uint8_t;
  using BufferType = uint16_t;
  using DestinationType = uint8_t;

  // Applies vertical filtering vector using SIMD operations.
  //
  // DST = [ SRC0, SRC1, SRC2, SRC3, SRC4, SRC5, SRC6 ] *
  //     * [ 2, 7, 14, 18, 14, 7, 2 ]T
  void vertical_vector_path(
      svbool_t pg, std::reference_wrapper<svuint8_t> src[7],
      BufferType *dst) const KLEIDICV_STREAMING_COMPATIBLE {
    svuint16_t acc_0_6_b = svaddlb_u16(src[0], src[6]);
    svuint16_t acc_0_6_t = svaddlt_u16(src[0], src[6]);

    svuint16_t acc_1_5_b = svaddlb_u16(src[1], src[5]);
    svuint16_t acc_1_5_t = svaddlt_u16(src[1], src[5]);

    svuint16_t acc_2_4_b = svaddlb_u16(src[2], src[4]);
    svuint16_t acc_2_4_t = svaddlt_u16(src[2], src[4]);

    svuint16_t acc_3_b = svmovlb_u16(src[3]);
    svuint16_t acc_3_t = svmovlt_u16(src[3]);

    svuint16_t acc_0_2_4_6_b = svmla_n_u16_x(pg, acc_0_6_b, acc_2_4_b, 7);
    svuint16_t acc_0_2_4_6_t = svmla_n_u16_x(pg, acc_0_6_t, acc_2_4_t, 7);

    svuint16_t acc_0_2_3_4_6_b = svmla_n_u16_x(pg, acc_0_2_4_6_b, acc_3_b, 9);
    svuint16_t acc_0_2_3_4_6_t = svmla_n_u16_x(pg, acc_0_2_4_6_t, acc_3_t, 9);
    acc_0_2_3_4_6_b = svlsl_n_u16_x(pg, acc_0_2_3_4_6_b, 1);
    acc_0_2_3_4_6_t = svlsl_n_u16_x(pg, acc_0_2_3_4_6_t, 1);

    svuint16_t acc_0_1_2_3_4_5_6_b =
        svmla_n_u16_x(pg, acc_0_2_3_4_6_b, acc_1_5_b, 7);
    svuint16_t acc_0_1_2_3_4_5_6_t =
        svmla_n_u16_x(pg, acc_0_2_3_4_6_t, acc_1_5_t, 7);

    svuint16x2_t interleaved =
        svcreate2(acc_0_1_2_3_4_5_6_b, acc_0_1_2_3_4_5_6_t);
    svst2(pg, &dst[0], interleaved);
  }

  // Applies horizontal filtering vector using SIMD operations.
  //
  // DST = 1/4096 * [ SRC0, SRC1, SRC2, SRC3, SRC4, SRC5, SRC6 ] *
  //              * [ 2, 7, 14, 18, 14, 7, 2 ]T
  void horizontal_vector_path(
      svbool_t pg, std::reference_wrapper<svuint16_t> src[7],
      DestinationType *dst) const KLEIDICV_STREAMING_COMPATIBLE {
    svuint32_t acc_0_6_b = svaddlb_u32(src[0], src[6]);
    svuint32_t acc_0_6_t = svaddlt_u32(src[0], src[6]);

    svuint32_t acc_1_5_b = svaddlb_u32(src[1], src[5]);
    svuint32_t acc_1_5_t = svaddlt_u32(src[1], src[5]);

    svuint16_t acc_2_4 = svadd_u16_x(pg, src[2], src[4]);

    svuint32_t acc_0_2_4_6_b = svmlalb_n_u32(acc_0_6_b, acc_2_4, 7);
    svuint32_t acc_0_2_4_6_t = svmlalt_n_u32(acc_0_6_t, acc_2_4, 7);

    svuint32_t acc_0_2_3_4_6_b = svmlalb_n_u32(acc_0_2_4_6_b, src[3], 9);
    svuint32_t acc_0_2_3_4_6_t = svmlalt_n_u32(acc_0_2_4_6_t, src[3], 9);

    acc_0_2_3_4_6_b = svlsl_n_u32_x(pg, acc_0_2_3_4_6_b, 1);
    acc_0_2_3_4_6_t = svlsl_n_u32_x(pg, acc_0_2_3_4_6_t, 1);

    svuint32_t acc_0_1_2_3_4_5_6_b =
        svmla_n_u32_x(pg, acc_0_2_3_4_6_b, acc_1_5_b, 7);
    svuint32_t acc_0_1_2_3_4_5_6_t =
        svmla_n_u32_x(pg, acc_0_2_3_4_6_t, acc_1_5_t, 7);

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
};  // end of class GaussianBlur<uint8_t, 7, true>

// CustomSigma variant
template <size_t KernelSize>
class GaussianBlur<uint8_t, KernelSize, false> {
 public:
  using SourceType = uint8_t;
  using BufferType = uint8_t;
  using DestinationType = uint8_t;
  using SourceVecTraits =
      typename ::KLEIDICV_TARGET_NAMESPACE::VecTraits<SourceType>;
  using SourceVectorType = typename SourceVecTraits::VectorType;

  static constexpr size_t kHalfKernelSize = get_half_kernel_size(KernelSize);

  explicit GaussianBlur(const uint16_t *half_kernel)
      : half_kernel_(half_kernel) {}

  void vertical_vector_path(
      svbool_t pg, std::reference_wrapper<SourceVectorType> src[KernelSize],
      BufferType *dst) const KLEIDICV_STREAMING_COMPATIBLE {
    common_vector_path(pg, src, dst);
  }

  void vertical_scalar_path(const SourceType src[KernelSize], BufferType *dst)
      const KLEIDICV_STREAMING_COMPATIBLE {
    uint32_t acc = static_cast<uint32_t>(src[kHalfKernelSize - 1]) *
                   half_kernel_[kHalfKernelSize - 1];

    // Optimization to avoid unnecessary branching in vector code.
    KLEIDICV_FORCE_LOOP_UNROLL
    for (size_t i = 0; i < kHalfKernelSize - 1; i++) {
      acc += (static_cast<uint32_t>(src[i]) +
              static_cast<uint32_t>(src[KernelSize - i - 1])) *
             half_kernel_[i];
    }

    dst[0] = static_cast<BufferType>(rounding_shift_right(acc, 8));
  }

  void horizontal_vector_path(
      svbool_t pg, std::reference_wrapper<SourceVectorType> src[KernelSize],
      BufferType *dst) const KLEIDICV_STREAMING_COMPATIBLE {
    common_vector_path(pg, src, dst);
  }

  void horizontal_scalar_path(const BufferType src[KernelSize],
                              DestinationType *dst) const
      KLEIDICV_STREAMING_COMPATIBLE {
    vertical_scalar_path(src, dst);
  }

 private:
  void common_vector_path(
      svbool_t pg, std::reference_wrapper<SourceVectorType> src[KernelSize],
      BufferType *dst) const KLEIDICV_STREAMING_COMPATIBLE {
    svbool_t pg16_all = svptrue_b16();
    svuint16_t acc_b = svmullb_n_u16(src[kHalfKernelSize - 1],
                                     half_kernel_[kHalfKernelSize - 1]);
    svuint16_t acc_t = svmullt_n_u16(src[kHalfKernelSize - 1],
                                     half_kernel_[kHalfKernelSize - 1]);

    // Optimization to avoid unnecessary branching in vector code.
    KLEIDICV_FORCE_LOOP_UNROLL
    for (size_t i = 0; i < kHalfKernelSize - 1; i++) {
      const size_t j = KernelSize - i - 1;
      svuint16_t vec_b = svaddlb_u16(src[i], src[j]);
      svuint16_t vec_t = svaddlt_u16(src[i], src[j]);

      acc_b = svmla_n_u16_x(pg16_all, acc_b, vec_b, half_kernel_[i]);
      acc_t = svmla_n_u16_x(pg16_all, acc_t, vec_t, half_kernel_[i]);
    }

    // Rounding before narrowing
    acc_b = svqadd_n_u16(acc_b, 128);
    acc_t = svqadd_n_u16(acc_t, 128);
    // Keep only the highest 8 bits
    svuint8_t result =
        svtrn2_u8(svreinterpret_u8_u16(acc_b), svreinterpret_u8_u16(acc_t));
    svst1(pg, &dst[0], result);
  }

  const uint16_t *half_kernel_;
};  // end of class GaussianBlur<uint8_t, KernelSize, false>

// Template for Generic kernel size Gaussian Blur filters.
template <typename ScalarType, FixedBorderType>
class GaussianBlurArbitrary;

template <FixedBorderType BorderT>
class GaussianBlurArbitrary<uint8_t, BorderT> {
 public:
  using SourceType = uint8_t;
  using BufferType = uint8_t;
  using DestinationType = uint8_t;
  using SourceVecTraits =
      typename KLEIDICV_TARGET_NAMESPACE::VecTraits<SourceType>;
  using SourceVectorType = typename SourceVecTraits::VectorType;
  using BufferVecTraits =
      typename KLEIDICV_TARGET_NAMESPACE::VecTraits<BufferType>;
  using BufferVectorType = typename BufferVecTraits::VectorType;
  using BorderType = FixedBorderType;

  GaussianBlurArbitrary(const uint16_t *half_kernel, ptrdiff_t half_kernel_size,
                        Rectangle &rect, size_t channels, svuint16_t &sv0,
                        svuint16_t &sv1, svuint16_t &sv2, svuint16_t &sv3,
                        svuint16_t &sv4, svuint16_t &sv5, svuint16_t &sv6,
                        svuint16_t &sv7) KLEIDICV_STREAMING_COMPATIBLE
      : half_kernel_size_(half_kernel_size),
        half_kernel_u16_(half_kernel),
        width_(static_cast<ptrdiff_t>(rect.width())),
        vertical_border_(rect.height(), channels, sv0, sv1, sv2, sv3),
        horizontal_border_(rect.width(), channels, sv4, sv5, sv6, sv7) {}

  // Not border-affected parts
  void process_arbitrary_vertical(size_t width, Rows<const SourceType> src_rows,
                                  Rows<BufferType> buffer_rows) const
      KLEIDICV_STREAMING_COMPATIBLE {
    LoopUnroll2 loop{width * src_rows.channels(), SourceVecTraits::num_lanes()};
    loop.unroll_once([&](size_t index) KLEIDICV_STREAMING_COMPATIBLE {
      vertical_vector_path(svptrue_b8(), src_rows, buffer_rows, index);
    });
    loop.remaining(
        [&](size_t index, size_t length) KLEIDICV_STREAMING_COMPATIBLE {
          svbool_t pg = SourceVecTraits::svwhilelt(index, length);
          vertical_vector_path(pg, src_rows, buffer_rows, index);
        });
  }

  // Border-affected parts
  void process_arbitrary_border_vertical(
      size_t width, Rows<const SourceType> src_rows, ptrdiff_t row_index,
      Rows<BufferType> buffer_rows) const KLEIDICV_STREAMING_COMPATIBLE {
    LoopUnroll2<TryToAvoidTailLoop> loop{width * src_rows.channels(),
                                         SourceVecTraits::num_lanes()};
    loop.unroll_once([&](size_t column_index) KLEIDICV_STREAMING_COMPATIBLE {
      vertical_border_vector_path(svptrue_b8(), src_rows, buffer_rows,
                                  row_index, column_index);
    });
    loop.remaining(
        [&](size_t column_index, size_t length) KLEIDICV_STREAMING_COMPATIBLE {
          svbool_t pg = SourceVecTraits::svwhilelt(column_index, length);
          vertical_border_vector_path(pg, src_rows, buffer_rows, row_index,
                                      column_index);
        });
  }

  void process_arbitrary_horizontal(
      size_t width, size_t kernel_size, Rows<BufferType> buffer_rows,
      Rows<DestinationType> dst_rows) KLEIDICV_STREAMING_COMPATIBLE {
    size_t x = 0;
    // Assume that there is always a widening when calculating, so the
    // horizontal vector path processes double-width vectors
    const size_t num_lanes = BufferVecTraits::num_lanes() / 2;
    const size_t block_len = num_lanes;
    const size_t margin = kernel_size / 2;
    const size_t border_len = buffer_rows.channels() * margin;
    const size_t border_process_len =
        ((border_len + block_len - 1) / block_len) * block_len;

    for (; x < border_process_len; x += num_lanes) {
      horizontal_left_border_vector_path(svptrue_b16(), buffer_rows, dst_rows,
                                         x);
    }

    // Process data which is not affected by any borders in bulk.
    if (width * buffer_rows.channels() > 2 * border_process_len) {
      size_t total_width_without_borders =
          width * buffer_rows.channels() - 2 * border_process_len;

      LoopUnroll2<TryToAvoidTailLoop> loop{total_width_without_borders,
                                           BufferVecTraits::num_lanes()};

      loop.unroll_twice([&](size_t index) KLEIDICV_STREAMING_COMPATIBLE {
        horizontal_vector_path(svptrue_b8(), buffer_rows, dst_rows, x + index);
        horizontal_vector_path(svptrue_b8(), buffer_rows, dst_rows,
                               x + index + BufferVecTraits::num_lanes());
      });

      loop.unroll_once([&](size_t index) KLEIDICV_STREAMING_COMPATIBLE {
        horizontal_vector_path(svptrue_b8(), buffer_rows, dst_rows, x + index);
      });

      loop.remaining(
          [&](size_t index, size_t length) KLEIDICV_STREAMING_COMPATIBLE {
            svbool_t pg = BufferVecTraits::svwhilelt(index, length);
            horizontal_vector_path(pg, buffer_rows, dst_rows, x + index);
          });

      x += total_width_without_borders;
    } else {
      // rewind if needed, so we'll have exact vector paths at the right side
      x = width * buffer_rows.channels() - border_process_len;
    }

    for (; x < width * buffer_rows.channels(); x += num_lanes) {
      horizontal_right_border_vector_path(svptrue_b16(), buffer_rows, dst_rows,
                                          x);
    }
  }

 private:
  void vertical_vector_path(svbool_t pg, Rows<const SourceType> src_rows,
                            Rows<BufferType> dst_rows,
                            ptrdiff_t x) const KLEIDICV_STREAMING_COMPATIBLE {
    svuint8_t src_mid = svld1_u8(pg, &src_rows[x]);
    svuint16_t acc_b =
        svmullb_n_u16(src_mid, half_kernel_u16_[half_kernel_size_ - 1]);
    svuint16_t acc_t =
        svmullt_n_u16(src_mid, half_kernel_u16_[half_kernel_size_ - 1]);

    ptrdiff_t i = 0, top = -half_kernel_size_ + 1,
              bottom = half_kernel_size_ - 1;
    svbool_t pg16_all = svptrue_b16();
    // Unroll 4 times
    for (; i < half_kernel_size_ - 4; i += 4) {
      svuint8_t src_i = svld1_u8(pg, &src_rows.at(top + i)[x]);
      svuint8_t src_j = svld1_u8(pg, &src_rows.at(bottom - i)[x]);
      svuint16_t vec_b = svaddlb_u16(src_i, src_j);
      svuint16_t vec_t = svaddlt_u16(src_i, src_j);
      svuint16_t prod0_b = svmul_n_u16_x(pg16_all, vec_b, half_kernel_u16_[i]);
      svuint16_t prod0_t = svmul_n_u16_x(pg16_all, vec_t, half_kernel_u16_[i]);

      src_i = svld1_u8(pg, &src_rows.at(top + i + 1)[x]);
      src_j = svld1_u8(pg, &src_rows.at(bottom - i - 1)[x]);
      vec_b = svaddlb_u16(src_i, src_j);
      vec_t = svaddlt_u16(src_i, src_j);
      svuint16_t prod1_b =
          svmul_n_u16_x(pg16_all, vec_b, half_kernel_u16_[i + 1]);
      svuint16_t prod1_t =
          svmul_n_u16_x(pg16_all, vec_t, half_kernel_u16_[i + 1]);

      src_i = svld1_u8(pg, &src_rows.at(top + i + 2)[x]);
      src_j = svld1_u8(pg, &src_rows.at(bottom - i - 2)[x]);
      vec_b = svaddlb_u16(src_i, src_j);
      vec_t = svaddlt_u16(src_i, src_j);
      svuint16_t prod2_b =
          svmul_n_u16_x(pg16_all, vec_b, half_kernel_u16_[i + 2]);
      svuint16_t prod2_t =
          svmul_n_u16_x(pg16_all, vec_t, half_kernel_u16_[i + 2]);

      src_i = svld1_u8(pg, &src_rows.at(top + i + 3)[x]);
      src_j = svld1_u8(pg, &src_rows.at(bottom - i - 3)[x]);
      vec_b = svaddlb_u16(src_i, src_j);
      vec_t = svaddlt_u16(src_i, src_j);
      svuint16_t prod3_b =
          svmul_n_u16_x(pg16_all, vec_b, half_kernel_u16_[i + 3]);
      svuint16_t prod3_t =
          svmul_n_u16_x(pg16_all, vec_t, half_kernel_u16_[i + 3]);

      svuint16_t acc0_b = svadd_u16_x(pg16_all, prod0_b, prod1_b);
      svuint16_t acc0_t = svadd_u16_x(pg16_all, prod0_t, prod1_t);
      svuint16_t acc1_b = svadd_u16_x(pg16_all, prod2_b, prod3_b);
      svuint16_t acc1_t = svadd_u16_x(pg16_all, prod2_t, prod3_t);

      svuint16_t acc_new_b = svadd_u16_x(pg16_all, acc0_b, acc1_b);
      svuint16_t acc_new_t = svadd_u16_x(pg16_all, acc0_t, acc1_t);

      acc_b = svadd_u16_x(pg16_all, acc_b, acc_new_b);
      acc_t = svadd_u16_x(pg16_all, acc_t, acc_new_t);
    }

    for (; i < half_kernel_size_ - 1; ++i) {
      svuint8_t src_i = svld1_u8(pg, &src_rows.at(top + i)[x]);
      svuint8_t src_j = svld1_u8(pg, &src_rows.at(bottom - i)[x]);
      svuint16_t vec_b = svaddlb_u16(src_i, src_j);
      svuint16_t vec_t = svaddlt_u16(src_i, src_j);
      acc_b = svmla_n_u16_x(pg16_all, acc_b, vec_b, half_kernel_u16_[i]);
      acc_t = svmla_n_u16_x(pg16_all, acc_t, vec_t, half_kernel_u16_[i]);
    }

    // Rounding & narrowing
    svuint8_t result = svraddhnb_n_u16(acc_b, 0);
    result = svraddhnt_n_u16(result, acc_t, 0);
    svst1(pg, &dst_rows[x], result);
  }

  // Where y is affected by border
  void vertical_border_vector_path(
      svbool_t pg, Rows<const SourceType> src_rows, Rows<BufferType> dst_rows,
      ptrdiff_t y, ptrdiff_t x) const KLEIDICV_STREAMING_COMPATIBLE {
    svuint8_t src_mid = svld1_u8(pg, &src_rows.at(y)[x]);
    svuint16_t acc_b =
        svmullb_n_u16(src_mid, half_kernel_u16_[half_kernel_size_ - 1]);
    svuint16_t acc_t =
        svmullt_n_u16(src_mid, half_kernel_u16_[half_kernel_size_ - 1]);

    ptrdiff_t i = 0, top = y - half_kernel_size_ + 1,
              bottom = y + half_kernel_size_ - 1;
    svbool_t pg16_all = svptrue_b16();

    for (; i < half_kernel_size_ - 1; ++i) {
      svuint8_t src_i =
          svld1_u8(pg, &src_rows.at(vertical_border_.get_column(top + i))[x]);
      svuint8_t src_j = svld1_u8(
          pg, &src_rows.at(vertical_border_.get_column(bottom - i))[x]);

      svuint16_t vec_b = svaddlb_u16(src_i, src_j);
      svuint16_t vec_t = svaddlt_u16(src_i, src_j);
      acc_b = svmla_n_u16_x(pg16_all, acc_b, vec_b, half_kernel_u16_[i]);
      acc_t = svmla_n_u16_x(pg16_all, acc_t, vec_t, half_kernel_u16_[i]);
    }

    // Rounding & narrowing
    svuint8_t result = svraddhnb_n_u16(acc_b, 0);
    result = svraddhnt_n_u16(result, acc_t, 0);
    svst1(pg, &dst_rows[x], result);
  }

  void horizontal_vector_path(svbool_t pg, Rows<BufferType> src_rows,
                              Rows<DestinationType> dst_rows,
                              ptrdiff_t x) const KLEIDICV_STREAMING_COMPATIBLE {
    // very similar to the vertical path, the difference is only the loading
    // pattern
    svuint8_t src_mid = svld1_u8(pg, &src_rows[x]);
    svuint16_t acc_b =
        svmullb_n_u16(src_mid, half_kernel_u16_[half_kernel_size_ - 1]);
    svuint16_t acc_t =
        svmullt_n_u16(src_mid, half_kernel_u16_[half_kernel_size_ - 1]);

    ptrdiff_t ch = static_cast<ptrdiff_t>(src_rows.channels()),
              left = x - ch * (half_kernel_size_ - 1),
              right = x + ch * (half_kernel_size_ - 1);
    svbool_t pg16_all = svptrue_b16();
    for (ptrdiff_t i = 0; i < half_kernel_size_ - 1; ++i) {
      svuint8_t src_i = svld1_u8(pg, &src_rows[left + i * ch]);
      svuint8_t src_j = svld1_u8(pg, &src_rows[right - i * ch]);
      svuint16_t vec_b = svaddlb_u16(src_i, src_j);
      svuint16_t vec_t = svaddlt_u16(src_i, src_j);
      acc_b = svmla_n_u16_x(pg16_all, acc_b, vec_b, half_kernel_u16_[i]);
      acc_t = svmla_n_u16_x(pg16_all, acc_t, vec_t, half_kernel_u16_[i]);
    }

    // Rounding & narrowing
    svuint8_t result = svraddhnb_n_u16(acc_b, 0);
    result = svraddhnt_n_u16(result, acc_t, 0);
    svst1(pg, &dst_rows[x], result);
  }

  void horizontal_left_border_vector_path(svbool_t pg,
                                          Rows<BufferType> src_rows,
                                          Rows<DestinationType> dst_rows,
                                          ptrdiff_t x) const {
    // similar to the simple horizontal path, except the loading pattern:
    // - this is loading indirect columns, and half of that data
    svbool_t pg16_all = svptrue_b16();
    svuint16_t src_mid = svld1ub_u16(pg, &src_rows[x]);
    svuint16_t acc = svmul_n_u16_x(pg16_all, src_mid,
                                   half_kernel_u16_[half_kernel_size_ - 1]);

    ptrdiff_t ch = static_cast<ptrdiff_t>(src_rows.channels());
    ptrdiff_t i = 0, left = x - ch * (half_kernel_size_ - 1),
              right = x + ch * (half_kernel_size_ - 1);
    for (; i * ch + left < 0; ++i) {
      svuint16_t src_i = horizontal_border_.load_left(src_rows, left + i * ch);
      svuint16_t src_j = svld1ub_u16(pg, &src_rows[right - i * ch]);
      svuint16_t vec = svadd_u16_x(pg16_all, src_i, src_j);
      acc = svmla_n_u16_x(pg16_all, acc, vec, half_kernel_u16_[i]);
    }

    for (; i < half_kernel_size_ - 1; ++i) {
      svuint16_t src_i = svld1ub_u16(pg, &src_rows[left + i * ch]);
      svuint16_t src_j = svld1ub_u16(pg, &src_rows[right - i * ch]);
      svuint16_t vec = svadd_u16_x(pg16_all, src_i, src_j);
      acc = svmla_n_u16_x(pg16_all, acc, vec, half_kernel_u16_[i]);
    }

    // Rounding & narrowing
    svuint8_t result = svraddhnb_n_u16(acc, 0);
    svst1b_u16(pg, &dst_rows[x], svreinterpret_u16_u8(result));
  }

  void horizontal_right_border_vector_path(svbool_t pg,
                                           Rows<BufferType> src_rows,
                                           Rows<DestinationType> dst_rows,
                                           ptrdiff_t x) const {
    // similar to the simple horizontal path, except the loading pattern:
    // - this is loading indirect columns, and half of that data
    svbool_t pg16_all = svptrue_b16();
    svuint16_t src_mid = svld1ub_u16(pg, &src_rows[x]);
    svuint16_t acc = svmul_n_u16_x(pg16_all, src_mid,
                                   half_kernel_u16_[half_kernel_size_ - 1]);
    const ptrdiff_t kLanes =
        static_cast<ptrdiff_t>(BufferVecTraits::num_lanes()) / 2;
    ptrdiff_t ch = static_cast<ptrdiff_t>(src_rows.channels());
    ptrdiff_t i = 0, left = x - ch * (half_kernel_size_ - 1),
              right = x + ch * (half_kernel_size_ - 1);
    for (; right - i * ch > width_ * ch - kLanes; ++i) {
      svuint16_t src_i = svld1ub_u16(pg, &src_rows[left + i * ch]);
      svuint16_t src_j =
          horizontal_border_.load_right(src_rows, right - i * ch);
      svuint16_t vec = svadd_u16_x(pg16_all, src_i, src_j);
      acc = svmla_n_u16_x(pg16_all, acc, vec, half_kernel_u16_[i]);
    }

    for (; i < half_kernel_size_ - 1; ++i) {
      svuint16_t src_i = svld1ub_u16(pg, &src_rows[left + i * ch]);
      svuint16_t src_j = svld1ub_u16(pg, &src_rows[right - i * ch]);
      svuint16_t vec = svadd_u16_x(pg16_all, src_i, src_j);
      acc = svmla_n_u16_x(pg16_all, acc, vec, half_kernel_u16_[i]);
    }

    // Rounding & narrowing
    svuint8_t result = svraddhnb_n_u16(acc, 0);
    svst1b_u16(pg, &dst_rows[x], svreinterpret_u16_u8(result));
  }

  const ptrdiff_t half_kernel_size_;
  const uint16_t *half_kernel_u16_;
  const ptrdiff_t width_;
  KLEIDICV_TARGET_NAMESPACE::GenericBorder<BorderT> vertical_border_,
      horizontal_border_;
};  // end of class GaussianBlurArbitrary<uint8_t>

template <size_t KernelSize, bool IsBinomial, typename ScalarType>
static kleidicv_error_t gaussian_blur_fixed_kernel_size(
    const ScalarType *src, size_t src_stride, ScalarType *dst,
    size_t dst_stride, Rectangle &rect, size_t y_begin, size_t y_end,
    size_t channels, float sigma, FixedBorderType border_type,
    SeparableFilterWorkspace *workspace) KLEIDICV_STREAMING_COMPATIBLE {
  using GaussianBlurFilter = GaussianBlur<ScalarType, KernelSize, IsBinomial>;

  Rows<const ScalarType> src_rows{src, src_stride, channels};
  Rows<ScalarType> dst_rows{dst, dst_stride, channels};

  if constexpr (IsBinomial) {
    GaussianBlurFilter blur;
    SeparableFilter<GaussianBlurFilter, KernelSize> filter{blur};
    workspace->process(rect, y_begin, y_end, src_rows, dst_rows, channels,
                       border_type, filter);

    return KLEIDICV_OK;
  } else {
    constexpr size_t kHalfKernelSize = get_half_kernel_size(KernelSize);
    uint16_t half_kernel[128];
    generate_gaussian_half_kernel(half_kernel, kHalfKernelSize, sigma);
    // If sigma is so small that the middle point gets all the weights, it's
    // just a copy
    if (half_kernel[kHalfKernelSize - 1] < 256) {
      GaussianBlurFilter blur(half_kernel);
      SeparableFilter<GaussianBlurFilter, KernelSize> filter{blur};
      workspace->process(rect, y_begin, y_end, src_rows, dst_rows, channels,
                         border_type, filter);
    } else {
      for (size_t row = y_begin; row < y_end; ++row) {
        std::memcpy(static_cast<void *>(&dst_rows.at(row)[0]),
                    static_cast<const void *>(&src_rows.at(row)[0]),
                    rect.width() * sizeof(ScalarType) * dst_rows.channels());
      }
    }
    return KLEIDICV_OK;
  }
}

template <typename ScalarType>
static kleidicv_error_t gaussian_blur_arbitrary_kernel_size(
    const ScalarType *src, size_t src_stride, ScalarType *dst,
    size_t dst_stride, Rectangle &rect, size_t kernel_size, size_t y_begin,
    size_t y_end, size_t channels, float sigma, FixedBorderType border_type,
    SeparableFilterWorkspace *workspace) {
  Rows<const ScalarType> src_rows{src, src_stride, channels};
  Rows<ScalarType> dst_rows{dst, dst_stride, channels};

  if (border_type != FixedBorderType::REPLICATE) {
    return KLEIDICV_ERROR_NOT_IMPLEMENTED;
  }

  const ptrdiff_t kHalfKernelSize =
      static_cast<ptrdiff_t>(get_half_kernel_size(kernel_size));
  uint16_t half_kernel[128];
  generate_gaussian_half_kernel(half_kernel, kHalfKernelSize, sigma);
  // If sigma is so small that the middle point gets all the weights, it's
  // just a copy
  if (half_kernel[kHalfKernelSize - 1] < 256) {
    svuint16_t sv0, sv1, sv2, sv3, sv4, sv5, sv6, sv7;
    //    switch (border_type) {
    //      case FixedBorderType::REPLICATE: {
    GaussianBlurArbitrary<ScalarType, FixedBorderType::REPLICATE> filter(
        half_kernel, kHalfKernelSize, rect, src_rows.channels(), sv0, sv1, sv2,
        sv3, sv4, sv5, sv6, sv7);
    workspace->process_arbitrary(rect, kernel_size, y_begin, y_end, src_rows,
                                 dst_rows, channels, border_type, filter);
    //        break;
    //      }
    //      default:
    //        return KLEIDICV_ERROR_NOT_IMPLEMENTED;
    //    }
  } else {
    for (size_t row = y_begin; row < y_end; ++row) {
      std::memcpy(static_cast<void *>(&dst_rows.at(row)[0]),
                  static_cast<const void *>(&src_rows.at(row)[0]),
                  rect.width() * sizeof(ScalarType) * dst_rows.channels());
    }
  }
  return KLEIDICV_OK;
}

template <bool IsBinomial, typename ScalarType>
static kleidicv_error_t gaussian_blur(
    size_t kernel_size, const ScalarType *src, size_t src_stride,
    ScalarType *dst, size_t dst_stride, Rectangle &rect, size_t y_begin,
    size_t y_end, size_t channels, float sigma, FixedBorderType border_type,
    SeparableFilterWorkspace *workspace) KLEIDICV_STREAMING_COMPATIBLE {
  switch (kernel_size) {
    case 3:
      return gaussian_blur_fixed_kernel_size<3, IsBinomial>(
          src, src_stride, dst, dst_stride, rect, y_begin, y_end, channels,
          sigma, border_type, workspace);
    case 5:
      return gaussian_blur_fixed_kernel_size<5, IsBinomial>(
          src, src_stride, dst, dst_stride, rect, y_begin, y_end, channels,
          sigma, border_type, workspace);
    case 7:
      return gaussian_blur_fixed_kernel_size<7, IsBinomial>(
          src, src_stride, dst, dst_stride, rect, y_begin, y_end, channels,
          sigma, border_type, workspace);
    case 15:
      // 15x15 does not have a binomial variant
      return gaussian_blur_fixed_kernel_size<15, false>(
          src, src_stride, dst, dst_stride, rect, y_begin, y_end, channels,
          sigma, border_type, workspace);
    case 21:
      // 21x21 does not have a binomial variant
      return gaussian_blur_fixed_kernel_size<21, false>(
          src, src_stride, dst, dst_stride, rect, y_begin, y_end, channels,
          sigma, border_type, workspace);
    default:
      return gaussian_blur_arbitrary_kernel_size(
          src, src_stride, dst, dst_stride, rect, kernel_size, y_begin, y_end,
          channels, sigma, border_type, workspace);
  }
}

static kleidicv_error_t gaussian_blur_fixed_stripe_u8_sc(
    const uint8_t *src, size_t src_stride, uint8_t *dst, size_t dst_stride,
    size_t width, size_t height, size_t y_begin, size_t y_end, size_t channels,
    size_t kernel_width, size_t /*kernel_height*/, float sigma_x,
    float /*sigma_y*/, FixedBorderType fixed_border_type,
    kleidicv_filter_context_t *context) KLEIDICV_STREAMING_COMPATIBLE {
  auto *workspace = reinterpret_cast<SeparableFilterWorkspace *>(context);
  kleidicv_error_t checks_result = gaussian_blur_checks(
      src, src_stride, dst, dst_stride, width, height, channels, workspace);

  if (checks_result != KLEIDICV_OK) {
    return checks_result;
  }

  Rectangle rect{width, height};

  if (sigma_x == 0.0) {
    return gaussian_blur<true>(kernel_width, src, src_stride, dst, dst_stride,
                               rect, y_begin, y_end, channels, sigma_x,
                               fixed_border_type, workspace);
  }

  return gaussian_blur<false>(kernel_width, src, src_stride, dst, dst_stride,
                              rect, y_begin, y_end, channels, sigma_x,
                              fixed_border_type, workspace);
}

}  // namespace KLEIDICV_TARGET_NAMESPACE

#endif  // KLEIDICV_GAUSSIAN_BLUR_SC_H
