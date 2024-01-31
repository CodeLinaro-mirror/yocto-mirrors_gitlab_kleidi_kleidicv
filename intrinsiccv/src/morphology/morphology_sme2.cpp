// SPDX-FileCopyrightText: 2023 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#include "morphology_sc.h"

namespace intrinsiccv::sme2 {

template <typename T>
INTRINSICCV_LOCALLY_STREAMING INTRINSICCV_TARGET_FN_ATTRS intrinsiccv_error_t
dilate(const T *src, size_t src_stride, T *dst, size_t dst_stride, size_t width,
       size_t height, const intrinsiccv_morphology_params_t *params) {
  return intrinsiccv::sve2::dilate_sc<T>(src, src_stride, dst, dst_stride,
                                         width, height, params);
}

template <typename T>
INTRINSICCV_LOCALLY_STREAMING INTRINSICCV_TARGET_FN_ATTRS intrinsiccv_error_t
erode(const T *src, size_t src_stride, T *dst, size_t dst_stride, size_t width,
      size_t height, const intrinsiccv_morphology_params_t *params) {
  return intrinsiccv::sve2::erode_sc<T>(src, src_stride, dst, dst_stride, width,
                                        height, params);
}

#define INTRINSICCV_INSTANTIATE_TEMPLATE(name, type)                    \
  template INTRINSICCV_TARGET_FN_ATTRS intrinsiccv_error_t name<type>(  \
      const type *src, size_t src_stride, type *dst, size_t dst_stride, \
      size_t width, size_t height,                                      \
      const intrinsiccv_morphology_params_t *params)

INTRINSICCV_INSTANTIATE_TEMPLATE(dilate, uint8_t);
INTRINSICCV_INSTANTIATE_TEMPLATE(erode, uint8_t);

}  // namespace intrinsiccv::sme2
