// SPDX-FileCopyrightText: 2024 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#ifndef INTRINSICCV_FLOAT_TO_INT_SC_H
#define INTRINSICCV_FLOAT_TO_INT_SC_H

#include <type_traits>

#include "intrinsiccv/intrinsiccv.h"
#include "intrinsiccv/sve2.h"

namespace INTRINSICCV_TARGET_NAMESPACE {

template <typename OutputType>
class float_to_int_operation final {
 public:
  using SrcVecTraits = INTRINSICCV_TARGET_NAMESPACE::VecTraits<float>;
  using SrcVectorType = typename SrcVecTraits::VectorType;
  using IntermediateVecTraits = INTRINSICCV_TARGET_NAMESPACE::VecTraits<
      std::conditional_t<std::is_signed_v<OutputType>, int32_t, uint32_t>>;
  using IntermediateVectorType = typename IntermediateVecTraits::VectorType;

  using VecTraits = SrcVecTraits;

  void process_row(size_t width, Columns<const float> src,
                   Columns<OutputType> dst) INTRINSICCV_STREAMING_COMPATIBLE {
    LoopUnroll{width, VecTraits::num_lanes()}
        .unroll_twice([&](size_t step) INTRINSICCV_STREAMING_COMPATIBLE {
          svbool_t pg = VecTraits::svptrue();
          SrcVectorType src_vector1 = svld1(pg, &src[0]);
          SrcVectorType src_vector2 = svld1_vnum(pg, &src[0], 1);
          IntermediateVectorType result_vector1 =
              vector_path<OutputType>(pg, src_vector1);
          IntermediateVectorType result_vector2 =
              vector_path<OutputType>(pg, src_vector2);
          svst1b(pg, &dst[0], result_vector1);
          svst1b_vnum(pg, &dst[0], 1, result_vector2);
          src += ptrdiff_t(step);
          dst += ptrdiff_t(step);
        })
        .remaining([&](size_t length, size_t) INTRINSICCV_STREAMING_COMPATIBLE {
          size_t index = 0;
          svbool_t pg = VecTraits::svwhilelt(index, length);
          while (svptest_first(VecTraits::svptrue(), pg)) {
            SrcVectorType src_vector = svld1(pg, &src[ptrdiff_t(index)]);
            IntermediateVectorType result_vector =
                vector_path<OutputType>(pg, src_vector);
            svst1b(pg, &dst[ptrdiff_t(index)], result_vector);
            // Update loop counter and calculate the next governing predicate.
            index += VecTraits::num_lanes();
            pg = VecTraits::svwhilelt(index, length);
          }
        });
  }

 private:
  template <typename T, std::enable_if_t<std::is_same_v<int8_t, T>, int> = 0>
  IntermediateVectorType vector_path(svbool_t& pg, SrcVectorType src)
      INTRINSICCV_STREAMING_COMPATIBLE {
    src = svrinti_f32_x(pg, src);

    svbool_t less = svcmplt_n_f32(pg, src, -128.0);
    src = svdup_n_f32_m(src, less, -128.0);

    svbool_t greater = svcmpgt_n_f32(pg, src, 127.0);
    src = svdup_n_f32_m(src, greater, 127.0);

    return svcvt_s32_f32_x(pg, src);
  }

  template <typename T, std::enable_if_t<std::is_same_v<uint8_t, T>, int> = 0>
  IntermediateVectorType vector_path(svbool_t& pg, SrcVectorType src)
      INTRINSICCV_STREAMING_COMPATIBLE {
    src = svrinti_f32_x(pg, src);

    svbool_t greater = svcmpgt_n_f32(pg, src, 255.0);
    src = svdup_n_f32_m(src, greater, 255.0);

    return svcvt_u32_f32_x(pg, src);
  }
};  // end of class float_to_int_operation<OutputType>

template <typename T>
static intrinsiccv_error_t type_conversion_float_to_int_sc(
    const float* src, size_t src_stride, T* dst, size_t dst_stride,
    size_t width, size_t height) INTRINSICCV_STREAMING_COMPATIBLE {
  CHECK_POINTER_AND_STRIDE(src, src_stride);
  CHECK_POINTER_AND_STRIDE(dst, dst_stride);
  CHECK_IMAGE_SIZE(width, height);

  float_to_int_operation<T> operation;
  Rectangle rect{width, height};
  Rows<const float> src_rows{src, src_stride};
  Rows<T> dst_rows{dst, dst_stride};
  zip_rows(operation, rect, src_rows, dst_rows);

  return INTRINSICCV_OK;
}

}  // namespace INTRINSICCV_TARGET_NAMESPACE

#endif  // INTRINSICCV_FLOAT_TO_INT_SC_H
