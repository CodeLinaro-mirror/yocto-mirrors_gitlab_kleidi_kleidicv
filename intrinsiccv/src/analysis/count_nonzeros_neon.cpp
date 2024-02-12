// SPDX-FileCopyrightText: 2023 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#include <limits>
#include <type_traits>

#include "intrinsiccv/intrinsiccv.h"
#include "intrinsiccv/neon.h"

namespace intrinsiccv {
namespace neon {

template <typename ScalarType>
class CountNonZeros final : public UnrollTwice {
 public:
  using VecTraits = neon::VecTraits<ScalarType>;
  using VectorType = typename VecTraits::VectorType;

  CountNonZeros() : accumulator_(0), v_accumulator_(vdupq_n_u8(0)) {}

  void vector_path(VectorType src) {
    v_accumulator_ = vaddq_u8(v_accumulator_, vtstq_u8(src, src));
  }

  void scalar_path(ScalarType src) { accumulator_ += !!src; }

  void on_block_finished(size_t) {
    accumulator_ += vaddlvq_u8(vnegq_s8(v_accumulator_));
    v_accumulator_ = vdupq_n_u8(0);
  }

  size_t max_vectors_per_block() const {
    return std::numeric_limits<std::make_unsigned_t<ScalarType>>::max();
  }

  size_t result() { return accumulator_; }

 private:
  size_t accumulator_;
  VectorType v_accumulator_;
};  // end of class CountNonZeros<ScalarType>

template <typename T>
static size_t count_nonzeros_impl(Rows<const T> src, Rectangle rect) {
  CountNonZeros<T> operation;
  apply_block_operation_by_rows(operation, rect, src);
  return operation.result();
}

}  // namespace neon

template <typename T>
INTRINSICCV_TARGET_FN_ATTRS static intrinsiccv_error_t count_nonzeros(
    const T *src, size_t src_stride, size_t width, size_t height,
    size_t *count) {
  CHECK_POINTERS(count);
  CHECK_POINTER_AND_STRIDE(src, src_stride);
  Rectangle rect{width, height};
  Rows<const T> src_rows{src, src_stride};
  *count = neon::count_nonzeros_impl(src_rows, rect);
  return INTRINSICCV_OK;
}

extern "C" {

INTRINSICCV_TARGET_FN_ATTRS
intrinsiccv_error_t intrinsiccv_count_nonzeros_u8(const uint8_t *src,
                                                  size_t src_stride,
                                                  size_t width, size_t height,
                                                  size_t *count) {
  return count_nonzeros<uint8_t>(src, src_stride, width, height, count);
}

}  // extern "C"

}  // namespace intrinsiccv
