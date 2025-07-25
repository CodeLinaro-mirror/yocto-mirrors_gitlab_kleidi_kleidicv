// SPDX-FileCopyrightText: 2023 - 2025 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#ifndef KLEIDICV_GAUSSIAN_BLUR_SC_H
#define KLEIDICV_GAUSSIAN_BLUR_SC_H

#include <array>
#include <cassert>

#include "kleidicv/filters/gaussian_blur.h"
#include "kleidicv/filters/separable_filter_15x15_sc.h"
#include "kleidicv/filters/separable_filter_21x21_sc.h"
#include "kleidicv/filters/separable_filter_3x3_sc.h"
#include "kleidicv/filters/separable_filter_5x5_sc.h"
#include "kleidicv/filters/separable_filter_7x7_sc.h"
#include "kleidicv/filters/sigma.h"
#include "kleidicv/workspace/separable.h"

#if KLEIDICV_TARGET_SME || KLEIDICV_TARGET_SME2
#include <arm_sme.h>
#endif

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
  void vertical_vector_path(svbool_t pg,
                            std::reference_wrapper<svuint8_t> src[3],
                            BufferType *dst) const KLEIDICV_STREAMING {
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
  void horizontal_vector_path(svbool_t pg,
                              std::reference_wrapper<svuint16_t> src[3],
                              DestinationType *dst) const KLEIDICV_STREAMING {
    svuint16_t acc_0_2 = svhadd_u16_x(pg, src[0], src[2]);

    svuint16_t acc = svadd_u16_x(pg, acc_0_2, src[1]);
    acc = svrshr_x(pg, acc, 3);

    svst1b(pg, &dst[0], acc);
  }

  // Applies horizontal filtering vector using scalar operations.
  //
  // DST = 1/16 * [ SRC0, SRC1, SRC2 ] * [ 1, 2, 1 ]T
  void horizontal_scalar_path(const BufferType src[3],
                              DestinationType *dst) const KLEIDICV_STREAMING {
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
  void vertical_vector_path(svbool_t pg,
                            std::reference_wrapper<svuint8_t> src[5],
                            BufferType *dst) const KLEIDICV_STREAMING {
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
  void horizontal_vector_path(svbool_t pg,
                              std::reference_wrapper<svuint16_t> src[5],
                              DestinationType *dst) const KLEIDICV_STREAMING {
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
  void horizontal_scalar_path(const BufferType src[5],
                              DestinationType *dst) const KLEIDICV_STREAMING {
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
  void vertical_vector_path(svbool_t pg,
                            std::reference_wrapper<svuint8_t> src[7],
                            BufferType *dst) const KLEIDICV_STREAMING {
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
  void horizontal_vector_path(svbool_t pg,
                              std::reference_wrapper<svuint16_t> src[7],
                              DestinationType *dst) const KLEIDICV_STREAMING {
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
  void horizontal_scalar_path(const BufferType src[7],
                              DestinationType *dst) const KLEIDICV_STREAMING {
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
      BufferType *dst) const KLEIDICV_STREAMING {
    common_vector_path(pg, src, dst);
  }

  void vertical_scalar_path(const SourceType src[KernelSize],
                            BufferType *dst) const KLEIDICV_STREAMING {
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
      BufferType *dst) const KLEIDICV_STREAMING {
    common_vector_path(pg, src, dst);
  }

  void horizontal_scalar_path(const BufferType src[KernelSize],
                              DestinationType *dst) const KLEIDICV_STREAMING {
    vertical_scalar_path(src, dst);
  }

 private:
  void common_vector_path(
      svbool_t pg, std::reference_wrapper<SourceVectorType> src[KernelSize],
      BufferType *dst) const KLEIDICV_STREAMING {
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

template <size_t KernelSize, bool IsBinomial, typename ScalarType>
static kleidicv_error_t gaussian_blur_fixed_kernel_size(
    const ScalarType *src, size_t src_stride, ScalarType *dst,
    size_t dst_stride, Rectangle &rect, size_t y_begin, size_t y_end,
    size_t channels, float sigma, FixedBorderType border_type,
    SeparableFilterWorkspace *workspace) KLEIDICV_STREAMING {
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
#if KLEIDICV_TARGET_SME
        __arm_sc_memcpy(
            static_cast<void *>(&dst_rows.at(row)[0]),
            static_cast<const void *>(&src_rows.at(row)[0]),
            rect.width() * sizeof(ScalarType) * dst_rows.channels());
#else
        std::memcpy(static_cast<void *>(&dst_rows.at(row)[0]),
                    static_cast<const void *>(&src_rows.at(row)[0]),
                    rect.width() * sizeof(ScalarType) * dst_rows.channels());
#endif
      }
    }
    return KLEIDICV_OK;
  }
}

template <bool IsBinomial, typename ScalarType>
static kleidicv_error_t gaussian_blur(
    size_t kernel_size, const ScalarType *src, size_t src_stride,
    ScalarType *dst, size_t dst_stride, Rectangle &rect, size_t y_begin,
    size_t y_end, size_t channels, float sigma, FixedBorderType border_type,
    SeparableFilterWorkspace *workspace) KLEIDICV_STREAMING {
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
      // gaussian_blur_is_implemented checked the kernel size already.
    // GCOVR_EXCL_START
    default:
      assert(!"kernel size not implemented");
      return KLEIDICV_ERROR_NOT_IMPLEMENTED;
      // GCOVR_EXCL_STOP
  }
}

static kleidicv_error_t gaussian_blur_fixed_stripe_u8_sc(
    const uint8_t *src, size_t src_stride, uint8_t *dst, size_t dst_stride,
    size_t width, size_t height, size_t y_begin, size_t y_end, size_t channels,
    size_t kernel_width, size_t /*kernel_height*/, float sigma_x,
    float /*sigma_y*/, FixedBorderType fixed_border_type,
    kleidicv_filter_context_t *context) KLEIDICV_STREAMING {
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
