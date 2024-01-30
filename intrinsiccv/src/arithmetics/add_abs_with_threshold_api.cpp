// SPDX-FileCopyrightText: 2023 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#include "dispatch.h"
#include "intrinsiccv.h"
#include "types.h"

namespace intrinsiccv {

namespace neon {

template <typename T>
void add_abs_with_threshold(const T *src_a, size_t src_a_stride, const T *src_b,
                            size_t src_b_stride, T *dst, size_t dst_stride,
                            size_t width, size_t height, T threshold);

}  // namespace neon

namespace sve2 {

template <typename T>
void add_abs_with_threshold(const T *src_a, size_t src_a_stride, const T *src_b,
                            size_t src_b_stride, T *dst, size_t dst_stride,
                            size_t width, size_t height, T threshold);

}  // namespace sve2

namespace sme2 {

template <typename T>
void add_abs_with_threshold(const T *src_a, size_t src_a_stride, const T *src_b,
                            size_t src_b_stride, T *dst, size_t dst_stride,
                            size_t width, size_t height, T threshold);

}  // namespace sme2

#define INTRINSICCV_DEFINE_C_API(name, type)                               \
  INTRINSICCV_MULTIVERSION_C_API(                                          \
      name, intrinsiccv::neon::add_abs_with_threshold<type>,               \
      INTRINSICCV_SVE2_IMPL_IF(                                            \
          intrinsiccv::sve2::add_abs_with_threshold<type>),                \
      intrinsiccv::sme2::add_abs_with_threshold<type>, void, const type *, \
      size_t, const type *, size_t, type *, size_t, size_t, size_t, type)

INTRINSICCV_DEFINE_C_API(intrinsiccv_add_abs_with_threshold, int16_t);

}  // namespace intrinsiccv
