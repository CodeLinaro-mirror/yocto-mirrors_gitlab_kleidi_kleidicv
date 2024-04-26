// SPDX-FileCopyrightText: 2023 - 2024 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#include "kleidicv/filters/gaussian_blur.h"
#include "kleidicv/kleidicv.h"
#include "kleidicv/neon.h"
#include "kleidicv/separable_filter_3x3_neon.h"
#include "kleidicv/separable_filter_5x5_neon.h"
#include "kleidicv/separable_filter_7x7_neon.h"

namespace kleidicv::neon {

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
template <>
class DiscreteGaussianBlur<uint8_t, 5> {
 public:
  using SourceType = uint8_t;
  using BufferType = uint16_t;
  using DestinationType = uint8_t;

  DiscreteGaussianBlur()
      : const_6_u8_half_{vdup_n_u8(6)},
        const_6_u16_{vdupq_n_u16(6)},
        const_4_u16_{vdupq_n_u16(4)} {}

  // Applies vertical filtering vector using SIMD operations.
  //
  // DST = [ SRC0, SRC1, SRC2, SRC3, SRC4 ] * [ 1, 4, 6, 4, 1 ]T
  void vertical_vector_path(uint8x16_t src[5], BufferType *dst) const {
    uint16x8_t acc_0_4_l = vaddl_u8(vget_low_u8(src[0]), vget_low_u8(src[4]));
    uint16x8_t acc_0_4_h = vaddl_u8(vget_high_u8(src[0]), vget_high_u8(src[4]));
    uint16x8_t acc_1_3_l = vaddl_u8(vget_low_u8(src[1]), vget_low_u8(src[3]));
    uint16x8_t acc_1_3_h = vaddl_u8(vget_high_u8(src[1]), vget_high_u8(src[3]));
    uint16x8_t acc_l =
        vmlal_u8(acc_0_4_l, vget_low_u8(src[2]), const_6_u8_half_);
    uint16x8_t acc_h =
        vmlal_u8(acc_0_4_h, vget_high_u8(src[2]), const_6_u8_half_);
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
  uint8x8_t const_6_u8_half_;
  uint16x8_t const_6_u16_;
  uint16x8_t const_4_u16_;
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

  DiscreteGaussianBlur()
      : const_7_u16_{vdupq_n_u16(7)},
        const_7_u32_{vdupq_n_u32(7)},
        const_9_u16_{vdupq_n_u16(9)} {}

  // Applies vertical filtering vector using SIMD operations.
  //
  // DST = [ SRC0, SRC1, SRC2, SRC3, SRC4, SRC5, SRC6 ] *
  //     * [ 2, 7, 14, 18, 14, 7, 2 ]T
  void vertical_vector_path(uint8x16_t src[7], BufferType *dst) const {
    uint16x8_t acc_0_6_l = vaddl_u8(vget_low_u8(src[0]), vget_low_u8(src[6]));
    uint16x8_t acc_0_6_h = vaddl_u8(vget_high_u8(src[0]), vget_high_u8(src[6]));

    uint16x8_t acc_1_5_l = vaddl_u8(vget_low_u8(src[1]), vget_low_u8(src[5]));
    uint16x8_t acc_1_5_h = vaddl_u8(vget_high_u8(src[1]), vget_high_u8(src[5]));

    uint16x8_t acc_2_4_l = vaddl_u8(vget_low_u8(src[2]), vget_low_u8(src[4]));
    uint16x8_t acc_2_4_h = vaddl_u8(vget_high_u8(src[2]), vget_high_u8(src[4]));

    uint16x8_t acc_3_l = vmovl_u8(vget_low_u8(src[3]));
    uint16x8_t acc_3_h = vmovl_u8(vget_high_u8(src[3]));

    uint16x8_t acc_0_2_4_6_l = vmlaq_u16(acc_0_6_l, acc_2_4_l, const_7_u16_);
    uint16x8_t acc_0_2_4_6_h = vmlaq_u16(acc_0_6_h, acc_2_4_h, const_7_u16_);

    uint16x8_t acc_0_2_3_4_6_l =
        vmlaq_u16(acc_0_2_4_6_l, acc_3_l, const_9_u16_);
    uint16x8_t acc_0_2_3_4_6_h =
        vmlaq_u16(acc_0_2_4_6_h, acc_3_h, const_9_u16_);

    acc_0_2_3_4_6_l = vshlq_n_u16(acc_0_2_3_4_6_l, 1);
    acc_0_2_3_4_6_h = vshlq_n_u16(acc_0_2_3_4_6_h, 1);

    uint16x8_t acc_0_1_2_3_4_5_6_l =
        vmlaq_u16(acc_0_2_3_4_6_l, acc_1_5_l, const_7_u16_);
    uint16x8_t acc_0_1_2_3_4_5_6_h =
        vmlaq_u16(acc_0_2_3_4_6_h, acc_1_5_h, const_7_u16_);

    vst1q(&dst[0], acc_0_1_2_3_4_5_6_l);
    vst1q(&dst[8], acc_0_1_2_3_4_5_6_h);
  }

  // Applies vertical filtering vector using scalar operations.
  //
  // DST = [ SRC0, SRC1, SRC2, SRC3, SRC4, SRC5, SRC6 ] *
  //     * [ 2, 7, 14, 18, 14, 7, 2 ]T
  void vertical_scalar_path(const SourceType src[7], BufferType *dst) const {
    uint32_t acc = src[0] * 2 + src[1] * 7 + src[2] * 14 + src[3] * 18 +
                   src[4] * 14 + src[5] * 7 + src[6] * 2;
    dst[0] = acc;
  }

  // Applies horizontal filtering vector using SIMD operations.
  //
  // DST = 1/4096 * [ SRC0, SRC1, SRC2, SRC3, SRC4, SRC5, SRC6 ] *
  //              * [ 2, 7, 14, 18, 14, 7, 2 ]T
  void horizontal_vector_path(uint16x8_t src[7], DestinationType *dst) const {
    uint32x4_t acc_0_6_l =
        vaddl_u16(vget_low_u16(src[0]), vget_low_u16(src[6]));
    uint32x4_t acc_0_6_h =
        vaddl_u16(vget_high_u16(src[0]), vget_high_u16(src[6]));

    uint32x4_t acc_1_5_l =
        vaddl_u16(vget_low_u16(src[1]), vget_low_u16(src[5]));
    uint32x4_t acc_1_5_h =
        vaddl_u16(vget_high_u16(src[1]), vget_high_u16(src[5]));

    uint16x8_t acc_2_4 = vaddq_u16(src[2], src[4]);

    uint32x4_t acc_0_2_4_6_l =
        vmlal_u16(acc_0_6_l, vget_low_u16(acc_2_4), vget_low_u16(const_7_u16_));
    uint32x4_t acc_0_2_4_6_h = vmlal_u16(acc_0_6_h, vget_high_u16(acc_2_4),
                                         vget_high_u16(const_7_u16_));

    uint32x4_t acc_0_2_3_4_6_l = vmlal_u16(acc_0_2_4_6_l, vget_low_u16(src[3]),
                                           vget_low_u16(const_9_u16_));
    uint32x4_t acc_0_2_3_4_6_h = vmlal_u16(acc_0_2_4_6_h, vget_high_u16(src[3]),
                                           vget_high_u16(const_9_u16_));

    acc_0_2_3_4_6_l = vshlq_n_u32(acc_0_2_3_4_6_l, 1);
    acc_0_2_3_4_6_h = vshlq_n_u32(acc_0_2_3_4_6_h, 1);

    uint32x4_t acc_0_1_2_3_4_5_6_l =
        vmlaq_u32(acc_0_2_3_4_6_l, acc_1_5_l, const_7_u32_);
    uint32x4_t acc_0_1_2_3_4_5_6_h =
        vmlaq_u32(acc_0_2_3_4_6_h, acc_1_5_h, const_7_u32_);

    uint16x4_t acc_0_1_2_3_4_5_6_u16_l = vrshrn_n_u32(acc_0_1_2_3_4_5_6_l, 12);
    uint16x4_t acc_0_1_2_3_4_5_6_u16_h = vrshrn_n_u32(acc_0_1_2_3_4_5_6_h, 12);

    uint16x8_t acc_0_1_2_3_4_5_6_u16 =
        vcombine_u16(acc_0_1_2_3_4_5_6_u16_l, acc_0_1_2_3_4_5_6_u16_h);
    uint8x8_t acc_0_1_2_3_4_5_6_u8 = vmovn_u16(acc_0_1_2_3_4_5_6_u16);

    vst1(&dst[0], acc_0_1_2_3_4_5_6_u8);
  }

  // Applies horizontal filtering vector using scalar operations.
  //
  // DST = 1/4096 * [ SRC0, SRC1, SRC2, SRC3, SRC4, SRC5, SRC6 ] *
  //              * [ 2, 7, 14, 18, 14, 7, 2 ]T
  void horizontal_scalar_path(const BufferType src[7],
                              DestinationType *dst) const {
    uint32_t acc = src[0] * 2 + src[1] * 7 + src[2] * 14 + src[3] * 18 +
                   src[4] * 14 + src[5] * 7 + src[6] * 2;
    dst[0] = rounding_shift_right(acc, 12);
  }

 private:
  uint16x8_t const_7_u16_;
  uint32x4_t const_7_u32_;
  uint16x8_t const_9_u16_;
};  // end of class DiscreteGaussianBlur<uint8_t, 7>

template <typename ScalarType, size_t KernelSize>
kleidicv_error_t discrete_gaussian_blur(const ScalarType *src,
                                        size_t src_stride, ScalarType *dst,
                                        size_t dst_stride, size_t width,
                                        size_t height, size_t channels,
                                        kleidicv_border_type_t border_type,
                                        kleidicv_filter_context_t *context) {
  using GaussianBlurFilterType = DiscreteGaussianBlur<ScalarType, KernelSize>;

  CHECK_POINTERS(context);
  CHECK_POINTER_AND_STRIDE(src, src_stride);
  CHECK_POINTER_AND_STRIDE(dst, dst_stride);
  CHECK_IMAGE_SIZE(width, height);

  if (width < KernelSize - 1 || height < KernelSize - 1) {
    return KLEIDICV_ERROR_NOT_IMPLEMENTED;
  }

  if (channels > KLEIDICV_MAXIMUM_CHANNEL_COUNT) {
    return KLEIDICV_ERROR_RANGE;
  }

  Rectangle rect{width, height};
  Rows<const ScalarType> src_rows{src, src_stride, channels};
  Rows<ScalarType> dst_rows{dst, dst_stride, channels};

  auto *workspace = reinterpret_cast<SeparableFilterWorkspace *>(context);

  if (workspace->intermediate_size() != 2 * sizeof(ScalarType)) {
    return KLEIDICV_ERROR_CONTEXT_MISMATCH;
  }

  if (workspace->channels() != channels) {
    return KLEIDICV_ERROR_CONTEXT_MISMATCH;
  }

  if (workspace->image_size() != rect) {
    return KLEIDICV_ERROR_CONTEXT_MISMATCH;
  }

  auto fixed_border_type = get_fixed_border_type(border_type);

  if (!fixed_border_type) {
    return KLEIDICV_ERROR_NOT_IMPLEMENTED;
  }

  GaussianBlurFilterType blur;
  SeparableFilter<GaussianBlurFilterType, KernelSize> filter{blur};
  workspace->process(rect, src_rows, dst_rows, channels, *fixed_border_type,
                     filter);
  return KLEIDICV_OK;
}

KLEIDICV_TARGET_FN_ATTRS
kleidicv_error_t gaussian_blur_3x3_u8(const uint8_t *src, size_t src_stride,
                                      uint8_t *dst, size_t dst_stride,
                                      size_t width, size_t height,
                                      size_t channels,
                                      kleidicv_border_type_t border_type,
                                      kleidicv_filter_context_t *context) {
  return discrete_gaussian_blur<uint8_t, 3>(src, src_stride, dst, dst_stride,
                                            width, height, channels,
                                            border_type, context);
}

KLEIDICV_TARGET_FN_ATTRS
kleidicv_error_t gaussian_blur_5x5_u8(const uint8_t *src, size_t src_stride,
                                      uint8_t *dst, size_t dst_stride,
                                      size_t width, size_t height,
                                      size_t channels,
                                      kleidicv_border_type_t border_type,
                                      kleidicv_filter_context_t *context) {
  return discrete_gaussian_blur<uint8_t, 5>(src, src_stride, dst, dst_stride,
                                            width, height, channels,
                                            border_type, context);
}

KLEIDICV_TARGET_FN_ATTRS
kleidicv_error_t gaussian_blur_7x7_u8(const uint8_t *src, size_t src_stride,
                                      uint8_t *dst, size_t dst_stride,
                                      size_t width, size_t height,
                                      size_t channels,
                                      kleidicv_border_type_t border_type,
                                      kleidicv_filter_context_t *context) {
  return discrete_gaussian_blur<uint8_t, 7>(src, src_stride, dst, dst_stride,
                                            width, height, channels,
                                            border_type, context);
}

}  // namespace kleidicv::neon
