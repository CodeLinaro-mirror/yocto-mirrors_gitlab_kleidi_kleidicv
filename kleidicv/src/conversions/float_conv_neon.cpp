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
            } else if (f < std::numeric_limits<OutputType>::lowest()) {
              f = std::numeric_limits<OutputType>::lowest();
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

  float_conversion_operation() : index_{initialize_indexes()} {}

  void process_row(size_t width, Columns<const InputType> src,
                   Columns<float> dst) {
    LoopUnroll{width, SrcVecTraits::num_lanes()}
        .unroll_twice([&](size_t step) {
          SrcVectorType src0 = vld1q(&src[0]);
          SrcVectorType src1 = vld1q(&src[SrcVecTraits::num_lanes()]);

          vector_path<InputType>(src0, &dst[0]);
          vector_path<InputType>(src1, &dst[SrcVecTraits::num_lanes()]);

          src += ptrdiff_t(step);
          dst += ptrdiff_t(step);
        })
        .remaining([&](size_t length, size_t) {
          for (size_t index = 0; index < length; ++index) {
            disable_loop_vectorization();
            dst[ptrdiff_t(index)] = src[ptrdiff_t(index)];
          }
        });
  }

 private:
  static uint8x16x4_t initialize_indexes() {
    if constexpr (std::is_signed_v<InputType>) {
      const uint8x16_t index0 = vcombine_u8(vcreate_u8(0x01ffffff00ffffffULL),
                                            vcreate_u8(0x03ffffff02ffffffULL));
      const uint8x16_t index1 = vcombine_u8(vcreate_u8(0x05ffffff04ffffffULL),
                                            vcreate_u8(0x07ffffff06ffffffULL));
      const uint8x16_t index2 = vcombine_u8(vcreate_u8(0x09ffffff08ffffffULL),
                                            vcreate_u8(0x0bffffff0affffffULL));
      const uint8x16_t index3 = vcombine_u8(vcreate_u8(0x0dffffff0cffffffULL),
                                            vcreate_u8(0x0fffffff0effffffULL));
      return {index0, index1, index2, index3};
    } else {
      const uint8x16_t index0 = vcombine_u8(vcreate_u8(0xffffff01ffffff00ULL),
                                            vcreate_u8(0xffffff03ffffff02ULL));
      const uint8x16_t index1 = vcombine_u8(vcreate_u8(0xffffff05ffffff04ULL),
                                            vcreate_u8(0xffffff07ffffff06ULL));
      const uint8x16_t index2 = vcombine_u8(vcreate_u8(0xffffff09ffffff08ULL),
                                            vcreate_u8(0xffffff0bffffff0aULL));
      const uint8x16_t index3 = vcombine_u8(vcreate_u8(0xffffff0dffffff0cULL),
                                            vcreate_u8(0xffffff0fffffff0eULL));
      return {index0, index1, index2, index3};
    }
  }

  template <
      typename I,
      std::enable_if_t<std::is_integral_v<I> && std::is_signed_v<I>, int> = 0>
  void vector_path(SrcVectorType src, float* dst) {
    int32x4_t a = vreinterpretq_s32_u8(vqtbl1q_u8(src, index_.val[0]));
    int32x4_t b = vreinterpretq_s32_u8(vqtbl1q_u8(src, index_.val[1]));
    int32x4_t c = vreinterpretq_s32_u8(vqtbl1q_u8(src, index_.val[2]));
    int32x4_t d = vreinterpretq_s32_u8(vqtbl1q_u8(src, index_.val[3]));
    float32x4x4_t output = {
        vcvtq_n_f32_s32(a, 24),
        vcvtq_n_f32_s32(b, 24),
        vcvtq_n_f32_s32(c, 24),
        vcvtq_n_f32_s32(d, 24),
    };
    vst1q_f32_x4(dst, output);
  }

  template <
      typename I,
      std::enable_if_t<std::is_integral_v<I> && !std::is_signed_v<I>, int> = 0>
  void vector_path(SrcVectorType src, float* dst) {
    uint32x4_t a = vreinterpretq_u32_u8(vqtbl1q_u8(src, index_.val[0]));
    uint32x4_t b = vreinterpretq_u32_u8(vqtbl1q_u8(src, index_.val[1]));
    uint32x4_t c = vreinterpretq_u32_u8(vqtbl1q_u8(src, index_.val[2]));
    uint32x4_t d = vreinterpretq_u32_u8(vqtbl1q_u8(src, index_.val[3]));
    float32x4x4_t output = {
        vcvtq_f32_u32(a),
        vcvtq_f32_u32(b),
        vcvtq_f32_u32(c),
        vcvtq_f32_u32(d),
    };
    vst1q_f32_x4(dst, output);
  }

  const uint8x16x4_t index_;
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
