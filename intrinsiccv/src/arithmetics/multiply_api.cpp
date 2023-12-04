// SPDX-FileCopyrightText: 2023 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#include "dispatch.h"
#include "intrinsiccv.h"
#include "types.h"

namespace intrinsiccv {

namespace neon {

template <typename T>
void saturating_multiply(const T *src_a, size_t src_a_stride, const T *src_b,
                         size_t src_b_stride, T *dst, size_t dst_stride,
                         size_t width, size_t height, double scale);

}  // namespace neon

namespace sve2 {

template <typename T>
void saturating_multiply(const T *src_a, size_t src_a_stride, const T *src_b,
                         size_t src_b_stride, T *dst, size_t dst_stride,
                         size_t width, size_t height, double scale);

}  // namespace sve2

// namespace sme2 {

// template <typename T>
// void saturating_multiply(const T *src_a, size_t src_a_stride, const T *src_b,
//               size_t src_b_stride, T *dst, size_t dst_stride,
//               size_t width, size_t height, double scale);

// } // namespace sme2

#define INTRINSICCV_DEFINE_C_API(name, type)                                 \
  static IFuncImpls name##_impls_builder(void) {                             \
    IFuncImpls impls;                                                        \
    INTRINSICCV_ADD_NEON_IMPL(intrinsiccv::neon::saturating_multiply<type>); \
    INTRINSICCV_ADD_SVE2_IMPL_IF(                                            \
        intrinsiccv::sve2::saturating_multiply<type>);                       \
    return impls;                                                            \
  }                                                                          \
  INTRINSICCV_MULTIVERSION_C_API(name, name##_impls_builder, void,           \
                                 const type *, size_t, const type *, size_t, \
                                 type *, size_t, size_t, size_t, double)

INTRINSICCV_DEFINE_C_API(saturating_multiply_u8, uint8_t);
INTRINSICCV_DEFINE_C_API(saturating_multiply_s8, int8_t);
INTRINSICCV_DEFINE_C_API(saturating_multiply_u16, uint16_t);
INTRINSICCV_DEFINE_C_API(saturating_multiply_s16, int16_t);
INTRINSICCV_DEFINE_C_API(saturating_multiply_s32, int32_t);

}  // namespace intrinsiccv
