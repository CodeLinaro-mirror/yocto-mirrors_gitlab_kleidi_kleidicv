// SPDX-FileCopyrightText: 2023 - 2024 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#include <limits>

#include "intrinsiccv/intrinsiccv.h"
#include "intrinsiccv/neon.h"

namespace intrinsiccv::neon {

template <typename ScalarType>
class MinMax final : public UnrollTwice {
 public:
  using VecTraits = neon::VecTraits<ScalarType>;
  using VectorType = typename VecTraits::VectorType;

  MinMax()
      : vmin_(vdupq_n(std::numeric_limits<ScalarType>::max())),
        vmax_(vdupq_n(std::numeric_limits<ScalarType>::min())),
        min_(std::numeric_limits<ScalarType>::max()),
        max_(std::numeric_limits<ScalarType>::min()) {}

  void vector_path(VectorType src) {
    vmin_ = vminq(vmin_, src);
    vmax_ = vmaxq(vmax_, src);
  }

  void scalar_path(ScalarType src) {
    min_ = std::min(min_, src);
    max_ = std::max(max_, src);
  }

  ScalarType get_min() const {
    return std::min<ScalarType>(min_, vminvq(vmin_));
  }

  ScalarType get_max() const {
    return std::max<ScalarType>(max_, vmaxvq(vmax_));
  }

 private:
  VectorType vmin_, vmax_;
  ScalarType min_, max_;
};  // end of class MinMax<T>

template <typename ScalarType>
intrinsiccv_error_t min_max(const ScalarType *src, size_t src_stride,
                            size_t width, size_t height, ScalarType *min_value,
                            ScalarType *max_value) {
  CHECK_POINTER_AND_STRIDE(src, src_stride);
  CHECK_IMAGE_SIZE(width, height);

  if (INTRINSICCV_UNLIKELY(width == 0 || height == 0)) {
    return INTRINSICCV_ERROR_RANGE;
  }

  Rectangle rect{width, height};
  Rows<const ScalarType> src_rows{src, src_stride};
  MinMax<ScalarType> operation;
  apply_operation_by_rows(operation, rect, src_rows);
  if (min_value) {
    *min_value = operation.get_min();
  }
  if (max_value) {
    *max_value = operation.get_max();
  }
  return INTRINSICCV_OK;
}

#define INTRINSICCV_INSTANTIATE_TEMPLATE(type)                            \
  template INTRINSICCV_TARGET_FN_ATTRS intrinsiccv_error_t min_max<type>( \
      const type *src, size_t src_stride, size_t width, size_t height,    \
      type *min_value, type *max_value)

INTRINSICCV_INSTANTIATE_TEMPLATE(int8_t);
INTRINSICCV_INSTANTIATE_TEMPLATE(uint8_t);
INTRINSICCV_INSTANTIATE_TEMPLATE(int16_t);
INTRINSICCV_INSTANTIATE_TEMPLATE(uint16_t);
INTRINSICCV_INSTANTIATE_TEMPLATE(int32_t);

}  //  namespace intrinsiccv::neon
