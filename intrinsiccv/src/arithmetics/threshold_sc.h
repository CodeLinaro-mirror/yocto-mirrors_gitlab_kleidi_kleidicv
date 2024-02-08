// SPDX-FileCopyrightText: 2023 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#ifndef INTRINSICCV_THRESHOLD_SC_H
#define INTRINSICCV_THRESHOLD_SC_H

#include "intrinsiccv.h"
#include "sve2.h"

namespace intrinsiccv::sve2 {

template <typename ScalarType>
class BinaryThreshold final : public UnrollTwice {
 public:
  using ContextType = sve2::Context;
  using VecTraits = sve2::VecTraits<ScalarType>;
  using VectorType = typename VecTraits::VectorType;

  BinaryThreshold(ScalarType threshold,
                  ScalarType value) INTRINSICCV_STREAMING_COMPATIBLE
      : threshold_(threshold),
        value_(value) {}

  VectorType vector_path(ContextType ctx,
                         VectorType src) INTRINSICCV_STREAMING_COMPATIBLE {
    svbool_t predicate = svcmpgt(ctx.predicate(), src, threshold_);
    return svsel_u8(predicate, svdup_u8(value_), svdup_u8(0));
  }

 private:
  ScalarType threshold_;
  ScalarType value_;
};  // end of class BinaryThreshold<ScalarType>

template <typename T>
intrinsiccv_error_t threshold_binary_sc(
    const T *src, size_t src_stride, T *dst, size_t dst_stride, size_t width,
    size_t height, T threshold, T value) INTRINSICCV_STREAMING_COMPATIBLE {
  CHECK_POINTER_AND_STRIDE(src, src_stride);
  CHECK_POINTER_AND_STRIDE(dst, dst_stride);

  Rectangle rect{width, height};
  Rows<const T> src_rows{src, src_stride};
  Rows<T> dst_rows{dst, dst_stride};
  BinaryThreshold<T> operation{threshold, value};
  sve2::apply_operation_by_rows(operation, rect, src_rows, dst_rows);
  return INTRINSICCV_OK;
}

}  // namespace intrinsiccv::sve2

#endif  // INTRINSICCV_THRESHOLD_SC_H
