// SPDX-FileCopyrightText: 2023 - 2024 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#include "intrinsiccv/dispatch.h"
#include "intrinsiccv/intrinsiccv.h"
#include "intrinsiccv/types.h"

namespace intrinsiccv {

namespace neon {

template <typename T>
intrinsiccv_error_t saturating_add_abs_with_threshold(
    const T *src_a, size_t src_a_stride, const T *src_b, size_t src_b_stride,
    T *dst, size_t dst_stride, size_t width, size_t height, T threshold);

}  // namespace neon

namespace sve2 {
template <typename T>
intrinsiccv_error_t saturating_add_abs_with_threshold(
    const T *src_a, size_t src_a_stride, const T *src_b, size_t src_b_stride,
    T *dst, size_t dst_stride, size_t width, size_t height, T threshold);

}  // namespace sve2
namespace sme2 {
template <typename T>
intrinsiccv_error_t saturating_add_abs_with_threshold(
    const T *src_a, size_t src_a_stride, const T *src_b, size_t src_b_stride,
    T *dst, size_t dst_stride, size_t width, size_t height, T threshold);

}  // namespace sme2

}  // namespace intrinsiccv

#define INTRINSICCV_DEFINE_C_API(name, type)                            \
  INTRINSICCV_MULTIVERSION_C_API(                                       \
      name, intrinsiccv::neon::saturating_add_abs_with_threshold<type>, \
      INTRINSICCV_SVE2_IMPL_IF(                                         \
          intrinsiccv::sve2::saturating_add_abs_with_threshold<type>),  \
      intrinsiccv::sme2::saturating_add_abs_with_threshold<type>)

INTRINSICCV_DEFINE_C_API(intrinsiccv_saturating_add_abs_with_threshold_s16,
                         int16_t);
