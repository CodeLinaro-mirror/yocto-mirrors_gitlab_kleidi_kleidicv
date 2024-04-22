// SPDX-FileCopyrightText: 2024 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#include <cmath>

#include "kleidicv/kleidicv.h"
#include "kleidicv/neon.h"

namespace kleidicv::neon {

template <typename InputType, typename OutputType>
class float_conversion_operation;

template <typename OutputType>
class float_conversion_operation<float, OutputType> {
 public:
  using SrcVecTraits = KLEIDICV_TARGET_NAMESPACE::VecTraits<float>;
  using SrcVectorType = typename SrcVecTraits::VectorType;
  using IntermediateVecTraits = KLEIDICV_TARGET_NAMESPACE::VecTraits<
      std::conditional_t<std::is_signed_v<OutputType>, int32_t, uint32_t>>;
  using IntermediateVectorType = typename IntermediateVecTraits::VectorType;

  void process_row(size_t width, Columns<const float> src,
                   Columns<OutputType> dst) {
    LoopUnroll{width, SrcVecTraits::num_lanes()}
        .unroll_twice([&](size_t step) {
          SrcVectorType src_vector1 = vld1q_f32(&src[0]);
          SrcVectorType src_vector2 =
              vld1q_f32(&src[SrcVecTraits::num_lanes()]);
          IntermediateVectorType result_vector1 =
              vector_path<OutputType>(src_vector1);
          IntermediateVectorType result_vector2 =
              vector_path<OutputType>(src_vector2);
          vst1(&dst[0], vqmovn(vcombine(vqmovn(result_vector1),
                                        vqmovn(result_vector2))));
          src += ptrdiff_t(step);
          dst += ptrdiff_t(step);
        })
        .remaining([&](size_t length, size_t) {
          for (size_t index = 0; index < length; ++index) {
            disable_loop_vectorization();
            float f = std::nearbyint(src[ptrdiff_t(index)]);
            if (f > std::numeric_limits<OutputType>::max()) {
              f = std::numeric_limits<OutputType>::max();
            } else if (f < std::numeric_limits<OutputType>::min()) {
              f = std::numeric_limits<OutputType>::min();
            }
            dst[index] = static_cast<OutputType>(f);
          }
        });
  }

 private:
  template <
      typename O,
      std::enable_if_t<std::is_integral_v<O> && std::is_signed_v<O>, int> = 0>
  IntermediateVectorType vector_path(SrcVectorType src) {
    IntermediateVectorType result = vcvtnq_s32_f32(src);
    return result;
  }

  template <
      typename O,
      std::enable_if_t<std::is_integral_v<O> && !std::is_signed_v<O>, int> = 0>
  IntermediateVectorType vector_path(SrcVectorType src) {
    IntermediateVectorType result = vcvtnq_u32_f32(src);
    return result;
  }
};  // end of class float_conversion_operation<float, OutputType>

template <typename InputType>
class float_conversion_operation<InputType, float> {
 public:
  using SrcVecTraits = KLEIDICV_TARGET_NAMESPACE::VecTraits<InputType>;
  using SrcVectorType = typename SrcVecTraits::VectorType;
  using DstVecTraits = KLEIDICV_TARGET_NAMESPACE::VecTraits<float>;
  using DstVectorType = typename DstVecTraits::VectorType;
  using DstVector4Type = typename DstVecTraits::Vector4Type;

  void process_row(size_t width, Columns<const InputType> src,
                   Columns<float> dst) {
    LoopUnroll{width, SrcVecTraits::num_lanes()}
        .unroll_twice([&](size_t step) {
          DstVector4Type result_vector1 =
              vector_path<InputType>(vld1q(&src[0]));
          DstVector4Type result_vector2 =
              vector_path<InputType>(vld1q(&src[SrcVecTraits::num_lanes()]));
          vst1q_f32_x4(&dst[0], result_vector1);
          vst1q_f32_x4(&dst[DstVecTraits::num_lanes() * 4], result_vector2);
          src += ptrdiff_t(step);
          dst += ptrdiff_t(step);
        })
        .remaining([&](size_t length, size_t) {
          for (size_t index = 0; index < length; ++index) {
            disable_loop_vectorization();
            InputType n = src[ptrdiff_t(index)];
            dst[ptrdiff_t(index)] = static_cast<float>(n);
          }
        });
  }

 private:
  template <
      typename I,
      std::enable_if_t<std::is_integral_v<I> && std::is_signed_v<I>, int> = 0>
  DstVector4Type vector_path(const SrcVectorType src) {
    DstVector4Type dst_vect;
    int16x8_t low = vmovl_s8(vget_low_s8(src));
    int16x8_t hi = vmovl_high_s8(src);
    int32x4_t lowlow = vmovl_s16(vget_low_s16(low));
    int32x4_t lowhi = vmovl_high_s16(low);
    int32x4_t hilow = vmovl_s16(vget_low_s16(hi));
    int32x4_t hihi = vmovl_high_s16(hi);
    dst_vect.val[0] = vcvtq_f32_s32(lowlow);
    dst_vect.val[1] = vcvtq_f32_s32(lowhi);
    dst_vect.val[2] = vcvtq_f32_s32(hilow);
    dst_vect.val[3] = vcvtq_f32_s32(hihi);
    return dst_vect;
  }

  template <
      typename I,
      std::enable_if_t<std::is_integral_v<I> && !std::is_signed_v<I>, int> = 0>
  DstVector4Type vector_path(const SrcVectorType src) {
    DstVector4Type dst_vect;
    uint16x8_t low = vmovl_u8(vget_low_u8(src));
    uint16x8_t hi = vmovl_high_u8(src);
    uint32x4_t lowlow = vmovl_u16(vget_low_u16(low));
    uint32x4_t lowhi = vmovl_high_u16(low);
    uint32x4_t hilow = vmovl_u16(vget_low_u16(hi));
    uint32x4_t hihi = vmovl_high_u16(hi);
    dst_vect.val[0] = vcvtq_f32_u32(lowlow);
    dst_vect.val[1] = vcvtq_f32_u32(lowhi);
    dst_vect.val[2] = vcvtq_f32_u32(hilow);
    dst_vect.val[3] = vcvtq_f32_u32(hihi);
    return dst_vect;
  }
};  // end of class float_conversion_operation<InputType, float>

template <typename InputType, typename OutputType>
kleidicv_error_t float_conversion(const InputType* src, size_t src_stride,
                                  OutputType* dst, size_t dst_stride,
                                  size_t width, size_t height) {
  CHECK_POINTER_AND_STRIDE(src, src_stride, height);
  CHECK_POINTER_AND_STRIDE(dst, dst_stride, height);
  CHECK_IMAGE_SIZE(width, height);

  float_conversion_operation<InputType, OutputType> operation;
  Rectangle rect{width, height};
  Rows<const InputType> src_rows{src, src_stride};
  Rows<OutputType> dst_rows{dst, dst_stride};
  zip_rows(operation, rect, src_rows, dst_rows);

  return KLEIDICV_OK;
}

#define KLEIDICV_INSTANTIATE_TEMPLATE(itype, otype)                           \
  template KLEIDICV_TARGET_FN_ATTRS kleidicv_error_t                          \
  float_conversion<itype, otype>(const itype* src, size_t src_stride,         \
                                 otype* dst, size_t dst_stride, size_t width, \
                                 size_t height)

KLEIDICV_INSTANTIATE_TEMPLATE(float, int8_t);
KLEIDICV_INSTANTIATE_TEMPLATE(float, uint8_t);
KLEIDICV_INSTANTIATE_TEMPLATE(int8_t, float);
KLEIDICV_INSTANTIATE_TEMPLATE(uint8_t, float);

}  // namespace kleidicv::neon
