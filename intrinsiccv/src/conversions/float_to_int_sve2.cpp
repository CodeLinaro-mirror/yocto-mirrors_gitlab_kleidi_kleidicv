// SPDX-FileCopyrightText: 2024 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#include "float_to_int_sc.h"

namespace intrinsiccv::sve2 {

template <typename T>
INTRINSICCV_TARGET_FN_ATTRS intrinsiccv_error_t
type_conversion_float_to_int(const float* src, size_t src_stride, T* dst,
                             size_t dst_stride, size_t width, size_t height) {
  return type_conversion_float_to_int_sc<T>(src, src_stride, dst, dst_stride,
                                            width, height);
}

#define INTRINSICCV_INSTANTIATE_TEMPLATE(type)                            \
  template INTRINSICCV_TARGET_FN_ATTRS intrinsiccv_error_t                \
  type_conversion_float_to_int<type>(const float* src, size_t src_stride, \
                                     type* dst, size_t dst_stride,        \
                                     size_t width, size_t height)

INTRINSICCV_INSTANTIATE_TEMPLATE(int8_t);
INTRINSICCV_INSTANTIATE_TEMPLATE(uint8_t);

}  // namespace intrinsiccv::sve2
