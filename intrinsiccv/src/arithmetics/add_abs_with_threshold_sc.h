// SPDX-FileCopyrightText: 2023 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#ifndef INTRINSICCV_ADD_ABS_WITH_THRESHOLD_SC_H
#define INTRINSICCV_ADD_ABS_WITH_THRESHOLD_SC_H

#include <limits>

#include "intrinsiccv.h"
#include "sve2.h"

namespace intrinsiccv::sve2 {

template <typename ScalarType>
class AddAbsWithThreshold final : public UnrollTwice {
 public:
  using ContextType = sve2::Context;
  using VecTraits = sve2::VecTraits<ScalarType>;
  using VectorType = typename VecTraits::VectorType;

  explicit AddAbsWithThreshold(ScalarType threshold)
      INTRINSICCV_STREAMING_COMPATIBLE : threshold_(threshold) {}

  VectorType vector_path(ContextType ctx, VectorType src_a,
                         VectorType src_b) INTRINSICCV_STREAMING_COMPATIBLE {
    auto pg = ctx.predicate();
    VectorType add_abs = svadd_x(pg, svabs_x(pg, src_a), svabs_x(pg, src_b));
    svbool_t predicate = svcmpgt(pg, add_abs, threshold_);
    return svsel(predicate, add_abs, VecTraits::svdup(0));
  }

 private:
  ScalarType threshold_;
};  // end of class AddAbsWithThreshold<ScalarType>

template <typename T>
void add_abs_with_threshold_sc(const T *src_a, size_t src_a_stride,
                               const T *src_b, size_t src_b_stride, T *dst,
                               size_t dst_stride, size_t width, size_t height,
                               T threshold) INTRINSICCV_STREAMING_COMPATIBLE {
  AddAbsWithThreshold<T> operation{threshold};
  Rectangle rect{width, height};
  Rows<const T> src_a_rows{src_a, src_a_stride};
  Rows<const T> src_b_rows{src_b, src_b_stride};
  Rows<T> dst_rows{dst, dst_stride};
  sve2::apply_operation_by_rows(operation, rect, src_a_rows, src_b_rows,
                                dst_rows);
}

}  // namespace intrinsiccv::sve2

#endif  // INTRINSICCV_ADD_ABS_WITH_THRESHOLD_SC_H
