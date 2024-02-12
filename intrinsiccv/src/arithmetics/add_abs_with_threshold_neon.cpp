// SPDX-FileCopyrightText: 2023 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#include <limits>

#include "intrinsiccv/intrinsiccv.h"
#include "intrinsiccv/neon.h"

namespace intrinsiccv::neon {

template <typename ScalarType>
class AddAbsWithThreshold final : public UnrollOnce, public UnrollTwice {
 public:
  using VecTraits = neon::VecTraits<ScalarType>;
  using VectorType = typename VecTraits::VectorType;

  explicit AddAbsWithThreshold(ScalarType threshold)
      : threshold_{threshold}, threshold_vec_{vdupq_n_s16(threshold)} {}

  VectorType vector_path(VectorType src_a, VectorType src_b) {
    VectorType add_abs = vaddq_s16(vabsq_s16(src_a), vabsq_s16(src_b));
    return vandq_s16(add_abs, vcgtq_s16(add_abs, threshold_vec_));
  }

  ScalarType scalar_path(ScalarType src_a, ScalarType src_b) {
    ScalarType add_abs = std::abs(src_a) + std::abs(src_b);
    return add_abs > threshold_ ? add_abs : 0;
  }

 private:
  ScalarType threshold_;
  VectorType threshold_vec_;
};  // end of class AddAbsWithThreshold<ScalarType>

template <typename T>
intrinsiccv_error_t add_abs_with_threshold(const T *src_a, size_t src_a_stride,
                                           const T *src_b, size_t src_b_stride,
                                           T *dst, size_t dst_stride,
                                           size_t width, size_t height,
                                           T threshold) {
  CHECK_POINTER_AND_STRIDE(src_a, src_a_stride);
  CHECK_POINTER_AND_STRIDE(src_b, src_b_stride);
  CHECK_POINTER_AND_STRIDE(dst, dst_stride);

  AddAbsWithThreshold<T> operation{threshold};
  Rectangle rect{width, height};
  Rows<const T> src_a_rows{src_a, src_a_stride};
  Rows<const T> src_b_rows{src_b, src_b_stride};
  Rows<T> dst_rows{dst, dst_stride};
  apply_operation_by_rows(operation, rect, src_a_rows, src_b_rows, dst_rows);
  return INTRINSICCV_OK;
}

#define INTRINSICCV_INSTANTIATE_TEMPLATE(type)                             \
  template INTRINSICCV_TARGET_FN_ATTRS intrinsiccv_error_t                 \
  add_abs_with_threshold<type>(const type *src_a, size_t src_a_stride,     \
                               const type *src_b, size_t src_b_stride,     \
                               type *dst, size_t dst_stride, size_t width, \
                               size_t height, type threshold)

INTRINSICCV_INSTANTIATE_TEMPLATE(int16_t);

}  // namespace intrinsiccv::neon
