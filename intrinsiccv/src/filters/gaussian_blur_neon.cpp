// SPDX-FileCopyrightText: 2023 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#include "filters/gaussian_blur.h"
#include "intrinsiccv.h"
#include "neon.h"

namespace intrinsiccv::neon {

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
  using ScalarType = uint8_t;
  using SourceType = ScalarType;
  using SourceVectorType = typename VecTraits<SourceType>::VectorType;
  using BufferType = double_element_width_t<ScalarType>;
  using BufferVectorType = typename VecTraits<BufferType>::VectorType;
  using DestinationType = ScalarType;

  // Applies vertical filtering vector using SIMD operations.
  //
  // DST = [ SRC0, SRC1, SRC2 ] * [ 1, 2, 1 ]T
  void vertical_vector_path(SourceVectorType src[3], BufferType *dst) const {
    // acc_0_2 = src[0] + src[2]
    BufferVectorType acc_0_2_l = vaddl(vget_low(src[0]), vget_low(src[2]));
    BufferVectorType acc_0_2_h = vaddl(vget_high(src[0]), vget_high(src[2]));
    // acc_1 = src[1] + src[1]
    BufferVectorType acc_1_l = vshll_n<1>(vget_low(src[1]));
    BufferVectorType acc_1_h = vshll_n<1>(vget_high(src[1]));
    // acc = acc_0_2 + acc_1
    BufferVectorType acc_l = vaddq(acc_0_2_l, acc_1_l);
    BufferVectorType acc_h = vaddq(acc_0_2_h, acc_1_h);

    VecTraits<BufferType>::store_consecutive(acc_l, acc_h, &dst[0]);
  }

  // Applies vertical filtering vector using scalar operations.
  //
  // DST = [ SRC0, SRC1, SRC2 ] * [ 1, 2, 1 ]T
  void vertical_scalar_path(const SourceType src[3], BufferType *dst) const {
    dst[0] = src[0] + 2 * src[1] + src[2];
  }

  // Applies horizontal filtering vector using SIMD operations.
  //
  // DST = 1/16 * [ SRC0, SRC1, SRC2 ] * [ 1, 2, 1 ]T
  void horizontal_vector_path(BufferVectorType src[3],
                              DestinationType *dst) const {
    BufferVectorType acc_wide = vaddq(src[0], src[2]);
    acc_wide = vaddq(acc_wide, vshlq_n<1>(src[1]));
    auto acc_narrow = vrshrn_n<4>(acc_wide);
    vst1(&dst[0], acc_narrow);
  }

  // Applies horizontal filtering vector using scalar operations.
  //
  // DST = 1/16 * [ SRC0, SRC1, SRC2 ] * [ 1, 2, 1 ]T
  void horizontal_scalar_path(const BufferType src[3],
                              DestinationType *dst) const {
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
// 5x5 Gaussian Blur filter for uint8_t types.
template <>
class DiscreteGaussianBlur<uint8_t, 5> {
 public:
  using SourceType = uint8_t;
  using BufferType = uint16_t;
  using DestinationType = uint8_t;

  DiscreteGaussianBlur()
      : const_6_u8_{vmov_n_u8(6)},
        const_6_u16_{vmovq_n_u16(6)},
        const_4_u16_{vmovq_n_u16(4)} {}

  // Applies vertical filtering vector using SIMD operations.
  //
  // DST = [ SRC0, SRC1, SRC2, SRC3, SRC4 ] * [ 1, 4, 6, 4, 1 ]T
  void vertical_vector_path(uint8x16_t src[5], BufferType *dst) const {
    uint16x8_t acc_0_4_l = vaddl_u8(vget_low_u8(src[0]), vget_low_u8(src[4]));
    uint16x8_t acc_0_4_h = vaddl_u8(vget_high_u8(src[0]), vget_high_u8(src[4]));
    uint16x8_t acc_1_3_l = vaddl_u8(vget_low_u8(src[1]), vget_low_u8(src[3]));
    uint16x8_t acc_1_3_h = vaddl_u8(vget_high_u8(src[1]), vget_high_u8(src[3]));
    uint16x8_t acc_l = vmlal_u8(acc_0_4_l, vget_low_u8(src[2]), const_6_u8_);
    uint16x8_t acc_h = vmlal_u8(acc_0_4_h, vget_high_u8(src[2]), const_6_u8_);
    acc_l = vmlaq_u16(acc_l, acc_1_3_l, const_4_u16_);
    acc_h = vmlaq_u16(acc_h, acc_1_3_h, const_4_u16_);
    vst1q(&dst[0], acc_l);
    vst1q(&dst[8], acc_h);
  }

  // Applies vertical filtering vector using scalar operations.
  //
  // DST = [ SRC0, SRC1, SRC2, SRC3, SRC4 ] * [ 1, 4, 6, 4, 1 ]T
  void vertical_scalar_path(const SourceType src[5], BufferType *dst) const {
    dst[0] = src[0] + src[4] + 4 * (src[1] + src[3]) + 6 * src[2];
  }

  // Applies horizontal filtering vector using SIMD operations.
  //
  // DST = 1/256 * [ SRC0, SRC1, SRC2, SRC3, SRC4 ] * [ 1, 4, 6, 4, 1 ]T
  void horizontal_vector_path(uint16x8_t src[5], DestinationType *dst) const {
    uint16x8_t acc_0_4 = vaddq_u16(src[0], src[4]);
    uint16x8_t acc_1_3 = vaddq_u16(src[1], src[3]);
    uint16x8_t acc_u16 = vmlaq_u16(acc_0_4, src[2], const_6_u16_);
    acc_u16 = vmlaq_u16(acc_u16, acc_1_3, const_4_u16_);
    uint8x8_t acc_u8 = vrshrn_n_u16(acc_u16, 8);
    vst1(&dst[0], acc_u8);
  }

  // Applies horizontal filtering vector using scalar operations.
  //
  // DST = 1/256 * [ SRC0, SRC1, SRC2, SRC3, SRC4 ] * [ 1, 4, 6, 4, 1 ]T
  void horizontal_scalar_path(const BufferType src[5],
                              DestinationType *dst) const {
    auto acc = src[0] + src[4] + 4 * (src[1] + src[3]) + 6 * src[2];
    dst[0] = rounding_shift_right(acc, 8);
  }

 private:
  uint8x8_t const_6_u8_;
  uint16x8_t const_6_u16_;
  uint16x8_t const_4_u16_;
};  // end of class DiscreteGaussianBlur<uint8_t, 5>

template <typename ScalarType, size_t KernelSize>
intrinsiccv_error_t discrete_gaussian_blur(
    const ScalarType *src, size_t src_stride, ScalarType *dst,
    size_t dst_stride, size_t width, size_t height, size_t channels,
    intrinsiccv_border_type_t border_type,
    const intrinsiccv_filter_params_t *params) {
  using GaussianBlurFilterType = DiscreteGaussianBlur<ScalarType, KernelSize>;

  Rectangle rect{width, height};
  Rows<const ScalarType> src_rows{src, src_stride, channels};
  Rows<ScalarType> dst_rows{dst, dst_stride, channels};

  auto *workspace =
      reinterpret_cast<SeparableFilterWorkspace *>(params->workspace);

  GaussianBlurFilterType blur;
  SeparableFilter<GaussianBlurFilterType, KernelSize> filter{blur};
  workspace->process(rect, src_rows, dst_rows, channels, border_type, filter);
  return INTRINSICCV_OK;
}

INTRINSICCV_TARGET_FN_ATTRS
intrinsiccv_error_t gaussian_blur_3x3_u8(
    const uint8_t *src, size_t src_stride, uint8_t *dst, size_t dst_stride,
    size_t width, size_t height, size_t channels,
    intrinsiccv_border_type_t border_type,
    const intrinsiccv_filter_params_t *params) {
  discrete_gaussian_blur<uint8_t, 3>(src, src_stride, dst, dst_stride, width,
                                     height, channels, border_type, params);
  return INTRINSICCV_OK;
}

INTRINSICCV_TARGET_FN_ATTRS
intrinsiccv_error_t gaussian_blur_5x5_u8(
    const uint8_t *src, size_t src_stride, uint8_t *dst, size_t dst_stride,
    size_t width, size_t height, size_t channels,
    intrinsiccv_border_type_t border_type,
    const intrinsiccv_filter_params_t *params) {
  discrete_gaussian_blur<uint8_t, 5>(src, src_stride, dst, dst_stride, width,
                                     height, channels, border_type, params);
  return INTRINSICCV_OK;
}

}  // namespace intrinsiccv::neon
